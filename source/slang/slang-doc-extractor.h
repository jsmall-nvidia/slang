// slang-doc.h
#ifndef SLANG_DOC_EXTRACTOR_H
#define SLANG_DOC_EXTRACTOR_H

#include "../core/slang-basic.h"
#include "slang-ast-all.h"

namespace Slang {

/* Holds the documentation markup that is associated with each node (typically a decl) from a module */
class DocMarkup : public RefObject
{
public:
    struct Entry
    {
        NodeBase* m_node;           ///< The node this documentation is associated with
        String m_markup;            ///< The raw contents of of markup associated with the decoration
    };

        /// Adds an entry, returns the reference to pre-existing node if there is one
    Entry& addEntry(NodeBase* base);
        /// Gets an entry for a node. Returns nullptr if there is no markup.
    Entry* getEntry(NodeBase* base);

        /// Get list of all of the entries in source order
    const List<Entry>& getEntries() const { return m_entries; }

protected:

        /// Map from AST nodes to documentation entries
    Dictionary<NodeBase*, Index> m_entryMap;
        /// All of the documentation entries in source order
    List<Entry> m_entries;
};

// ---------------------------------------------------------------------------
SLANG_INLINE DocMarkup::Entry& DocMarkup::addEntry(NodeBase* base)
{
    const Index count = m_entries.getCount();
    const Index index = m_entryMap.GetOrAddValue(base, count);

    if (index == count)
    {
        Entry entry;
        entry.m_node = base;
        m_entries.add(entry);
    }
    return m_entries[index];
}

// ---------------------------------------------------------------------------
SLANG_INLINE DocMarkup::Entry* DocMarkup::getEntry(NodeBase* base)
{
    Index* indexPtr = m_entryMap.TryGetValue(base);
    return (indexPtr) ? &m_entries[*indexPtr] : nullptr;
}

/* Extracts 'markup' from comments in Slang source core. The comments are extracted and associated in declarations. The association
is held in DocMarkup type. The comment style follows the doxygen style */
class DocMarkupExtractor
{
public:

    typedef uint32_t MarkupFlags;
    struct MarkupFlag
    {
        enum Enum : MarkupFlags
        {
            Before = 0x1,
            After = 0x2,
            IsMultiToken = 0x4,          ///< Can use more than one token
            IsBlock = 0x8,          ///< 
        };
    };

    // NOTE! Don't change order without fixing isBefore and isAfter
    enum class MarkupType
    {
        None,

        BlockBefore,        /// /**  */ or /*!  */.
        LineBangBefore,      /// //! Can be multiple lines
        LineSlashBefore,     /// /// Can be multiple lines

        BlockAfter,         /// /*!< */ or /**< */
        LineBangAfter,       /// //!< Can be multiple lines
        LineSlashAfter,      /// ///< Can be multiple lines
    };

    static bool isBefore(MarkupType type) { return Index(type) >= Index(MarkupType::BlockBefore) && Index(type) <= Index(MarkupType::LineSlashBefore); }
    static bool isAfter(MarkupType type) { return Index(type) >= Index(MarkupType::BlockAfter); }

    struct IndexRange
    {
        SLANG_FORCE_INLINE Index getCount() const { return end - start; }

        Index start;
        Index end;
    };

    enum class Location
    {
        None,                           ///< No defined location
        Before,
        AfterParam,                     ///< Can have trailing , or )
        AfterSemicolon,                 ///< Can have a trailing ;
        AfterEnumCase,                  ///< Can have a , or before }
    };

    static bool isAfter(Location location) { return Index(location) >= Index(Location::AfterParam); }
    static bool isBefore(Location location) { return location == Location::Before; }

    struct FoundMarkup
    {
        void reset()
        {
            location = Location::None;
            type = MarkupType::None;
            range = IndexRange{ 0, 0 };
        }

        Location location = Location::None;
        MarkupType type = MarkupType::None;
        IndexRange range;
    };

    struct FindInfo
    {

        SourceView* sourceView;         ///< The source view the tokens were generated from
        TokenList* tokenList;           ///< The token list
        Index declTokenIndex;           ///< The token index location (where searches start from)
        Index declLineIndex;            ///< The line number for the decl
    };

        /// Extracts documentation from the nodes held in the module using the source manager. Found documentation is placed
        /// in outMarkup
    SlangResult extract(ModuleDecl* moduleDecl, SourceManager* sourceManager, DiagnosticSink* sink, DocMarkup* outMarkup);

    static MarkupFlags getFlags(MarkupType type);
    static MarkupType findMarkupType(const Token& tok);
    static UnownedStringSlice removeStart(MarkupType type, const UnownedStringSlice& comment);

protected:
    /// returns SLANG_E_NOT_FOUND if not found, SLANG_OK on success else an error
    SlangResult _findMarkup(const FindInfo& info, Location location, FoundMarkup& out);

    /// Locations are processed in order, and the first successful used. If found in another location will issue a warning.
    /// returns SLANG_E_NOT_FOUND if not found, SLANG_OK on success else an error
    SlangResult _findFirstMarkup(const FindInfo& info, const Location* locs, Index locCount, FoundMarkup& out, Index& outIndex);

    SlangResult _findMarkup(const FindInfo& info, const Location* locs, Index locCount, FoundMarkup& out);

    /// Given the decl, the token stream, and the decls tokenIndex, try to find some associated markup
    SlangResult _findMarkup(const FindInfo& info, Decl* decl, FoundMarkup& out);

    /// Given a found markup location extracts the contents of the tokens into out
    SlangResult _extractMarkup(const FindInfo& info, const FoundMarkup& foundMarkup, StringBuilder& out);

    /// Given a location, try to find the first token index that could potentially be markup
    /// Will return -1 if not found
    Index _findStartIndex(const FindInfo& info, Location location);

    /// True if the tok is 'on' lineIndex. Interpretation of 'on' depends on the markup type.
    static bool _isTokenOnLineIndex(SourceView* sourceView, MarkupType type, const Token& tok, Index lineIndex);

    void _addDecl(Decl* decl);
    void _addDeclRec(Decl* decl);
    void _findDecls(ModuleDecl* moduleDecl);

    List<Decl*> m_decls;                ///< All the decls found from the module

    DocMarkup* m_doc;                   ///< The doc to write the documentation information found
    ModuleDecl* m_moduleDecl;
    SourceManager* m_sourceManager;
    DiagnosticSink* m_sink;
};

} // namespace Slang

#endif
