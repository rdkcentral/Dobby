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
 * File:   DobbyRootfs.cpp
 *
 */
#include "DobbyRootfs.h"
#include "DobbyBundle.h"

#include "IDobbyUtils.h"

#include <Logging.h>
#include <FileUtilities.h>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>


#if defined(LEGACY_COMPONENTS)
// -----------------------------------------------------------------------------
/**
 *  @brief Constructor that creates the rootfs for a container.
 *
 *  @param[in]  utils       The daemon utils object.
 *  @param[in]  bundle      An instance of DobbyBundle
 *  @param[in]  config      An instance of DobbySpecConfig
 */
DobbyRootfs::DobbyRootfs(const std::shared_ptr<IDobbyUtils>& utils,
                         const std::shared_ptr<const DobbyBundle>& bundle,
                         const std::shared_ptr<const DobbySpecConfig>& config)
    : mUtilities(utils)
    , mBundle(bundle)
    , mDirFd(-1)
    , mPersist(false)
{
    AI_LOG_FN_ENTRY();

    if (!mBundle || !mBundle->isValid())
    {
        AI_LOG_ERROR_EXIT("invalid bundle");
        return;
    }

    const std::string dirName(config->rootfsPath());

    // create the directory as a subdirectory in the bundle
    if (mkdirat(bundle->dirFd(), dirName.c_str(), 0755) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create rootfs directory @ '%s'",
                              dirName.c_str());
        return;
    }

    // try and open the new directory
    mDirFd = openat(bundle->dirFd(), dirName.c_str(), O_CLOEXEC | O_DIRECTORY);
    if (mDirFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open rootfs directory @ '%s'",
                              dirName.c_str());
        cleanUp();
        return;
    }

    // and finally construct the rootfs contents based on the config
    if (!constructRootfs(mDirFd, config))
    {
        AI_LOG_ERROR_EXIT("failed to construct bundle rootfs");
        cleanUp();
        return;
    }

    // store the complete path
    mPath = bundle->path() + "/" + dirName + "/";

    AI_LOG_FN_EXIT();
}
#endif //defined(LEGACY_COMPONENTS)

// -----------------------------------------------------------------------------
/**
 *  @brief Constructor that populates the object's member variables required
 *  internally by Dobby.
 *
 *  @param[in]  utils       The daemon utils object.
 *  @param[in]  bundle      An instance of DobbyBundle
 *  @param[in]  config      An instance of DobbyBundleConfig
 */
DobbyRootfs::DobbyRootfs(const std::shared_ptr<IDobbyUtils>& utils,
                         const std::shared_ptr<const DobbyBundle>& bundle,
                         const std::shared_ptr<const DobbyBundleConfig>& config)
    : mUtilities(utils)
    , mBundle(bundle)
    , mDirFd(-1)
    , mPersist(false)
{
    AI_LOG_FN_ENTRY();

    if (!mBundle || !mBundle->isValid())
    {
        AI_LOG_ERROR_EXIT("invalid bundle");
        return;
    }

    if (config->rootfsPath().empty())
    {
        AI_LOG_ERROR_EXIT("invalid rootfs");
        return;
    }

    std::string rootfsDirPath;

    // absolute path to rootfs
    if (config->rootfsPath().front() == '/')
    {
        rootfsDirPath = config->rootfsPath() + "/";
    }
    // relative path to rootfs
    else
    {
        rootfsDirPath = bundle->path() + "/" + config->rootfsPath() + "/";
    }

    // check that rootfs exists in bundle
    int dirFd = open(rootfsDirPath.c_str(), O_CLOEXEC | O_DIRECTORY);
    if (dirFd == -1)
    {
        if (errno == ENOENT)
        {
            AI_LOG_ERROR_EXIT("could not find rootfs at %s", rootfsDirPath.c_str());
            return;
        }
        else
        {
            AI_LOG_SYS_ERROR(errno, "failed to open rootfs directory '%s'", rootfsDirPath.c_str());
            return;
        }
    }
    else
    {
        mDirFd = dirFd;
    }

    // store the complete path
    mPath = std::move(rootfsDirPath);

    AI_LOG_FN_EXIT();
}

DobbyRootfs::~DobbyRootfs()
{
    cleanUp();
}

bool DobbyRootfs::isValid() const
{
    return (mDirFd >= 0) && !mPath.empty();
}

const std::string& DobbyRootfs::path() const
{
    return mPath;
}

