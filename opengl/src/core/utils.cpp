#include <core/utils.h>

std::string Utils::getPath(const std::string& path)
{
#ifdef ROOT_PATH
    return std::string(ROOT_PATH) + "/" + path;
#else
    return "../../../" + path;
#endif
}
