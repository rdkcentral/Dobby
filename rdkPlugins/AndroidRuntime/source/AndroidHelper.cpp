/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "AndroidHelper.h"
#include "DobbyRdkPluginUtils.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <list>
#include <map>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <string.h>
#include <sys/sysmacros.h>

#if defined(__linux__)
#include <linux/loop.h>
#endif

// The major number of the loop back devices
#define LOOP_DEV_MAJOR_NUM          7

// Storage helper methods (doesn't require state to work)

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to open an available loop device
 *
 *  WARNING this method requires sudo
 *
 *  @param[out]  loopDevice         Loop device name
 *
 *  @return on success a positive file descriptor corresponding to a free
 *  loop device, -1 on error.
 */
int AndroidHelper::openLoopDevice(std::string* loopDevice)
{
    AI_LOG_FN_ENTRY();

    // This is the part which require sudo!!
    int devCtlFd = open("/dev/loop-control", O_RDWR | O_CLOEXEC);
    if (devCtlFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open '/dev/loop-control'");
        return -1;
    }

    int devFd = -1;
    for (unsigned attempts = 0; (attempts < 5) && (devFd < 0); attempts++)
    {
        int devNum = ioctl(devCtlFd, LOOP_CTL_GET_FREE);
        if (devNum < 0)
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "failed to get free device from loop control");
            close(devCtlFd);
            return -1;
        }

        AI_LOG_DEBUG("found free loop device number %d", devNum);

        char loopDevPath[32];
        sprintf(loopDevPath, "/dev/loop%d", devNum);

        devFd = open(loopDevPath, O_RDWR | O_CLOEXEC);
        if (devFd < 0)
        {
            // check if we failed because the devnode didn't exist, if this
            // happens then we should go create the dev node ourselves, at this
            // point we're racing against udev which may also be trying to
            // create the dev node ... we don't care who wins as long as there
            // is a dev node there when we try and open it
            if (errno == ENOENT)
            {
                if (mknod(loopDevPath, (S_IFBLK | 0660),
                          makedev(LOOP_DEV_MAJOR_NUM, devNum)) != 0)
                {
                    if (errno != EEXIST)
                        AI_LOG_SYS_ERROR(errno, "failed to mknod '%s'", loopDevPath);
                }
            }

            // try and open the devnode once again
            devFd = open(loopDevPath, O_RDWR | O_CLOEXEC);
        }

        // check again if managed to open the file
        if (devFd < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to open '%s'", loopDevPath);

            // try and release the loop device we created (but failed to
            // connect to)
            if (ioctl(devCtlFd, LOOP_CTL_REMOVE, devNum) != 0)
                AI_LOG_SYS_ERROR(errno, "failed to free device from loop control");
        }
        else if (loopDevice != nullptr)
        {
            loopDevice->assign(loopDevPath);
        }
    }

    if (close(devCtlFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close '/dev/loop-control'");
    }

    AI_LOG_FN_EXIT();
    return devFd;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to attach the file to the loop device
 *
 *  @param[in] loopFd           An open file descriptor to the loop device
 *  @param[in] fileFd           An open file descriptor that should be associate
 *                              with the loop device.
 *
 *  @return on success a positive file desccriptor corresponding to a free
 *  loop device, -1 on error.
 */
bool AndroidHelper::attachFileToLoopDevice(int loopFd, int fileFd)
{
    AI_LOG_FN_ENTRY();

    if (ioctl(loopFd, LOOP_SET_FD, fileFd) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to attach to file to loop device");
        return false;
    }

    struct loop_info64 loopInfo;
    bzero(&loopInfo, sizeof(loopInfo));
    loopInfo.lo_flags = LO_FLAGS_AUTOCLEAR;

    if (ioctl(loopFd, LOOP_SET_STATUS64, &loopInfo) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to set the autoclear flag");

        if (ioctl(loopFd, LOOP_CLR_FD, 0) < 0)
        {
            AI_LOG_SYS_WARN(errno, "failed to detach from loop device");
        }

        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_DEBUG("attached file to loop device");

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Associates a give file descriptor with a loop device
 *
 *  First the function attempts to get a free loop device, if that succeeds it
 *  attaches the supplied file descriptor to it and returns an fd to the loop
 *  device and (optionally) writes the path to the loop device in the
 *  @a loopDevPath string.
 *
 *  @param[in]  fileFd          An open file descriptor to associate with
 *                              the loop device.
 *  @param[out] loopDevPath     If not null, the method will write the path
 *                              to the loop device dev node into the string
 *
 *  @return on success returns the open file descriptor to the loop device
 *  associated with the file, on failure -1.
 */
int AndroidHelper::loopDeviceAssociate(int fileFd, std::string* loopDevPath /*= nullptr*/)
{
    AI_LOG_FN_ENTRY();

    int loopDevFd = openLoopDevice(loopDevPath);
    if (loopDevFd < 0)
    {
        AI_LOG_ERROR_EXIT("failed to open loop device");
        return -1;
    }

    if (!attachFileToLoopDevice(loopDevFd, fileFd))
    {
        AI_LOG_ERROR_EXIT("failed to attach file to loop device");
        close(loopDevFd);
        return -1;
    }

    AI_LOG_FN_EXIT();
    return loopDevFd;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attaches the given file to an available loop device
 *
 *  @param[in]  sourceFile  The path to the image file.
 *  @param[out] loopDevice  The path to the loop device that was attached.
 *
 *  @return the file descriptor to the loop device if attached, otherwise -1.
 */
int AndroidHelper::attachLoopDevice(const std::string& sourceFile,
                                       std::string* loopDevice)
{
    AI_LOG_FN_ENTRY();

    // check we managed to open the file
    int fd = open(sourceFile.c_str(), O_CLOEXEC | O_RDWR);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open file @ '%s'",
                              sourceFile.c_str());
        return -1;
    }

    // associate the fd with a free loop device
    int loopDevFd = loopDeviceAssociate(fd, loopDevice);

    // no longer need the backing file, can close
    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close file");
    }

    AI_LOG_INFO("Attached sourcefile '%s' to loopdevice '%s' with file description %d",
                sourceFile.c_str(),
                loopDevice->c_str(),
                loopDevFd);

    AI_LOG_FN_EXIT();
    return loopDevFd;
}

// -------------------------------------------------------------------------
/**
 *  @brief Removes a directory and all it's contents.
 *
 *  This is equivalent to the 'rm -rf' command.
 *
 *  If the pathname given in pathname is relative, then it is interpreted
 *  relative to the directory referred to by the file descriptor dirFd, if
 *  dirFd is not supplied then it's relative to the cwd.
 *
 *  @warning This function only supports deleting directories with contents
 *  that are less than 128 levels deep, this is to avoid running out of
 *  file descriptors.
 *
 *  @param[in]  dirFd           If specified the path should be relative to
 *                              to this directory.
 *  @param[in]  path            The path to the directory to create.
 *
 *  @return true on success, false on failure.
 */
bool AndroidHelper::rmdirRecursive(int dirFd, const std::string& path)
{
    AI_LOG_FN_ENTRY();

    // remove the directory contents first
    bool success = rmdirContents(dirFd, path);
    if (success)
    {
        // then delete the directory itself
        if (unlinkat(dirFd, path.c_str(), AT_REMOVEDIR) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to remove dir at '%s", path.c_str());
            success = false;
        }
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -------------------------------------------------------------------------
/**
 *  @brief Removes the contents of a directory but leave the actual
 *  directory in place.
 *
 *  This is equivalent to the 'cd <dir>; rm -rf *' command.
 *
 *  If the pathname given in @a path is relative, then it is interpreted
 *  relative to the directory referred to by the file descriptor @a dirFd, if
 *  @a dirFd is not supplied then it's relative to the cwd.
 *
 *  @warning This function only supports deleting directories with contents
 *  that are less than 128 levels deep, this is to avoid running out of
 *  file descriptors.
 *
 *  @param[in]  dirFd           If specified the path should be relative to
 *                              to this directory.
 *  @param[in]  path            The path to the directory to create.
 *
 *  @return true on success, false on failure.
 */
bool AndroidHelper::rmdirContents(int dirFd, const std::string& path)
{
    AI_LOG_FN_ENTRY();

    // get the fd of the directory to delete
    int toDeleteFd = openat(dirFd, path.c_str(), O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
    if (toDeleteFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open dir @ '%s'", path.c_str());
        return false;
    }

    // recursively walks the directory deleting all the files and directories
    // within it, this will also close the file descriptor
    bool success = deleteRecursive(toDeleteFd, 128);

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Recursive function that deletes everything within the supplied
 *  directory (as a descriptor).
 *
 *  @param[in]  dirFd           If specified the path should be relative to
 *                              to this directory.
 *  @param[in]  availDepth      Maximal depth of recursion
 *
 *  @return true on success, false on failure.
 *
 *
 */
bool AndroidHelper::deleteRecursive(int dirfd, int availDepth)
{
    DIR* dir = fdopendir(dirfd);
    if (!dir)
    {
        AI_LOG_SYS_ERROR(errno, "fdopendir failed");

        // to maintain consistency we should close the fd in case of failure
        if (close(dirfd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to close dirfd");
        }

        return false;
    }

    bool success = true;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // skip the '.' and '..' entries
        if ((entry->d_name[0] == '.') && ((entry->d_name[1] == '\0') ||
                                          ((entry->d_name[1] == '.') && (entry->d_name[2] == '\0'))))
        {
            continue;
        }

        // if a directory then recurse into it
        if (entry->d_type == DT_DIR)
        {
            // check we're not going to deep
            if (--availDepth <= 0)
            {
                AI_LOG_ERROR("recursing to deep, aborting");
                success = false;
                break;
            }

            // try and open the dir
            int fd = openat(dirfd, entry->d_name, O_CLOEXEC | O_DIRECTORY | O_NOFOLLOW);
            if (fd < 0)
            {
                AI_LOG_SYS_ERROR(errno, "failed to open directory '%s'",
                                 entry->d_name);
                success = false;
                break;
            }
            else
            {
                // recurse into the directory deleting it's contents, the
                // function assumes ownership of the fd and will free (closedir)
                if (!deleteRecursive(fd, availDepth))
                {
                    success = false;
                    break;
                }
            }
        }

        int flags = (entry->d_type == DT_DIR) ? AT_REMOVEDIR : 0;

        // try unlinking the file / directory / symlink / whatever
        if (unlinkat(dirfd, entry->d_name, flags) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to remove '%s'", entry->d_name);
            success = false;
            break;
        }
    }

    closedir(dir);

    return success;
}

// Tests Storage helpers
#ifdef ENABLE_TESTS
// cppcheck-suppress unusedFunction
bool AndroidHelper::Test_mkdirRecursive(const std::string& rootfsPath)
{
    std::string tmp = "/home/private/";
    tmp = rootfsPath + tmp;

    tmp.append(".temp");

    AI_LOG_INFO("temp path = '%s'", tmp.c_str());

    // step 1 - create a directory within the rootfs
    if (DobbyRdkPluginUtils::mkdirRecursive(tmp, 0700))
    {
        AI_LOG_INFO("Success");
        return true;
    }
    else
    {
        AI_LOG_INFO("Fail");
        return false;
    }
}

// cppcheck-suppress unusedFunction
bool AndroidHelper::Test_openLoopDevice()
{
    std::string loopDevPath;

    int loopDevFd = openLoopDevice(&loopDevPath);
    if (loopDevFd < 0)
    {
        AI_LOG_ERROR_EXIT("failed to open loop device");
        return false;
    }
    else
    {
        AI_LOG_INFO("Opened loop mount =%s", loopDevPath.c_str());
    }

    if (close(loopDevFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close file");
        return false;
    }
    return true;
}

// cppcheck-suppress unusedFunction
bool AndroidHelper::Test_attachLoopDevice(std::string& imagePath)
{
    std::string loopDevice;

    createFileIfNeeded(imagePath, 1024*10*12, 123, "ext4");

    int loopDevFd = attachLoopDevice(imagePath, &loopDevice);
    if ((loopDevFd < 0) || (loopDevice.empty()))
    {
        AI_LOG_ERROR("failed to attach file to loop device");
        return false;
    }
    else if (close(loopDevFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close file");
        return false;
    }
    else
    {
        AI_LOG_INFO("Successfully attached loop device =%s", loopDevice.c_str());
    }

    return true;
}

// cppcheck-suppress unusedFunction
bool AndroidHelper::Test_cleanMountLostAndFound(const std::string& rootfsPath)
{
    std::string tmp = "/lost+found/some/long/path/file.xyz";
    tmp = rootfsPath + tmp;

    createFileIfNeeded(tmp, 1024*12*12, 123, "ext4");

    cleanMountLostAndFound(rootfsPath, std::string("0"));
}

// cppcheck-suppress unusedFunction
bool AndroidHelper::Test_checkWriteReadMount(const std::string& tmpPath)
{
    //Test
    ssize_t nrd;
    const char text[] = "Storage was runned\n";
    const unsigned int BUFFER_SIZE = 100;

    //std::string tmpPath = "/home/private/test.txt";
    AI_LOG_INFO("path = '%s'", tmpPath.c_str());

    int fd = open(tmpPath.c_str(), O_RDWR | O_CREAT | O_APPEND, 0777);
    if (fd < 0)
        AI_LOG_SYS_ERROR(errno, "failed to open");
    else
    {
        AI_LOG_INFO("write fd = %d", fd);

        nrd = write(fd,text, sizeof(text));
        AI_LOG_INFO("write nrd = %d", nrd);
        close(fd);
    }

    fd = open(tmpPath.c_str(), O_RDONLY, 0777);
    if (fd < 0)
        AI_LOG_SYS_ERROR(errno, "failed to open");
    else
    {
        char buffer[BUFFER_SIZE] = "";
        nrd = read(fd,buffer,BUFFER_SIZE);
        if (nrd > 0) {
            AI_LOG_INFO("Test file content '%s'", buffer);
        }

        AI_LOG_INFO("read nrd = %d", nrd);
        close(fd);
    }
}
#endif // ENABLE_TESTS
