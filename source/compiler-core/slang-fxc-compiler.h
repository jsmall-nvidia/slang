#ifndef SLANG_FXC_COMPILER_UTIL_H
#define SLANG_FXC_COMPILER_UTIL_H

#include "slang-downstream-compiler.h"

#include "../core/slang-platform.h"

namespace Slang
{

struct FXCDownstreamCompilerUtil
{
    static SlangResult locateCompilers(const String& path, ISlangSharedLibraryLoader* loader, DownstreamCompilerSet* set);
};

}

#endif