int DobbyRootfs::dirFd() const
{
    return mDirFd;
}

void DobbyRootfs::setPersistence(bool persist)
{
    mPersist = persist;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Unmounts anything mounted at the given path prefix
 *
 *
 *  @param[in]  pathPrefix      The path prefix to check against the mount
 *                              points.
 */
void DobbyRootfs::unmountAllAt(const std::string& pathPrefix)
{
    AI_LOG_FN_ENTRY();

    // open /proc/<pid>/mountinfo rather than /proc/mounts as we also want to
    // unmount any bind mounts
    int fd = open("/proc/self/mountinfo", O_CLOEXEC | O_RDONLY);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open '/proc/self/mountinfo'");
        return;
    }

    FILE* fp = fdopen(fd, "r");
    if (fp == nullptr)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "fdopen failed");
        return;
    }

    char *line = nullptr;
    size_t len = 0;
    ssize_t rd;

    // process each line of the file, we are looking for any mount point that
    // is prefixed with the above path
    char mntRoot[256 + 1] = {0};
    char mntPoint[256 + 1] = {0};
    while ((rd = getline(&line, &len, fp)) >= 0)
    {
        if ((line == nullptr) || (rd < 8))
            continue;

        // ensure the line is null terminated
        line[rd] = '\0';

        // process the individual mount line
        int mntId, parentMntId;
        unsigned devMajor, devMinor;
        if (sscanf(line, "%i %i %u:%u %256s %256s", &mntId, &parentMntId,
                                                    &devMajor, &devMinor,
                                                    mntRoot, mntPoint) != 6)
        {
            AI_LOG_WARN("failed to parse mountinfo line '%s'", line);
            continue;
        }

        // if the mount point is within the rootfs path then unmount it
        if (pathPrefix.compare(0, std::string::npos, mntPoint, pathPrefix.length()) == 0)
        {
            AI_LOG_ERROR("found mount left in the container rootfs @ '%s', this"
                         " should to be fixed", mntPoint);

            if (umount2(mntPoint, UMOUNT_NOFOLLOW) != 0)
            {
                AI_LOG_SYS_ERROR(errno, "failed to unmount '%s'", mntPoint);
            }
        }
    }

    if (line != nullptr)
    {
        free(line);
    }

    fclose(fp);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes the rootfs directory and all it's contents if set persistence
 *  set to false
 *
 */
void DobbyRootfs::cleanUp()
{
    AI_LOG_FN_ENTRY();

    // before blindly doing a recursive delete of the directory make sure that
    // nothing is mounted there.  This is to fix any bugs / sloppy plugins
    // that do things like bind mounts inside the rootfs and then don't clean
    // up after themselves
    if (!mPath.empty())
    {
        unmountAllAt(mPath);
    }

    if (mDirFd >= 0)
    {
        if (!mPersist)
        {
            if (!mUtilities->rmdirContents(mDirFd))
            {
                AI_LOG_ERROR("failed to delete contents of rootfs dir");

                // TODO: if something has gone wrong somewhere then it may not be
                // possible to delete the rootfs dir as it could contain mount
                // points ... one solution to this is to iterate through current
                // mounts and umount anything in the rootfs and then try deleting
                // again
            }

            // the rootfs directory should now be empty, so can now delete it
            if (!mPath.empty() && (rmdir(mPath.c_str()) != 0))
            {
                AI_LOG_SYS_ERROR(errno, "failed to delete rootfs dir");
            }
        }

        if (close(mDirFd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to close rootfs dir");
        }

        mDirFd = -1;
    }

    mPath.clear();

    AI_LOG_FN_EXIT();
}

#if defined(LEGACY_COMPONENTS)
// -----------------------------------------------------------------------------
/**
 *  @brief Utility method that simply creates a file at the given path and
 *  writes a string to it.
 *
 *  If the file already exists, it is truncated before writing the new contents.
 *
 *  If the pathname given in @a filePath is relative, then it is interpreted
 *  relative to the directory referred to by the file descriptor @a dirFd_.
 *  If @a filePath is relative and dirfd is the special value @a AT_FDCWD, then
 *  @a filePath is interpreted relative to the current working directory.
 *
 *  @param[in]  dirFd_          The directory to create the file in (see above)
 *  @param[in]  filePath        The path to the file to create
 *  @param[in]  fileContents    The string to write into the file
 *  @param[in]  mode            The access mode to give the file (if created)
 *
 *  @return true on success, false on failure.
 */
bool DobbyRootfs::createAndWriteFileAt(int dirFd_,
                                       const std::string& filePath,
                                       const std::string& fileContents,
                                       mode_t mode /*= 0644*/) const
{
    AI_LOG_FN_ENTRY();

    const int flags = O_CLOEXEC | O_CREAT | O_WRONLY | O_TRUNC;

    int fd = openat(dirFd_, filePath.c_str(), flags, mode);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open/create file '%s'",
                              filePath.c_str());
        return false;
    }

    size_t n = fileContents.length();
    const char* s = fileContents.c_str();

    while (n > 0)
    {
        ssize_t written = TEMP_FAILURE_RETRY(write(fd, s, n));
        if (written < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to write to file");
            break;
        }
        else if (written == 0)
        {
            break;
        }

        s += written;
        n -= written;
    }

    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close '%s'", filePath.c_str());
    }

    AI_LOG_FN_EXIT();
    return (n == 0);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates the mount point as either a file or directory in the rootfs.
 *
 *  All directories created are given perms 0755 and all files 0644.
 *
 *  @param[in]  dirFd          The fd for the rootfs directory.
 *  @param[in]  path           The path to the mount point.
 *  @param[in]  isDirectory    If true creates a directory, if false a file is
 *                             created.
 *
 *  @returns true on success, false on failure.
 */
