#ifndef SLANG_CORE_SMART_POINTER_H
#define SLANG_CORE_SMART_POINTER_H

#include "slang-common.h"
#include "slang-hash.h"
#include "slang-type-traits.h"

#include <atomic>
#include <thread>

#include "../../slang.h"

namespace Slang
{

    // Base class for all reference-counted objects
    class RefObject
    {
    private:
        UInt referenceCount;

    public:
        RefObject()
            : referenceCount(0)
        {}

        RefObject(const RefObject &)
            : referenceCount(0)
        {}

        RefObject& operator=(const RefObject& rhs) = default;

        virtual ~RefObject()
        {}

        UInt addReference()
        {
            return ++referenceCount;
        }

        UInt decreaseReference()
        {
            return --referenceCount;
        }

        UInt releaseReference()
        {
            SLANG_ASSERT(referenceCount != 0);
            if(--referenceCount == 0)
            {
                delete this;
                return 0;
            }
            return referenceCount;
        }

        bool isUniquelyReferenced()
        {
            SLANG_ASSERT(referenceCount != 0);
            return referenceCount == 1;
        }

        UInt debugGetReferenceCount()
        {
            return referenceCount;
        }
    };

    SLANG_FORCE_INLINE void addReference(RefObject* obj)
    {
        if(obj) obj->addReference();
    }

    SLANG_FORCE_INLINE void releaseReference(RefObject* obj)
    {
        if(obj) obj->releaseReference();
    }

    // For straight dynamic cast.
    // Use instead of dynamic_cast as it allows for replacement without using Rtti in the future
    template <typename T>
    SLANG_FORCE_INLINE T* dynamicCast(RefObject* obj) { return dynamic_cast<T*>(obj); }
    template <typename T>
    SLANG_FORCE_INLINE const T* dynamicCast(const RefObject* obj) { return dynamic_cast<const T*>(obj); }

    // Like a dynamicCast, but allows a type to implement a specific implementation that is suitable for it
    template <typename T>
    SLANG_FORCE_INLINE T* as(RefObject* obj) { return dynamicCast<T>(obj); }
    template <typename T>
    SLANG_FORCE_INLINE const T* as(const RefObject* obj) { return dynamicCast<T>(obj); }

    // "Smart" pointer to a reference-counted object
    template<typename T>
    struct RefPtr
    {
        RefPtr()
            : pointer(nullptr)
        {}

        RefPtr(T* p)
            : pointer(p)
        {
            addReference(p);
        }

        RefPtr(RefPtr<T> const& p)
            : pointer(p.pointer)
        {
            addReference(p.pointer);
        }

        RefPtr(RefPtr<T>&& p)
            : pointer(p.pointer)
        {
            p.pointer = nullptr;
        }

        template <typename U>
        RefPtr(RefPtr<U> const& p,
            typename EnableIf<IsConvertible<T*, U*>::Value, void>::type * = 0)
            : pointer(static_cast<U*>(p))
        {
            addReference(static_cast<U*>(p));
        }

#if 0
        void operator=(T* p)
        {
            T* old = pointer;
            addReference(p);
            pointer = p;
            releaseReference(old);
        }
#endif

        void operator=(RefPtr<T> const& p)
        {
            T* old = pointer;
            addReference(p.pointer);
            pointer = p.pointer;
            releaseReference(old);
        }

        void operator=(RefPtr<T>&& p)
        {
            T* old = pointer;
            pointer = p.pointer;
            p.pointer = old;
        }

        template <typename U>
        typename EnableIf<IsConvertible<T*, U*>::value, void>::type
            operator=(RefPtr<U> const& p)
        {
            T* old = pointer;
            addReference(p.pointer);
            pointer = p.pointer;
            releaseReference(old);
        }

        HashCode getHashCode()
        {
            // Note: We need a `RefPtr<T>` to hash the same as a `T*`,
            // so that a `T*` can be used as a key in a dictionary with
            // `RefPtr<T>` keys, and vice versa.
            //
            return Slang::getHashCode(pointer);
        }

        bool operator==(const T * ptr) const
        {
            return pointer == ptr;
        }

        bool operator!=(const T * ptr) const
        {
            return pointer != ptr;
        }

		bool operator==(RefPtr<T> const& ptr) const
		{
			return pointer == ptr.pointer;
		}

		bool operator!=(RefPtr<T> const& ptr) const
		{
			return pointer != ptr.pointer;
		}

        template<typename U>
        RefPtr<U> dynamicCast() const
        {
            return RefPtr<U>(Slang::dynamicCast<U>(pointer));
        }

        template<typename U>
        RefPtr<U> as() const
        {
            return RefPtr<U>(Slang::as<U>(pointer));
        }

        template <typename U>
        bool is() const { return Slang::as<U>(pointer) != nullptr; }

        ~RefPtr()
        {
            releaseReference(static_cast<Slang::RefObject*>(pointer));
        }

        T& operator*() const
        {
            return *pointer;
        }

