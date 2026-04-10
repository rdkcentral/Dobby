/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
/*
 * File:   DobbyUtils.cpp
 *
 */
#include "DobbyUtils.h"
#include "DobbyTimer.h"

#include <Logging.h>
#include <FileUtilities.h>

#include <thread>
#include <sstream>
#include <fstream>
#include <cstring>
#include <ext/stdio_filebuf.h>

#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sched.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#if defined(__linux__)
#include <linux/loop.h>
#endif

// The major number of the loop back devices
#define LOOP_DEV_MAJOR_NUM          7

// All sky kernels have loop control enabled, however the ST toolchain was
// built against old kernel headers meaning that the define is not present
// in the standard loop.h headers. Hence the need to define it here.
#ifndef LOOP_CTL_REMOVE
    #define LOOP_CTL_REMOVE         0x4C81
#endif
#ifndef LOOP_CTL_GET_FREE
    #define LOOP_CTL_GET_FREE       0x4C82
#endif

// Workaround for ST (old) toolchains that (for some reason) have the setns
// function defined in their headers but lack the symbol in the version of
// libc they use.  So to workaround it we just force the use of the syscall
// directly.
#if ( defined(__arm__) || defined (__aarch64__) ) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ < 15)
    #include <syscall.h>
    #if !defined(SYS_setns)
        #define SYS_setns 375
    #endif
    #define setns(fd, nstype) \
        syscall(SYS_setns, fd, nstype)
#endif




DobbyUtils::DobbyUtils()
{
    AI_LOG_FN_ENTRY();

    // build the whitelist of allowed device nodes
    buildDeviceWhitelist();

    // create the timer queue thread
    mTimerQueue = std::make_shared<DobbyTimer>();

    AI_LOG_FN_EXIT();
}

DobbyUtils::~DobbyUtils()
{
    AI_LOG_FN_ENTRY();

    mTimerQueue.reset();

    AI_LOG_FN_EXIT();
}

// -------------------------------------------------------------------------
/**
 *  @brief Makes a directory and all parent directories as needed
 *
 *  This is equivalent to the 'mkdir -p' command.
 *
 *  All directories created will have access mode set by @a mode, for this
 *  reason the mode should be at least 'rwx------'.
 *
 *  If the pathname given in @a path is relative, then it is interpreted
 *  relative to the directory referred to by the file descriptor dirFd, if
 *  dirFd is not supplied then it's relative to the cwd.
 *
 *  @param[in]  dirFd           If specified the path should be relative to
 *                              to this directory.
 *  @param[in]  path            The path to the directory to create.
 *  @param[in]  mode            The file access mode to give to all
 *                              directories created.
 *
 *  @return true on success, false on failure.
 */
