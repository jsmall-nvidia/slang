// slang-ast-serialize.h
#ifndef SLANG_AST_SERIALIZE_H
#define SLANG_AST_SERIALIZE_H

#include <type_traits>

#include "slang-ast-support-types.h"
#include "slang-ast-all.h"

#include "../core/slang-byte-encode-util.h"

#include "../core/slang-stream.h"

namespace Slang
{

class ASTSerialClasses;

// Type used to implement mechanisms to convert to and from serial types.
template <typename T>
struct ASTSerialTypeInfo;

struct ASTSerialInfo
{
    enum
    {
        // Data held in serialized format, the maximally allowed alignment 
        MAX_ALIGNMENT = 8,
    };

    // We only allow up to MAX_ALIGNMENT bytes of alignment. We store alignments as shifts, so 2 bits needed for 1 - 8
    enum class EntryInfo : uint8_t
    {
        Alignment1 = 0,
    };

    static EntryInfo makeEntryInfo(int alignment, int nextAlignment)
    {
        // Make sure they are power of 2
        SLANG_ASSERT((alignment & (alignment - 1)) == 0);
        SLANG_ASSERT((nextAlignment & (nextAlignment - 1)) == 0);

        const int alignmentShift = ByteEncodeUtil::calcMsb8(alignment);
        const int nextAlignmentShift = ByteEncodeUtil::calcMsb8(nextAlignment);
        return EntryInfo((nextAlignmentShift << 2) | alignmentShift);
    }
    static EntryInfo makeEntryInfo(int alignment)
    {
        // Make sure they are power of 2
        SLANG_ASSERT((alignment & (alignment - 1)) == 0);
        return EntryInfo(ByteEncodeUtil::calcMsb8(alignment));
    }
        /// Apply with the next alignment
    static EntryInfo combineWithNext(EntryInfo cur, EntryInfo next)
    {
        return EntryInfo((int(cur) & ~0xc0) | ((int(next) & 3) << 2));
    }

    static int getAlignment(EntryInfo info) { return 1 << (int(info) & 3); }
    static int getNextAlignment(EntryInfo info) { return 1 << ((int(info) >> 2) & 3); }

    enum class Type : uint8_t
    {
        String,             ///< String                         
        Node,               ///< NodeBase derived
        RefObject,          ///< RefObject derived types          
        Array,              ///< Array
    };

    
    /* Alignment is a little tricky. We have a 'Entry' header before the payload. The payload alignment may change.
    If we only align on the Entry header, then it's size *must* be some modulo of the maximum alignment allowed.

    We could hold Entry separate from payload. We could make the header not require the alignment of the payload - but then
    we'd need payload alignment separate from entry alignment.
    */
    struct Entry
    {
        Type type;
        EntryInfo info;

        size_t calcSize(ASTSerialClasses* serialClasses) const;
    };

    struct StringEntry : Entry
    {
        char sizeAndChars[1];
    };

    struct NodeEntry : Entry
    {
        uint16_t astNodeType;
        uint32_t _pad0;             ///< Necessary, because a node *can* have MAX_ALIGNEMENT
    };

    struct RefObjectEntry : Entry
    {
        enum class SubType : uint8_t
        {
            Breadcrumb,
        };
        SubType subType;
        uint8_t _pad0;
        uint32_t _pad1;             ///< Necessary because RefObjectEntry *can* have MAX_ALIGNEMENT
    };

    struct ArrayEntry : Entry
    {
        uint16_t elementSize;
        uint32_t elementCount;
    };
};

typedef uint32_t ASTSerialIndexRaw;
enum class ASTSerialIndex : ASTSerialIndexRaw;
typedef uint32_t ASTSerialSourceLoc;

/* A type to convert pointers into types such that they can be passed around to readers/writers without
having to know the specific type. If there was a base class that all the serialized types derived from,
that was dynamically castable this would not be necessary */
struct ASTSerialPointer
{
    enum class Kind
    {
        Unknown,
        RefObject,
        NodeBase
    };

    // Helpers so we can choose what kind of pointer we have based on the (unused) type of the pointer passed in
    SLANG_FORCE_INLINE RefObject* _get(const RefObject*) { return m_kind == Kind::RefObject ? reinterpret_cast<RefObject*>(m_ptr) : nullptr; }
    SLANG_FORCE_INLINE NodeBase* _get(const NodeBase*) { return m_kind == Kind::NodeBase ? reinterpret_cast<NodeBase*>(m_ptr) : nullptr; }

    template <typename T>
    T* dynamicCast()
    {
        return Slang::dynamicCast<T>(_get((T*)nullptr));
    }

    ASTSerialPointer() :
        m_kind(Kind::Unknown),
        m_ptr(nullptr)
    {
    }

    ASTSerialPointer(RefObject* in) :
        m_kind(Kind::RefObject),
        m_ptr((void*)in)
    {
    }
    ASTSerialPointer(NodeBase* in) :
        m_kind(Kind::NodeBase),
        m_ptr((void*)in)
    {
    }

    static Kind getKind(const RefObject*) { return Kind::RefObject; }
    static Kind getKind(const NodeBase*) { return Kind::NodeBase; }

    Kind m_kind;
    void* m_ptr;
};


/* This class is the interface used by toNative implementations to recreate a type */
class ASTSerialReader : public RefObject
{
public:

    typedef ASTSerialInfo::Entry Entry;
    typedef ASTSerialInfo::Type Type;

    template <typename T>
    void getArray(ASTSerialIndex index, List<T>& out);

    const void* getArray(ASTSerialIndex index, Index& outCount);