        T* operator->() const
        {
            return pointer;
        }

		T * Ptr() const
		{
			return pointer;
		}

        operator T*() const
        {
            return pointer;
        }

        void attach(T* p)
        {
            T* old = pointer;
            pointer = p;
            releaseReference(old);
        }

        T* detach()
        {
            auto rs = pointer;
            pointer = nullptr;
            return rs;
        }

        SLANG_FORCE_INLINE void setNull()
        {
            releaseReference(pointer);
            pointer = nullptr;
        }

        /// Get ready for writing (nulls contents)
        SLANG_FORCE_INLINE T** writeRef() { *this = nullptr; return &pointer; }

        /// Get for read access
        SLANG_FORCE_INLINE T*const* readRef() const { return &pointer; }

    private:
        T* pointer;
	};

    // Base class for all reference-counted objects
    class AtomicRefObject
    {
    private:
        UInt referenceCount;

        std::thread::id m_threadId = std::this_thread::get_id();

    public:
        AtomicRefObject()
            : referenceCount(0)
        {}

        AtomicRefObject(const RefObject&)
            : referenceCount(0)
        {}

        AtomicRefObject& operator=(const AtomicRefObject& rhs) = default;

        virtual ~AtomicRefObject()
        {}

        UInt addReference()
        {
            checkThreadID();
            return ++referenceCount;
        }

        UInt decreaseReference()
        {
            checkThreadID();
            return --referenceCount;
        }

        UInt releaseReference()
        {
            checkThreadID();

            SLANG_ASSERT(referenceCount != 0);
            if (--referenceCount == 0)
            {
                delete this;
                return 0;
            }
            return referenceCount;
        }

        bool isUniquelyReferenced()
        {
            SLANG_ASSERT(referenceCount != 0);
            return referenceCount == 1;
        }

        UInt debugGetReferenceCount()
        {
            return referenceCount;
        }

        void checkThreadID()
        {
            if (m_threadId != std::this_thread::get_id())
            {
                SLANG_BREAKPOINT(0);
            }
        }
    };

    SLANG_FORCE_INLINE void addReference(AtomicRefObject* obj)
    {
        if (obj) obj->addReference();
    }

    SLANG_FORCE_INLINE void releaseReference(AtomicRefObject* obj)
    {
        if (obj) obj->releaseReference();
    }

    // For straight dynamic cast.
    // Use instead of dynamic_cast as it allows for replacement without using Rtti in the future
    template <typename T>
    SLANG_FORCE_INLINE T* dynamicCast(AtomicRefObject* obj) { return dynamic_cast<T*>(obj); }
    template <typename T>
    SLANG_FORCE_INLINE const T* dynamicCast(const AtomicRefObject* obj) { return dynamic_cast<const T*>(obj); }

    // Like a dynamicCast, but allows a type to implement a specific implementation that is suitable for it
    template <typename T>
    SLANG_FORCE_INLINE T* as(AtomicRefObject* obj) { return dynamicCast<T>(obj); }
    template <typename T>
    SLANG_FORCE_INLINE const T* as(const AtomicRefObject* obj) { return dynamicCast<T>(obj); }

    // "Smart" pointer to a reference-counted object
    template<typename T>
    struct AtomicRefPtr
    {
        AtomicRefPtr()
            : pointer(nullptr)
        {}

        AtomicRefPtr(T* p)
            : pointer(p)
        {
            addReference(p);
        }

        AtomicRefPtr(AtomicRefPtr<T> const& p)
            : pointer(p.pointer)
        {
            addReference(p.pointer);
        }

        AtomicRefPtr(AtomicRefPtr<T>&& p)
            : pointer(p.pointer)
        {
            p.pointer = nullptr;
        }

        template <typename U>
        AtomicRefPtr(AtomicRefPtr<U> const& p,
            typename EnableIf<IsConvertible<T*, U*>::Value, void>::type* = 0)
            : pointer(static_cast<U*>(p))
        {
            addReference(static_cast<U*>(p));
        }

#if 0
        void operator=(T* p)
        {
            T* old = pointer;
            addReference(p);
            pointer = p;
            releaseReference(old);
        }
#endif

        void operator=(AtomicRefPtr<T> const& p)
        {
            T* old = pointer;
            addReference(p.pointer);
            pointer = p.pointer;
            releaseReference(old);
        }

        void operator=(AtomicRefPtr<T>&& p)
        {
            T* old = pointer;
            pointer = p.pointer;
            p.pointer = old;
        }

        template <typename U>
        typename EnableIf<IsConvertible<T*, U*>::value, void>::type
            operator=(AtomicRefPtr<U> const& p)
        {
            T* old = pointer;
            addReference(p.pointer);
            pointer = p.pointer;
            releaseReference(old);
        }

        HashCode getHashCode()
        {
            // Note: We need a `RefPtr<T>` to hash the same as a `T*`,
            // so that a `T*` can be used as a key in a dictionary with
            // `RefPtr<T>` keys, and vice versa.
            //
            return Slang::getHashCode(pointer);
        }

