#include "core/application.h"
#include <exception>
#include <iostream>

int main()
{
    try
    {
        Application app;
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
