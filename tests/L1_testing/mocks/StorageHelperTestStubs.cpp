#include "DobbyRdkPluginUtils.h"
#include "ImageManager.h"

bool ImageManager::checkFSImage(const std::string&, uid_t, const std::string&, bool)
{
    return false;
}

bool ImageManager::createFSImage(const std::string&, uid_t, size_t, const std::string&)
{
    return false;
}

bool DobbyRdkPluginUtils::mkdirRecursive(const std::string&, mode_t)
{
    return false;
}
