// main.cpp

#include "../../slang.h"

SLANG_API void spSetCommandLineCompilerMode(SlangCompileRequest* request);

#include "../core/slang-io.h"
#include "../core/slang-test-tool-util.h"

using namespace Slang;

#include <assert.h>

#ifdef _WIN32
#define MAIN slangc_main
#else
#define MAIN main
#endif

static void _diagnosticCallback(
    char const* message,
    void*       /*userData*/)
{
    auto stdError = StdWriters::getError();
    stdError.put(message);
    stdError.flush();
}

static SlangResult _compile(SlangCompileRequest* compileRequest, int argc, const char*const* argv)
{
    spSetDiagnosticCallback(compileRequest, &_diagnosticCallback, nullptr);
    spSetCommandLineCompilerMode(compileRequest);

    char const* appName = "slangc";
    if (argc > 0) appName = argv[0];

    {
        const SlangResult res = spProcessCommandLineArguments(compileRequest, &argv[1], argc - 1);
        if (SLANG_FAILED(res))
        {
            // TODO: print usage message
            return res;
        }
    }

    SlangResult res = SLANG_OK;

#ifndef _DEBUG
    try
#endif
    {
        // Run the compiler (this will produce any diagnostics through SLANG_WRITER_TARGET_TYPE_DIAGNOSTIC).
        res = spCompile(compileRequest);
        // If the compilation failed, then get out of here...
        // Turn into an internal Result -> such that return code can be used to vary result to match previous behavior
        res = SLANG_FAILED(res) ? SLANG_E_INTERNAL_FAIL : res;
    }
#ifndef _DEBUG
    catch (const Exception& e)
    {
        StdWriters::getOut().print("internal compiler error: %S\n", e.Message.toWString().begin());
        res = SLANG_FAIL;
    }
#endif

    return res;
}

SLANG_TEST_TOOL_API SlangResult innerMain(StdWriters* stdWriters, SlangSession* session, int argc, const char*const* argv)
{
    StdWriters::setSingleton(stdWriters);

    SlangCompileRequest* compileRequest = spCreateCompileRequest(session);
    SlangResult res = _compile(compileRequest, argc, argv);
    // Now that we are done, clean up after ourselves
    spDestroyCompileRequest(compileRequest);

    return res;
}

static SlangResult _commandLineMain(int argc, char** argv)
{
    ComPtr<slang::IGlobalSession> globalSession;
    slang_createGlobalSessionWithoutStdLib(SLANG_API_VERSION, globalSession.writeRef());
    // I guess we can be fancy and remove that parameter so it only works from the command line

    bool loadStdLib = false;

    List<char*> args;
    args.add(argv[0]);

    // If one of the params is -load-stdlib, then load the stdlib
    for (Index i = 1; i < argc; ++i)
    {
        if (UnownedStringSlice(argv[i]) == "-load-stdlib")
        {
            loadStdLib = true;
        }
        else
        {
            args.add(argv[i]);
        }
    }

    if (loadStdLib)
    {
        SLANG_RETURN_ON_FAIL(globalSession->loadStdLib());
    }
    else
    {
        SLANG_RETURN_ON_FAIL(globalSession->compileStdLib());
    }

    TestToolUtil::setSessionDefaultPreludeFromExePath(argv[0], globalSession);

    auto stdWriters = StdWriters::initDefaultSingleton();

    return innerMain(stdWriters, globalSession, int(args.getCount()), args.getBuffer());
}

int MAIN(int argc, char** argv)
{
    return (int)TestToolUtil::getReturnCode(_commandLineMain(argc, argv));
}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv)
{
    int result = 0;

    {
        // Convert the wide-character Unicode arguments to UTF-8,
        // since that is what Slang expects on the API side.

        List<String> args;
        for(int ii = 0; ii < argc; ++ii)
        {
            args.add(String::fromWString(argv[ii]));
        }
        List<char const*> argBuffers;
        for(int ii = 0; ii < argc; ++ii)
        {
            argBuffers.add(args[ii].getBuffer());
        }

        result = MAIN(argc, (char**) &argBuffers[0]);
    }

#ifdef _MSC_VER
    _CrtDumpMemoryLeaks();
#endif

    return result;
}
#endif
