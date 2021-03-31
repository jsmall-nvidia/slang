#ifndef SLANG_COMPILER_H_INCLUDED
#define SLANG_COMPILER_H_INCLUDED

#include "../core/slang-basic.h"
#include "../core/slang-shared-library.h"
#include "../core/slang-archive-file-system.h"
#include "../core/slang-file-system.h"

#include "../compiler-core/slang-downstream-compiler.h"
#include "../compiler-core/slang-name.h"

#include "../../slang-com-ptr.h"

#include "slang-capability.h"
#include "slang-diagnostics.h"

#include "slang-preprocessor.h"
#include "slang-profile.h"
#include "slang-syntax.h"


#include "slang-include-system.h"

#include "slang-serialize-ir-types.h"

#include "../../slang.h"

namespace Slang
{
    struct PathInfo;
    struct IncludeHandler;
    class ProgramLayout;
    class PtrType;
    class TargetProgram;
    class TargetRequest;
    class TypeLayout;

    extern const Guid IID_EndToEndCompileRequest;

    enum class CompilerMode
    {
        ProduceLibrary,
        ProduceShader,
        GenerateChoice
    };

    enum class StageTarget
    {
        Unknown,
        VertexShader,
        HullShader,
        DomainShader,
        GeometryShader,
        FragmentShader,
        ComputeShader,
    };

    enum class CodeGenTarget 
    {
        Unknown             = SLANG_TARGET_UNKNOWN,
        None                = SLANG_TARGET_NONE,
        GLSL                = SLANG_GLSL,
        GLSL_Vulkan         = SLANG_GLSL_VULKAN,
        GLSL_Vulkan_OneDesc = SLANG_GLSL_VULKAN_ONE_DESC,
        HLSL                = SLANG_HLSL,
        SPIRV               = SLANG_SPIRV,
        SPIRVAssembly       = SLANG_SPIRV_ASM,
        DXBytecode          = SLANG_DXBC,
        DXBytecodeAssembly  = SLANG_DXBC_ASM,
        DXIL                = SLANG_DXIL,
        DXILAssembly        = SLANG_DXIL_ASM,
        CSource             = SLANG_C_SOURCE,
        CPPSource           = SLANG_CPP_SOURCE,
        Executable          = SLANG_EXECUTABLE,
        SharedLibrary       = SLANG_SHARED_LIBRARY,
        HostCallable        = SLANG_HOST_CALLABLE,
        CUDASource          = SLANG_CUDA_SOURCE,
        PTX                 = SLANG_PTX,
        CountOf             = SLANG_TARGET_COUNT_OF,
    };

    void printDiagnosticArg(StringBuilder& sb, CodeGenTarget val);

    enum class ContainerFormat : SlangContainerFormat
    {
        None            = SLANG_CONTAINER_FORMAT_NONE,
        SlangModule     = SLANG_CONTAINER_FORMAT_SLANG_MODULE,
    };

    enum class LineDirectiveMode : SlangLineDirectiveMode
    {
        Default     = SLANG_LINE_DIRECTIVE_MODE_DEFAULT,
        None        = SLANG_LINE_DIRECTIVE_MODE_NONE,
        Standard    = SLANG_LINE_DIRECTIVE_MODE_STANDARD,
        GLSL        = SLANG_LINE_DIRECTIVE_MODE_GLSL,
    };

    enum class ResultFormat
    {
        None,
        Text,
        Binary,
    };

    // When storing the layout for a matrix-type
    // value, we need to know whether it has been
    // laid out with row-major or column-major
    // storage.
    //
    enum MatrixLayoutMode
    {
        kMatrixLayoutMode_RowMajor      = SLANG_MATRIX_LAYOUT_ROW_MAJOR,
        kMatrixLayoutMode_ColumnMajor   = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
    };

    enum class DebugInfoLevel : SlangDebugInfoLevel
    {
        None        = SLANG_DEBUG_INFO_LEVEL_NONE,
        Minimal     = SLANG_DEBUG_INFO_LEVEL_MINIMAL,
        Standard    = SLANG_DEBUG_INFO_LEVEL_STANDARD,
        Maximal     = SLANG_DEBUG_INFO_LEVEL_MAXIMAL,
    };

    enum class OptimizationLevel : SlangOptimizationLevel
    {
        None    = SLANG_OPTIMIZATION_LEVEL_NONE,
        Default = SLANG_OPTIMIZATION_LEVEL_DEFAULT,
        High    = SLANG_OPTIMIZATION_LEVEL_HIGH,
        Maximal = SLANG_OPTIMIZATION_LEVEL_MAXIMAL,
    };

    class Linkage;
    class Module;
    class FrontEndCompileRequest;
    class BackEndCompileRequest;
    class EndToEndCompileRequest;
    class TranslationUnitRequest;

    // Result of compiling an entry point.
    // Should only ever be string, binary or shared library
    class CompileResult
    {
    public:
        CompileResult() = default;
        explicit CompileResult(String const& str) : format(ResultFormat::Text), outputString(str) {}
        explicit CompileResult(ISlangBlob* inBlob) : format(ResultFormat::Binary), blob(inBlob) {}
        explicit CompileResult(DownstreamCompileResult* inDownstreamResult): format(ResultFormat::Binary), downstreamResult(inDownstreamResult) {}
        explicit CompileResult(const UnownedStringSlice& slice ) : format(ResultFormat::Text), outputString(slice) {}

        SlangResult getBlob(ComPtr<ISlangBlob>& outBlob) const;
        SlangResult getSharedLibrary(ComPtr<ISlangSharedLibrary>& outSharedLibrary);

        ResultFormat format = ResultFormat::None;
        String outputString;                    ///< Only set if result type is ResultFormat::Text

        mutable ComPtr<ISlangBlob> blob;

        RefPtr<DownstreamCompileResult> downstreamResult;
    };

        /// Information collected about global or entry-point shader parameters
    struct ShaderParamInfo
    {
        DeclRef<VarDeclBase>    paramDeclRef;
        Int                     firstSpecializationParamIndex = 0;
        Int                     specializationParamCount = 0;
    };

        /// A request for the front-end to find and validate an entry-point function
    struct FrontEndEntryPointRequest : RefObject
    {
    public:
            /// Create a request for an entry point.
        FrontEndEntryPointRequest(
            FrontEndCompileRequest* compileRequest,
            int                     translationUnitIndex,
            Name*                   name,
            Profile                 profile);

            /// Get the parent front-end compile request.
        FrontEndCompileRequest* getCompileRequest() { return m_compileRequest; }

            /// Get the translation unit that contains the entry point.
        TranslationUnitRequest* getTranslationUnit();

            /// Get the name of the entry point to find.
        Name* getName() { return m_name; }

            /// Get the stage that the entry point is to be compiled for
        Stage getStage() { return m_profile.getStage(); }

            /// Get the profile that the entry point is to be compiled for
        Profile getProfile() { return m_profile; }

            /// Get the index to the translation unit
        int getTranslationUnitIndex() const { return m_translationUnitIndex; }

    private:
        // The parent compile request
        FrontEndCompileRequest* m_compileRequest;

        // The index of the translation unit that will hold the entry point
        int                     m_translationUnitIndex;

        // The name of the entry point function to look for
        Name*                   m_name;

        // The profile to compile for (including stage)
        Profile                 m_profile;
    };

        /// Tracks an ordered list of modules that something depends on.
    struct ModuleDependencyList
    {
    public:
            /// Get the list of modules that are depended on.
        List<Module*> const& getModuleList() { return m_moduleList; }

            /// Add a module and everything it depends on to the list.
        void addDependency(Module* module);

            /// Add a module to the list, but not the modules it depends on.
        void addLeafDependency(Module* module);

    private:
        void _addDependency(Module* module);

        List<Module*>       m_moduleList;
        HashSet<Module*>    m_moduleSet;
    };

        /// Tracks an unordered list of filesystem paths that something depends on
    struct FilePathDependencyList
    {
    public:
            /// Get the list of paths that are depended on.
        List<String> const& getFilePathList() { return m_filePathList; }

            /// Add a path to the list, if it is not already present
        void addDependency(String const& path);

            /// Add all of the paths that `module` depends on to the list
        void addDependency(Module* module);

    private:

        // TODO: We are using a `HashSet` here to deduplicate
        // the paths so that we don't return the same path
        // multiple times from `getFilePathList`, but because
        // order isn't important, we could potentially do better
        // in terms of memory (at some cost in performance) by
        // just sorting the `m_filePathList` every once in
        // a while and then deduplicating.

        List<String>    m_filePathList;
        HashSet<String> m_filePathSet;
    };

    class EntryPoint;

    class ComponentType;
    class ComponentTypeVisitor;

        /// Base class for "component types" that represent the pieces a final
        /// shader program gets linked together from.
        ///
    class ComponentType : public RefObject, public slang::IComponentType
    {
    public:
        //
        // ISlangUnknown interface
        //

        SLANG_REF_OBJECT_IUNKNOWN_ALL;
        ISlangUnknown* getInterface(Guid const& guid);

        //
        // slang::IComponentType interface
        //

        SLANG_NO_THROW slang::ISession* SLANG_MCALL getSession() SLANG_OVERRIDE;
        SLANG_NO_THROW slang::ProgramLayout* SLANG_MCALL getLayout(
            SlangInt        targetIndex,
            slang::IBlob**  outDiagnostics) SLANG_OVERRIDE;
        SLANG_NO_THROW SlangResult SLANG_MCALL getEntryPointCode(
            SlangInt        entryPointIndex,
            SlangInt        targetIndex,
            slang::IBlob**  outCode,
            slang::IBlob**  outDiagnostics) SLANG_OVERRIDE;
        SLANG_NO_THROW SlangResult SLANG_MCALL specialize(
            slang::SpecializationArg const* specializationArgs,
            SlangInt                        specializationArgCount,
            slang::IComponentType**         outSpecializedComponentType,
            ISlangBlob**                    outDiagnostics) SLANG_OVERRIDE;
        SLANG_NO_THROW SlangResult SLANG_MCALL link(
            slang::IComponentType**         outLinkedComponentType,
            ISlangBlob**                    outDiagnostics) SLANG_OVERRIDE;
        SLANG_NO_THROW SlangResult SLANG_MCALL getEntryPointHostCallable(
            int                     entryPointIndex,
            int                     targetIndex,
            ISlangSharedLibrary**   outSharedLibrary,
            slang::IBlob**          outDiagnostics) SLANG_OVERRIDE;

            /// Get the linkage (aka "session" in the public API) for this component type.
        Linkage* getLinkage() { return m_linkage; }

            /// Get the target-specific version of this program for the given `target`.
            ///
            /// The `target` must be a target on the `Linkage` that was used to create this program.
        TargetProgram* getTargetProgram(TargetRequest* target);

            /// Get the number of entry points linked into this component type.
        virtual Index getEntryPointCount() = 0;

            /// Get one of the entry points linked into this component type.
        virtual RefPtr<EntryPoint> getEntryPoint(Index index) = 0;

            /// Get the mangled name of one of the entry points linked into this component type.
        virtual String getEntryPointMangledName(Index index) = 0;

            /// Get the number of global shader parameters linked into this component type.
        virtual Index getShaderParamCount() = 0;

            /// Get one of the global shader parametesr linked into this component type.
        virtual ShaderParamInfo getShaderParam(Index index) = 0;

            /// Get the specialization parameter at `index`.
        virtual SpecializationParam const& getSpecializationParam(Index index) = 0;

