#pragma once

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

namespace AICommon
{

inline DIR* openDirectoryNoFollow(const std::string& path)
{
    int fd = open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0)
    {
        return nullptr;
    }

    DIR* dir = fdopendir(fd);
    if (!dir)
    {
        int error = errno;
        close(fd);
        errno = error;
    }
    return dir;
}

inline bool directoryEntryIsDirectory(int dirFd, const dirent* entry)
{
    if (entry->d_type == DT_DIR)
    {
        return true;
    }
    if (entry->d_type != DT_UNKNOWN)
    {
        return false;
    }

    struct stat info;
    return fstatat(dirFd, entry->d_name, &info, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(info.st_mode);
}

}
