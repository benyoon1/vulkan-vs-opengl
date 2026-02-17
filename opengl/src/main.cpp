#include "core/application.h"
#include <cstring>
#include <exception>
#include <iostream>

static void printUsage(const char* programName)
{
    std::cerr << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  --scene <name>  Select scene to load (default: asteroid)\n"
              << "                  Available scenes: asteroid, bistro\n"
              << "  --help          Show this help message\n";
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
                std::cerr << "Error: --scene requires an argument\n";
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
                std::cerr << "Error: unknown scene '" << argv[i] << "'\n";
                printUsage(argv[0]);
                return 1;
            }
        }
        else
        {
            std::cerr << "Error: unknown option '" << argv[i] << "'\n";
            printUsage(argv[0]);
            return 1;
        }
    }

    try
    {
        Application app(initialScene);
        app.run();
        return 0;
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "Initialization error: " << e.what() << std::endl;
        return -2;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Fatal std::exception: " << e.what() << std::endl;
        return -1;
    }
}