            /// Get the number of "requirements" that this component type has.
            ///
            /// A requirement represents another component type that this component
            /// needs in order to function correctly. For example, the dependency
            /// of one module on another module that it `import`s is represented
            /// as a requirement, as is the dependency of an entry point on the
            /// module that defines it.
            ///
        virtual Index getRequirementCount() = 0;

            /// Get the requirement at `index`.
        virtual RefPtr<ComponentType> getRequirement(Index index) = 0;

            /// Parse a type from a string, in the context of this component type.
            ///
            /// Any names in the string will be resolved using the modules
            /// referenced by the program.
            ///
            /// On an error, returns null and reports diagnostic messages
            /// to the provided `sink`.
            ///
            /// TODO: This function shouldn't be on the base class, since
            /// it only really makes sense on `Module`.
            ///
        Type* getTypeFromString(
            String const&   typeStr,
            DiagnosticSink* sink);

            /// Get a list of modules that this component type depends on.
            ///
        virtual List<Module*> const& getModuleDependencies() = 0;

            /// Get the full list of filesystem paths this component type depends on.
            ///
        virtual List<String> const& getFilePathDependencies() = 0;

            /// Callback for use with `enumerateIRModules`
        typedef void (*EnumerateIRModulesCallback)(IRModule* irModule, void* userData);

            /// Invoke `callback` on all the IR modules that are (transitively) linked into this component type.
        void enumerateIRModules(EnumerateIRModulesCallback callback, void* userData);

            /// Invoke `callback` on all the IR modules that are (transitively) linked into this component type.
        template<typename F>
        void enumerateIRModules(F const& callback)
        {
            struct Helper
            {
                static void helper(IRModule* irModule, void* userData)
                {
                    (*(F*)userData)(irModule);
                }
            };
            enumerateIRModules(&Helper::helper, (void*)&callback);
        }

            /// Callback for use with `enumerateModules`
        typedef void (*EnumerateModulesCallback)(Module* module, void* userData);

            /// Invoke `callback` on all the modules that are (transitively) linked into this component type.
        void enumerateModules(EnumerateModulesCallback callback, void* userData);

            /// Invoke `callback` on all the modules that are (transitively) linked into this component type.
        template<typename F>
        void enumerateModules(F const& callback)
        {
            struct Helper
            {
                static void helper(Module* module, void* userData)
                {
                    (*(F*)userData)(module);
                }
            };
            enumerateModules(&Helper::helper, (void*)&callback);
        }

            /// Side-band information generated when specializing this component type.
            ///
            /// Difference subclasses of `ComponentType` are expected to create their
            /// own subclass of `SpecializationInfo` as the output of `_validateSpecializationArgs`.
            /// Later, whenever we want to use a specialized component type we will
            /// also have the `SpecializationInfo` available and will expect it to
            /// have the correct (subclass-specific) type.
            ///
        class SpecializationInfo : public RefObject
        {
        };

            /// Validate the given specialization `args` and compute any side-band specialization info.
            ///
            /// Any errors will be reported to `sink`, which can thus be used to test
            /// if the operation was successful.
            ///
            /// A null return value is allowed, since not all subclasses require
            /// custom side-band specialization information.
            ///
            /// This function is an implementation detail of `specialize()`.
            ///
        virtual RefPtr<SpecializationInfo> _validateSpecializationArgsImpl(
            SpecializationArg const*    args,
            Index                       argCount,
            DiagnosticSink*             sink) = 0;

            /// Validate the given specialization `args` and compute any side-band specialization info.
            ///
            /// Any errors will be reported to `sink`, which can thus be used to test
            /// if the operation was successful.
            ///
            /// A null return value is allowed, since not all subclasses require
            /// custom side-band specialization information.
            ///
            /// This function is an implementation detail of `specialize()`.
            ///
        RefPtr<SpecializationInfo> _validateSpecializationArgs(
            SpecializationArg const*    args,
            Index                       argCount,
            DiagnosticSink*             sink)
        {
            if(argCount == 0) return nullptr;
            return _validateSpecializationArgsImpl(args, argCount, sink);
        }

            /// Specialize this component type given `specializationArgs`
            ///
            /// Any diagnostics will be reported to `sink`, which can be used
            /// to determine if the operation was successful. It is allowed
            /// for this operation to have a non-null return even when an
            /// error is ecnountered.
            ///
        RefPtr<ComponentType> specialize(
            SpecializationArg const*    specializationArgs,
            SlangInt                    specializationArgCount,
            DiagnosticSink*             sink);

            /// Invoke `visitor` on this component type, using the appropriate dynamic type.
            ///
            /// This function implements the "visitor pattern" for `ComponentType`.
            ///
            /// If the `specializationInfo` argument is non-null, it must be specialization
            /// information generated for this specific component type by `_validateSpecializationArgs`.
            /// In that case, appropriately-typed specialization information will be passed
            /// when invoking the `visitor`.
            ///
        virtual void acceptVisitor(ComponentTypeVisitor* visitor, SpecializationInfo* specializationInfo) = 0;

            /// Create a scope suitable for looking up names or parsing specialization arguments.
            ///
            /// This facility is only needed to support legacy APIs for string-based lookup
            /// and parsing via Slang reflection, and is not recommended for future APIs to use.
            ///
        RefPtr<Scope> _createScopeForLegacyLookup();

    protected:
        ComponentType(Linkage* linkage);

    private:
        Linkage* m_linkage;

        // Cache of target-specific programs for each target.
        Dictionary<TargetRequest*, RefPtr<TargetProgram>> m_targetPrograms;

        // Any types looked up dynamically using `getTypeFromString`
        //
        // TODO: Remove this. Type lookup should only be supported on `Module`s.
        //
        Dictionary<String, Type*> m_types;
    };

        /// A component type built up from other component types.
    class CompositeComponentType : public ComponentType
    {
    public:
        static RefPtr<ComponentType> create(
            Linkage*                            linkage,
            List<RefPtr<ComponentType>> const&  childComponents);

        List<RefPtr<ComponentType>> const& getChildComponents() { return m_childComponents; };
        Index getChildComponentCount() { return m_childComponents.getCount(); }
        RefPtr<ComponentType> getChildComponent(Index index) { return m_childComponents[index]; }

        Index getEntryPointCount() SLANG_OVERRIDE;
        RefPtr<EntryPoint> getEntryPoint(Index index) SLANG_OVERRIDE;
        String getEntryPointMangledName(Index index) SLANG_OVERRIDE;

        Index getShaderParamCount() SLANG_OVERRIDE;
        ShaderParamInfo getShaderParam(Index index) SLANG_OVERRIDE;

        SLANG_NO_THROW Index SLANG_MCALL getSpecializationParamCount() SLANG_OVERRIDE;
        SpecializationParam const& getSpecializationParam(Index index) SLANG_OVERRIDE;

        Index getRequirementCount() SLANG_OVERRIDE;
        RefPtr<ComponentType> getRequirement(Index index) SLANG_OVERRIDE;

        List<Module*> const& getModuleDependencies() SLANG_OVERRIDE;
        List<String> const& getFilePathDependencies() SLANG_OVERRIDE;

        class CompositeSpecializationInfo : public SpecializationInfo
        {
        public:
            List<RefPtr<SpecializationInfo>> childInfos;
        };

    protected:
        void acceptVisitor(ComponentTypeVisitor* visitor, SpecializationInfo* specializationInfo) SLANG_OVERRIDE;


        RefPtr<SpecializationInfo> _validateSpecializationArgsImpl(
            SpecializationArg const*    args,
            Index                       argCount,
            DiagnosticSink*             sink) SLANG_OVERRIDE;

    private:
        CompositeComponentType(
            Linkage*                            linkage,
            List<RefPtr<ComponentType>> const&  childComponents);

        List<RefPtr<ComponentType>> m_childComponents;

        // The following arrays hold the concatenated entry points, parameters,
        // etc. from the child components. This approach allows for reasonably
        // fast (constant time) access through operations like `getShaderParam`,
        // but means that the memory usage of a composite is proportional to
        // the sum of the memory usage of the children, rather than being fixed
        // by the number of children (as it would be if we just stored 
        // `m_childComponents`).
        //
        // TODO: We could conceivably build some O(numChildren) arrays that
        // support binary-search to provide logarithmic-time access to entry
        // points, parameters, etc. while giving a better overall memory usage.
        //
        List<EntryPoint*> m_entryPoints;
        List<String> m_entryPointMangledNames;
        List<ShaderParamInfo> m_shaderParams;
        List<SpecializationParam> m_specializationParams;
        List<ComponentType*> m_requirements;

        ModuleDependencyList m_moduleDependencyList;
        FilePathDependencyList m_filePathDependencyList;
    };

        /// A component type created by specializing another component type.
    class SpecializedComponentType : public ComponentType
    {
    public:
        SpecializedComponentType(
            ComponentType*                  base,
            SpecializationInfo*             specializationInfo,
            List<SpecializationArg> const&  specializationArgs,
            DiagnosticSink*                 sink);

            /// Get the base (unspecialized) component type that is being specialized.
        RefPtr<ComponentType> getBaseComponentType() { return m_base; }

        RefPtr<SpecializationInfo> getSpecializationInfo() { return m_specializationInfo; }

            /// Get the number of arguments supplied for existential type parameters.
            ///
            /// Note that the number of arguments may not match the number of parameters.
            /// In particular, an unspecialized entry point may have many parameters, but zero arguments.
        Index getSpecializationArgCount() { return m_specializationArgs.getCount(); }

            /// Get the existential type argument (type and witness table) at `index`.
        SpecializationArg const& getSpecializationArg(Index index) { return m_specializationArgs[index]; }

            /// Get an array of all existential type arguments.
        SpecializationArg const* getSpecializationArgs() { return m_specializationArgs.getBuffer(); }

        Index getEntryPointCount() SLANG_OVERRIDE { return m_base->getEntryPointCount(); }
        RefPtr<EntryPoint> getEntryPoint(Index index) SLANG_OVERRIDE { return m_base->getEntryPoint(index); }
        String getEntryPointMangledName(Index index) SLANG_OVERRIDE;

        Index getShaderParamCount() SLANG_OVERRIDE { return m_base->getShaderParamCount(); }
        ShaderParamInfo getShaderParam(Index index) SLANG_OVERRIDE { return m_base->getShaderParam(index); }

        SLANG_NO_THROW Index SLANG_MCALL getSpecializationParamCount() SLANG_OVERRIDE { return 0; }
        SpecializationParam const& getSpecializationParam(Index index) SLANG_OVERRIDE { SLANG_UNUSED(index); static SpecializationParam dummy; return dummy; }

        Index getRequirementCount() SLANG_OVERRIDE;
        RefPtr<ComponentType> getRequirement(Index index) SLANG_OVERRIDE;

        List<Module*> const& getModuleDependencies() SLANG_OVERRIDE { return m_moduleDependencies; }
        List<String> const& getFilePathDependencies() SLANG_OVERRIDE { return m_filePathDependencies; }

                    /// Get a list of tagged-union types referenced by the specialization parameters.
        List<TaggedUnionType*> const& getTaggedUnionTypes() { return m_taggedUnionTypes; }

        RefPtr<IRModule> getIRModule() { return m_irModule; }

        void acceptVisitor(ComponentTypeVisitor* visitor, SpecializationInfo* specializationInfo) SLANG_OVERRIDE;

    protected:

        RefPtr<SpecializationInfo> _validateSpecializationArgsImpl(
            SpecializationArg const*    args,
            Index                       argCount,
            DiagnosticSink*             sink) SLANG_OVERRIDE
        {
            SLANG_UNUSED(args);
            SLANG_UNUSED(argCount);
            SLANG_UNUSED(sink);
            return nullptr;
        }

    private:
        RefPtr<ComponentType> m_base;
        RefPtr<SpecializationInfo> m_specializationInfo;
        SpecializationArgs m_specializationArgs;
        RefPtr<IRModule> m_irModule;

        List<String> m_entryPointMangledNames;

        // Any tagged union types that were referenced by the specialization arguments.
        List<TaggedUnionType*> m_taggedUnionTypes;

        List<Module*> m_moduleDependencies;
        List<String> m_filePathDependencies;
        List<RefPtr<ComponentType>> m_requirements;
    };

        /// Describes an entry point for the purposes of layout and code generation.
        ///
        /// This class also tracks any generic arguments to the entry point,
        /// in the case that it is a specialization of a generic entry point.
        ///
        /// There is also a provision for creating a "dummy" entry point for
        /// the purposes of pass-through compilation modes. Only the
        /// `getName()` and `getProfile()` methods should be expected to
        /// return useful data on pass-through entry points.
        ///
    class EntryPoint : public ComponentType, public slang::IEntryPoint
    {
        typedef ComponentType Super;

    public:
        SLANG_REF_OBJECT_IUNKNOWN_ALL

        ISlangUnknown* getInterface(const Guid& guid);


        // Forward `IComponentType` methods

        SLANG_NO_THROW slang::ISession* SLANG_MCALL getSession() SLANG_OVERRIDE
        {
            return Super::getSession();
        }