        bool operator==(const T* ptr) const
        {
            return pointer == ptr;
        }

        bool operator!=(const T* ptr) const
        {
            return pointer != ptr;
        }

        bool operator==(AtomicRefPtr<T> const& ptr) const
        {
            return pointer == ptr.pointer;
        }

        bool operator!=(AtomicRefPtr<T> const& ptr) const
        {
            return pointer != ptr.pointer;
        }

        template<typename U>
        AtomicRefPtr<U> dynamicCast() const
        {
            return AtomicRefPtr<U>(Slang::dynamicCast<U>(pointer));
        }

        template<typename U>
        AtomicRefPtr<U> as() const
        {
            return AtomicRefPtr<U>(Slang::as<U>(pointer));
        }

        template <typename U>
        bool is() const { return Slang::as<U>(pointer) != nullptr; }

        ~AtomicRefPtr()
        {
            releaseReference(static_cast<Slang::AtomicRefObject*>(pointer));
        }

        T& operator*() const
        {
            return *pointer;
        }

        T* operator->() const
        {
            return pointer;
        }

        T* Ptr() const
        {
            return pointer;
        }

        operator T* () const
        {
            return pointer;
        }

        void attach(T* p)
        {
            T* old = pointer;
            pointer = p;
            releaseReference(old);
        }

        T* detach()
        {
            auto rs = pointer;
            pointer = nullptr;
            return rs;
        }

        SLANG_FORCE_INLINE void setNull()
        {
            releaseReference(pointer);
            pointer = nullptr;
        }

        /// Get ready for writing (nulls contents)
        SLANG_FORCE_INLINE T** writeRef() { *this = nullptr; return &pointer; }

        /// Get for read access
        SLANG_FORCE_INLINE T* const* readRef() const { return &pointer; }

    private:
        T* pointer;
    };

    // Helper type for implementing weak pointers. The object being pointed at weakly creates a WeakSink object
    // that other objects can reference and share. When the object is destroyed it detaches the sink
    // doing so will make other users call to 'get' return null. Thus any user of the WeakSink, must check if the weakly pointed to
    // things pointer is nullptr before using.
    template <typename T>
    class WeakSink : public RefObject
    {
    public:
        WeakSink(T* ptr):
            m_ptr(ptr)
        {
        }

        SLANG_FORCE_INLINE T* get() const { return m_ptr; }
        SLANG_FORCE_INLINE void detach() { m_ptr = nullptr; }

    private:
        T* m_ptr;
    };

    // A pointer that can be transformed to hold either a weak reference or a strong reference.
    template<typename T>
    class TransformablePtr
    {
    private:
        T* m_weakPtr = nullptr;
        RefPtr<T> m_strongPtr;

    public:
        TransformablePtr() = default;
        TransformablePtr(T* ptr) { *this = ptr; }
        TransformablePtr(RefPtr<T> ptr) { *this = ptr; }
        TransformablePtr(const TransformablePtr<T>& ptr) = default;

        void promoteToStrongReference() { m_strongPtr = m_weakPtr; }
        void demoteToWeakReference() { m_strongPtr = nullptr; }
        bool isStrongReference() const { return m_strongPtr != nullptr; }

        T& operator*() const { return *m_weakPtr; }

        T* operator->() const { return m_weakPtr; }

        T* Ptr() const { return m_weakPtr; }
        T* get() const { return m_weakPtr; }

        operator T*() const { return m_weakPtr; }
        operator RefPtr<T>() const { return m_weakPtr; }


        TransformablePtr<T>& operator=(T* ptr)
        {
            m_weakPtr = ptr;
            m_strongPtr = ptr;
            return *this;
        }
        template<typename U>
        TransformablePtr<T>& operator=(const RefPtr<U>& ptr)
        {
            m_weakPtr = ptr.Ptr();
            m_strongPtr = ptr;
            return *this;
        }
        
        HashCode getHashCode() const
        {
            // Note: We need a `RefPtr<T>` to hash the same as a `T*`,
            // so that a `T*` can be used as a key in a dictionary with
            // `RefPtr<T>` keys, and vice versa.
            //
            return Slang::getHashCode(m_weakPtr);
        }

        bool operator==(const T* ptr) const { return m_weakPtr == ptr; }

        bool operator!=(const T* ptr) const { return m_weakPtr != ptr; }

        bool operator==(RefPtr<T> const& ptr) const { return m_weakPtr == ptr.Ptr(); }

        bool operator!=(RefPtr<T> const& ptr) const { return m_weakPtr != ptr.Ptr(); }

        bool operator==(TransformablePtr<T> const& ptr) const { return m_weakPtr == ptr.m_weakPtr; }

        bool operator!=(TransformablePtr<T> const& ptr) const { return m_weakPtr != ptr.m_weakPtr; }
    };
}
#endif
