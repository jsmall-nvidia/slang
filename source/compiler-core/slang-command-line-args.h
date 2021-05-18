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

struct CommandLineArgs
{
    typedef CommandLineArg Arg;

    SLANG_FORCE_INLINE Index getArgCount() const { return m_args.getCount(); }
    const Arg& operator[](Index i) const { return m_args[i]; }

    const Arg* begin() const { return m_args.begin(); }
    const Arg* end() const { return m_args.end(); }

        /// NOTE! Should NOT include the executable name
    void setArgs(const char*const* args, size_t argCount);

        /// Ctor with a source manager
    CommandLineArgs(SourceManager* manager):
        m_sourceManager(manager),
        m_sourceView(nullptr)
    {
    }

    String m_executablePath;                ///< Can be optionally be set

    List<Arg> m_args;                       ///< The args
    SourceManager* m_sourceManager;         ///< The source manager and associated diagnostics sink
    SourceView* m_sourceView;        ///< contains the command line as source
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
    bool atEnd() const { return m_index >= m_args->m_args.getCount(); }
        /// True if has a current arg
    bool hasArg() const { return !atEnd(); }

        /// Advance to next arg
    void advance() { SLANG_ASSERT(m_index < m_args->getCount()); m_index++; }
        /// Removes arg at current position
    void removeArg() { SLANG_ASSERT(hasArg()); m_args->m_args.removeAt(m_index); }

    String getPreviousValue() const; 

        /// If there is an arg outArg is set and advanced
        /// Note, this *assumes* the previous arg is the option that initated this
    SlangResult expectArg(String& outArg);
    SlangResult expectArg(CommandLineArg& outArg);

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

} // namespace Slang

#endif
