// slang-lexer.cpp
#include "slang-lexer.h"

// This file implements the lexer/scanner, which is responsible for taking a raw stream of
// input bytes and turning it into semantically useful tokens.
//

#include "slang-compiler.h"
#include "slang-source-loc.h"

#include <assert.h>

namespace Slang
{
    Token TokenReader::getEndOfFileToken()
    {
        return Token(TokenType::EndOfFile, UnownedStringSlice::fromLiteral(""), SourceLoc());
    }

    const Token* TokenList::begin() const
    {
        SLANG_ASSERT(m_tokens.getCount());
        return &m_tokens[0];
    }

    const Token* TokenList::end() const
    {
        SLANG_ASSERT(m_tokens.getCount());
        SLANG_ASSERT(m_tokens[m_tokens.getCount() - 1].type == TokenType::EndOfFile);
        return &m_tokens[m_tokens.getCount() - 1];
    }

    TokenSpan::TokenSpan()
        : m_begin(nullptr)
        , m_end  (nullptr)
    {}

    TokenReader::TokenReader()
        : m_cursor(nullptr)
        , m_end   (nullptr)
    {}


    Token& TokenReader::peekToken()
    {
        return m_nextToken;
    }

    TokenType TokenReader::peekTokenType() const
    {
        return m_nextToken.type;
    }

    SourceLoc TokenReader::peekLoc() const
    {
        return m_nextToken.loc;
    }

    Token TokenReader::advanceToken()
    {
        if (!m_cursor)
            return getEndOfFileToken();

        Token token = m_nextToken;
        if (m_cursor < m_end)
        {
            m_cursor++;
            m_nextToken = *m_cursor;
        }
        else
            m_nextToken.type = TokenType::EndOfFile;
        return token;
    }

    // Lexer

    void Lexer::initialize(
        SourceView*     inSourceView,
        DiagnosticSink* inSink,
        NamePool*       inNamePool,
        MemoryArena*    inMemoryArena)
    {
        m_sourceView  = inSourceView;
        m_sink        = inSink;
        m_namePool    = inNamePool;
        m_memoryArena = inMemoryArena;

        auto content = inSourceView->getContent();
        
        m_begin   = content.begin();
        m_cursor  = content.begin();
        m_end     = content.end();

        // Set the start location
        m_startLoc = inSourceView->getRange().begin;

        m_tokenFlags = TokenFlag::AtStartOfLine | TokenFlag::AfterWhitespace;
        m_lexerFlags = 0;
    }

    Lexer::~Lexer()
    {
    }

    enum { kEOF = -1 };

    // Get the next input byte, without any handling of
    // escaped newlines, non-ASCII code points, source locations, etc.
    static int _peekRaw(Lexer* lexer)
    {
        // If we are at the end of the input, return a designated end-of-file value
        if(lexer->m_cursor == lexer->m_end)
            return kEOF;

        // Otherwise, just look at the next byte
        return *lexer->m_cursor;
    }

    // Read one input byte without any special handling (similar to `peekRaw`)
    static int _advanceRaw(Lexer* lexer)
    {
        // The logic here is basically the same as for `peekRaw()`,
        // escape we advance `cursor` if we aren't at the end.

        if (lexer->m_cursor == lexer->m_end)
            return kEOF;

        return *lexer->m_cursor++;
    }

    // When the cursor is already at the first byte of an end-of-line sequence,
    // consume one or two bytes that compose the sequence.
    //
    // Basically, a newline is one of:
    //
    //  "\n"
    //  "\r"
    //  "\r\n"
    //  "\n\r"
    //
    // We always look for the longest match possible.
    //
    static void _handleNewLineInner(Lexer* lexer, int c)
    {
        SLANG_ASSERT(c == '\n' || c == '\r');

        int d = _peekRaw(lexer);
        if( (c ^ d) == ('\n' ^ '\r') )
        {
            _advanceRaw(lexer);
        }
    }

    // Look ahead one code point, dealing with complications like
    // escaped newlines.
    static int _peek(Lexer* lexer)
    {
        // Look at the next raw byte, and decide what to do
        int c = _peekRaw(lexer);

        if(c == '\\')
        {
            // We might have a backslash-escaped newline.
            // Look at the next byte (if any) to see.
            //
            // Note(tfoley): We are assuming a null-terminated input here,
            // so that we can safely look at the next byte without issue.
            int d = lexer->m_cursor[1];
            switch (d)
            {
            case '\r': case '\n':
                {
                    // The newline was escaped, so return the code point after *that*

                    int e = lexer->m_cursor[2];
                    if ((d ^ e) == ('\r' ^ '\n'))
                        return lexer->m_cursor[3];
                    return e;
                }

            default:
                break;
            }
        }
        // TODO: handle UTF-8 encoding for non-ASCII code points here

        // Default case is to just hand along the byte we read as an ASCII code point.
        return c;
    }

