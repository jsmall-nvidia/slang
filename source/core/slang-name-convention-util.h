#ifndef SLANG_CORE_NAME_CONVENTION_UTIL_H
#define SLANG_CORE_NAME_CONVENTION_UTIL_H

#include "slang-string.h"
#include "slang-list.h"

namespace Slang
{

enum class NameConvention
{
    Kabab,     /// Words are separated with -. WORDS-ARE-SEPARATED
    Snake,     /// Words are separated with _. WORDS_ARE_SEPARATED
    Camel,     /// Words start with a capital. (Upper will make first words character capitalized, aka PascalCase)
};

enum class CharCase
{
    None,
    Upper,
    Lower,
};

struct NameConventionUtil
{
        /// Given a slice and a naming convention, split into it's constituent parts. 
    static void split(NameConvention convention, const UnownedStringSlice& slice, List<UnownedStringSlice>& out);

        /// Given slices, join together with the specified convention into out
    static void join(const UnownedStringSlice* slices, Index slicesCount, CharCase charCase, NameConvention convention, StringBuilder& out);

        /// Join with a join char, and potentially changing case of input slices
    static void join(const UnownedStringSlice* slices, Index slicesCount, CharCase charCase, char joinChar, StringBuilder& out);

        /// Convert from one convention to another
    static void convert(NameConvention fromConvention, const UnownedStringSlice& slice, CharCase charCase, NameConvention toConvention, StringBuilder& out);
};

}

#endif // SLANG_CORE_NAME_CONVENTION_UTIL_H
