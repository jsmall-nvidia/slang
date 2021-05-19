#ifndef SLANG_COMMAND_LINE_ARGS_H
#define SLANG_COMMAND_LINE_ARGS_H

// This file defines the `Name` type, used to represent
// the name of types, variables, etc. in the AST.

#include "../core/slang-basic.h"

#include "slang-source-loc.h"
#include "slang-diagnostic-sink.h"

namespace Slang {

struct CommandLineArg
{
    String value;           ///< The value of the arg
    SourceLoc loc;          ///< The location of the arg
};

class CommandLineContext : public RefObject
{
public:        
        /// Get the source manager
    SourceManager* getSourceManager() { return &m_sourceManager; }

    CommandLineContext(ISlangFileSystemExt* fileSystemExt = nullptr)
    {
        m_sourceManager.initialize(nullptr, fileSystemExt);
        // Make range start from high value, so can be differentiated from other uses
        m_sourceManager.allocateSourceRange(~(~SourceLoc::RawValue(0) >> 1));
    }

protected:
    SourceManager m_sourceManager;
};

struct CommandLineArgs
{
    typedef CommandLineArg Arg;

    SLANG_FORCE_INLINE Index getArgCount() const { return m_args.getCount(); }
    const Arg& operator[](Index i) const { return m_args[i]; }

    const Arg* begin() const { return m_args.begin(); }
    const Arg* end() const { return m_args.end(); }

        /// NOTE! Should NOT include the executable name
    void setArgs(const char*const* args, size_t argCount);

        /// True if has args in same order
    bool hasArgs(const char*const* args, Index count) const;

        /// Add an arg
    void add(const Arg& arg) { m_args.add(arg); }

        /// Ctor with a context
    CommandLineArgs(CommandLineContext* context):
        m_context(context)
    {
    }
        /// Default Ctor
    CommandLineArgs() {}

    //String m_executablePath;                ///< Can be optionally be set
    List<Arg> m_args;                       ///< The args
    RefPtr<CommandLineContext> m_context;   ///< The context, which mainly has source manager
};

struct CommandLineReader
{
        /// Peek the current location
    SourceLoc peekLoc() const { return m_index < m_args->getArgCount() ? (*m_args)[m_index].loc : SourceLoc(); }
        /// Peek the current arg
    const CommandLineArg& peekArg() const { SLANG_ASSERT(hasArg()); return (*m_args)[m_index]; }

        /// Peek the string value at that position
    const String& peekValue() const { SLANG_ASSERT(hasArg()); return (*m_args)[m_index].value; }

        /// Get the arg and advance
    CommandLineArg getArgAndAdvance() { CommandLineArg arg(peekArg()); advance(); return arg; }

    const String& getValueAndAdvance() { const String& value = peekValue(); advance(); return value; }

        /// True if at end
    bool atEnd() const { return m_index >= m_args->getArgCount(); }
        /// True if has a current arg
    bool hasArg() const { return !atEnd(); }

        /// Advance to next arg
    void advance() { SLANG_ASSERT(m_index < m_args->getArgCount()); m_index++; }
        /// Removes arg at current position
    void removeArg() { SLANG_ASSERT(hasArg()); m_args->m_args.removeAt(m_index); }

        /// Get the value from the arg previous to the current position. Will assert if there isn't one.
    String getPreviousValue() const; 

        /// If there is an arg outArg is set and advanced
        /// Note, this *assumes* the previous arg is the option that initated this
    SlangResult expectArg(String& outArg);
    SlangResult expectArg(CommandLineArg& outArg);

        /// Get the current index
    Index getIndex() const { return m_index; }
        /// Set the current index
    void setIndex(Index index) { SLANG_ASSERT(index >= 0 && index <= m_args->getArgCount()); m_index = index; }

        /// Set up reader with args
    CommandLineReader(CommandLineArgs* args, DiagnosticSink* sink):
        m_args(args),
        m_index(0),
        m_sink(sink)
    {
    }

    DiagnosticSink* m_sink;
    CommandLineArgs* m_args;
    Index m_index;
};

struct DownstreamArgs
{
    typedef uint32_t Flags;
    struct Flag
    {
        enum Enum : Flags
        {
            AllowNewNames = 0x01,
        };
    };

        /// Add a name, returns the index
    Index addName(const String& name);
        /// Find the index of a name. Returns < 0 if not found.
    Index findName(const String& name) const { return m_names.indexOf(name); }

        /// Get the args at the nameIndex
    CommandLineArgs& getArgsAt(Index nameIndex) { return m_args[nameIndex]; }
        /// Get args by name - will assert if name isn't found
    CommandLineArgs& getArgsByName(char* name);

        /// Looks for '-X' expressions, removing them from ioArgs and putting in appropriate args 
    SlangResult stripDownstreamArgs(CommandLineArgs& ioArgs, Flags flags, DiagnosticSink* sink);

    DownstreamArgs(CommandLineContext* context):
        m_context(context)
    {
    }

protected:
    Index _findOrAddName(SourceLoc loc, const UnownedStringSlice& name, Flags flags, DiagnosticSink* sink);

    List<String> m_names;
    List<CommandLineArgs> m_args;

    RefPtr<CommandLineContext> m_context;
};


} // namespace Slang

#endif