    // Get the next code point from the input, and advance the cursor.
    static int _advance(Lexer* lexer)
    {
        // We are going to loop, but only as a way of handling
        // escaped line endings.
        for (;;)
        {
            // If we are at the end of the input, then the task is easy.
            if (lexer->m_cursor == lexer->m_end)
                return kEOF;

            // Look at the next raw byte, and decide what to do
            int c = *lexer->m_cursor++;

            if (c == '\\')
            {
                // We might have a backslash-escaped newline.
                // Look at the next byte (if any) to see.
                //
                // Note(tfoley): We are assuming a null-terminated input here,
                // so that we can safely look at the next byte without issue.
                int d = *lexer->m_cursor;
                switch (d)
                {
                case '\r': case '\n':
                    // handle the end-of-line for our source location tracking
                    lexer->m_cursor++;
                    _handleNewLineInner(lexer, d);

                    lexer->m_tokenFlags |= TokenFlag::ScrubbingNeeded;

                    // Now try again, looking at the character after the
                    // escaped newline.
                    continue;

                default:
                    break;
                }
            }

            // TODO: Need to handle non-ASCII code points.

            // Default case is to return the raw byte we saw.
            return c;
        }
    }

    static void _handleNewLine(Lexer* lexer)
    {
        int c = _advance(lexer);
        _handleNewLineInner(lexer, c);
    }

    static void _lexLineComment(Lexer* lexer)
    {
        for(;;)
        {
            switch(_peek(lexer))
            {
            case '\n': case '\r': case kEOF:
                return;

            default:
                _advance(lexer);
                continue;
            }
        }
    }

    static void _lexBlockComment(Lexer* lexer)
    {
        for(;;)
        {
            switch(_peek(lexer))
            {
            case kEOF:
                // TODO(tfoley) diagnostic!
                return;

            case '\n': case '\r':
                _handleNewLine(lexer);
                continue;

            case '*':
                _advance(lexer);
                switch( _peek(lexer) )
                {
                case '/':
                    _advance(lexer);
                    return;

                default:
                    continue;
                }

            default:
                _advance(lexer);
                continue;
            }
        }
    }

    static void _lexHorizontalSpace(Lexer* lexer)
    {
        for(;;)
        {
            switch(_peek(lexer))
            {
            case ' ': case '\t':
                _advance(lexer);
                continue;

            default:
                return;
            }
        }
    }

    static void _lexIdentifier(Lexer* lexer)
    {
        for(;;)
        {
            int c = _peek(lexer);
            if(('a' <= c ) && (c <= 'z')
                || ('A' <= c) && (c <= 'Z')
                || ('0' <= c) && (c <= '9')
                || (c == '_'))
            {
                _advance(lexer);
                continue;
            }

            return;
        }
    }

    static SourceLoc _getSourceLoc(Lexer* lexer)
    {
        return lexer->m_startLoc + (lexer->m_cursor - lexer->m_begin);
    }

    static void _lexDigits(Lexer* lexer, int base)
    {
        for(;;)
        {
            int c = _peek(lexer);

            int digitVal = 0;
            switch(c)
            {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                digitVal = c - '0';
                break;

            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                if(base <= 10) return;
                digitVal = 10 + c - 'a';
                break;

            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                if(base <= 10) return;
                digitVal = 10 + c - 'A';
                break;

            default:
                // Not more digits!
                return;
            }

            if(digitVal >= base)
            {
                char buffer[] = { (char) c, 0 };
                lexer->m_sink->diagnose(_getSourceLoc(lexer), Diagnostics::invalidDigitForBase, buffer, base);
            }

            _advance(lexer);
        }
    }

    static TokenType _maybeLexNumberSuffix(Lexer* lexer, TokenType tokenType)
    {
        // Be liberal in what we accept here, so that figuring out
        // the semantics of a numeric suffix is left up to the parser
        // and semantic checking logic.
        //
        for( ;;)
        {
            int c = _peek(lexer);

            // Accept any alphanumeric character, plus underscores.
            if(('a' <= c ) && (c <= 'z')
                || ('A' <= c) && (c <= 'Z')
                || ('0' <= c) && (c <= '9')
                || (c == '_'))
            {
                _advance(lexer);
                continue;
            }

            // Stop at the first character that isn't
            // alphanumeric.
            return tokenType;
        }
    }

    static bool _isNumberExponent(int c, int base)
    {
        switch( c )
        {
        default:
            return false;

        case 'e': case 'E':
            if(base != 10) return false;
            break;

        case 'p': case 'P':
            if(base != 16) return false;
            break;
        }

        return true;
    }

    static bool _maybeLexNumberExponent(Lexer* lexer, int base)
    {
        if(!_isNumberExponent(_peek(lexer), base))
            return false;

        // we saw an exponent marker
        _advance(lexer);

        // Now start to read the exponent
        switch( _peek(lexer) )
        {
        case '+': case '-':
            _advance(lexer);
            break;
        }

        // TODO(tfoley): it would be an error to not see digits here...

        _lexDigits(lexer, 10);

        return true;
    }

