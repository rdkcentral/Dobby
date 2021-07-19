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


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <Logging.h>
#include <FileUtilities.h>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/xattr.h>

// On newer glibc ENOATTR is defined to be a synonym for ENODATA
#ifndef ENOATTR
#define ENOATTR ENODATA
#endif


#include "ImageManager.h"

const std::string ImageManager::xAttrUserIdName = "user.storage.plugin";


/**
 * @brief Checks if the given fs image exists and is mountable.
 *
 * The function will fail in the following cases:
 *   - the image file doesn't exist
 *   - xattr are supported and the value for the "user.storage.plugin" doesn't
 *     match the supplied userId
 *   - the image file exists but fsck.ext failed and it couldn't correct the
 *     errors
 *
 * If the file system image existed and fsck validated (or repaired) it, true
 * is returned.
 *
 * @param[in] filepath The fully qualified path to the filesystem image
 * @param[in] userId   The expected user id set in the xattr of the image file
 * @param[in] fix      If true and the filesystem image exists but is corrupt it
 *                     will attempt to fix the corruption.
 *
 * @return true on success and false on the failure.
 */
bool ImageManager::checkFSImage(const std::string & filepath,
                                      uid_t userId,
                                      const std::string & fs,
                                      bool fix /*= true*/)
{
    return checkFSImageAt(AT_FDCWD, filepath, userId, fs, fix);
}

bool ImageManager::checkFSImageAt(int dirFd,
                                        const std::string & filepath,
                                        uid_t userId,
                                        const std::string & fs,
                                        bool fix /*= true*/)
{
    AI_LOG_FN_ENTRY();

    struct stat buf;

    if ((fstatat(dirFd, filepath.c_str(), &buf, 0) < 0) || !S_ISREG(buf.st_mode))
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    int imageFd = openat(dirFd, filepath.c_str(), O_CLOEXEC | O_RDONLY);
    if (imageFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open file @ '%s'", filepath.c_str());
        return false;
    }


    // check the xattr on the file match the supplied value, this is just a
    // sanity check, we expect them to match if they don't then it indicates
    // there is a flaw in the code
    boost::optional<std::string> result = AICommon::getXAttrib(imageFd, xAttrUserIdName);
    if (!result)
    {
        // if xattr's not supported just ignore the failure
        if (errno == ENOTSUP)
        {
            AI_LOG_WARN("xattr not supported, failed to validate data.img, but carrying on");
        }
        // if the xattr not present then treat the image as corrupt
        else if (errno == ENOATTR)
        {
            AI_LOG_ERROR_EXIT("xattr missing on data file, re-generating a new one");
            close(imageFd);
            return false;
        }
        // any other error we log but carry on regardless
        else
        {
            AI_LOG_SYS_ERROR(errno, "failed to read xattr from data.img, ignoring");
        }
    }
    else
    {
        if (result->empty())
        {
            AI_LOG_ERROR_EXIT("xattr empty, will re-generate a new data.img file");
            close(imageFd);
            return false;
        }
        else if (static_cast<uid_t>(strtoul(result->c_str(), nullptr, 0)) != userId)
        {
            AI_LOG_ERROR_EXIT("xattr of data.img file doesn't match (expected %d"
                              ", actual %s)", userId, result->c_str());
            close(imageFd);
            return false;
        }
    }

    const bool isXfsFs = (0 == strcasecmp(fs.c_str(), "xfs"));

    // fork and exec the process
    pid_t pid = vfork();
    if (pid < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to fork and launch image checker");
    }
    else if (pid == 0)
    {
        // within forked client so exec the e2fsck utility
        int devnull = open("/dev/null", O_RDWR, 0);
        if (devnull < 0)
        {
            fprintf(stderr, "failed to redirect stdin, stdout and stderr to "
                    "/dev/null\n");
        }
        else
        {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO)
                close(devnull);
        }

        // dup the fd and pass that to the e2fsck
        int duppedImageFd = dup(imageFd);
        if (duppedImageFd < 0)
            _exit(64);

        char filePathBuf[64];
        sprintf(filePathBuf, "/proc/self/fd/%d", duppedImageFd);

        if (isXfsFs)
        {
            if (fix)
                execlp("/sbin/xfs_repair", "xfs_repair", "-o", "force_geometry", filePathBuf, nullptr);
            else
                execlp("/sbin/xfs_repair", "xfs_repair", "-n", filePathBuf, nullptr);
        }
        else
        {
            if (fix)
                execlp("/sbin/e2fsck", "e2fsck", "-f", "-p", filePathBuf, nullptr);
            else
                execlp("/sbin/e2fsck", "e2fsck", "-f", "-n", filePathBuf, nullptr);
        }

        // execlp failed, but don't bother trying to print an error as we've
        // already redirected stdout & stderr to /dev/null
        _exit(64);
    }

    // finished with the image file
    if (close(imageFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close image file");
    }

    // check if failed to fork
    if (pid < 0)
    {
        return false;
    }

    // in APP_Process so wait for the forked child to finish
    int status;
    if ((waitpid(pid, &status, 0) == -1) || !WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("file system check failed");
        return false;
    }

    if (isXfsFs)
    {
        if (WEXITSTATUS(status) == 64)
        {
            AI_LOG_ERROR_EXIT("failed to run the xfs_repair utility");
            return false;
        }

        if (WEXITSTATUS(status) == 1)
        {
            AI_LOG_ERROR_EXIT("xfs_repair run in no modify mode and filesystem corruption was detected");
            return false;
        }
        AI_LOG_FN_EXIT();
        return true;
    }

    // Quote from the man page:
    //  The exit code returned by e2fsck is the sum of the following conditions:
    //  0 - No errors
    //  1 - File system errors corrected
    //  2 - File system errors corrected, system should be rebooted
    //  4 - File system errors left uncorrected
    //  8 - Operational error
    //  16 - Usage or syntax error
    //  32 - E2fsck canceled by user request
    //  128 - Shared library error
    //
    // Also added the following for us
    //  64 - Failed to exec e2fsck

    if (WEXITSTATUS(status) & 0xfc)
    {
        if (WEXITSTATUS(status) == 64)
        {
            AI_LOG_ERROR_EXIT("failed to run the e2fsck utility, is it present "
                              "on the rootfs?");
        }
        else
        {
            AI_LOG_ERROR_EXIT("the e2fsck function failed with status 0x%02x",
                              WEXITSTATUS(status));
        }
        return false;
    }

    if (WEXITSTATUS(status) & 0x03)
    {
        AI_LOG_WARN("detect some errors in fs image '%s', but they have been "
                    "corrected (probably)", filepath.c_str());
    }

    AI_LOG_FN_EXIT();
    return true;
}



