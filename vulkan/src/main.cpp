#include <vk_engine.h>

#include <cstdio>
#include <cstring>

static void printUsage(const char* progName)
{
    std::printf("Usage: %s [options]\n", progName);
    std::printf("Options:\n");
    std::printf("  --scene <name>  Select scene to load (default: asteroid)\n");
    std::printf("                  Available scenes: asteroid, bistro\n");
    std::printf("  --help          Show this help message\n");
}

int main(int argc, char* argv[])
{
    int initialScene = 0;

    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--help") == 0)
        {
            printUsage(argv[0]);
            return 0;
        }
        else if (std::strcmp(argv[i], "--scene") == 0)
        {
            if (i + 1 >= argc)
            {
                std::fprintf(stderr, "Error: --scene requires a scene name\n\n");
                printUsage(argv[0]);
                return 1;
            }
            ++i;
            if (std::strcmp(argv[i], "asteroid") == 0)
            {
                initialScene = 0;
            }
            else if (std::strcmp(argv[i], "bistro") == 0)
            {
                initialScene = 1;
            }
            else
            {
                std::fprintf(stderr, "Error: unknown scene '%s'\n\n", argv[i]);
                printUsage(argv[0]);
                return 1;
            }
        }
        else
        {
            std::fprintf(stderr, "Error: unknown option '%s'\n\n", argv[i]);
            printUsage(argv[0]);
            return 1;
        }
    }

    VulkanEngine engine;

    engine.init(initialScene);

    engine.run();

    engine.cleanup();

    return 0;
}