    static TokenType _lexNumberAfterDecimalPoint(Lexer* lexer, int base)
    {
        _lexDigits(lexer, base);
        _maybeLexNumberExponent(lexer, base);

        return _maybeLexNumberSuffix(lexer, TokenType::FloatingPointLiteral);
    }

    static TokenType _lexNumber(Lexer* lexer, int base)
    {
        // TODO(tfoley): Need to consider whehter to allow any kind of digit separator character.

        TokenType tokenType = TokenType::IntegerLiteral;

        // At the start of things, we just concern ourselves with digits
        _lexDigits(lexer, base);

        if( _peek(lexer) == '.' )
        {
            tokenType = TokenType::FloatingPointLiteral;

            _advance(lexer);
            _lexDigits(lexer, base);
        }

        if( _maybeLexNumberExponent(lexer, base))
        {
            tokenType = TokenType::FloatingPointLiteral;
        }

        _maybeLexNumberSuffix(lexer, tokenType);
        return tokenType;
    }

    static int _maybeReadDigit(char const** ioCursor, int base)
    {
        auto& cursor = *ioCursor;

        for(;;)
        {
            int c = *cursor;
            switch(c)
            {
            default:
                return -1;

            // TODO: need to decide on digit separator characters
            case '_':
                cursor++;
                continue;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                cursor++;
                return c - '0';

            case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
                if(base > 10)
                {
                    cursor++;
                    return 10 + c - 'a';
                }
                return -1;

            case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
                if(base > 10)
                {
                    cursor++;
                    return 10 + c - 'A';
                }
                return -1;
            }
        }
    }

    static int _readOptionalBase(char const** ioCursor)
    {
        auto& cursor = *ioCursor;
        if( *cursor == '0' )
        {
            cursor++;
            switch(*cursor)
            {
            case 'x': case 'X':
                cursor++;
                return 16;

            case 'b': case 'B':
                cursor++;
                return 2;

            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                return 8;

            default:
                return 10;
            }
        }

        return 10;
    }



    IntegerLiteralValue getIntegerLiteralValue(Token const& token, UnownedStringSlice* outSuffix)
    {
        IntegerLiteralValue value = 0;

        char const* cursor = token.Content.begin();
        char const* end = token.Content.end();

        int base = _readOptionalBase(&cursor);

        for( ;;)
        {
            int digit = _maybeReadDigit(&cursor, base);
            if(digit < 0)
                break;

            value = value*base + digit;
        }

        if(outSuffix)
        {
            *outSuffix = UnownedStringSlice(cursor, end);
        }

        return value;
    }

#if 0
    static FloatingPointLiteralValue _getFloatingPointLiteralValue(Token const& token, UnownedStringSlice* outSuffix)
    {
        FloatingPointLiteralValue value = 0;

        char const* cursor = token.Content.begin();
        char const* end = token.Content.end();

        int radix = _readOptionalBase(&cursor);

        bool seenDot = false;
        FloatingPointLiteralValue divisor = 1;
        for (;;)
        {
            if (*cursor == '.')
            {
                cursor++;
                seenDot = true;
                continue;
            }

            int digit = _maybeReadDigit(&cursor, radix);
            if (digit < 0)
                break;

            value = value * radix + digit;

            if (seenDot)
            {
                divisor *= radix;
            }
        }

        // Now read optional exponent
        if (_isNumberExponent(*cursor, radix))
        {
            cursor++;

            bool exponentIsNegative = false;
            switch (*cursor)
            {
                default:
                    break;

                case '-':
                    exponentIsNegative = true;
                    cursor++;
                    break;

                case '+':
                    cursor++;
                    break;
            }

            int exponentRadix = 10;
            int exponent = 0;

            for (;;)
            {
                int digit = _maybeReadDigit(&cursor, exponentRadix);
                if (digit < 0)
                    break;

                exponent = exponent * exponentRadix + digit;
            }

            FloatingPointLiteralValue exponentBase = 10;
            if (radix == 16)
            {
                exponentBase = 2;
            }

            FloatingPointLiteralValue exponentValue = pow(exponentBase, exponent);

            if (exponentIsNegative)
            {
                divisor *= exponentValue;
            }
            else
            {
                value *= exponentValue;
            }
        }

        value /= divisor;

        if (outSuffix)
        {
            *outSuffix = UnownedStringSlice(cursor, end);
        }

        return value;
    }
#endif