/**
 * @brief Create a filesystem image of the given size and format
 *
 * This function creates an empty file of the given size and then uses one
 * of the mkfs tools to write a file system structure into the file.
 *
 *
 * @warning If this function aborts in the middle of the process there is
 * a possibility it could leak temporary files.  Some sort of clean-up needs
 * be down at start-up to remove previous temporary files.
 *
 * @param[in] filepath The fully qualified path to the filesystem image
 * @param[in] userId   The user id to set set in the xattr of the image
 * @param[in] size     This size of the image to create in bytes
 * @param[in] fs       The filesystem type of image file (ext4)
 *
 * @return true on success and false on the failure.
 */
bool ImageManager::createFSImage(const std::string & filepath,
                                uid_t userId,
                                size_t size,
                                const std::string & fs)
{
    return createFSImageAt(AT_FDCWD, filepath, userId, size, fs);
}

bool ImageManager::createFSImageAt(int dirFd,
                                    const std::string & filepath,
                                    uid_t userId,
                                    size_t size,
                                    const std::string & fs)
{
    AI_LOG_FN_ENTRY();

    // NB: Kernel 3.11 and newer have a nice feature were you can specify
    // O_TMPFILE to create a file with no name that is freed on the last
    // close.  This would mean we wouldn't leak tmp files if the process
    // was aborted half way through

    int len;
    char tempFilename[PATH_MAX];
    if (dirFd == AT_FDCWD)
        len = snprintf(tempFilename, PATH_MAX, "%s.XXXXXX", filepath.c_str());
    else
        len = snprintf(tempFilename, PATH_MAX, "/proc/self/fd/%d/data.img.XXXXXX", dirFd);

    if (len >= PATH_MAX)
    {
        AI_LOG_ERROR_EXIT("directory name for package private data is too large");
        return false;
    }

#if defined(__GLIBC__) && ((__GLIBC__ > 2) || ((__GLIBC__ == 2) && (__GLIBC_MINOR__ >= 11)))
    int imageFd = mkostemp(tempFilename, O_CLOEXEC);
#else
    int imageFd = mkstemp(tempFilename);
    if (imageFd >= 0)
        fcntl(fd, F_SETFD, (fcntl(fd, F_GETFD) | FD_CLOEXEC));
#endif
    if (imageFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create application private data"
                                     " store at '%s'", tempFilename);
        return false;
    }

    if (ftruncate(imageFd, size) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed set the size of the private data"
                                     " image to %zu bytes", size);
        unlink(tempFilename);
        close(imageFd);
        return false;
    }

    // fork so we can launch the mkfs.ext[2,3,4] utility to format the data
    pid_t pid = vfork();
    if (pid < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to fork and launch image formatter");
    }
    else if (pid == 0)
    {
        // within forked client so exec the mkfs.ext utilities

        // redirect stdin, stdout and stderr file descriptors to /dev/null
        int devnull = open("/dev/null", O_RDWR, 0);
        if (devnull < 0)
        {
            fprintf(stderr, "failed to redirect stdin, stdout and stderr to "
                            "/dev/null\n");
        }
        else
        {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO)
                close(devnull);
        }

        // dup the fd and pass that to the mke2fs
        int duppedImageFd = dup(imageFd);
        if (duppedImageFd < 0)
            _exit(64);

        char filePathBuf[64];
        sprintf(filePathBuf, "/proc/self/fd/%d", duppedImageFd);

        // the format type
        std::string type;
        if (0 == strcasecmp(fs.c_str(), "ext2"))
        {
            type = "ext2";
        }
        else if (0 == strcasecmp(fs.c_str(), "ext3"))
        {
            type = "ext3";
        }
        else if (0 == strcasecmp(fs.c_str(), "ext4"))
        {
            type = "ext4";
        }
        else if (0 == strcasecmp(fs.c_str(), "xfs"))
        {
            type = "xfs";
        }
        else
        {
            //default "ext4"
            AI_LOG_WARN("Unsupported filesystem type '%s', using default 'ext4'", fs.c_str());
            type = "ext4";
        }

        if (type.compare("xfs") == 0)
        {
            execlp("/sbin/mkfs.xfs", "mkfs.xfs", filePathBuf, nullptr);
        }
        else
        {
            execlp("/sbin/mke2fs", "mke2fs", "-t", type.c_str(), "-F", filePathBuf, nullptr);
        }

        // execlp failed, but don't bother trying to print an error as we've
        // already redirected stdout & stderr to /dev/null
        _exit(EXIT_FAILURE);
    }

    // finished with the image file
    if (close(imageFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close image file");
    }

    // check if failed to fork
    if (pid < 0)
    {
        return false;
    }

    // in APP_Process so wait for the forked child to finish
    int status;
    if ((waitpid(pid, &status, 0) == -1) ||
        !WIFEXITED(status) || (WEXITSTATUS(status) != EXIT_SUCCESS))
    {
        unlink(tempFilename);
        AI_LOG_ERROR_EXIT("the mkfs function failed with status %d",
                          WEXITSTATUS(status));
        return false;
    }

    // ok now got a formatted image file, set the xattr to match the uid and ...
    if (!AICommon::setXAttrib(tempFilename, xAttrUserIdName,
                              std::to_string(userId)))
    {
        // treat as non-fatal for now ... as these attribs are only currently
        // used for sanity checking
        if (errno == ENOTSUP)
            AI_LOG_WARN("xattr not supported, can't add uid attribute");
        else
            AI_LOG_SYS_ERROR(errno, "failed to set uid xattr on data.img");
    }

    // ... move it to the correct spot
    if (renameat(AT_FDCWD, tempFilename, dirFd, filepath.c_str()) < 0)
    {
        unlink(tempFilename);
        AI_LOG_SYS_ERROR_EXIT(errno, "the rename function failed");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}


/**
 * @brief Removes a package's private data image file
 *
 * Simply a wrapper around the unlink call.
 *
 * @param[in] filepath The fully qualified path to the filesystem image
 *
 */
void ImageManager::deleteFSImage(const std::string & filepath)
{
    return deleteFSImageAt(AT_FDCWD, filepath);
}

void ImageManager::deleteFSImageAt(int dirFd,
                                         const std::string & filepath)
{
    AI_LOG_FN_ENTRY();

    if (unlinkat(dirFd, filepath.c_str(), 0) < 0)
    {
        if ((errno != ENOENT) && (errno != ENOTDIR))
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "failed to unlink app private data");
        }
   }

    AI_LOG_FN_EXIT();
}