    ASTSerialPointer getPointer(ASTSerialIndex index);
    String getString(ASTSerialIndex index);
    Name* getName(ASTSerialIndex index);
    UnownedStringSlice getStringSlice(ASTSerialIndex index);
    SourceLoc getSourceLoc(ASTSerialSourceLoc loc);

        /// NOTE! data must stay ins scope when reading takes place
    SlangResult load(const uint8_t* data, ASTBuilder* builder, NamePool* namePool, size_t dataCount);

    ASTSerialReader(ASTSerialClasses* classes):
        m_classes(classes)
    {
    }

protected:
    List<const Entry*> m_entries;       ///< The entries
    List<void*> m_objects;              ///< The constructed objects

    List<RefPtr<RefObject>> m_scope;    ///< Objects to keep in scope during construction

    NamePool* m_namePool;

    ASTSerialClasses* m_classes;        ///< Used to deserialize 
};

// ---------------------------------------------------------------------------
template <typename T>
void ASTSerialReader::getArray(ASTSerialIndex index, List<T>& out)
{
    typedef ASTSerialTypeInfo<T> ElementTypeInfo;
    typedef typename ElementTypeInfo::SerialType ElementSerialType;

    Index count;
    auto serialElements = (const ElementSerialType*)getArray(index, count);

    if (count == 0)
    {
        out.clear();
        return;
    }

    if (std::is_same<T, ElementSerialType>::value)
    {
        // If they are the same we can just write out
        out.clear();
        out.insertRange(0, (const T*)serialElements, count);
    }
    else
    {
        // Else we need to convert
        out.setCount(count);
        for (Index i = 0; i < count; ++i)
        {
            ElementTypeInfo::toNative(this, (const void*)&serialElements[i], (void*)&out[i]);
        }
    }
}


class ASTSerialClasses;

/* This is a class used tby toSerial implementations to turn native type into the serial type */
class ASTSerialWriter : public RefObject
{
public:
    ASTSerialIndex addPointer(const NodeBase* ptr);
    ASTSerialIndex addPointer(const RefObject* ptr);

    template <typename T>
    ASTSerialIndex addArray(const T* in, Index count);

    ASTSerialIndex addString(const UnownedStringSlice& slice);
    ASTSerialIndex addString(const String& in);
    ASTSerialIndex addName(const Name* name);
    ASTSerialSourceLoc addSourceLoc(SourceLoc sourceLoc);

        /// Write to a stream
    SlangResult write(Stream* stream);

    ASTSerialWriter(ASTSerialClasses* classes);

protected:

    ASTSerialIndex _addArray(size_t elementSize, size_t alignment, const void* elements, Index elementCount);

    ASTSerialIndex _add(const void* nativePtr, ASTSerialInfo::Entry* entry)
    {
        m_entries.add(entry);
        // Okay I need to allocate space for this
        ASTSerialIndex index = ASTSerialIndex(m_entries.getCount() - 1);
        // Add to the map
        m_ptrMap.Add(nativePtr, Index(index));
        return index;
    }

    Dictionary<const void*, Index> m_ptrMap;    // Maps a pointer to an entry index

    // NOTE! Assumes the content stays in scope!
    Dictionary<UnownedStringSlice, Index> m_sliceMap;

    List<ASTSerialInfo::Entry*> m_entries;      ///< The entries
    MemoryArena m_arena;                        ///< Holds the payloads
    ASTSerialClasses* m_classes;
};

// ---------------------------------------------------------------------------
template <typename T>
ASTSerialIndex ASTSerialWriter::addArray(const T* in, Index count)
{
    typedef ASTSerialTypeInfo<T> ElementTypeInfo;
    typedef typename ElementTypeInfo::SerialType ElementSerialType;

    if (std::is_same<T, ElementSerialType>::value)
    {
        // If they are the same we can just write out
        return _addArray(sizeof(T), SLANG_ALIGN_OF(ElementSerialType), in, count);
    }
    else
    {
        // Else we need to convert
        List<ElementSerialType> work;
        work.setCount(count);

        for (Index i = 0; i < count; ++i)
        {
            ElementTypeInfo::toSerial(this, &in[i], &work[i]);
        }
        return _addArray(sizeof(ElementSerialType), SLANG_ALIGN_OF(ElementSerialType), in, count);
    }
}

struct ASTSerialType
{
    typedef void(*ToSerialFunc)(ASTSerialWriter* writer, const void* src, void* dst);
    typedef void(*ToNativeFunc)(ASTSerialReader* reader, const void* src, void* dst);

    size_t serialSizeInBytes;
    uint8_t serialAlignment;
    ToSerialFunc toSerialFunc;
    ToNativeFunc toNativeFunc;
};

struct ASTSerialField
{
    const char* name;           ///< The name of the field
    const ASTSerialType* type;        ///< The type of the field
    uint32_t nativeOffset;      ///< Offset to field from base of type
    uint32_t serialOffset;      ///< Offset in serial type    
};


struct ASTSerialClass
{
    ASTNodeType type;
    uint8_t alignment;
    ASTSerialField* fields;
    Index fieldsCount;
    uint32_t size;
};

// An instance could be shared across Sessions, but for simplicity of life time
// here we don't deal with that 
class ASTSerialClasses : public RefObject
{
public:

    const ASTSerialClass* getSerialClass(ASTNodeType type) const { return &m_classes[Index(type)]; }

        /// Ctor
    ASTSerialClasses();

protected:
    MemoryArena m_arena;

    ASTSerialClass m_classes[Index(ASTNodeType::CountOf)];
};

struct ASTSerializeUtil
{
    static SlangResult selfTest();
};

} // namespace Slang

#endif