    static const FloatingPointLiteralValue s_pow10[]
    {
        pow(10.0, -32),
        pow(10.0, -31),
        pow(10.0, -30),
        pow(10.0, -29),

        pow(10.0, -28),
        pow(10.0, -27),
        pow(10.0, -26),
        pow(10.0, -25),

        pow(10.0, -24),
        pow(10.0, -23),
        pow(10.0, -22),
        pow(10.0, -21),

        pow(10.0, -20),
        pow(10.0, -19),
        pow(10.0, -18),
        pow(10.0, -17),

        pow(10.0, -16),
        pow(10.0, -15),
        pow(10.0, -14),
        pow(10.0, -13),

        pow(10.0, -12),
        pow(10.0, -11),
        pow(10.0, -10),
        pow(10.0, -9),

        pow(10.0, -8),
        pow(10.0, -7),
        pow(10.0, -6),
        pow(10.0, -5),

        pow(10.0, -4),
        pow(10.0, -3),
        pow(10.0, -2),
        pow(10.0, -1),

        pow(10.0, 0),
        pow(10.0, 1),
        pow(10.0, 2),
        pow(10.0, 3),

        pow(10.0, 4),
        pow(10.0, 5),
        pow(10.0, 6),
        pow(10.0, 7),

        pow(10.0, 8),
        pow(10.0, 9),
        pow(10.0, 10),
        pow(10.0, 11),

        pow(10.0, 12),
        pow(10.0, 13),
        pow(10.0, 14),
        pow(10.0, 15),

        pow(10.0, 16),
        pow(10.0, 17),
        pow(10.0, 18),
        pow(10.0, 19),

        pow(10.0, 20),
        pow(10.0, 21),
        pow(10.0, 22),
        pow(10.0, 23),

        pow(10.0, 24),
        pow(10.0, 25),
        pow(10.0, 26),
        pow(10.0, 27),

        pow(10.0, 28),
        pow(10.0, 29),
        pow(10.0, 30),
        pow(10.0, 31),

        pow(10.0, 32),
    };

    static FloatingPointLiteralValue _pow10(int value)
    {
        if (value >= -32 && value <= 32)
        {
            return s_pow10[value + 32];
        }
        else
        {
            return pow(10.0, value);
        }
    }


    static FloatingPointLiteralValue _getFloatingPointLiteralValueWithRadix(const UnownedStringSlice& in , int radix, UnownedStringSlice* outSuffix)
    {
        FloatingPointLiteralValue value = 0;

        char const* cursor = in.begin();
        char const* end = in.end();

        bool seenDot = false;
        FloatingPointLiteralValue divisor = 1;
        for (;;)
        {
            if (*cursor == '.')
            {
                cursor++;
                seenDot = true;
                continue;
            }

            int digit = _maybeReadDigit(&cursor, radix);
            if (digit < 0)
                break;

            value = value * radix + digit;

            if (seenDot)
            {
                divisor *= radix;
            }
        }

        // Now read optional exponent
        if (_isNumberExponent(*cursor, radix))
        {
            cursor++;

            bool exponentIsNegative = false;
            switch (*cursor)
            {
                default:
                    break;

                case '-':
                    exponentIsNegative = true;
                    cursor++;
                    break;

                case '+':
                    cursor++;
                    break;
            }

            int exponentRadix = 10;
            int exponent = 0;

            for (;;)
            {
                int digit = _maybeReadDigit(&cursor, exponentRadix);
                if (digit < 0)
                    break;

                exponent = exponent * exponentRadix + digit;
            }

            FloatingPointLiteralValue exponentBase = 10;
            if (radix == 16)
            {
                exponentBase = 2;
            }

            FloatingPointLiteralValue exponentValue = pow(exponentBase, exponent);

            if (exponentIsNegative)
            {
                divisor *= exponentValue;
            }
            else
            {
                value *= exponentValue;
            }
        }

        value /= divisor;

        if (outSuffix)
        {
            *outSuffix = UnownedStringSlice(cursor, end);
        }

        return value;
    }

    struct SplitFloat64
    {
        int64_t value;          ///< Value (without exponent)
        int exponent;           ///< Raised to this exponent. What it is raised to depends on base of what was parsed..
    };

    static const char* _consumeDigits(const char* cursor)
    {
        while ((*cursor >= '0' && *cursor <= '9') || *cursor == '_')
        {
            cursor++;
        }
        return cursor;
    }

    static SplitFloat64 _parseSplitFloat64(const char** ioCursor)
    {
        const char* cursor = *ioCursor;
        // Most radix 10 digits can be held in a int64_t
        const int maxDigits = 18;

        int64_t value = 0;
        int exponent = 0;

        int remainingDigits = maxDigits;
        // Lets assume we can fit the digits before into int64_t
        // We using int64_t, because uint64_t conversion can be slow on some targets
        while (remainingDigits > 0)
        {
            const char c = *cursor;
            if (c == '_')
            {
                cursor++;
            }
            else if (c >= '0' && c <= '9')
            {
                value = value * 10 + (c - '0');
                cursor++;
                remainingDigits--;
            }
            else
            {
                break;
            }
        }

        // If there are no remaining digits
        if (remainingDigits == 0)
        {
            // We now just need to find the dot or the end
            while (true)
            {
                const char c = *cursor;
                if (c == '_')
                {
                    cursor++;
                    continue;
                }
                else if (c >= '0' && c <= '9')
                {
                    cursor++;
                    exponent++;
                    continue;
                }
                else if (c == '.')
                {
                    // Skip the ., and consume any digits following
                    cursor = _consumeDigits(cursor + 1);
                }
                break;
            }
        }
        else if (*cursor == '.')
        {
            // Save the value, before we do digits after .
            const auto copyValue = value;

            // Skip the .
            cursor++;
            // We'll use this to check if all digits are 0
            int orDigits = 0;

            while (remainingDigits > 0)
            {
                const char c = *cursor;
                if (c == '_')
                {
                    cursor++;
                }
                else if (c >= '0' && c <= '9')
                {
                    const int digit = (c - '0');
                    orDigits |= digit;
                    // 
                    value = value * 10 + digit;
                    cursor++;
                    exponent--;
                    remainingDigits--;
                }
                else
                {
                    break;
                }
            }
            // Consume any following digits
            cursor = _consumeDigits(cursor);

            // If the digits are all 0, then we can just throw away the exponent, and use the old mantissa
            if (orDigits == 0)
            {
                exponent = 0;
                value = copyValue;
            }
        }


        // We are done
        *ioCursor = cursor;
        return SplitFloat64{ value, exponent };
    }