bool DobbyUtils::mkdirRecursive(int dirFd, const std::string& path, mode_t mode) const
{
    AI_LOG_FN_ENTRY();

    if (path.empty())
    {
        AI_LOG_ERROR_EXIT("empty path supplied");
        return false;
    }

    std::istringstream stream(path);
    std::string token;

    std::string partial((path.front() == '/') ? "/" : "");
    while (std::getline(stream, token, '/'))
    {
        if (token.empty())
            continue;

        partial += token;
        partial += '/';

        if (mkdirat(dirFd, partial.c_str(), mode) != 0)
        {
            if (errno == EEXIST)
                continue;

            AI_LOG_SYS_ERROR_EXIT(errno, "failed to create dir '%s'",
                                  partial.c_str());
            return false;
        }

        if (fchmodat(dirFd, partial.c_str(), mode, 0) != 0)
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "failed to set dir '%s' perms to 0%03o",
                                  partial.c_str(), mode);
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

bool DobbyUtils::mkdirRecursive(const std::string& path, mode_t mode) const
{
    return mkdirRecursive(AT_FDCWD, path, mode);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Recursive function that deletes everything within the supplied
 *  directory (as a descriptor).
 *
 *
 *
 *
 */
bool DobbyUtils::deleteRecursive(int dirfd, int availDepth)
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
 *  @param[in]  mode            The file access mode to give to all
 *                              directories created.
 *
 *  @return true on success, false on failure.
 */
bool DobbyUtils::rmdirRecursive(int dirFd, const std::string& path) const
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

bool DobbyUtils::rmdirRecursive(const std::string& path) const
{
    return rmdirRecursive(AT_FDCWD, path);
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
 *  @param[in]  mode            The file access mode to give to all
 *                              directories created.
 *
 *  @return true on success, false on failure.
 */
bool DobbyUtils::rmdirContents(int dirFd, const std::string& path) const
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

bool DobbyUtils::rmdirContents(const std::string& path) const
{
    return rmdirContents(AT_FDCWD, path);
}

bool DobbyUtils::rmdirContents(int dirFd) const
{
    AI_LOG_FN_ENTRY();

    // need to dup the dirfd as the deleteRecursive function closes the
    // supplied fd
    int fd = fcntl(dirFd, F_DUPFD_CLOEXEC, 3);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to dup fd");
        return false;
    }

    // recursively walks the directory deleting all the files and directories
    // within it, this will also close the file descriptor
    bool success = deleteRecursive(fd, 128);

    AI_LOG_FN_EXIT();
    return success;
}

// -------------------------------------------------------------------------
/**
 *  @brief Logs and deletes any files found in the lost+found directory of
 *  the mount point.
 *
 *  We should be clearing the lost+found to avoid cruft building up and
 *  taking all the space in the loop mount.
 *
 *
 *  @param[in]  mountPoint      The absolute path to the mounted device,
 *                              NOT the the location of the lost+found dir.
 *  @param[in]  logTag          If not empty then a log warning message will
 *                              be printed containing the name of the file
 *                              that was deleted and referencing the the
 *                              string in logTag.
 */
void DobbyUtils::cleanMountLostAndFound(const std::string& mountPoint,
                                        const std::string& logTag) const
{
    AI_LOG_FN_ENTRY();

    std::string lostFoundDirPath(mountPoint);
    lostFoundDirPath.append("/lost+found");

    // iterate through the directory
    DIR* dir = opendir(lostFoundDirPath.c_str());
    if (!dir)
    {
        AI_LOG_SYS_ERROR(errno, "opendir failed for '%s'",
                         lostFoundDirPath.c_str());
        return;
    }

    // log and delete all the files / dirs in the lost+found
    int deletedEntries;
    do
    {
        deletedEntries = 0;

        struct dirent *entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            // skip the '.' and '..' entries
            if ((entry->d_name[0] == '.') && ((entry->d_name[1] == '\0') ||
                                              ((entry->d_name[1] == '.') && (entry->d_name[2] == '\0'))))
            {
                continue;
            }

            // if a directory then recursively delete it
            if (entry->d_type == DT_DIR)
            {
                if (!logTag.empty())
                    AI_LOG_WARN("cleaning dir '%s' from lost+found for '%s'",
                                entry->d_name, logTag.c_str());

                if (rmdirRecursive(dirfd(dir), entry->d_name))
                {
                    deletedEntries++;
                }
            }

            // if any other file type, including sockets, fifo, symlinks,
            // dev nodes, etc then unlink them
            else
            {
                if (!logTag.empty())
                    AI_LOG_WARN("cleaning file '%s' from lost+found for '%s'",
                                entry->d_name, logTag.c_str());

                if (unlinkat(dirfd(dir), entry->d_name, 0) != 0)
                {
                    AI_LOG_SYS_ERROR(errno, "failed to delete '%s' in lost+found'",
                                     entry->d_name);
                }
                else
                {
                    deletedEntries++;
                }
            }

        }

        // if we deleted files we should re-scan the directory to make sure we
        // haven't missed anything
        if (deletedEntries > 0)
        {
            rewinddir(dir);
        }

    } while (deletedEntries > 0);

    // clean up
    closedir(dir);

    AI_LOG_FN_EXIT();
}

// -------------------------------------------------------------------------
/**
 *  @brief Returns a file descriptor to the given namespace of the process
 *
 *  The caller is responsible for closing the returned file descriptor when
 *  it is no longer required.
 *
 *  The returned namespace can used in the setns(...) call, or other calls
 *  that enter / manipulate namespaces.
 *
 *  @param[in]  pid         The pid of the process to get the namespace of.
 *  @param[in]  nsType      The type of namespace to get, it should be one
 *                          of the CLONE_NEWxxx constants, see the setns
 *                          man page for possible values.
 *
 *  @return on success the file descriptor to the given namespace, on
 *  failure -1
 */
int DobbyUtils::getNamespaceFd(pid_t pid, int nsType) const
{
    AI_LOG_FN_ENTRY();

    // determine the type of namespace to enter
    char nsPath[64];
    switch (nsType)
    {
        case CLONE_NEWIPC:
            sprintf(nsPath, "/proc/%d/ns/ipc", pid);
            break;
        case CLONE_NEWNET:
            sprintf(nsPath, "/proc/%d/ns/net", pid);
            break;
        case CLONE_NEWNS:
            sprintf(nsPath, "/proc/%d/ns/mnt", pid);
            break;
        case CLONE_NEWPID:
            sprintf(nsPath, "/proc/%d/ns/pid", pid);
             break;
        case CLONE_NEWUSER:
            sprintf(nsPath, "/proc/%d/ns/user", pid);
             break;
        case CLONE_NEWUTS:
            sprintf(nsPath, "/proc/%d/ns/uts", pid);
             break;
        default:
            AI_LOG_ERROR_EXIT("invalid nsType (%d)", nsType);
            return -1;
    }

    // try and open the namespace
    int newNsFd = open(nsPath, O_RDONLY | O_CLOEXEC);
    if (newNsFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open namespace @ '%s'", nsPath);
    }

    AI_LOG_FN_EXIT();
    return newNsFd;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Thread helper function that implements the setns syscall
 *
 *  This must be executed as a thread as it calls setns which switches
 *  namespaces and you don't really want to do this in the main thread.
 *
 *
 *  @param[in]  newNsFd     The namespace to switch into.
 *  @param[in]  func        The function to execute in the new namespace.
 *
 *  @return true if successifully entered the namespace, otherwise false.
 */
void DobbyUtils::nsThread(int newNsFd, int nsType, bool* success,
                          std::function<bool()>& func) const
{
    AI_LOG_FN_ENTRY();

    // unshare the specific namespace from the thread
    if (unshare(nsType) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to unshare");
        *success = false;
        return;
    }

    // switch into the new namespace
    if (setns(newNsFd, nsType) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to switch into new namespace");
        *success = false;
        return;
    }

    // execute the caller's function
     *success = func();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Utility function to run some code in a specific namespace of the
 *  container.
 *
 *  This function uses the setns syscall and therefore it must spawn a thread
 *  to run the callback in.  However this function blocks until the thread
 *  completes, so although it is multi-threaded it's API is blocking, i.e.
 *  effectively single threaded.
 *
 *  @param[in]  namespaceFd The fd of the namespace to enter.
 *  @param[in]  nsType      The namespace to run the function in, see above.
 *  @param[in]  func        The function to execute.
 *
 *  @return true if successifully entered the namespace, otherwise false.
 */
bool DobbyUtils::callInNamespaceImpl(int namespaceFd,
                                     const std::function<bool()>& func) const
{
    AI_LOG_FN_ENTRY();

    bool success = true;

    // spawn the thread to run the callback in
    std::thread _nsThread = std::thread(std::bind(&DobbyUtils::nsThread,
                                                  this, namespaceFd, 0,
                                                  &success, func));

    // block until the thread completes
    _nsThread.join();

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Utility function to run some code in a specific namespace of the
 *  container.
 *
 *  This function uses the setns syscall and therefore it must spawn a thread
 *  to run the callback in.  However this function blocks until the thread
 *  completes, so although it is multi-threaded it's API is blocking, i.e.
 *  effectively single threaded.
 *
 *  The @a nsType argument should be one of the following values:
 *        CLONE_NEWIPC  - run in a IPC namespace
 *        CLONE_NEWNET  - run in a network namespace
 *        CLONE_NEWNS   - run in a mount namespace
 *        CLONE_NEWPID  - run in a PID namespace
 *        CLONE_NEWUSER - run in a user namespace
 *        CLONE_NEWUTS  - run in a UTS namespace
 *
 *  @param[in]  pid         The pid of the process namespace to enter.
 *  @param[in]  nsType      The namespace to run the function in, see above.
 *  @param[in]  func        The function to execute.
 *
 *  @return true if successifully entered the namespace, otherwise false.
 */
bool DobbyUtils::callInNamespaceImpl(pid_t pid, int nsType,
                                     const std::function<bool()>& func) const
{
    AI_LOG_FN_ENTRY();

    char nsName[8];
    char nsPath[32];

    // determine the type of namespace to enter
    switch (nsType)
    {
        case CLONE_NEWIPC:
            strcpy(nsName, "ipc");
            break;
        case CLONE_NEWNET:
            strcpy(nsName, "net");
            break;
        case CLONE_NEWNS:
            strcpy(nsName, "mnt");
            break;
        // the following namespaces are tricky and have special restrictions,
        // at the moment no hook should be using them so disable until needed
        /*
        case CLONE_NEWPID:
            strcpy(nsName, "pid");
            break;
        case CLONE_NEWUSER:
            strcpy(nsName, "user");
            break;
        case CLONE_NEWUTS:
            strcpy(nsName, "uts");
            break;
        */
        case CLONE_NEWPID:
        case CLONE_NEWUSER:
        case CLONE_NEWUTS:
            AI_LOG_ERROR_EXIT("unsupported nsType (%d)", nsType);
            return false;
        default:
            AI_LOG_ERROR_EXIT("invalid nsType (%d)", nsType);
            return false;
    }

    bool success = true;

    // get the namespace of the containered app
    sprintf(nsPath, "/proc/%d/ns/%s", pid, nsName);
    int newNsFd = open(nsPath, O_RDONLY | O_CLOEXEC);
    if (newNsFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open container namespace @ '%s'",
                         nsPath);
        success = false;
    }
    else
    {
        // spawn the thread to run the callback in
        std::thread _nsThread = std::thread(std::bind(&DobbyUtils::nsThread,
                                                      this, newNsFd, nsType,
                                                      &success, func));

        // block until the thread completes
        _nsThread.join();
    }

    // close the namespaces
    if ((newNsFd >= 0) && (close(newNsFd) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close namespace");
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to open an available loop device
 *
 *
 *  @return on success a positive file desccriptor corresponding to a free
 *  loop device, -1 on error.
 */
int DobbyUtils::openLoopDevice(std::string* loopDevice) const
{
    AI_LOG_FN_ENTRY();

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
 *
 *  @return on success a positive file desccriptor corresponding to a free
 *  loop device, -1 on error.
 */
bool DobbyUtils::attachFileToLoopDevice(int loopFd, int fileFd) const
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
int DobbyUtils::loopDeviceAssociate(int fileFd, std::string* loopDevPath /*= nullptr*/) const
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
 *  @brief Run the E2FS tool inside the given directory with given args
 *
 *  This function does a fork/exec to launch the process, it drops root
 *  privileges and runs the tool as user 1000:1000, therefore the file that is
 *  being checked should be readable and writeble by user 1000.
 *
 *  If this function returns false the image file should probably be deleted /
 *  reformatted.
 *
 *  @param[in]  dirFd           The fd of the directory containing the image to
 *                              check.  The function will switch to this
 *                              directory before dropping permissions (provided
 *                              it's not AT_FCWD).
 *  @param[out] consoleOutput   String list to store the lines of output from
 *                              the tool.
 *  @param[in]  e2fsTool        The tool to run, should be either "e2fsck" or
 *                              "mke2fs".
 *  @param[in]  ...             Extra arguments to supply to the tool.
 *
 *  @return if the file passes the check (or was successifully repaired) true is
 *  returned, otherwise false.
 */
int DobbyUtils::runE2fsTool(int dirFd, std::list<std::string>* consoleOutput,
                            const char* e2fsTool, ...) const
{
    AI_LOG_FN_ENTRY();

    // create a pipe to store the stderr and stdout of the mke2fs utility, this
    // is just to help with debugging issues
    int pipeFds[2] = { -1, -1 };
    if (pipe2(pipeFds, O_CLOEXEC) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create stderr/stdout pipe");
        return -1;
    }


    // fork the process
    pid_t pid = fork();
    if (pid < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to vfork");
        close(pipeFds[0]);
        close(pipeFds[1]);
        return -1;
    }

    if (pid == 0)
    {
        // within forked client so exec the e2fs utility

        // close the read side of the pipe
        close(pipeFds[0]);


        // create the arguments vector, this is a string list terminated by a
        // nullptr string pointer
        std::vector<char*> args;
        args.push_back(strdup(e2fsTool));

        va_list ap;
        va_start(ap, e2fsTool);

        const char* arg_;
        while ((arg_ = va_arg(ap, const char *)) != nullptr)
        {
            args.push_back(strdup(arg_));
        }

        va_end(ap);
        args.push_back(nullptr);


        // dup the write end of the pipe to stderr and stdout, dropping the
        // O_CLOSEXEC flag at the same time
        dup2(pipeFds[1], STDOUT_FILENO);
        dup2(pipeFds[1], STDERR_FILENO);
        if (pipeFds[1] > STDERR_FILENO)
        {
            close(pipeFds[1]);
        }

        // route stdin to /dev/null
        int devnull = open("/dev/null", O_RDWR, 0);
        if (devnull >= 0)
        {
            dup2(devnull, STDIN_FILENO);
            if (devnull != STDIN_FILENO)
                close(devnull);
        }

        // for extra safety drop root privilege, but first change into the
        // directory containing the image
        if (dirFd != AT_FDCWD)
        {
            if (fchdir(dirFd) < 0)
            {
                fprintf(stderr, "failed to switch into fs image directory "
                        "(%d - %s)\n", errno, strerror(errno));
                _exit(EXIT_FAILURE);
            }

            if (close(dirFd) != 0)
            {
                fprintf(stderr, "failed to close dirfd (%d - %s)\n", errno,
                        strerror(errno));
            }
        }

        setgid(1000);
        setuid(1000);

        // run the e2fsck call
        execvp(e2fsTool, args.data());

        // execve failed? usual cause is due to missing mke2fs tool
        fprintf(stderr, "failed to exec tool (%d - %s)\n", errno, strerror(errno));
        _exit(64);
    }


    // in main process so close the write side of the pipe and wait for the
    // forked child to finish
    if ((pipeFds[1] >= 0) && (close(pipeFds[1]) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close write end of the pipe");
    }

    int status;
    if ((TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) == -1) || !WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("the %s call failed", e2fsTool);
        close(pipeFds[0]);
        return -1;
    }

    // read some data from the pipe and then close it, this is only used for
    // logging debug messages from the tool, the write end should now be closed
    // so the read shouldn't block
    char outputBuf[512];
    if (pipeFds[0] >= 0)
    {
        ssize_t rd = TEMP_FAILURE_RETRY(read(pipeFds[0], outputBuf, sizeof(outputBuf) - 1));
        if (rd < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to read from pipe");
            outputBuf[0] = '\0';
        }
        else
        {
            outputBuf[rd] = '\0';
        }

        // close the read end of the pipe
        if (close(pipeFds[0]) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to close read end of the pipe");
        }
    }
    else
    {
        AI_LOG_SYS_ERROR(errno, "failed to read from pipe");
        outputBuf[0] = '\0';
    }
    
    // chop up the output, assuming each message is delimited by a newline
    if (consoleOutput != nullptr)
    {
        const char deLims[] = "\n\r";
        char *savePtr = nullptr;
        char *line;

        line = strtok_r(outputBuf, deLims, &savePtr);
        while (line)
        {
            consoleOutput->push_back(line);
            line = strtok_r(nullptr, deLims, &savePtr);
        }
    }

    AI_LOG_FN_EXIT();
    return status;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the e2fsck tool on a file system image to check it's integrity
 *
 *  This function does a fork/exec to launch the process, it drops root
 *  privileges and runs the tool as user 1000:1000, therefore the file that is
 *  being checked should be readable and writeble by user 1000.
 *
 *  If this function returns false the image file should probably be deleted /
 *  reformatted.
 *
 *  @param[in]  dirFd           The fd of the directory containing the image to
 *                              check.  The function will switch to this
 *                              directory before dropping permissions (provided
 *                              it's not AT_FCWD).
 *  @param[in]  imageFileName   The name of the file to check.
 *  @param[in]  repair          If true we'll ask the tool to try and repair the
 *                              file if it detects any errors.
 *
 *  @return if the file passes the check (or was successifully repaired) true is
 *  returned, otherwise false.
 */
bool DobbyUtils::checkExtImageFile(int dirFd, const std::string& imageFileName,
                                   bool repair /*= true*/) const
{
    AI_LOG_FN_ENTRY();

    int status;
    std::list<std::string> consoleOutput;

    // run the e2fsck tool
    if (repair)
    {
        status = runE2fsTool(dirFd, &consoleOutput,
                             "e2fsck", "-f", "-p", imageFileName.c_str(),
                             (const char*)nullptr);
    }
    else
    {
        status = runE2fsTool(dirFd, &consoleOutput,
                             "e2fsck", "-f", "-n", imageFileName.c_str(),
                             (const char*)nullptr);
    }

    // check if it failed for weird reasons
    if (status < 0)
    {
        AI_LOG_ERROR_EXIT("failed to run the e2fsck tool");
        return false;
    }

    // quote from the man page:
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

        // print out any error messages, assume they're delimited by newlines
        for (const std::string& line : consoleOutput)
        {
            AI_LOG_ERROR("e2fsck error '%s'", line.c_str());
        }

        AI_LOG_FN_EXIT();
        return false;
    }

    if (WEXITSTATUS(status) & 0x03)
    {
        AI_LOG_WARN("detected some errors in fs image '%s', but they have been "
                    "corrected (probably)", imageFileName.c_str());
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -------------------------------------------------------------------------
/**
 *  @brief Runs the mke2fs tool to format a file system image
 *
 *  This function does a fork/exec to launch the process, it drops root
 *  privileges and runs the tool as user 1000:1000, therefore the file that it's
 *  formatting should be readable and writeble by user 1000.
 *
 *
 *  @param[in]  dirFd           The fd of the directory containing theimage to
 *                              write.  The function will switch to this
 *                              directory before dropping permissions (provided
 *                              it's not AT_FCWD).
 *  @param[in]  imageFileName   The name of the file to format, it must already
 *                              exist.
 *  @param[in]  fsType          The ext version to format the file as, this is
 *                              equivalent to the '-t' option and should be one
 *                              of; 'ext2', 'ext3' or 'ext4'
 *
 *  @return on success returns true on failure false.
 */
bool DobbyUtils::formatExtImageFile(int dirFd, const std::string& imageFileName,
                                    const std::string& fsType /* = "ext4" */) const
{
    AI_LOG_FN_ENTRY();

    // run the mke2fs tool
    std::list<std::string> consoleOutput;
    int status = runE2fsTool(dirFd, &consoleOutput,
                             "mke2fs", "-t", fsType.c_str(), "-F", imageFileName.c_str(),
                             (const char*)nullptr);

    // check if it failed for weird reasons
    if (status < 0)
    {
        AI_LOG_ERROR_EXIT("failed to run the mke2fs tool");
        return false;
    }

    // check the status of the mke2fs call, on failure print out anything we
    // received from the stdout / stderr pipe
    if (!WIFEXITED(status) || (WEXITSTATUS(status) != EXIT_SUCCESS))
    {
        AI_LOG_ERROR("the mke2fs function failed with status %d",
                     WEXITSTATUS(status));

        // print out any error messages
        for (const std::string& line : consoleOutput)
        {
            AI_LOG_ERROR("mke2fs error '%s'", line.c_str());
        }

        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -------------------------------------------------------------------------
/**
 *  @brief Simply writes a string into a file
 *
 *  Not much more to say really.
 *
 *  If the pathname given in @a filePath is relative, then it is interpreted
 *  relative to the directory referred to by the file descriptor @a dirFd,
 *  if @a dirFd is not supplied then it's relative to the cwd.
 *
 *  @param[in]  dirFd           If specified the path should be relative to
 *                              to this directory.
 *  @param[in]  path            The path to file to write to.
 *  @param[in]  flags           Open flags, these will be OR'd with O_WRONLY
 *                              and O_CLOEXEC.
 *  @param[in]  mode            The file access mode to set if O_CREAT was
 *                              specified in flags and the file was created.
 *
 *  @return true on success, false on failure.
 */
bool DobbyUtils::writeTextFileAt(int dirFd, const std::string& path,
                                 const std::string& str,
                                 int flags, mode_t mode /*= 0644*/) const
{
    int fd = openat(dirFd, path.c_str(), O_WRONLY | O_CLOEXEC | flags, mode);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open%s '%s'",
                         (flags & O_CREAT) ? "/create" : "", path.c_str());
        return false;
    }

    const char* dataPtr = str.data();
    size_t remaining = str.size();

    while (remaining > 0)
    {
        ssize_t written = TEMP_FAILURE_RETRY(write(fd, dataPtr, remaining));
        if (written < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to write to file");
            }
        else if (written == 0)
        {
            AI_LOG_ERROR("didn't write any data, odd");
            break;
        }
        else
        {
            if (static_cast<size_t>(written) > remaining)
            {
                AI_LOG_SYS_ERROR(errno, "write returned more bytes than expected");
                break;
            }

            remaining -= static_cast<size_t>(written);
            dataPtr += written;
        }
    }

    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close '%s'", path.c_str());
    }

    return (remaining == 0);
}

bool DobbyUtils::writeTextFile(const std::string& path,
                               const std::string& str,
                               int flags, mode_t mode /*= 0644*/) const
{
    return writeTextFileAt(AT_FDCWD, path, str, flags, mode);
}

// -------------------------------------------------------------------------
/**
 *  @brief Simply read a string from a file
 *
 *  Not much more to say really.
 *
 *  If the pathname given in @a filePath is relative, then it is interpreted
 *  relative to the directory referred to by the file descriptor @a dirFd,
 *  if @a dirFd is not supplied then it's relative to the cwd.
 *
 *  @param[in]  dirFd           If specified the path should be relative to
 *                              to this directory.
 *  @param[in]  path            The path to file to write to.
 *  @param[in]  maxLen          The maximum number of characters to read,
 *                              defaults to 4096.
 *
 *  @return the string read from the file, on failure an empty string.
 */
std::string DobbyUtils::readTextFileAt(int dirFd, const std::string& path,
                                       size_t maxLen /*= 4096*/) const
{
    int fd = openat(dirFd, path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open '%s'", path.c_str());
        return std::string();
    }

    std::string str;
    ssize_t leftToRead = maxLen;
    char buf[256];

    while (leftToRead > 0)
    {
        ssize_t rd = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
        if (rd < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to read from file");
            break;
        }
        else if (rd == 0)
        {
            break;
        }
        else
        {
            str.append(buf, std::min<size_t>(leftToRead, rd));
            leftToRead -= rd;
        }
    }

    // can now close the read side of the pipe
    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close file");
    }

    return str;
}

std::string DobbyUtils::readTextFile(const std::string& path,
                                     size_t maxLen /*= 4096*/) const
{
    return readTextFileAt(AT_FDCWD, path, maxLen);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds a new timer to the timer queue
 *
 *  The @a handler function will be called after the timeout period and then
 *  if @a oneShot is false periodically at the given timeout interval.
 *
 *  The @a handler will be called from the context of the timer queue, bare
 *  in mind for any locking restrictions.
 *
 *  A timer can be cancelled by either calling @a cancelTimer() or returning
 *  false from the handler.  One shot timers are automatically removed after
 *  they are fired, there is not need to call @a cancelTimer() for them.
 *
 *  @param[in]  timeout     The time after which to call the supplied
 *                          handler.
 *  @param[in]  oneShot     If true the timer is automatically removed after
 *                          it times out the first time.
 *  @param[in]  handler     The handler function to call when the timer
 *                          times out.
 *
 *  @return on success returns a (greater than zero) timer id integer which
 *  identifies the timer, on failure -1 is returned.
 */
int DobbyUtils::startTimerImpl(const std::chrono::milliseconds& timeout,
                               bool oneShot,
                               const std::function<bool()>& handler) const
{
    return mTimerQueue->add(timeout, oneShot, handler);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes the given timer from the timer queue
 *
 *  Once this method returns (successfully) you are guaranteed that the
 *  timer handler will not be called, i.e. this is synchronisation point.
 *
 *  This method will fail if called from the context of a timer handler, if
 *  you want to cancel a repeating timer then just return false in the
 *  handler.
 *
 *  @param[in]  timerId     The id of the timer to cancel as returned by the
 *                          startTimer() method.
 *
 *  @return true if the timer was found and was removed from the queue,
 *  otherwise false
 */
bool DobbyUtils::cancelTimer(int timerId) const
{
    return mTimerQueue->remove(timerId);
}

// -------------------------------------------------------------------------
/**
 *  @brief Returns the major number assigned to a given driver
 *
 *  This function tries to find the major number assigned to a given driver,
 *  it does this by parsing the /proc/devices file.
 *
 *  @warning Currently this function doesn't work for 'misc' devices, which
 *  are devices listed by /proc/misc.
 *
 *  @param[in]  driverName  The name of the driver to get the major number for.
 *
 *  @return if found the major number is returned, if not found then 0 is
 *  returned.
 */
unsigned int DobbyUtils::getDriverMajorNumber(const std::string &driverName) const
{
    std::lock_guard<std::mutex> locker(mMajorNumberLock);

    // check if we already have the driver major in the cache
    std::map<std::string, unsigned int>::const_iterator it =
        mMajorNumberCache.find(driverName);
    if (it != mMajorNumberCache.end())
    {
        return it->second;
    }

    // not in the cache so lookup in the /proc/devices file
    int fd = open("/proc/devices", O_CLOEXEC | O_RDONLY);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open '/proc/devices'");
        return 0;
    }

    __gnu_cxx::stdio_filebuf<char> fileBuf(fd, std::ios::in);
    std::istream fileStream(&fileBuf);

    std::string line;
    while (std::getline(fileStream, line))
    {
        unsigned int major;
        char name[65];

        if (sscanf(line.c_str(), "%3u %64s", &major, name) == 2)
        {
            if ((driverName == name) && (major > 0) && (major < 1024))
            {
                // cache the name vs major number to speed up look-ups
                if (mMajorNumberCache.size() < 100)
                {
                    mMajorNumberCache[driverName] = major;
                }

                return major;
            }
        }
    }

    AI_LOG_WARN("failed to get major number for driver '%s'", driverName.c_str());
    return 0;
}

// -------------------------------------------------------------------------
/**
 *  @brief Returns true if the given device is allowed in the container
 *
 *  This is here for security reasons as I didn't want just any device added
 *  to the container whitelist.  If we trust the code on the other end of
 *  Dobby that is creating the containers then this is not needed, but just
 *  in case that got hacked I didn't want people to create containers
 *  enabling access to CDI / system device nodes.
 *
 *  @warning This method doesn't take into account drivers being insmod /
 *  rmmod and the re-use of major numbers, however if a user could do that
 *  then this check is the least of our problems.
 *
 *  @param[in]  device      The device number.
 *
 *  @return true if the device is allowed, otherwise false.
 */
bool DobbyUtils::deviceAllowed(dev_t device) const
{
    return (mDeviceWhitelist.count(device) != 0);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Builds the whitelist of allowed device numbers
 *
 *  At the moment only the following devices are added:
 *      hidraw, 0 - 100
 *      input, 64 - 95
 *
 *  The list is exclusive of the opengl / graphics device nodes and the set of
 *  'standard' device nodes.
 *
 */
void DobbyUtils::buildDeviceWhitelist()
{
    // add the hidraw devices
    const unsigned int hidrawMajor = getDriverMajorNumber("hidraw");
    if (hidrawMajor == 0)
    {
        AI_LOG_WARN("failed to get major number of hidraw device");
    }
    else
    {
        for (unsigned int minor = 0; minor < 100; minor++)
        {
            mDeviceWhitelist.insert(makedev(hidrawMajor, minor));
        }
    }

    // add the input device(s), their major number is fixed at 13 with minor
    // numbers in different ranges, see https://www.kernel.org/doc/Documentation/input/input.txt
    const unsigned int inputMajor = 13;
    for (unsigned int minor = 64; minor < 96; minor++)
    {
        mDeviceWhitelist.insert(makedev(inputMajor, minor));
    }
}


// -------------------------------------------------------------------------
/**
 *  @brief Sets / Gets integer meta data for the given container.
 *
 *  You can use this to share meta data about the container across different
 *  plugins.  For example if network namespaces are enabled.
 *
 *  The data is cleared automatically when the container is shutdown.
 */
void DobbyUtils::setIntegerMetaData(const ContainerId &id,
                                    const std::string &key,
                                    int value)
{
    std::lock_guard<std::mutex> locker(mMetaDataLock);
    mIntegerMetaData[std::make_pair(id, key)] = value;
}

int DobbyUtils::getIntegerMetaData(const ContainerId &id,
                                   const std::string &key,
                                   int defaultValue) const
{
    std::lock_guard<std::mutex> locker(mMetaDataLock);
    auto it = mIntegerMetaData.find(std::make_pair(id, key));
    if (it == mIntegerMetaData.end())
        return defaultValue;
    else
        return it->second;
}

// -------------------------------------------------------------------------
/**
 *  @brief Sets / Gets string meta data for the given container.
 *
 *  You can use this to share meta data about the container across different
 *  plugins.  For example the ip address assigned to the container.
 *
 *  The data is cleared automatically when the container is shutdown.
 */
void DobbyUtils::setStringMetaData(const ContainerId &id,
                                   const std::string &key,
                                   const std::string &value)
{
    std::lock_guard<std::mutex> locker(mMetaDataLock);
    mStringMetaData[std::make_pair(id, key)] = value;
}

std::string DobbyUtils::getStringMetaData(const ContainerId &id,
                                          const std::string &key,
                                          const std::string &defaultValue) const
{
    std::lock_guard<std::mutex> locker(mMetaDataLock);
    auto it = mStringMetaData.find(std::make_pair(id, key));
    if (it == mStringMetaData.end())
        return defaultValue;
    else
        return it->second;
}

// -------------------------------------------------------------------------
/**
 *  @brief Clears all the meta data stored for a given container.
 *
 *  This is called by the DobbyManager when a container is starting or has just
 *  stopped.
 */
void DobbyUtils::clearContainerMetaData(const ContainerId &id)
{
    std::lock_guard<std::mutex> locker(mMetaDataLock);

    {
        auto it = mStringMetaData.begin();
        while (it != mStringMetaData.end())
        {
            if (it->first.first == id)
                it = mStringMetaData.erase(it);
            else
                ++it;
        }
    }

    {
        auto it = mIntegerMetaData.begin();
        while (it != mIntegerMetaData.end())
        {
            if (it->first.first == id)
                it = mIntegerMetaData.erase(it);
            else
                ++it;
        }
    }
}

// -------------------------------------------------------------------------
/**
 *  @brief Inserts the given ebtables rule to the existing set.
 *
 *  This doesn't flush out any old rules, it just adds the new one at
 *  the beginning of the table.
 *
 *  @param[in]  args  The args of one rule to add.
 *
 *  @return true if the rule was added, otherwise false.
 */
bool DobbyUtils::insertEbtablesRule(const std::string &args) const
{
    // TODO: replace with library call rather than using ebtables tool
    return executeCommand("ebtables -I " + args);
}

// -------------------------------------------------------------------------
/**
 *  @brief Deletes the given ebtables rule from the existing set.
 *
 *  This only performs a delete, if the a rule is not
 *  currently installed then false is returned
 *
 *  @param[in]  args     The set of one rule to remove.
 *
 *  @return true if the rules were removed, otherwise false.
 */
bool DobbyUtils::deleteEbtablesRule(const std::string &args) const
{
    // TODO: replace with library call rather than using ebtables tool
    return executeCommand("ebtables -D " + args);
}

bool DobbyUtils::executeCommand(const std::string &command) const
{
    std::string noOutputCommand = command + " &> /dev/null";

    FILE* pipe = popen(noOutputCommand.c_str(), "re");
    if (!pipe)
    {
        AI_LOG_SYS_ERROR(errno, "popen failed");
        return false;
    }

    int returnCode = pclose(pipe);
    if (returnCode < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to exec command `%s`",
                         noOutputCommand.c_str());
        return false;
    }
    else if (returnCode > 0)
    {
        AI_LOG_ERROR("failed to exec command `%s`, command returned code %d",
                     noOutputCommand.c_str(), returnCode);
        return false;
    }

    return true;
}
// -------------------------------------------------------------------------
/**
 *  @brief Returns the effective GID or UID for the given PID 
 *         by parsing /proc/<PID>/status
 *
 *  @param[in]  pid    The PID of the process to get the GID for
 *  @param[in]  idType The type of ID to get, either "Gid" or "Uid"
 *
 *  @return the GID or UID of the process, or -1 if the ID could not be found
 */
int DobbyUtils::getGIDorUID(pid_t pid, const std::string& idType) const
{
    char filePathBuf[32];
    sprintf(filePathBuf, "/proc/%d/status", pid);

    int fd = open(filePathBuf, O_CLOEXEC | O_RDONLY);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open procfs file @ '%s'", filePathBuf);
        return -1;
    }

    __gnu_cxx::stdio_filebuf<char> statusFileBuf(fd, std::ios::in);
    std::istream statusFileStream(&statusFileBuf);
    
    std::string line;
    while (std::getline(statusFileStream, line))
    {
        if (line.compare(0, idType.size(), idType) == 0)
        {
            int realId = -1, effectiveId = -1, savedId = 1, filesystemId = 1;

            if (sscanf(line.c_str(), (idType + ":\t%d\t%d\t%d\t%d").c_str(), &realId, &effectiveId, &savedId, &filesystemId) != 4)
            {
                AI_LOG_WARN("failed to parse %s field, '%s'", idType.c_str(), line.c_str());
                return -1;
            }

            return effectiveId;
        }
    }

    AI_LOG_WARN("failed to find the %s field in the '%s' file", idType.c_str(), filePathBuf);
    return -1;
}
// -------------------------------------------------------------------------
/**
 *  @brief Returns the GID for the given PID
 *
 *  @param[in]  pid    The PID of the process to get the GID for
 *
 *  @return the GID of the process, or -1 if the GID could not be found
 */
gid_t DobbyUtils::getGID(pid_t pid) const
{
    return static_cast<gid_t>(getGIDorUID(pid, "Gid"));
}
// -------------------------------------------------------------------------
/**
 *  @brief Returns the UID for the given PID
 *
 *  @param[in]  pid    The PID of the process to get the UID for
 *
 *  @return the UID of the process, or -1 if the UID could not be found
 */
uid_t DobbyUtils::getUID(pid_t pid) const
{
    return static_cast<uid_t>(getGIDorUID(pid, "Uid"));
}
