// unit-compression.cpp

#include "test-context.h"

#include "../../source/core/slang-zip-file-system.h"

using namespace Slang;

static bool _equals(const void* data, size_t size, ISlangBlob* blob)
{
    return blob && blob->getBufferSize() == size && memcmp(blob->getBufferPointer(), data, size) == 0;
}

template <size_t SIZE>
static bool _equals(const char (&text)[SIZE], ISlangBlob* blob)
{
    return _equals(text, SIZE, blob);
}

static void compressionUnitTest()
{
    // Create a zip to add stuff to
    RefPtr<CompressedFileSystem> buildFileSystem;
    CompressedFileSystem::createZip(buildFileSystem);

    const char contents[] = "I'm compressed";
    const char contents2[] = "Some more stuff";
    const char contents3[] = "Replace it";

    {
        ISlangMutableFileSystem* fileSystem = buildFileSystem;

        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->createDirectory("hello")));
        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->createDirectory("hello2")));
        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->remove("hello")));
        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->createDirectory("hello")));

        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->saveFile("file.txt", contents, SLANG_COUNT_OF(contents))));

        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->saveFile("file2.txt", contents2, SLANG_COUNT_OF(contents2))));

        ComPtr<ISlangBlob> blob;
        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->loadFile("file.txt", blob.writeRef())));
        SLANG_CHECK(_equals(contents, blob));

        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->loadFile("file2.txt", blob.writeRef())));
        SLANG_CHECK(_equals(contents2, blob));

        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->saveFile("file2.txt", contents3, SLANG_COUNT_OF(contents3))));

        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->loadFile("file2.txt", blob.writeRef())));
        SLANG_CHECK(_equals(contents3, blob));

        // Check the path type
        {
            SlangPathType pathType;
            SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->getPathType("file2.txt", &pathType)));
            SLANG_CHECK(pathType == SLANG_PATH_TYPE_FILE);

            SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->getPathType("hello", &pathType)));
            SLANG_CHECK(pathType == SLANG_PATH_TYPE_DIRECTORY);
        }

        // Enumerate
        {
            List<String> objs;
            fileSystem->enumeratePathContents("", [](SlangPathType pathType, const char* name, void* userData) {
                List<String>& out = *(List<String>*)userData;
                out.add(name);
            }, &objs);

            for (const auto& obj : objs)
            {
                // All of these should exist
                SlangPathType pathType;
                SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->getPathType(obj.getBuffer(), &pathType)));
            }
        }
    }

    // Load and check its okay
    {
        const auto archive = buildFileSystem->getArchive();

        RefPtr<CompressedFileSystem> fileSystem;
        CompressedFileSystem::createZip(archive.getBuffer(), archive.getCount(), fileSystem);

        ComPtr<ISlangBlob> blob;

        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->loadFile("file.txt", blob.writeRef())));
        SLANG_CHECK(_equals(contents, blob));

        SLANG_CHECK(SLANG_SUCCEEDED(fileSystem->loadFile("file2.txt", blob.writeRef())));
        SLANG_CHECK(_equals(contents3, blob));
    }
}

SLANG_UNIT_TEST("Compression", compressionUnitTest);