    // NOTE! Does not handle pathological cases like 0x80000000, or ints with more than 9 digits
    // Thats ok here, because we are using just for the exponent, so it would be out of range
    // To handle those cases, it will return +/-0x3fffffff. This is still well out of range and will mean
    // no overflow when added with any exponent that came from 'regular' part.
    static int _parseExponentInt(const char** ioCursor)
    {
        const char* cursor = *ioCursor;
        bool exponentIsNegative = false;
        switch (*cursor)
        {
            default:
                break;
            case '-':
                exponentIsNegative = true;
                cursor++;
                break;

            case '+':
                cursor++;
                break;
        }

        int exponent = 0;
        for (;;)
        {
            // Maximum digits in a 32 bit int
            const int maxDigits = 9;
            int remainingDigits = maxDigits;
            while (remainingDigits > 0)
            {
                const char c = *cursor;
                if (c == '_')
                {
                    cursor++;
                }
                else if (c >= '0' && c <= '9')
                {
                    exponent = exponent * 10 + (c - '0');
                    cursor++;
                    remainingDigits--;
                }
                else
                {
                    break;
                }
            }
            // If too big/small to hold, just make large number
            exponent = (remainingDigits == 0) ? 0x3fffffff : exponent;

            // Save the cursor pos
            *ioCursor = cursor;
            return exponentIsNegative ? -exponent : exponent;
        }
    }

    static FloatingPointLiteralValue _getFloatingPointLiteralValueRadix10(const UnownedStringSlice& in, UnownedStringSlice* outSuffix)
    {
        char const* cursor = in.begin();
        //char const* end = in.end();

        SplitFloat64 splitFloat = _parseSplitFloat64(&cursor);

        // Handle exponent
        if (*cursor == 'e' || *cursor == 'E')
        {
            // Skip the e
            cursor++;
            int exponent = _parseExponentInt(&cursor);
            splitFloat.exponent += exponent;
        }

        // Okay turn the splitFloat into a floating point value
        FloatingPointLiteralValue value = FloatingPointLiteralValue(splitFloat.value);
        if (splitFloat.exponent)
        {
            value *= _pow10(splitFloat.exponent);
        }

        if (outSuffix)
        {
            *outSuffix = UnownedStringSlice(cursor, in.end());
        }

        return value;
    }

    FloatingPointLiteralValue getFloatingPointLiteralValue(Token const& token, UnownedStringSlice* outSuffix)
    {
        char const* cursor = token.Content.begin();
        char const* end = token.Content.end();
        int radix = _readOptionalBase(&cursor);
        UnownedStringSlice remaining(cursor, end);
        switch (radix)
        {
            case 10:
            {
                return _getFloatingPointLiteralValueRadix10(remaining, outSuffix);
            }
            default:
            {
                return _getFloatingPointLiteralValueWithRadix(remaining, radix, outSuffix);
            }
        }
    }

    static void _lexStringLiteralBody(Lexer* lexer, char quote)
    {
        for(;;)
        {
            int c = _peek(lexer);
            if(c == quote)
            {
                _advance(lexer);
                return;
            }

            switch(c)
            {
            case kEOF:
                lexer->m_sink->diagnose(_getSourceLoc(lexer), Diagnostics::endOfFileInLiteral);
                return;

            case '\n': case '\r':
                lexer->m_sink->diagnose(_getSourceLoc(lexer), Diagnostics::newlineInLiteral);
                return;

            case '\\':
                // Need to handle various escape sequence cases
                _advance(lexer);
                switch(_peek(lexer))
                {
                case '\'':
                case '\"':
                case '\\':
                case '?':
                case 'a':
                case 'b':
                case 'f':
                case 'n':
                case 'r':
                case 't':
                case 'v':
                    _advance(lexer);
                    break;

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7':
                    // octal escape: up to 3 characters
                    _advance(lexer);
                    for(int ii = 0; ii < 3; ++ii)
                    {
                        int d = _peek(lexer);
                        if(('0' <= d) && (d <= '7'))
                        {
                            _advance(lexer);
                            continue;
                        }
                        else
                        {
                            break;
                        }
                    }
                    break;

                case 'x':
                    // hexadecimal escape: any number of characters
                    _advance(lexer);
                    for(;;)
                    {
                        int d = _peek(lexer);
                        if(('0' <= d) && (d <= '9')
                            || ('a' <= d) && (d <= 'f')
                            || ('A' <= d) && (d <= 'F'))
                        {
                            _advance(lexer);
                            continue;
                        }
                        else
                        {
                            break;
                        }
                    }
                    break;

                // TODO: Unicode escape sequences

                }
                break;

            default:
                _advance(lexer);
                continue;
            }
        }
    }