        SLANG_NO_THROW slang::ProgramLayout* SLANG_MCALL getLayout(
            SlangInt        targetIndex,
            slang::IBlob**  outDiagnostics) SLANG_OVERRIDE
        {
            return Super::getLayout(targetIndex, outDiagnostics);
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL getEntryPointCode(
            SlangInt        entryPointIndex,
            SlangInt        targetIndex,
            slang::IBlob**  outCode,
            slang::IBlob**  outDiagnostics) SLANG_OVERRIDE
        {
            return Super::getEntryPointCode(entryPointIndex, targetIndex, outCode, outDiagnostics);
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL specialize(
            slang::SpecializationArg const* specializationArgs,
            SlangInt                        specializationArgCount,
            slang::IComponentType**         outSpecializedComponentType,
            ISlangBlob**                    outDiagnostics) SLANG_OVERRIDE
        {
            return Super::specialize(
                specializationArgs,
                specializationArgCount,
                outSpecializedComponentType,
                outDiagnostics);
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL link(
            slang::IComponentType**         outLinkedComponentType,
            ISlangBlob**                    outDiagnostics) SLANG_OVERRIDE
        {
            return Super::link(
                outLinkedComponentType,
                outDiagnostics);
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL getEntryPointHostCallable(
            int                     entryPointIndex,
            int                     targetIndex,
            ISlangSharedLibrary**   outSharedLibrary,
            slang::IBlob**          outDiagnostics) SLANG_OVERRIDE
        {
            return Super::getEntryPointHostCallable(entryPointIndex, targetIndex, outSharedLibrary, outDiagnostics);
        }

            /// Create an entry point that refers to the given function.
        static RefPtr<EntryPoint> create(
            Linkage*            linkage,
            DeclRef<FuncDecl>   funcDeclRef,
            Profile             profile);

            /// Get the function decl-ref, including any generic arguments.
        DeclRef<FuncDecl> getFuncDeclRef() { return m_funcDeclRef; }

            /// Get the function declaration (without generic arguments).
        FuncDecl* getFuncDecl() { return m_funcDeclRef.getDecl(); }

            /// Get the name of the entry point
        Name* getName() { return m_name; }

            /// Get the profile associated with the entry point
            ///
            /// Note: only the stage part of the profile is expected
            /// to contain useful data, but certain legacy code paths
            /// allow for "shader model" information to come via this path.
            ///
        Profile getProfile() { return m_profile; }

            /// Get the stage that the entry point is for.
        Stage getStage() { return m_profile.getStage(); }

            /// Get the module that contains the entry point.
        Module* getModule();

            /// Get a list of modules that this entry point depends on.
            ///
            /// This will include the module that defines the entry point (see `getModule()`),
            /// but may also include modules that are required by its generic type arguments.
            ///
        List<Module*> const& getModuleDependencies() SLANG_OVERRIDE; // { return getModule()->getModuleDependencies(); }
        List<String> const& getFilePathDependencies() SLANG_OVERRIDE; // { return getModule()->getFilePathDependencies(); }

            /// Create a dummy `EntryPoint` that is only usable for pass-through compilation.
        static RefPtr<EntryPoint> createDummyForPassThrough(
            Linkage*    linkage,
            Name*       name,
            Profile     profile);

            /// Create a dummy `EntryPoint` that stands in for a serialized entry point
        static RefPtr<EntryPoint> createDummyForDeserialize(
            Linkage*    linkage,
            Name*       name,
            Profile     profile,
            String      mangledName);

            /// Get the number of existential type parameters for the entry point.
        SLANG_NO_THROW Index SLANG_MCALL getSpecializationParamCount() SLANG_OVERRIDE;

            /// Get the existential type parameter at `index`.
        SpecializationParam const& getSpecializationParam(Index index) SLANG_OVERRIDE;

        Index getRequirementCount() SLANG_OVERRIDE;
        RefPtr<ComponentType> getRequirement(Index index) SLANG_OVERRIDE;

        SpecializationParams const& getExistentialSpecializationParams() { return m_existentialSpecializationParams; }

        Index getGenericSpecializationParamCount() { return m_genericSpecializationParams.getCount(); }
        Index getExistentialSpecializationParamCount() { return m_existentialSpecializationParams.getCount(); }

            /// Get an array of all entry-point shader parameters.
        List<ShaderParamInfo> const& getShaderParams() { return m_shaderParams; }

        Index getEntryPointCount() SLANG_OVERRIDE { return 1; };
        RefPtr<EntryPoint> getEntryPoint(Index index) SLANG_OVERRIDE { SLANG_UNUSED(index); return this; }
        String getEntryPointMangledName(Index index) SLANG_OVERRIDE;

        Index getShaderParamCount() SLANG_OVERRIDE { return 0; }
        ShaderParamInfo getShaderParam(Index index) SLANG_OVERRIDE { SLANG_UNUSED(index); return ShaderParamInfo(); }

        class EntryPointSpecializationInfo : public SpecializationInfo
        {
        public:
            DeclRef<FuncDecl> specializedFuncDeclRef;
            List<ExpandedSpecializationArg> existentialSpecializationArgs;
        };

    protected:
        void acceptVisitor(ComponentTypeVisitor* visitor, SpecializationInfo* specializationInfo) SLANG_OVERRIDE;

        RefPtr<SpecializationInfo> _validateSpecializationArgsImpl(
            SpecializationArg const*    args,
            Index                       argCount,
            DiagnosticSink*             sink) SLANG_OVERRIDE;

    private:
        EntryPoint(
            Linkage*            linkage,
            Name*               name,
            Profile             profile,
            DeclRef<FuncDecl>   funcDeclRef);

        void _collectGenericSpecializationParamsRec(Decl* decl);
        void _collectShaderParams();

        // The name of the entry point function (e.g., `main`)
        //
        Name* m_name = nullptr;

        // The declaration of the entry-point function itself.
        //
        DeclRef<FuncDecl> m_funcDeclRef;

            /// The mangled name of the entry point function
        String m_mangledName;

        SpecializationParams m_genericSpecializationParams;
        SpecializationParams m_existentialSpecializationParams;

            /// Information about entry-point parameters
        List<ShaderParamInfo> m_shaderParams;

        // The profile that the entry point will be compiled for
        // (this is a combination of the target stage, and also
        // a feature level that sets capabilities)
        //
        // Note: the profile-version part of this should probably
        // be moving towards deprecation, in favor of the version
        // information (e.g., "Shader Model 5.1") always coming
        // from the target, while the stage part is all that is
        // intrinsic to the entry point.
        //
        Profile m_profile;
    };

    enum class PassThroughMode : SlangPassThroughIntegral
    {
        None = SLANG_PASS_THROUGH_NONE,	                    ///< don't pass through: use Slang compiler
        Fxc = SLANG_PASS_THROUGH_FXC,	                    ///< pass through HLSL to `D3DCompile` API
        Dxc = SLANG_PASS_THROUGH_DXC,	                    ///< pass through HLSL to `IDxcCompiler` API
        Glslang = SLANG_PASS_THROUGH_GLSLANG,	            ///< pass through GLSL to `glslang` library
        Clang = SLANG_PASS_THROUGH_CLANG,                   ///< Pass through clang compiler
        VisualStudio = SLANG_PASS_THROUGH_VISUAL_STUDIO,    ///< Visual studio compiler
        Gcc = SLANG_PASS_THROUGH_GCC,                       ///< Gcc compiler
        GenericCCpp = SLANG_PASS_THROUGH_GENERIC_C_CPP,     ///< Generic C/C++ compiler
        NVRTC = SLANG_PASS_THROUGH_NVRTC,
        CountOf = SLANG_PASS_THROUGH_COUNT_OF,              
    };
    void printDiagnosticArg(StringBuilder& sb, PassThroughMode val);

    class SourceFile;

        /// A module of code that has been compiled through the front-end
        ///
        /// A module comprises all the code from one translation unit (which
        /// may span multiple Slang source files), and provides access
        /// to both the AST and IR representations of that code.
        ///
    class Module : public ComponentType, public slang::IModule
    {
        typedef ComponentType Super;

    public:
        SLANG_REF_OBJECT_IUNKNOWN_ALL

        ISlangUnknown* getInterface(const Guid& guid);


        // Forward `IComponentType` methods

        SLANG_NO_THROW slang::ISession* SLANG_MCALL getSession() SLANG_OVERRIDE
        {
            return Super::getSession();
        }

        SLANG_NO_THROW slang::ProgramLayout* SLANG_MCALL getLayout(
            SlangInt        targetIndex,
            slang::IBlob**  outDiagnostics) SLANG_OVERRIDE
        {
            return Super::getLayout(targetIndex, outDiagnostics);
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL getEntryPointCode(
            SlangInt        entryPointIndex,
            SlangInt        targetIndex,
            slang::IBlob**  outCode,
            slang::IBlob**  outDiagnostics) SLANG_OVERRIDE
        {
            return Super::getEntryPointCode(entryPointIndex, targetIndex, outCode, outDiagnostics);
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL specialize(
            slang::SpecializationArg const* specializationArgs,
            SlangInt                        specializationArgCount,
            slang::IComponentType**         outSpecializedComponentType,
            ISlangBlob**                    outDiagnostics) SLANG_OVERRIDE
        {
            return Super::specialize(
                specializationArgs,
                specializationArgCount,
                outSpecializedComponentType,
                outDiagnostics);
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL link(
            slang::IComponentType**         outLinkedComponentType,
            ISlangBlob**                    outDiagnostics) SLANG_OVERRIDE
        {
            return Super::link(
                outLinkedComponentType,
                outDiagnostics);
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL getEntryPointHostCallable(
            int                     entryPointIndex,
            int                     targetIndex,
            ISlangSharedLibrary**   outSharedLibrary,
            slang::IBlob**          outDiagnostics) SLANG_OVERRIDE
        {
            return Super::getEntryPointHostCallable(entryPointIndex, targetIndex, outSharedLibrary, outDiagnostics);
        }

        SLANG_NO_THROW SlangResult SLANG_MCALL findEntryPointByName(
            char const*             name,
            slang::IEntryPoint**     outEntryPoint) SLANG_OVERRIDE
        {
            ComPtr<slang::IEntryPoint> entryPoint(findEntryPointByName(UnownedStringSlice(name)));
            if((!entryPoint))
                return SLANG_FAIL;

            *outEntryPoint = entryPoint.detach();
            return SLANG_OK;
        }

        //

            /// Create a module (initially empty).
        Module(Linkage* linkage, ASTBuilder* astBuilder = nullptr);

            /// Get the AST for the module (if it has been parsed)
        ModuleDecl* getModuleDecl() { return m_moduleDecl; }

            /// The the IR for the module (if it has been generated)
        IRModule* getIRModule() { return m_irModule; }

            /// Get the list of other modules this module depends on
        List<Module*> const& getModuleDependencyList() { return m_moduleDependencyList.getModuleList(); }

            /// Get the list of filesystem paths this module depends on
        List<String> const& getFilePathDependencyList() { return m_filePathDependencyList.getFilePathList(); }

            /// Register a module that this module depends on
        void addModuleDependency(Module* module);

            /// Register a filesystem path that this module depends on
        void addFilePathDependency(String const& path);

            /// Set the AST for this module.
            ///
            /// This should only be called once, during creation of the module.
            ///
        void setModuleDecl(ModuleDecl* moduleDecl);// { m_moduleDecl = moduleDecl; }

            /// Set the IR for this module.
            ///
            /// This should only be called once, during creation of the module.
            ///
        void setIRModule(IRModule* irModule) { m_irModule = irModule; }

        Index getEntryPointCount() SLANG_OVERRIDE { return 0; }
        RefPtr<EntryPoint> getEntryPoint(Index index) SLANG_OVERRIDE { SLANG_UNUSED(index); return nullptr; }
        String getEntryPointMangledName(Index index) SLANG_OVERRIDE { SLANG_UNUSED(index); return String(); }

        Index getShaderParamCount() SLANG_OVERRIDE { return m_shaderParams.getCount(); }
        ShaderParamInfo getShaderParam(Index index) SLANG_OVERRIDE { return m_shaderParams[index]; }

        SLANG_NO_THROW Index SLANG_MCALL getSpecializationParamCount() SLANG_OVERRIDE { return m_specializationParams.getCount(); }
        SpecializationParam const& getSpecializationParam(Index index) SLANG_OVERRIDE { return m_specializationParams[index]; }

        Index getRequirementCount() SLANG_OVERRIDE;
        RefPtr<ComponentType> getRequirement(Index index) SLANG_OVERRIDE;

        List<Module*> const& getModuleDependencies() SLANG_OVERRIDE { return m_moduleDependencyList.getModuleList(); }
        List<String> const& getFilePathDependencies() SLANG_OVERRIDE { return m_filePathDependencyList.getFilePathList(); }

            /// Given a mangled name finds the exported NodeBase associated with this module.
            /// If not found returns nullptr.
        NodeBase* findExportFromMangledName(const UnownedStringSlice& slice);

            /// Get the ASTBuilder
        ASTBuilder* getASTBuilder() { return m_astBuilder; }

            /// Collect information on the shader parameters of the module.
            ///
            /// This method should only be called once, after the core
            /// structured of the module (its AST and IR) have been created,
            /// and before any of the `ComponentType` APIs are used.
            ///
            /// TODO: We might eventually consider a non-stateful approach
            /// to constructing a `Module`.
            ///
        void _collectShaderParams();

        class ModuleSpecializationInfo : public SpecializationInfo
        {
        public:
            struct GenericArgInfo
            {
                Decl* paramDecl = nullptr;
                Val* argVal = nullptr;
            };

            List<GenericArgInfo> genericArgs;
            List<ExpandedSpecializationArg> existentialArgs;
        };

        RefPtr<EntryPoint> findEntryPointByName(UnownedStringSlice const& name);

        List<RefPtr<EntryPoint>> const& getEntryPoints() { return m_entryPoints; }
        void _addEntryPoint(EntryPoint* entryPoint);
        void _processFindDeclsExportSymbolsRec(Decl* decl);

    protected:
        void acceptVisitor(ComponentTypeVisitor* visitor, SpecializationInfo* specializationInfo) SLANG_OVERRIDE;

        RefPtr<SpecializationInfo> _validateSpecializationArgsImpl(
            SpecializationArg const*    args,
            Index                       argCount,
            DiagnosticSink*             sink) SLANG_OVERRIDE;

    private:
        // The AST for the module
        ModuleDecl*  m_moduleDecl = nullptr;

        // The IR for the module
        RefPtr<IRModule> m_irModule = nullptr;

        List<ShaderParamInfo> m_shaderParams;
        SpecializationParams m_specializationParams;

        List<Module*> m_requirements;

        // List of modules this module depends on
        ModuleDependencyList m_moduleDependencyList;

        // List of filesystem paths this module depends on
        FilePathDependencyList m_filePathDependencyList;

        // Entry points that were defined in thsi module
        //
        // Note: the entry point defined in the module are *not*
        // part of the memory image/layout of the module when
        // it is considered as an IComponentType. This can be
        // a bit confusing, but if all the entry points in the
        // module were automatically linked into the component
        // type, we'd need a way to access just the global
        // scope of the module without the entry points, in
        // case we wanted to link a single entry point against
        // the global scope. The `Module` type provides exactly
        // that "module without its entry points" unit of
        // granularity for linking.
        //
        // This list only exists for lookup purposes, so that
        // the user can find an existing entry-point function
        // that was defined as part of the module.
        //
        List<RefPtr<EntryPoint>> m_entryPoints;

        // The builder that owns all of the AST nodes from parsing the source of
        // this module. 
        RefPtr<ASTBuilder> m_astBuilder;

        // Holds map of exported mangled names to symbols. m_mangledExportPool maps names to indices,
        // and m_mangledExportSymbols holds the NodeBase* values for each index. 
        StringSlicePool m_mangledExportPool;
        List<NodeBase*> m_mangledExportSymbols;
    };
    typedef Module LoadedModule;

        /// A request for the front-end to compile a translation unit.
    class TranslationUnitRequest : public RefObject
    {
    public:
        TranslationUnitRequest(
            FrontEndCompileRequest* compileRequest);

        // The parent compile request
        FrontEndCompileRequest* compileRequest = nullptr;

        // The language in which the source file(s)
        // are assumed to be written
        SourceLanguage sourceLanguage = SourceLanguage::Unknown;

        // The source file(s) that will be compiled to form this translation unit
        //
        // Usually, for HLSL or GLSL there will be only one file.
        List<SourceFile*> m_sourceFiles;

        List<SourceFile*> const& getSourceFiles() { return m_sourceFiles; }
        void addSourceFile(SourceFile* sourceFile);

        // The entry points associated with this translation unit
        List<RefPtr<EntryPoint>> const& getEntryPoints() { return module->getEntryPoints(); }

        void _addEntryPoint(EntryPoint* entryPoint) { module->_addEntryPoint(entryPoint); }

        // Preprocessor definitions to use for this translation unit only
        // (whereas the ones on `compileRequest` will be shared)
        Dictionary<String, String> preprocessorDefinitions;

            /// The name that will be used for the module this translation unit produces.
        Name* moduleName = nullptr;

            /// Result of compiling this translation unit (a module)
        RefPtr<Module> module;

        Module* getModule() { return module; }
        ModuleDecl* getModuleDecl() { return module->getModuleDecl(); }

        Session* getSession();
        NamePool* getNamePool();
        SourceManager* getSourceManager();
    };

    enum class FloatingPointMode : SlangFloatingPointMode
    {
        Default = SLANG_FLOATING_POINT_MODE_DEFAULT,
        Fast = SLANG_FLOATING_POINT_MODE_FAST,
        Precise = SLANG_FLOATING_POINT_MODE_PRECISE,
    };

    enum class WriterChannel : SlangWriterChannel
    {
        Diagnostic = SLANG_WRITER_CHANNEL_DIAGNOSTIC,
        StdOutput = SLANG_WRITER_CHANNEL_STD_OUTPUT,
        StdError = SLANG_WRITER_CHANNEL_STD_ERROR,
        CountOf = SLANG_WRITER_CHANNEL_COUNT_OF,
    };

    enum class WriterMode : SlangWriterMode
    {
        Text = SLANG_WRITER_MODE_TEXT,
        Binary = SLANG_WRITER_MODE_BINARY,
    };

        /// A request to generate output in some target format.
    class TargetRequest : public RefObject
    {
    public:
        TargetRequest(Linkage* linkage, CodeGenTarget format);

        void addTargetFlags(SlangTargetFlags flags)
        {
            targetFlags |= flags;
        }
        void setTargetProfile(Slang::Profile profile)
        {
            targetProfile = profile;
        }
        void setFloatingPointMode(FloatingPointMode mode)
        {
            floatingPointMode = mode;
        }
        void addCapability(CapabilityAtom capability);


        bool isWholeProgramRequest()
        {
            return (targetFlags & SLANG_TARGET_FLAG_GENERATE_WHOLE_PROGRAM) != 0;
        }

        Linkage* getLinkage() { return linkage; }
        CodeGenTarget getTarget() { return format; }
        Profile getTargetProfile() { return targetProfile; }
        FloatingPointMode getFloatingPointMode() { return floatingPointMode; }
        SlangTargetFlags getTargetFlags() { return targetFlags; }
        CapabilitySet getTargetCaps();

        Session* getSession();
        MatrixLayoutMode getDefaultMatrixLayoutMode();

        // TypeLayouts created on the fly by reflection API
        Dictionary<Type*, RefPtr<TypeLayout>> typeLayouts;

        Dictionary<Type*, RefPtr<TypeLayout>>& getTypeLayouts() { return typeLayouts; }

        TypeLayout* getTypeLayout(Type* type);

    private:
        Linkage*                linkage = nullptr;
        CodeGenTarget           format = CodeGenTarget::Unknown;
        SlangTargetFlags        targetFlags = 0;
        Slang::Profile          targetProfile = Slang::Profile();
        FloatingPointMode       floatingPointMode = FloatingPointMode::Default;
        List<CapabilityAtom>    rawCapabilities;
        CapabilitySet           cookedCapabilities;
    };

        /// Are we generating code for a D3D API?
    bool isD3DTarget(TargetRequest* targetReq);

        /// Are we generating code for a Khronos API (OpenGL or Vulkan)?
    bool isKhronosTarget(TargetRequest* targetReq);

        /// Are resource types "bindless" (implemented as ordinary data) on the given `target`?
    bool areResourceTypesBindlessOnTarget(TargetRequest* target);

    // Compute the "effective" profile to use when outputting the given entry point
    // for the chosen code-generation target.
    //
    // The stage of the effective profile will always come from the entry point, while
    // the profile version (aka "shader model") will be computed as follows:
    //
    // - If the entry point and target belong to the same profile family, then take
    //   the latest version between the two (e.g., if the entry point specified `ps_5_1`
    //   and the target specifies `sm_5_0` then use `sm_5_1` as the version).
    //
    // - If the entry point and target disagree on the profile family, always use the
    //   profile family and version from the target.
    //
    Profile getEffectiveProfile(EntryPoint* entryPoint, TargetRequest* target);


        /// Given a target returns the required downstream compiler
    PassThroughMode getDownstreamCompilerRequiredForTarget(CodeGenTarget target);
        /// Given a target returns a downstream compiler the prelude should be taken from.
    SourceLanguage getDefaultSourceLanguageForDownstreamCompiler(PassThroughMode compiler);

        /// Get the build tag string
    const char* getBuildTagString();

    struct TypeCheckingCache;
    
        /// A context for loading and re-using code modules.
    class Linkage : public RefObject, public slang::ISession
    {
    public:
        SLANG_REF_OBJECT_IUNKNOWN_ALL

        ISlangUnknown* getInterface(const Guid& guid);

        SLANG_NO_THROW slang::IGlobalSession* SLANG_MCALL getGlobalSession() override;
        SLANG_NO_THROW slang::IModule* SLANG_MCALL loadModule(
            const char* moduleName,
            slang::IBlob**     outDiagnostics = nullptr) override;
        SLANG_NO_THROW SlangResult SLANG_MCALL createCompositeComponentType(
            slang::IComponentType* const*   componentTypes,
            SlangInt                        componentTypeCount,
            slang::IComponentType**         outCompositeComponentType,
            ISlangBlob**                    outDiagnostics = nullptr) override;
        SLANG_NO_THROW slang::TypeReflection* SLANG_MCALL specializeType(
            slang::TypeReflection*          type,
            slang::SpecializationArg const* specializationArgs,
            SlangInt                        specializationArgCount,
            ISlangBlob**                    outDiagnostics = nullptr) override;
        SLANG_NO_THROW slang::TypeLayoutReflection* SLANG_MCALL getTypeLayout(
            slang::TypeReflection* type,
            SlangInt               targetIndex = 0,
            slang::LayoutRules     rules = slang::LayoutRules::Default,
            ISlangBlob**    outDiagnostics = nullptr) override;
        SLANG_NO_THROW SlangResult SLANG_MCALL getTypeRTTIMangledName(
            slang::TypeReflection* type,
            ISlangBlob** outNameBlob) override;
        SLANG_NO_THROW SlangResult SLANG_MCALL getTypeConformanceWitnessMangledName(
            slang::TypeReflection* type,
            slang::TypeReflection* interfaceType,
            ISlangBlob** outNameBlob) override;
        SLANG_NO_THROW SlangResult SLANG_MCALL getTypeConformanceWitnessSequentialID(
            slang::TypeReflection* type,
            slang::TypeReflection* interfaceType,
            uint32_t*              outId) override;
        SLANG_NO_THROW SlangResult SLANG_MCALL createCompileRequest(
            SlangCompileRequest**   outCompileRequest) override;

        void addTarget(
            slang::TargetDesc const& desc);
        SlangResult addSearchPath(
            char const* path);
        SlangResult addPreprocessorDefine(
            char const* name,
            char const* value);
        SlangResult setMatrixLayoutMode(
            SlangMatrixLayoutMode mode);

            /// Create an initially-empty linkage
        Linkage(Session* session, ASTBuilder* astBuilder, Linkage* builtinLinkage);

            /// Dtor
        ~Linkage();

            /// Get the parent session for this linkage
        Session* getSessionImpl() { return m_session; }

        // Information on the targets we are being asked to
        // generate code for.
        List<RefPtr<TargetRequest>> targets;

        // Directories to search for `#include` files or `import`ed modules
        SearchDirectoryList searchDirectories;

        SearchDirectoryList const& getSearchDirectories() { return searchDirectories; }

        // Definitions to provide during preprocessing
        Dictionary<String, String> preprocessorDefinitions;

        // Source manager to help track files loaded
        SourceManager m_defaultSourceManager;
        SourceManager* m_sourceManager = nullptr;

        bool m_obfuscateCode = false;

        // Determine whether to output heterogeneity-related code
        bool m_heterogeneous = false;

        // Name pool for looking up names
        NamePool namePool;

        NamePool* getNamePool() { return &namePool; }

        ASTBuilder* getASTBuilder() { return m_astBuilder; }

       
        RefPtr<ASTBuilder> m_astBuilder;

            // cache used by type checking, implemented in check.cpp
        TypeCheckingCache* getTypeCheckingCache();
        void destroyTypeCheckingCache();

        TypeCheckingCache* m_typeCheckingCache = nullptr;

        // Modules that have been dynamically loaded via `import`
        //
        // This is a list of unique modules loaded, in the order they were encountered.
        List<RefPtr<LoadedModule> > loadedModulesList;

        // Map from the path (or uniqueIdentity if available) of a module file to its definition
        Dictionary<String, RefPtr<LoadedModule>> mapPathToLoadedModule;

        // Map from the logical name of a module to its definition
        Dictionary<Name*, RefPtr<LoadedModule>> mapNameToLoadedModules;

        // Map from the mangled name of RTTI objects to sequential IDs
        // used by `switch`-based dynamic dispatch.
        Dictionary<String, uint32_t> mapMangledNameToRTTIObjectIndex;

        // Counters for allocating sequential IDs to witness tables conforming to each interface type.
        Dictionary<String, uint32_t> mapInterfaceMangledNameToSequentialIDCounters;

        // The resulting specialized IR module for each entry point request
        List<RefPtr<IRModule>> compiledModules;

        /// File system implementation to use when loading files from disk.
        ///
        /// If this member is `null`, a default implementation that tries
        /// to use the native OS filesystem will be used instead.
        ///
        ComPtr<ISlangFileSystem> m_fileSystem;

        /// The extended file system implementation. Will be set to a default implementation
        /// if fileSystem is nullptr. Otherwise it will either be fileSystem's interface, 
        /// or a wrapped impl that makes fileSystem operate as fileSystemExt
        ComPtr<ISlangFileSystemExt> m_fileSystemExt;

        
        /// Set if fileSystemExt is a cache file system
        RefPtr<CacheFileSystem> m_cacheFileSystem;

        ISlangFileSystemExt* getFileSystemExt() { return m_fileSystemExt; }
        CacheFileSystem* getCacheFileSystem() const { return m_cacheFileSystem; }

        /// Load a file into memory using the configured file system.
        ///
        /// @param path The path to attempt to load from
        /// @param outBlob A destination pointer to receive the loaded blob
        /// @returns A `SlangResult` to indicate success or failure.
        ///
        SlangResult loadFile(String const& path, PathInfo& outPathInfo, ISlangBlob** outBlob);

        Expr* parseTermString(String str, RefPtr<Scope> scope);

        Type* specializeType(
            Type*           unspecializedType,
            Int             argCount,
            Type* const*    args,
            DiagnosticSink* sink);

            /// Add a mew target amd return its index.
        UInt addTarget(
            CodeGenTarget   target);

        RefPtr<Module> loadModule(
            Name*               name,
            const PathInfo&     filePathInfo,
            ISlangBlob*         fileContentsBlob,
            SourceLoc const&    loc,
            DiagnosticSink*     sink);

        void loadParsedModule(
            RefPtr<FrontEndCompileRequest>  compileRequest,
            RefPtr<TranslationUnitRequest>  translationUnit,
            Name*                           name,
            PathInfo const&                 pathInfo);

            /// Load a module of the given name.
        Module* loadModule(String const& name);

        RefPtr<Module> findOrImportModule(
            Name*               name,
            SourceLoc const&    loc,
            DiagnosticSink*     sink);

        SourceManager* getSourceManager()
        {
            return m_sourceManager;
        }

            /// Override the source manager for the linkage.
            ///
            /// This is only used to install a temporary override when
            /// parsing stuff from strings (where we don't want to retain
            /// full source files for the parsed result).
            ///
            /// TODO: We should remove the need for this hack.
            ///
        void setSourceManager(SourceManager* sourceManager)
        {
            m_sourceManager = sourceManager;
        }

        void setRequireCacheFileSystem(bool requireCacheFileSystem);

        void setFileSystem(ISlangFileSystem* fileSystem);

        /// The layout to use for matrices by default (row/column major)
        MatrixLayoutMode defaultMatrixLayoutMode = kMatrixLayoutMode_ColumnMajor;
        MatrixLayoutMode getDefaultMatrixLayoutMode() { return defaultMatrixLayoutMode; }

        DebugInfoLevel debugInfoLevel = DebugInfoLevel::None;

        OptimizationLevel optimizationLevel = OptimizationLevel::Default;

        SerialCompressionType serialCompressionType = SerialCompressionType::VariableByteLite;

        bool m_requireCacheFileSystem = false;
        bool m_useFalcorCustomSharedKeywordSemantics = false;

        // Modules that have been read in with the -r option
        List<RefPtr<IRModule>> m_libModules;

        void _stopRetainingParentSession()
        {
            m_retainedSession = nullptr;
        }

    private:
            /// The global Slang library session that this linkage is a child of
        Session* m_session = nullptr;

        RefPtr<Session> m_retainedSession;



            /// Tracks state of modules currently being loaded.
            ///
            /// This information is used to diagnose cases where
            /// a user tries to recursively import the same module
            /// (possibly along a transitive chain of `import`s).
            ///
        struct ModuleBeingImportedRAII
        {
        public:
            ModuleBeingImportedRAII(
                Linkage*    linkage,
                Module*     module)
                : linkage(linkage)
                , module(module)
            {
                next = linkage->m_modulesBeingImported;
                linkage->m_modulesBeingImported = this;
            }

            ~ModuleBeingImportedRAII()
            {
                linkage->m_modulesBeingImported = next;
            }

            Linkage* linkage;
            Module* module;
            ModuleBeingImportedRAII* next;
        };

        // Any modules currently being imported will be listed here
        ModuleBeingImportedRAII* m_modulesBeingImported = nullptr;

            /// Is the given module in the middle of being imported?
        bool isBeingImported(Module* module);

        List<Type*> m_specializedTypes;

    };

        /// Shared functionality between front- and back-end compile requests.
        ///
        /// This is the base class for both `FrontEndCompileRequest` and
        /// `BackEndCompileRequest`, and allows a small number of parts of
        /// the compiler to be easily invocable from either front-end or
        /// back-end work.
        ///
    class CompileRequestBase : public RefObject
    {
        // TODO: We really shouldn't need this type in the long run.
        // The few places that rely on it should be refactored to just
        // depend on the underlying information (a linkage and a diagnostic
        // sink) directly.
        //
        // The flags to control dumping and validation of IR should be
        // moved to some kind of shared settings/options `struct` that
        // both front-end and back-end requests can store.

    public:
        Session* getSession();
        Linkage* getLinkage() { return m_linkage; }
        DiagnosticSink* getSink() { return m_sink; }
        SourceManager* getSourceManager() { return getLinkage()->getSourceManager(); }
        NamePool* getNamePool() { return getLinkage()->getNamePool(); }
        ISlangFileSystemExt* getFileSystemExt() { return getLinkage()->getFileSystemExt(); }
        SlangResult loadFile(String const& path, PathInfo& outPathInfo, ISlangBlob** outBlob) { return getLinkage()->loadFile(path, outPathInfo, outBlob); }

        bool shouldDumpIR = false;
        bool shouldValidateIR = false;

        bool shouldDumpAST = false;
        bool shouldDocument = false;

            /// If true will after lexical analysis output the hierarchy of includes to stdout
        bool outputIncludes = false;

    protected:
        CompileRequestBase(
            Linkage*        linkage,
            DiagnosticSink* sink);

    private:
        Linkage* m_linkage = nullptr;
        DiagnosticSink* m_sink = nullptr;
    };

        /// A request to compile source code to an AST + IR.
    class FrontEndCompileRequest : public CompileRequestBase
    {
    public:
        FrontEndCompileRequest(
            Linkage*        linkage,
            DiagnosticSink* sink);

        int addEntryPoint(
            int                     translationUnitIndex,
            String const&           name,
            Profile                 entryPointProfile);

        // Translation units we are being asked to compile
        List<RefPtr<TranslationUnitRequest> > translationUnits;

        RefPtr<TranslationUnitRequest> getTranslationUnit(UInt index) { return translationUnits[index]; }

        // Compile flags to be shared by all translation units
        SlangCompileFlags compileFlags = 0;

        // If true then generateIR will serialize out IR, and serialize back in again. Making 
        // serialization a bottleneck or firewall between the front end and the backend
        bool useSerialIRBottleneck = false; 

        // If true will serialize and de-serialize with debug information
        bool verifyDebugSerialization = false;

        List<RefPtr<FrontEndEntryPointRequest>> m_entryPointReqs;

        List<RefPtr<FrontEndEntryPointRequest>> const& getEntryPointReqs() { return m_entryPointReqs; }
        UInt getEntryPointReqCount() { return m_entryPointReqs.getCount(); }
        FrontEndEntryPointRequest* getEntryPointReq(UInt index) { return m_entryPointReqs[index]; }

        // Directories to search for `#include` files or `import`ed modules
        // NOTE! That for now these search directories are not settable via the API
        // so the search directories on Linkage is used for #include as well as for modules.
        SearchDirectoryList searchDirectories;

        SearchDirectoryList const& getSearchDirectories() { return searchDirectories; }

        // Definitions to provide during preprocessing
        Dictionary<String, String> preprocessorDefinitions;

        void parseTranslationUnit(
            TranslationUnitRequest* translationUnit);

        // Perform primary semantic checking on all
        // of the translation units in the program
        void checkAllTranslationUnits();

        void checkEntryPoints();

        void generateIR();

        SlangResult executeActionsInner();

            /// Add a translation unit to be compiled.
            ///
            /// @param language The source language that the translation unit will use (e.g., `SourceLanguage::Slang`
            /// @param moduleName The name that will be used for the module compile from the translation unit. 
            ///
            /// If moduleName is passed as nullptr a module name is generated.
            /// If all translation units in a compile request use automatically generated
            /// module names, then they are guaranteed not to conflict with one another.
            /// 
            /// @return The zero-based index of the translation unit in this compile request.
        int addTranslationUnit(SourceLanguage language, Name* moduleName);

        int addTranslationUnit(TranslationUnitRequest* translationUnit);

        void addTranslationUnitSourceFile(
            int             translationUnitIndex,
            SourceFile*     sourceFile);

        void addTranslationUnitSourceBlob(
            int             translationUnitIndex,
            String const&   path,
            ISlangBlob*     sourceBlob);

        void addTranslationUnitSourceString(
            int             translationUnitIndex,
            String const&   path,
            String const&   source);

        void addTranslationUnitSourceFile(
            int             translationUnitIndex,
            String const&   path);

            /// Get a component type that represents the global scope of the compile request.
        ComponentType* getGlobalComponentType()  { return m_globalComponentType; }

            /// Get a component type that represents the global scope of the compile request, plus the requested entry points.
        ComponentType* getGlobalAndEntryPointsComponentType() { return m_globalAndEntryPointsComponentType; }

        List<RefPtr<ComponentType>> const& getUnspecializedEntryPoints() { return m_unspecializedEntryPoints; }

            /// Does the code we are compiling represent part of the Slang standard library?
        bool m_isStandardLibraryCode = false;

        Name* m_defaultModuleName = nullptr;

            /// An "extra" entry point that was added via a library reference
        struct ExtraEntryPointInfo
        {
            Name*   name;
            Profile profile;
            String  mangledName;
        };

            /// A list of "extra" entry points added via a library reference
        List<ExtraEntryPointInfo> m_extraEntryPoints;

    private:
            /// A component type that includes only the global scopes of the translation unit(s) that were compiled.
        RefPtr<ComponentType> m_globalComponentType;

            /// A component type that extends the global scopes with all of the entry points that were specified.
        RefPtr<ComponentType> m_globalAndEntryPointsComponentType;

        List<RefPtr<ComponentType>> m_unspecializedEntryPoints;
    };

        /// A visitor for use with `ComponentType`s, allowing dispatch over the concrete subclasses.
    class ComponentTypeVisitor
    {
    public:
        // The following methods should be overriden in a concrete subclass
        // to customize how it acts on each of the concrete types of component.
        //
        // In cases where the application wants to simply "recurse" on a
        // composite, specialized, or legacy component type it can use
        // the `visitChildren` methods below.
        //
        virtual void visitEntryPoint(EntryPoint* entryPoint, EntryPoint::EntryPointSpecializationInfo* specializationInfo) = 0;
        virtual void visitModule(Module* module, Module::ModuleSpecializationInfo* specializationInfo) = 0;
        virtual void visitComposite(CompositeComponentType* composite, CompositeComponentType::CompositeSpecializationInfo* specializationInfo) = 0;
        virtual void visitSpecialized(SpecializedComponentType* specialized) = 0;

    protected:
        // These helpers can be used to recurse into the logical children of a
        // component type, and are useful for the common case where a visitor
        // only cares about a few leaf cases.
        //
        void visitChildren(CompositeComponentType* composite, CompositeComponentType::CompositeSpecializationInfo* specializationInfo);
        void visitChildren(SpecializedComponentType* specialized);
    };

        /// A `TargetProgram` represents a `ComponentType` specialized for a particular `TargetRequest`
        ///
        /// TODO: This should probably be renamed to `TargetComponentType`.
        ///
        /// By binding a component type to a specific target, a `TargetProgram` allows
        /// for things like layout to be computed, that fundamentally depend on
        /// the choice of target.
        ///
        /// A `TargetProgram` handles request for compiled kernel code for
        /// entry point functions. In practice, kernel code can only be
        /// correctly generated when the underlying `ComponentType` is "fully linked"
        /// (has no remaining unsatisfied requirements).
        ///
    class TargetProgram : public RefObject
    {
    public:
        TargetProgram(
            ComponentType*  componentType,
            TargetRequest*  targetReq);

            /// Get the underlying program
        ComponentType* getProgram() { return m_program; }

            /// Get the underlying target
        TargetRequest* getTargetReq() { return m_targetReq; }

            /// Get the layout for the program on the target.
            ///
            /// If this is the first time the layout has been
            /// requested, report any errors that arise during
            /// layout to the given `sink`.
            ///
        ProgramLayout* getOrCreateLayout(DiagnosticSink* sink);

            /// Get the layout for the program on the taarget.
            ///
            /// This routine assumes that `getOrCreateLayout`
            /// has already been called previously.
            ///
        ProgramLayout* getExistingLayout()
        {
            SLANG_ASSERT(m_layout);
            return m_layout;
        }

            /// Get the compiled code for an entry point on the target.
            ///
            /// If this is the first time that code generation has
            /// been requested, report any errors that arise during
            /// code generation to the given `sink`.
            ///
        CompileResult& getOrCreateEntryPointResult(Int entryPointIndex, DiagnosticSink* sink);
        CompileResult& getOrCreateWholeProgramResult(DiagnosticSink* sink);


        CompileResult& getExistingWholeProgramResult()
        {
            return m_wholeProgramResult;
        }
            /// Get the compiled code for an entry point on the target.
            ///
            /// This routine assumes that `getOrCreateEntryPointResult`
            /// has already been called previously.
            ///
        CompileResult& getExistingEntryPointResult(Int entryPointIndex)
        {
            return m_entryPointResults[entryPointIndex];
        }

        CompileResult& _createWholeProgramResult(
            BackEndCompileRequest*  backEndRequest,
            EndToEndCompileRequest* endToEndRequest);
            /// Internal helper for `getOrCreateEntryPointResult`.
            ///
            /// This is used so that command-line and API-based
            /// requests for code can bottleneck through the same place.
            ///
            /// Shouldn't be called directly by most code.
            ///
        CompileResult& _createEntryPointResult(
            Int                     entryPointIndex,
            BackEndCompileRequest*  backEndRequest,
            EndToEndCompileRequest* endToEndRequest);

        RefPtr<IRModule> getOrCreateIRModuleForLayout(DiagnosticSink* sink);

        RefPtr<IRModule> getExistingIRModuleForLayout()
        {
            return m_irModuleForLayout;
        }

    private:
        RefPtr<IRModule> createIRModuleForLayout(DiagnosticSink* sink);

        // The program being compiled or laid out
        ComponentType* m_program;

        // The target that code/layout will be generated for
        TargetRequest* m_targetReq;

        // The computed layout, if it has been generated yet
        RefPtr<ProgramLayout> m_layout;

        // Generated compile results for each entry point
        // in the parent `Program` (indexing matches
        // the order they are given in the `Program`)
        CompileResult m_wholeProgramResult;
        List<CompileResult> m_entryPointResults;

        RefPtr<IRModule> m_irModuleForLayout;
    };

        /// A request to generate code for a program
    class BackEndCompileRequest : public CompileRequestBase
    {
    public:
        BackEndCompileRequest(
            Linkage*        linkage,
            DiagnosticSink* sink,
            ComponentType*  program = nullptr);

        // Should we dump intermediate results along the way, for debugging?
        bool shouldDumpIntermediates = false;

        // How should `#line` directives be emitted (if at all)?
        LineDirectiveMode lineDirectiveMode = LineDirectiveMode::Default;

        LineDirectiveMode getLineDirectiveMode() { return lineDirectiveMode; }

        ComponentType* getProgram() { return m_program; }
        void setProgram(ComponentType* program) { m_program = program; }

        // Should R/W images without explicit formats be assumed to have "unknown" format?
        //
        // The default behavior is to make a best-effort guess as to what format is intended.
        //
        bool useUnknownImageFormatAsDefault = false;

            /// Should SPIR-V be generated directly from Slang IR rather than via translation to GLSL?
        bool shouldEmitSPIRVDirectly = false;


        // If true will disable generics/existential value specialization pass.
        bool disableSpecialization = false;

        // If true will disable generating dynamic dispatch code.
        bool disableDynamicDispatch = false;

        String m_dumpIntermediatePrefix;

    private:
        RefPtr<ComponentType> m_program;
    };

    // UUID to identify EndToEndCompileRequest from an interface
    #define SLANG_UUID_EndToEndCompileRequest { 0xce6d2383, 0xee1b, 0x4fd7, { 0xa0, 0xf, 0xb8, 0xb6, 0x33, 0x12, 0x95, 0xc8 } };

        /// A compile request that spans the front and back ends of the compiler
        ///
        /// This is what the command-line `slangc` uses, as well as the legacy
        /// C API. It ties together the functionality of `Linkage`,
        /// `FrontEndCompileRequest`, and `BackEndCompileRequest`, plus a small
        /// number of additional features that primarily make sense for
        /// command-line usage.
        ///
    class EndToEndCompileRequest : public RefObject, public slang::ICompileRequest
    {
    public:
        // ISlangUnknown
        SLANG_NO_THROW SlangResult SLANG_MCALL queryInterface(SlangUUID const& uuid, void** outObject) SLANG_OVERRIDE;
        SLANG_REF_OBJECT_IUNKNOWN_ADD_REF
        SLANG_REF_OBJECT_IUNKNOWN_RELEASE

        // slang::ICompileRequest
        virtual SLANG_NO_THROW void SLANG_MCALL setFileSystem(ISlangFileSystem* fileSystem) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setCompileFlags(SlangCompileFlags flags) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setDumpIntermediates(int  enable) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setDumpIntermediatePrefix(const char* prefix) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setLineDirectiveMode(SlangLineDirectiveMode  mode) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setCodeGenTarget(SlangCompileTarget target) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW int SLANG_MCALL addCodeGenTarget(SlangCompileTarget target) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setTargetProfile(int targetIndex, SlangProfileID profile) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setTargetFlags(int targetIndex, SlangTargetFlags flags) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setTargetFloatingPointMode(int targetIndex, SlangFloatingPointMode mode) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setTargetMatrixLayoutMode(int targetIndex, SlangMatrixLayoutMode mode) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setMatrixLayoutMode(SlangMatrixLayoutMode mode) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setDebugInfoLevel(SlangDebugInfoLevel level) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setOptimizationLevel(SlangOptimizationLevel level) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setOutputContainerFormat(SlangContainerFormat format) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setPassThrough(SlangPassThrough passThrough) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setDiagnosticCallback(SlangDiagnosticCallback callback, void const* userData) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setWriter(SlangWriterChannel channel, ISlangWriter* writer) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW ISlangWriter* SLANG_MCALL getWriter(SlangWriterChannel channel) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL addSearchPath(const char* searchDir) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL addPreprocessorDefine(const char* key, const char* value) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL processCommandLineArguments(char const* const* args, int argCount) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW int SLANG_MCALL addTranslationUnit(SlangSourceLanguage language, char const* name) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setDefaultModuleName(const char* defaultModuleName) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL addTranslationUnitPreprocessorDefine(int translationUnitIndex, const char* key, const char* value) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL addTranslationUnitSourceFile(int translationUnitIndex, char const* path) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL addTranslationUnitSourceString(int translationUnitIndex, char const* path, char const* source) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL addLibraryReference(const void* libData, size_t libDataSize) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL addTranslationUnitSourceStringSpan(int translationUnitIndex, char const* path, char const* sourceBegin, char const* sourceEnd) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL addTranslationUnitSourceBlob(int translationUnitIndex, char const* path, ISlangBlob* sourceBlob) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW int SLANG_MCALL addEntryPoint(int translationUnitIndex, char const* name, SlangStage stage) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW int SLANG_MCALL addEntryPointEx(int translationUnitIndex, char const* name, SlangStage stage, int genericArgCount, char const** genericArgs) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL setGlobalGenericArgs(int genericArgCount, char const** genericArgs) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL setTypeNameForGlobalExistentialTypeParam(int slotIndex, char const* typeName) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL setTypeNameForEntryPointExistentialTypeParam(int entryPointIndex, int slotIndex, char const* typeName) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL compile() SLANG_OVERRIDE;
        virtual SLANG_NO_THROW char const* SLANG_MCALL getDiagnosticOutput() SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getDiagnosticOutputBlob(ISlangBlob** outBlob) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW int SLANG_MCALL getDependencyFileCount() SLANG_OVERRIDE;
        virtual SLANG_NO_THROW char const* SLANG_MCALL getDependencyFilePath(int index) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW int SLANG_MCALL getTranslationUnitCount() SLANG_OVERRIDE;
        virtual SLANG_NO_THROW char const* SLANG_MCALL getEntryPointSource(int entryPointIndex) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void const* SLANG_MCALL getEntryPointCode(int entryPointIndex, size_t* outSize) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getEntryPointCodeBlob(int entryPointIndex, int targetIndex, ISlangBlob** outBlob) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getEntryPointHostCallable(int entryPointIndex, int targetIndex, ISlangSharedLibrary**   outSharedLibrary) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getTargetCodeBlob(int targetIndex, ISlangBlob** outBlob) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getTargetHostCallable(int targetIndex, ISlangSharedLibrary** outSharedLibrary) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void const* SLANG_MCALL getCompileRequestCode(size_t* outSize) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getContainerCode(ISlangBlob** outBlob) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL loadRepro(ISlangFileSystem* fileSystem, const void* data, size_t size) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL saveRepro(ISlangBlob** outBlob) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL enableReproCapture() SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getProgram(slang::IComponentType** outProgram) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getEntryPoint(SlangInt entryPointIndex, slang::IComponentType** outEntryPoint) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getModule(SlangInt translationUnitIndex, slang::IModule** outModule) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getSession(slang::ISession** outSession) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangReflection* SLANG_MCALL getReflection() SLANG_OVERRIDE;
        virtual SLANG_NO_THROW void SLANG_MCALL setCommandLineCompilerMode() SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL addTargetCapability(SlangInt targetIndex, SlangCapabilityID capability) SLANG_OVERRIDE;
        virtual SLANG_NO_THROW SlangResult SLANG_MCALL getProgramWithEntryPoints(slang::IComponentType** outProgram) SLANG_OVERRIDE;


        EndToEndCompileRequest(
            Session* session);

        EndToEndCompileRequest(
            Linkage* linkage);

            // What container format are we being asked to generate?
            // If it's set to a format, the container blob will be calculated during compile
        ContainerFormat m_containerFormat = ContainerFormat::None;

            /// Where the container blob is stored. This is calculated as part of compile if m_containerFormat is set to
            /// a supported format. 
        ComPtr<ISlangBlob> m_containerBlob;

            // Path to output container to
        String m_containerOutputPath;

        // Should we just pass the input to another compiler?
        PassThroughMode m_passThrough = PassThroughMode::None;

            /// Source code for the specialization arguments to use for the global specialization parameters of the program.
        List<String> m_globalSpecializationArgStrings;

        bool m_shouldSkipCodegen = false;

        // Are we being driven by the command-line `slangc`, and should act accordingly?
        bool m_isCommandLineCompile = false;

        String m_diagnosticOutput;


            // If set, will dump the compilation state 
        String m_dumpRepro;

            /// If set, if a compilation failure occurs will attempt to save off a dump repro with a unique name
        bool m_dumpReproOnError = false;

            /// A blob holding the diagnostic output
        ComPtr<ISlangBlob> m_diagnosticOutputBlob;

            /// Per-entry-point information not tracked by other compile requests
        class EntryPointInfo : public RefObject
        {
        public:
                /// Source code for the specialization arguments to use for the specialization parameters of the entry point.
            List<String> specializationArgStrings;
        };
        List<EntryPointInfo> m_entryPoints;

            /// Per-target information only needed for command-line compiles
        class TargetInfo : public RefObject
        {
        public:
            // Requested output paths for each entry point.
            // An empty string indices no output desired for
            // the given entry point.
            Dictionary<Int, String> entryPointOutputPaths;
            String wholeTargetOutputPath;
        };
        Dictionary<TargetRequest*, RefPtr<TargetInfo>> m_targetInfos;

            /// Writes the modules in a container to the stream
        SlangResult writeContainerToStream(Stream* stream);
        
            /// If a container format has been specified produce a container (stored in m_containerBlob)
        SlangResult maybeCreateContainer();
            /// If a container has been constructed and the filename/path has contents will try to write
            /// the container contents to the file
        SlangResult maybeWriteContainer(const String& fileName);

        Linkage* getLinkage() { return m_linkage; }

        int addEntryPoint(
            int                     translationUnitIndex,
            String const&           name,
            Profile                 profile,
            List<String> const &    genericTypeNames);

        void setWriter(WriterChannel chan, ISlangWriter* writer);
        ISlangWriter* getWriter(WriterChannel chan) const { return m_writers[int(chan)]; }

            /// The end to end request can be passed as nullptr, if not driven by one
        SlangResult executeActionsInner();
        SlangResult executeActions();

        Session* getSession() { return m_session; }
        DiagnosticSink* getSink() { return &m_sink; }
        NamePool* getNamePool() { return getLinkage()->getNamePool(); }

        FrontEndCompileRequest* getFrontEndReq() { return m_frontEndReq; }
        BackEndCompileRequest* getBackEndReq() { return m_backEndReq; }

        ComponentType* getUnspecializedGlobalComponentType() { return getFrontEndReq()->getGlobalComponentType(); }
        ComponentType* getUnspecializedGlobalAndEntryPointsComponentType()
        {
            return getFrontEndReq()->getGlobalAndEntryPointsComponentType();
        }

        ComponentType* getSpecializedGlobalComponentType() { return m_specializedGlobalComponentType; }
        ComponentType* getSpecializedGlobalAndEntryPointsComponentType() { return m_specializedGlobalAndEntryPointsComponentType; }

        ComponentType* getSpecializedEntryPointComponentType(Index index)
        {
            return m_specializedEntryPoints[index];
        }

    private:

        ISlangUnknown* getInterface(const Guid& guid);

        void init();

        Session*                        m_session = nullptr;
        RefPtr<Linkage>                 m_linkage;
        DiagnosticSink                  m_sink;
        RefPtr<FrontEndCompileRequest>  m_frontEndReq;
        RefPtr<ComponentType>           m_specializedGlobalComponentType;
        RefPtr<ComponentType>           m_specializedGlobalAndEntryPointsComponentType;
        List<RefPtr<ComponentType>>     m_specializedEntryPoints;
        RefPtr<BackEndCompileRequest>   m_backEndReq;

        // For output
        ComPtr<ISlangWriter> m_writers[SLANG_WRITER_CHANNEL_COUNT_OF];
    };

    void generateOutput(
        BackEndCompileRequest* compileRequest);

    void generateOutput(
        EndToEndCompileRequest* compileRequest);

    // Helper to dump intermediate output when debugging
    void maybeDumpIntermediate(
        BackEndCompileRequest* compileRequest,
        void const*     data,
        size_t          size,
        CodeGenTarget   target);
    void maybeDumpIntermediate(
        BackEndCompileRequest* compileRequest,
        char const*     text,
        CodeGenTarget   target);

    void maybeDumpIntermediate(
        BackEndCompileRequest* compileRequest,
        DownstreamCompileResult* compileResult,
        CodeGenTarget   target);

    /* Returns SLANG_OK if pass through support is available */
    SlangResult checkExternalCompilerSupport(Session* session, PassThroughMode passThrough);
    /* Report an error appearing from external compiler to the diagnostic sink error to the diagnostic sink.
    @param compilerName The name of the compiler the error came for (or nullptr if not known)
    @param res Result associated with the error. The error code will be reported. (Can take HRESULT - and will expand to string if known)
    @param diagnostic The diagnostic string associated with the compile failure
    @param sink The diagnostic sink to report to */
    void reportExternalCompileError(const char* compilerName, SlangResult res, const UnownedStringSlice& diagnostic, DiagnosticSink* sink);

    /* Determines a suitable filename to identify the input for a given entry point being compiled.
    If the end-to-end compile is a pass-through case, will attempt to find the (unique) source file
    pathname for the translation unit containing the entry point at `entryPointIndex.
    If the compilation is not in a pass-through case, then always returns `"slang-generated"`.
    @param endToEndReq The end-to-end compile request which might be using pass-through compilation
    @param entryPointIndex The index of the entry point to compute a filename for.
    @return the appropriate source filename */
    String calcSourcePathForEntryPoint(EndToEndCompileRequest* endToEndReq, Int entryPointIndex);
    String calcSourcePathForEntryPoints(EndToEndCompileRequest* endToEndReq, const List<Int>& entryPointIndices);

    struct SourceResult
    {
        void reset()
        {
            source = String();
            extensionTracker.setNull();
        }

        String source;
        // Must be cast to a specific extension tracker such as GLSLExtensionTracker
        RefPtr<RefObject> extensionTracker;
    };

    /* Emits entry point source taking into account if a pass-through or not. Uses 'target' to determine
    the target (not targetReq) */
    SlangResult emitEntryPointsSource(
        BackEndCompileRequest*  compileRequest,
        const List<Int>&        entryPointIndices,
        TargetRequest*          targetReq,
        CodeGenTarget           target,
        EndToEndCompileRequest* endToEndReq,
        SourceResult&           outSource);

    SlangResult emitEntryPointSource(
        BackEndCompileRequest*  compileRequest,
        Int                     entryPointIndex,
        TargetRequest*          targetReq,
        CodeGenTarget           target,
        EndToEndCompileRequest* endToEndReq,
        SourceResult&           outSource);

    //

    // Information about BaseType that's useful for checking literals 
    struct BaseTypeInfo
    {
        typedef uint8_t Flags;
        struct Flag 
        {
            enum Enum : Flags
            {
                Signed = 0x1,
                FloatingPoint = 0x2,
                Integer = 0x4,
            };
        };

        SLANG_FORCE_INLINE static const BaseTypeInfo& getInfo(BaseType baseType) { return s_info[Index(baseType)]; }

        static UnownedStringSlice asText(BaseType baseType);

        uint8_t sizeInBytes;               ///< Size of type in bytes
        Flags flags;
        uint8_t baseType;

        static bool check();

    private:
        static const BaseTypeInfo s_info[Index(BaseType::CountOf)];
    };

    class Session : public RefObject, public slang::IGlobalSession
    {
    public:
        SLANG_REF_OBJECT_IUNKNOWN_ALL

        ISlangUnknown* getInterface(const Guid& guid);

        // slang::IGlobalSession 
        SLANG_NO_THROW SlangResult SLANG_MCALL createSession(slang::SessionDesc const&  desc, slang::ISession** outSession) override;
        SLANG_NO_THROW SlangProfileID SLANG_MCALL findProfile(char const* name) override;
        SLANG_NO_THROW void SLANG_MCALL setDownstreamCompilerPath(SlangPassThrough passThrough, char const* path) override;
        SLANG_NO_THROW void SLANG_MCALL setDownstreamCompilerPrelude(SlangPassThrough inPassThrough, char const* prelude) override;
        SLANG_NO_THROW void SLANG_MCALL getDownstreamCompilerPrelude(SlangPassThrough inPassThrough, ISlangBlob** outPrelude) override;
        SLANG_NO_THROW const char* SLANG_MCALL getBuildTagString() override;
        SLANG_NO_THROW SlangResult SLANG_MCALL setDefaultDownstreamCompiler(SlangSourceLanguage sourceLanguage, SlangPassThrough defaultCompiler) override;
        SLANG_NO_THROW SlangPassThrough SLANG_MCALL getDefaultDownstreamCompiler(SlangSourceLanguage sourceLanguage) override;

        SLANG_NO_THROW void SLANG_MCALL setLanguagePrelude(SlangSourceLanguage inSourceLanguage, char const* prelude) override;
        SLANG_NO_THROW void SLANG_MCALL getLanguagePrelude(SlangSourceLanguage inSourceLanguage, ISlangBlob** outPrelude) override;

        SLANG_NO_THROW SlangResult SLANG_MCALL createCompileRequest(slang::ICompileRequest** outCompileRequest) override;
        
        SLANG_NO_THROW void SLANG_MCALL addBuiltins(char const* sourcePath, char const* sourceString) override;
        SLANG_NO_THROW void SLANG_MCALL setSharedLibraryLoader(ISlangSharedLibraryLoader* loader) override;
        SLANG_NO_THROW ISlangSharedLibraryLoader* SLANG_MCALL getSharedLibraryLoader() override;
        SLANG_NO_THROW SlangResult SLANG_MCALL checkCompileTargetSupport(SlangCompileTarget target) override;
        SLANG_NO_THROW SlangResult SLANG_MCALL checkPassThroughSupport(SlangPassThrough passThrough) override;

        SLANG_NO_THROW SlangResult SLANG_MCALL compileStdLib(slang::CompileStdLibFlags flags) override;
        SLANG_NO_THROW SlangResult SLANG_MCALL loadStdLib(const void* stdLib, size_t stdLibSizeInBytes) override;
        SLANG_NO_THROW SlangResult SLANG_MCALL saveStdLib(SlangArchiveType archiveType, ISlangBlob** outBlob) override;

        SLANG_NO_THROW SlangCapabilityID SLANG_MCALL findCapability(char const* name) override;

            /// Get the default compiler for a language
        DownstreamCompiler* getDefaultDownstreamCompiler(SourceLanguage sourceLanguage);

        enum class SharedLibraryFuncType
        {
            Glslang_Compile_1_0,
            Glslang_Compile_1_1,
            Fxc_D3DCompile,
            Fxc_D3DDisassemble,
            Dxc_DxcCreateInstance,
            CountOf,
        };

        //

        RefPtr<Scope>   baseLanguageScope;
        RefPtr<Scope>   coreLanguageScope;
        RefPtr<Scope>   hlslLanguageScope;
        RefPtr<Scope>   slangLanguageScope;

        ModuleDecl* baseModuleDecl = nullptr;
        List<RefPtr<Module>> stdlibModules;

        SourceManager   builtinSourceManager;

        SourceManager* getBuiltinSourceManager() { return &builtinSourceManager; }

        // Name pool stuff for unique-ing identifiers

        RootNamePool rootNamePool;
        NamePool namePool;

        RootNamePool* getRootNamePool() { return &rootNamePool; }
        NamePool* getNamePool() { return &namePool; }
        Name* getNameObj(String name) { return namePool.getName(name); }
        Name* tryGetNameObj(String name) { return namePool.tryGetName(name); }
        //

            /// This AST Builder should only be used for creating AST nodes that are global across requests
            /// not doing so could lead to memory being consumed but not used.
        ASTBuilder* getGlobalASTBuilder() { return globalAstBuilder; }

        RefPtr<ASTBuilder> globalAstBuilder;

        // Generated code for stdlib, etc.
        String stdlibPath;
        String coreLibraryCode;
        String slangLibraryCode;
        String hlslLibraryCode;
        String glslLibraryCode;

        String getStdlibPath();
        String getCoreLibraryCode();
        String getHLSLLibraryCode();

     
        RefPtr<SharedASTBuilder> m_sharedASTBuilder;


        //

        void _setSharedLibraryLoader(ISlangSharedLibraryLoader* loader);

            /// Will try to load the library by specified name (using the set loader), if not one already available.
        DownstreamCompiler* getOrLoadDownstreamCompiler(PassThroughMode type, DiagnosticSink* sink);
            /// Will unload the specified shared library if it's currently loaded 
        void resetDownstreamCompiler(PassThroughMode type);

        SlangFuncPtr getSharedLibraryFunc(SharedLibraryFuncType type, DiagnosticSink* sink);

            /// Get the prelude associated with the language
        const String& getPreludeForLanguage(SourceLanguage language) { return m_languagePreludes[int(language)]; }

            /// Get the built in linkage -> handy to get the stdlibs from
        Linkage* getBuiltinLinkage() const { return m_builtinLinkage; }

        void init();

        void addBuiltinSource(
            RefPtr<Scope> const&    scope,
            String const&           path,
            String const&           source);
        ~Session();

        ComPtr<ISlangSharedLibraryLoader> m_sharedLibraryLoader;                    ///< The shared library loader (never null)

        SlangFuncPtr m_sharedLibraryFunctions[int(SharedLibraryFuncType::CountOf)]; ///< Functions from shared libraries

        int m_downstreamCompilerInitialized = 0;                                        

        RefPtr<DownstreamCompilerSet> m_downstreamCompilerSet;                                  ///< Information about all available downstream compilers.
        RefPtr<DownstreamCompiler> m_downstreamCompilers[int(PassThroughMode::CountOf)];        ///< A downstream compiler for a pass through
        DownstreamCompilerLocatorFunc m_downstreamCompilerLocators[int(PassThroughMode::CountOf)];

    private:

        SlangResult _readBuiltinModule(ISlangFileSystem* fileSystem, Scope* scope, String moduleName);

        SlangResult _loadRequest(EndToEndCompileRequest* request, const void* data, size_t size);

            /// Linkage used for all built-in (stdlib) code.
        RefPtr<Linkage> m_builtinLinkage;

        String m_downstreamCompilerPaths[int(PassThroughMode::CountOf)];         ///< Paths for each pass through
        String m_languagePreludes[int(SourceLanguage::CountOf)];                  ///< Prelude for each source language
        PassThroughMode m_defaultDownstreamCompilers[int(SourceLanguage::CountOf)];
    };


//
// The following functions are utilties to convert between
// matching "external" (public API) and "internal" (implementation)
// types. They are favored over explicit casts because they
// help avoid making incorrect conversions (e.g., when using
// `reinterpret_cast` or C-style casts), and because they
// abstract over the conversion required for each pair of types.
//

SLANG_FORCE_INLINE slang::IGlobalSession* asExternal(Session* session)
{
    return static_cast<slang::IGlobalSession*>(session);
}

SLANG_FORCE_INLINE Session* asInternal(slang::IGlobalSession* session)
{
    return static_cast<Session*>(session);
}

SLANG_FORCE_INLINE slang::ISession* asExternal(Linkage* linkage)
{
    return static_cast<slang::ISession*>(linkage);
}

SLANG_FORCE_INLINE Module* asInternal(slang::IModule* module)
{
    return static_cast<Module*>(module);
}

SLANG_FORCE_INLINE slang::IModule* asExternal(Module* module)
{
    return static_cast<slang::IModule*>(module);
}

ComponentType* asInternal(slang::IComponentType* inComponentType);

SLANG_FORCE_INLINE slang::IComponentType* asExternal(ComponentType* componentType)
{
    return static_cast<slang::IComponentType*>(componentType);
}

SLANG_FORCE_INLINE slang::ProgramLayout* asExternal(ProgramLayout* programLayout)
{
    return (slang::ProgramLayout*) programLayout;
}

SLANG_FORCE_INLINE Type* asInternal(slang::TypeReflection* type)
{
    return reinterpret_cast<Type*>(type);
}

SLANG_FORCE_INLINE slang::TypeReflection* asExternal(Type* type)
{
    return reinterpret_cast<slang::TypeReflection*>(type);
}

SLANG_FORCE_INLINE TypeLayout* asInternal(slang::TypeLayoutReflection* type)
{
    return reinterpret_cast<TypeLayout*>(type);
}

SLANG_FORCE_INLINE slang::TypeLayoutReflection* asExternal(TypeLayout* type)
{
    return reinterpret_cast<slang::TypeLayoutReflection*>(type);
}

SLANG_FORCE_INLINE SlangCompileRequest* asExternal(EndToEndCompileRequest* request)
{
    return static_cast<SlangCompileRequest*>(request);
}

SLANG_FORCE_INLINE EndToEndCompileRequest* asInternal(SlangCompileRequest* request)
{
    // Converts to the internal type -- does a runtime type check through queryInterfae
    SLANG_ASSERT(request);
    EndToEndCompileRequest* endToEndRequest = nullptr;
    // NOTE! We aren't using to access an interface, so *doesn't* return with a refcount
    request->queryInterface(IID_EndToEndCompileRequest, (void**)&endToEndRequest);
    SLANG_ASSERT(endToEndRequest);
    return endToEndRequest;
}

}

#endif
