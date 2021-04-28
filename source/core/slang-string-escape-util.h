#ifndef SLANG_CORE_STRING_ESCAPE_UTIL_H
#define SLANG_CORE_STRING_ESCAPE_UTIL_H

#include "slang-string.h"

namespace Slang {

struct StringSpaceEscapeUtil
{
        /// True if quoting is needed
    static bool isQuotingNeeded(const UnownedStringSlice& slice);

    static SlangResult appendEscaped(const UnownedStringSlice& slice, StringBuilder& out);

    static SlangResult appendUnescaped(const UnownedStringSlice& slice, StringBuilder& out);

    static SlangResult appendUnquoted(const UnownedStringSlice& slice, StringBuilder& out);

    static void appendMaybeQuoted(const UnownedStringSlice& slice, StringBuilder& out);

        /// If the slice appears to be quoted for the style, unquote it, else just append to out
    static SlangResult appendMaybeUnquoted(const UnownedStringSlice& slice, StringBuilder& out);

        /// Append with quotes (even if not needed)
    static void appendQuoted(const UnownedStringSlice& slice, StringBuilder& out);

        /// Lex quoted text.
        /// The first character of cursor should be the quoteCharacter. 
        /// cursor points to the string to be lexed - must typically be 0 terminated.
        /// outCursor on successful lex will be at the next character after was processed.
    static SlangResult lexQuoted(const char* cursor, char quoteChar, const char** outCursor);
};

/* A set of function that can be used for escaping/unescaping quoting/unquoting strings.

The distinction between 'escaping' and 'quoting' here, is just that escaping is the 'payload' of quotes. 
In *principal* the Style can determine different styles of escaping that can be used.

TODO(JS): NOTE! Currently style is largely ignored. 

Use CommandLine::kQuoteStyle for the quoting style that the command line for the current platform uses. 
*/
struct StringCppEscapeUtil
{
        /// True if quoting is needed
    static bool isQuotingNeeded(const UnownedStringSlice& slice);

        /// If slice needs quoting for the specified style, append quoted to out
    static void appendMaybeQuoted(const UnownedStringSlice& slice, StringBuilder& out);

        /// If the slice appears to be quoted for the style, unquote it, else just append to out
    static SlangResult appendMaybeUnquoted(const UnownedStringSlice& slice, StringBuilder& out);

        /// Append with quotes (even if not needed)
    static void appendQuoted(const UnownedStringSlice& slice, StringBuilder& out);

        /// Append unquoted to out. 
    static SlangResult appendUnquoted(const UnownedStringSlice& slice, StringBuilder& out);

        /// Takes slice and adds C++/C type escaping for special characters (like '\', '"' and if not ascii will write out as hex sequence)
        /// Does not append double quotes around the output
    static void appendEscaped(const UnownedStringSlice& slice, StringBuilder& out);

        /// Given a slice append it unescaped
        /// Does not consume surrounding quotes
    static SlangResult appendUnescaped(const UnownedStringSlice& slice, StringBuilder& out);

        /// Lex quoted text.
        /// The first character of cursor should be the quoteCharacter. 
        /// cursor points to the string to be lexed - must typically be 0 terminated.
        /// outCursor on successful lex will be at the next character after was processed.
    static SlangResult lexQuoted(const char* cursor, char quoteChar, const char** outCursor);
};

} // namespace Slang

#endif // SLANG_CORE_STRING_ESCAPE_UTIL_H