    String getStringLiteralTokenValue(Token const& token)
    {
        SLANG_ASSERT(token.type == TokenType::StringLiteral
            || token.type == TokenType::CharLiteral);

        char const* cursor = token.Content.begin();
        char const* end = token.Content.end();
        SLANG_UNREFERENCED_VARIABLE(end);

        auto quote = *cursor++;
        SLANG_ASSERT(quote == '\'' || quote == '"');

        StringBuilder valueBuilder;
        for(;;)
        {
            SLANG_ASSERT(cursor != end);

            auto c = *cursor++;

            // If we see a closing quote, then we are at the end of the string literal
            if(c == quote)
            {
                SLANG_ASSERT(cursor == end);
                return valueBuilder.ProduceString();
            }

            // Characters that don't being escape sequences are easy;
            // just append them to the buffer and move on.
            if(c != '\\')
            {
                valueBuilder.Append(c);
                continue;
            }

            // Now we look at another character to figure out the kind of
            // escape sequence we are dealing with:

            char d = *cursor++;

            switch(d)
            {
            // Simple characters that just needed to be escaped
            case '\'':
            case '\"':
            case '\\':
            case '?':
                valueBuilder.Append(d);
                continue;

            // Traditional escape sequences for special characters
            case 'a': valueBuilder.Append('\a'); continue;
            case 'b': valueBuilder.Append('\b'); continue;
            case 'f': valueBuilder.Append('\f'); continue;
            case 'n': valueBuilder.Append('\n'); continue;
            case 'r': valueBuilder.Append('\r'); continue;
            case 't': valueBuilder.Append('\t'); continue;
            case 'v': valueBuilder.Append('\v'); continue;

            // Octal escape: up to 3 characterws
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7':
                {
                    cursor--;
                    int value = 0;
                    for(int ii = 0; ii < 3; ++ii)
                    {
                        d = *cursor;
                        if(('0' <= d) && (d <= '7'))
                        {
                            value = value*8 + (d - '0');

                            cursor++;
                            continue;
                        }
                        else
                        {
                            break;
                        }
                    }

                    // TODO: add support for appending an arbitrary code point?
                    valueBuilder.Append((char) value);
                }
                continue;

            // Hexadecimal escape: any number of characters
            case 'x':
                {
                    cursor--;
                    int value = 0;
                    for(;;)
                    {
                        d = *cursor++;
                        int digitValue = 0;
                        if(('0' <= d) && (d <= '9'))
                        {
                            digitValue = d - '0';
                        }
                        else if( ('a' <= d) && (d <= 'f') )
                        {
                            digitValue = d - 'a';
                        }
                        else if( ('A' <= d) && (d <= 'F') )
                        {
                            digitValue = d - 'A';
                        }
                        else
                        {
                            cursor--;
                            break;
                        }

                        value = value*16 + digitValue;
                    }

                    // TODO: add support for appending an arbitrary code point?
                    valueBuilder.Append((char) value);
                }
                continue;

            // TODO: Unicode escape sequences

            }
        }
    }

    String getFileNameTokenValue(Token const& token)
    {
        // A file name usually doesn't process escape sequences
        // (this is import on Windows, where `\\` is a valid
        // path separator character).

        // Just trim off the first and last characters to remove the quotes
        // (whether they were `""` or `<>`.
        return String(token.Content.begin() + 1, token.Content.end() - 1); 
    }



