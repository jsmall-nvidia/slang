// slang-ast-dump.h
#ifndef SLANG_AST_DUMP_H
#define SLANG_AST_DUMP_H

#include "slang-syntax.h"

#include "slang-emit-source-writer.h"

namespace Slang
{

struct ASTDumpAccess;

struct ASTDumpUtil
{
    enum class Style
    {
        Hierachical,
        Flat,
    };

    typedef uint32_t Flags;
    struct Flag
    {
        enum Enum : Flags
        {
            HideSourceLoc = 0x1,
            HideScope     = 0x2,
        };
    };

    static void dump(NodeBase* node, Style style, Flags flags, SourceWriter* writer);
};

struct ASTAccessUtil
{
        /// Finds all the contained NodeBase references from within nodes fields.
        /// Only outputs fields that are specified in the ASTNodeType. To output all nodes requires iterating through the
        /// base classes. 
        /// NOTE! Does not act recursively
    static void findFieldContained(ASTNodeType astNodeType, NodeBase* node, List<NodeBase*>& out);
};

} // namespace Slang

#endif