bool DobbyRootfs::createMountPoint(int dirfd, const std::string &path,
                                   bool isDirectory) const
{
    // remove leading slashes if present
    size_t n = 0;
    while ((n < path.size()) && (path[n] == '/'))
        n++;

    std::string relativePath = path.substr(n);
    if (relativePath.empty())
    {
        AI_LOG_ERROR("empty relative path '%s'", path.c_str());
        return false;
    }


    // divide into components and if none then nothing to do
    std::vector<std::string> components = AICommon::splitPath(relativePath);
    auto it = components.begin();
    while (it != components.end())
    {
        if (*it == ".")
        {
            it = components.erase(it);
        }
        else if (*it == "..")
        {
            AI_LOG_ERROR("mount path is not allowed to have \"..\" in it ('%s')",
                         path.c_str());
            return false;
        }
        else
        {
            ++it;
        }
    }

    if (components.empty())
    {
        return true;
    }

    // remove the last component which is the file / directory
    components.pop_back();

    // create all the leading dirs if required
    std::string prefix;
    for (const std::string &leadingDir : components)
    {
        prefix += leadingDir;

        AI_LOG_DEBUG("checking / creating leading dir @ '%s'", prefix.c_str());

        if (mkdirat(dirfd, prefix.c_str(), 0755) != 0)
        {
            if (errno != EEXIST)
            {
                AI_LOG_SYS_ERROR(errno, "failed to create dir '%s' in rootfs",
                                 prefix.c_str());
                return false;
            }
        }

        prefix += '/';
    }

    // finally create the last component of the path
    if (isDirectory)
    {
        if (mkdirat(dirfd, relativePath.c_str(), 0755) != 0)
        {
            if (errno != EEXIST)
            {
                AI_LOG_SYS_ERROR(errno, "failed to mkdir @ '%s'",
                                 relativePath.c_str());
                return false;
            }
        }
    }
    else
    {
        int fd = openat(dirfd, relativePath.c_str(),
                        O_CLOEXEC | O_CREAT | O_WRONLY | O_TRUNC, 0644);
        if (fd < 0)
        {
            if (errno != EEXIST)
            {
                AI_LOG_SYS_ERROR(errno, "failed to create file @ '%s'",
                                 relativePath.c_str());
                return false;
            }
        }
        else if (close(fd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to close file @ '%s'", path.c_str());
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the standard mount points as defined in the runc template
 *  json.
 *
 *
 *  @param[in]  dirFd          The fd for the rootfs directory
 *
 */
bool DobbyRootfs::createStandardMountPoints(int dirfd) const
{
    static const std::list<std::string> stdMountPoints =
        {
            "/proc",
            "/tmp",
            "/dev",
            "/sys",
            "/sys/fs/cgroup",
            "/lib",
#if defined(DEV_VM)
            "/lib64",
#endif
            "/bin",
            "/sbin",
            "/usr",
        };

    for (const std::string &mountPoint : stdMountPoints)
    {
        if (!createMountPoint(dirfd, mountPoint, true))
        {
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the rootfs directory in the bundle and populates it with
 *  the necessary /etc files
 *
 *  The following is the default layout of the directory created:
 *
 *     ├── bin
 *     ├── dev
 *     ├── etc
 *     │   ├── group
 *     │   ├── hosts
 *     │   ├── ld.so.preload
 *     │   ├── nsswitch.conf
 *     │   ├── passwd
 *     │   ├── resolv.conf
 *     │   └── services
 *     ├── home
 *     │   └── private
 *     ├── lib
 *     ├── opt
 *     │   └── libexec
 *     ├── proc
 *     ├── sys
 *     │   └── fs
 *     │       └── cgroup
 *     ├── tmp
 *     └── usr
 *
 *
 *  Where the following files have the default contents:
 *
 *   /etc/nsswitch.conf      "hosts:     files mdns4_minimal [NOTFOUND=return] dns mdns4\n"
 *                           "protocols: files\n"
 *
 *   /etc/passwd             "root::0:0:root:/root:/bin/false\n"
 *
 *   /etc/hosts              "127.0.0.1\tlocalhost\n"
 *
 *  The rest of the etc files are empty.
 *
 *
 */
bool DobbyRootfs::constructRootfs(int dirfd,
                                  const std::shared_ptr<const DobbySpecConfig>& config)
{
    AI_LOG_FN_ENTRY();

    // create standard mount points (as defined in the container template)
    if (!createStandardMountPoints(dirfd))
    {
        AI_LOG_ERROR_EXIT("failed to create standard mount points in rootfs");
        return false;
    }

    // create a home directory
    if (mkdirat(dirfd, "home", 0755) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create dir @ '%shome'",
                              mPath.c_str());
        return false;
    }

    // create a home/private directory (this is only needed because we have
    // traditionally set the HOME env var inside a container to /home/private)
    if (mkdirat(dirfd, "home/private", 0755) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create dir @ '%shome/private'",
                              mPath.c_str());
        return false;
    }

    // create the rootfs/etc directory
    if (mkdirat(dirfd, "etc", 0755) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create etc directory @ '%setc'",
                              mPath.c_str());
        return false;
    }

    // create the rootfs/etc/ssl directory
    if (mkdirat(dirfd, "etc/ssl", 0755) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create etc/ssl directory @ '%setc/ssl'",
                              mPath.c_str());
        return false;
    }

    // create the rootfs/etc/ssl/certs directory
    if (mkdirat(dirfd, "etc/ssl/certs", 0755) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create etc/ssl/certs directory @ '%setc/ssl/certs'",
                              mPath.c_str());
        return false;
    }


    // write all the etc files as specified in the config
    if (!createAndWriteFileAt(dirfd, "etc/group",         config->etcGroup()) ||
        !createAndWriteFileAt(dirfd, "etc/passwd",        config->etcPasswd()) ||
        !createAndWriteFileAt(dirfd, "etc/hosts",         config->etcHosts()) ||
        !createAndWriteFileAt(dirfd, "etc/services",      config->etcServices()) ||
        !createAndWriteFileAt(dirfd, "etc/ld.so.preload", config->etcLdSoPreload()))
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    // write the static /etc/nsswitch.conf file (this is the default config as
    // read from /etc/nsswitch.conf in the rootfs)
    static const std::string nsswitchConf("hosts:     files mdns4_minimal [NOTFOUND=return] dns mdns4\n"
                                          "protocols: files\n");
    if (!createAndWriteFileAt(dirfd, "etc/nsswitch.conf", nsswitchConf))
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    // write empty /etc/resolv.conf file in case we want to mount it from the host
    if (!createAndWriteFileAt(dirfd, "etc/resolv.conf", std::string()))
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    // process any extra mounts added by the client
    const std::vector<DobbySpecConfig::MountPoint> extraMounts = config->mountPoints();
    for (const DobbySpecConfig::MountPoint &mountPoint : extraMounts)
    {
        AI_LOG_DEBUG("attempting to create mount point '%s' %s",
                     mountPoint.destination.c_str(),
                     (mountPoint.type == DobbySpecConfig::MountPoint::Directory) ?
                     "directory" : "file");

        if (!createMountPoint(dirfd, mountPoint.destination,
                             (mountPoint.type == DobbySpecConfig::MountPoint::Directory)))
        {
            AI_LOG_FN_EXIT();
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}
#endif //defined(LEGACY_COMPONENTS)