    static TokenType _lexTokenImpl(Lexer* lexer, LexerFlags effectiveFlags)
    {
        if(effectiveFlags & kLexerFlag_ExpectDirectiveMessage)
        {
            for(;;)
            {
                switch(_peek(lexer))
                {
                default:
                    _advance(lexer);
                    continue;

                case kEOF: case '\r': case '\n':
                    break;
                }
                break;
            }
            return TokenType::DirectiveMessage;
        }

        switch(_peek(lexer))
        {
        default:
            break;

        case kEOF:
            if((effectiveFlags & kLexerFlag_InDirective) != 0)
                return TokenType::EndOfDirective;
            return TokenType::EndOfFile;

        case '\r': case '\n':
            if((effectiveFlags & kLexerFlag_InDirective) != 0)
                return TokenType::EndOfDirective;
            _handleNewLine(lexer);
            return TokenType::NewLine;

        case ' ': case '\t':
            _lexHorizontalSpace(lexer);
            return TokenType::WhiteSpace;

        case '.':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                return _lexNumberAfterDecimalPoint(lexer, 10);

            // TODO(tfoley): handle ellipsis (`...`)

            default:
                return TokenType::Dot;
            }

                    case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return _lexNumber(lexer, 10);

        case '0':
            {
                auto loc = _getSourceLoc(lexer);
                _advance(lexer);
                switch(_peek(lexer))
                {
                default:
                    return _maybeLexNumberSuffix(lexer, TokenType::IntegerLiteral);

                case '.':
                    _advance(lexer);
                    return _lexNumberAfterDecimalPoint(lexer, 10);

                case 'x': case 'X':
                    _advance(lexer);
                    return _lexNumber(lexer, 16);

                case 'b': case 'B':
                    _advance(lexer);
                    return _lexNumber(lexer, 2);

                case '0': case '1': case '2': case '3': case '4':
                case '5': case '6': case '7': case '8': case '9':
                    lexer->m_sink->diagnose(loc, Diagnostics::octalLiteral);
                    return _lexNumber(lexer, 8);
                }
            }

        case 'a': case 'b': case 'c': case 'd': case 'e':
        case 'f': case 'g': case 'h': case 'i': case 'j':
        case 'k': case 'l': case 'm': case 'n': case 'o':
        case 'p': case 'q': case 'r': case 's': case 't':
        case 'u': case 'v': case 'w': case 'x': case 'y':
        case 'z':
        case 'A': case 'B': case 'C': case 'D': case 'E':
        case 'F': case 'G': case 'H': case 'I': case 'J':
        case 'K': case 'L': case 'M': case 'N': case 'O':
        case 'P': case 'Q': case 'R': case 'S': case 'T':
        case 'U': case 'V': case 'W': case 'X': case 'Y':
        case 'Z':
        case '_':
            _lexIdentifier(lexer);
            return TokenType::Identifier;

        case '\"':
            _advance(lexer);
            _lexStringLiteralBody(lexer, '\"');
            return TokenType::StringLiteral;

        case '\'':
            _advance(lexer);
            _lexStringLiteralBody(lexer, '\'');
            return TokenType::CharLiteral;

        case '+':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '+': _advance(lexer); return TokenType::OpInc;
            case '=': _advance(lexer); return TokenType::OpAddAssign;
            default:
                return TokenType::OpAdd;
            }

        case '-':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '-': _advance(lexer); return TokenType::OpDec;
            case '=': _advance(lexer); return TokenType::OpSubAssign;
            case '>': _advance(lexer); return TokenType::RightArrow;
            default:
                return TokenType::OpSub;
            }

        case '*':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '=': _advance(lexer); return TokenType::OpMulAssign;
            default:
                return TokenType::OpMul;
            }

        case '/':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '=': _advance(lexer); return TokenType::OpDivAssign;
            case '/': _advance(lexer); _lexLineComment(lexer); return TokenType::LineComment;
            case '*': _advance(lexer); _lexBlockComment(lexer); return TokenType::BlockComment;
            default:
                return TokenType::OpDiv;
            }

        case '%':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '=': _advance(lexer); return TokenType::OpModAssign;
            default:
                return TokenType::OpMod;
            }

        case '|':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '|': _advance(lexer); return TokenType::OpOr;
            case '=': _advance(lexer); return TokenType::OpOrAssign;
            default:
                return TokenType::OpBitOr;
            }

        case '&':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '&': _advance(lexer); return TokenType::OpAnd;
            case '=': _advance(lexer); return TokenType::OpAndAssign;
            default:
                return TokenType::OpBitAnd;
            }

        case '^':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '=': _advance(lexer); return TokenType::OpXorAssign;
            default:
                return TokenType::OpBitXor;
            }

        case '>':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '>':
                _advance(lexer);
                switch(_peek(lexer))
                {
                case '=': _advance(lexer); return TokenType::OpShrAssign;
                default: return TokenType::OpRsh;
                }
            case '=': _advance(lexer); return TokenType::OpGeq;
            default:
                return TokenType::OpGreater;
            }

        case '<':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '<':
                _advance(lexer);
                switch(_peek(lexer))
                {
                case '=': _advance(lexer); return TokenType::OpShlAssign;
                default: return TokenType::OpLsh;
                }
            case '=': _advance(lexer); return TokenType::OpLeq;
            default:
                return TokenType::OpLess;
            }

        case '=':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '=': _advance(lexer); return TokenType::OpEql;
            default:
                return TokenType::OpAssign;
            }

        case '!':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '=': _advance(lexer); return TokenType::OpNeq;
            default:
                return TokenType::OpNot;
            }

        case '#':
            _advance(lexer);
            switch(_peek(lexer))
            {
            case '#': _advance(lexer); return TokenType::PoundPound;
            default:
                return TokenType::Pound;
            }

        case '~': _advance(lexer); return TokenType::OpBitNot;

        case ':': 
        {
            _advance(lexer);
            if (_peek(lexer) == ':')
            {
                _advance(lexer);
                return TokenType::Scope;
            }
            return TokenType::Colon;
        }
        case ';': _advance(lexer); return TokenType::Semicolon;
        case ',': _advance(lexer); return TokenType::Comma;

        case '{': _advance(lexer); return TokenType::LBrace;
        case '}': _advance(lexer); return TokenType::RBrace;
        case '[': _advance(lexer); return TokenType::LBracket;
        case ']': _advance(lexer); return TokenType::RBracket;
        case '(': _advance(lexer); return TokenType::LParent;
        case ')': _advance(lexer); return TokenType::RParent;

        case '?': _advance(lexer); return TokenType::QuestionMark;
        case '@': _advance(lexer); return TokenType::At;
        case '$': _advance(lexer); return TokenType::Dollar;

        }

        // TODO(tfoley): If we ever wanted to support proper Unicode
        // in identifiers, etc., then this would be the right place
        // to perform a more expensive dispatch based on the actual
        // code point (and not just the first byte).

        {
            // If none of the above cases matched, then we have an
            // unexpected/invalid character.

            auto loc = _getSourceLoc(lexer);
            int c = _advance(lexer);
            if(!(effectiveFlags & kLexerFlag_IgnoreInvalid))
            {
                auto sink = lexer->m_sink;
                if(c >= 0x20 && c <=  0x7E)
                {
                    char buffer[] = { (char) c, 0 };
                    sink->diagnose(loc, Diagnostics::illegalCharacterPrint, buffer);
                }
                else
                {
                    // Fallback: print as hexadecimal
                    sink->diagnose(loc, Diagnostics::illegalCharacterHex, String((unsigned char)c, 16));
                }
            }

            return TokenType::Invalid;
        }
    }

    Token Lexer::lexToken(LexerFlags extraFlags)
    {
        auto& flags = m_tokenFlags;
        for(;;)
        {
            Token token;
            token.loc = _getSourceLoc(this);

            char const* textBegin = m_cursor;

            auto tokenType = _lexTokenImpl(this, m_lexerFlags | extraFlags);

            // The low-level lexer produces tokens for things we want
            // to ignore, such as white space, so we skip them here.
            switch(tokenType)
            {
            case TokenType::Invalid:
                flags = 0;
                continue;

            case TokenType::NewLine:
                flags = TokenFlag::AtStartOfLine | TokenFlag::AfterWhitespace;
                continue;

            case TokenType::WhiteSpace:
            case TokenType::LineComment:
            case TokenType::BlockComment:
                flags |= TokenFlag::AfterWhitespace;
                continue;

            // We don't want to skip the end-of-file token, but we *do*
            // want to make sure it has appropriate flags to make our life easier
            case TokenType::EndOfFile:
                flags |= TokenFlag::AtStartOfLine | TokenFlag::AfterWhitespace;
                break;

            // We will also do some book-keeping around preprocessor directives here:
            //
            // If we see a `#` at the start of a line, then we are entering a
            // preprocessor directive.
            case TokenType::Pound:
                if((flags & TokenFlag::AtStartOfLine) != 0)
                    m_lexerFlags |= kLexerFlag_InDirective;
                break;
            //
            // And if we saw an end-of-line during a directive, then we are
            // now leaving that directive.
            //
            case TokenType::EndOfDirective:
                m_lexerFlags &= ~kLexerFlag_InDirective;
                break;

            default:
                break;
            }

            token.type =  tokenType;

            char const* textEnd = m_cursor;

            // Note(tfoley): `StringBuilder::Append()` seems to crash when appending zero bytes
            if(textEnd != textBegin)
            {
                // "scrubbing" token value here to remove escaped newlines...
                //
                // Only perform this work if we encountered an escaped newline
                // while lexing this token (e.g., keep a flag on the lexer), or
                // do it on-demand when the actual value of the token is needed.
                if (m_tokenFlags & TokenFlag::ScrubbingNeeded)
                {
                    // Allocate space that will always be more than enough for stripped contents
                    char* startDst = (char*)m_memoryArena->allocateUnaligned(textEnd - textBegin);
                    char* dst = startDst;

                    auto tt = textBegin;
                    while (tt != textEnd)
                    {
                        char c = *tt++;
                        if (c == '\\')
                        {
                            char d = *tt;
                            switch (d)
                            {
                            case '\r': case '\n':
                            {
                                tt++;
                                char e = *tt;
                                if ((d ^ e) == ('\r' ^ '\n'))
                                {
                                    tt++;
                                }
                            }
                            continue;

                            default:
                                break;
                            }
                        }
                        *dst++ = c;
                    }
                    token.Content = UnownedStringSlice(startDst, dst);
                }
                else
                {
                    token.Content = UnownedStringSlice(textBegin, textEnd);
                }
            }

            token.flags = flags;

            m_tokenFlags = 0;

            if (tokenType == TokenType::Identifier)
            {
                token.ptrValue = m_namePool->getName(token.Content);
            }

            return token;
        }
    }

    TokenList Lexer::lexAllTokens()
    {
        TokenList tokenList;
        for(;;)
        {
            Token token = lexToken();
            tokenList.add(token);

            if(token.type == TokenType::EndOfFile)
                return tokenList;
        }
    }
}
