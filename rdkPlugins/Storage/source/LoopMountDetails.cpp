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

#include "LoopMountDetails.h"
#include "StorageHelper.h"
#include "DobbyRdkPluginUtils.h"


#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sstream>
#include <fstream>
#include <dirent.h>


#if defined(__linux__)
#include <linux/loop.h>
#endif


// Loop mount details class
LoopMountDetails::LoopMountDetails(const std::string& rootfsPath,
                                   const LoopMount& mount,
                                   const uid_t& userId,
                                   const gid_t& groupId,
                                   const std::shared_ptr<DobbyRdkPluginUtils> &utils)
    : mMount(mount),
    mUserId(userId),
    mGroupId(groupId),
    mUtils(utils)
{
    AI_LOG_FN_ENTRY();

    mMountPointOutsideContainer = rootfsPath + mount.destination;
    mTempMountPointOutsideContainer = mMountPointOutsideContainer + ".temp";

    AI_LOG_FN_EXIT();
}

LoopMountDetails::~LoopMountDetails()
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Opens the data.img file and mounts it to temp location inside container
 *
 *  @return true on success, false on failure.
 */
bool LoopMountDetails::onPreCreate()
{
    AI_LOG_FN_ENTRY();
    bool success = false;

    // step 1 - Create image file if not yet there
    if (!StorageHelper::createFileIfNeeded(mMount.fsImagePath,
                                            mMount.imgSize,
                                            mUserId,
                                            mMount.fsImageType))
    {
        // logging already provided by createFileIfNeeded
        return false;
    }

    // step 2 - try and open the source file and attach to a space loop device
    std::string loopDevice;
    int loopDevFd = StorageHelper::attachLoopDevice(mMount.fsImagePath, &loopDevice);
    if ((loopDevFd < 0) || (loopDevice.empty()))
    {
        AI_LOG_ERROR("failed to attach file to loop device");
        return false;
    }

    // step 3 - do the loop mount in a temporary location within the rootfs
    success = doLoopMount(loopDevice);

    // step 4 - close the loop device, if the mount succeeded then the file
    // will remain associated with the loop device until it's unmounted
    if (close(loopDevFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close loop device dir");
        return false;
    }

    return success;

    AI_LOG_FN_EXIT();
}


// -----------------------------------------------------------------------------
/**
 *  @brief Performs the loop mount, this should be called prior to the container
 *  being runned.
 *
 *  @param[in]  loopDevice      The loop device to mount.
 *
 *  @return true on success, false on failure.
 */
bool LoopMountDetails::doLoopMount(const std::string& loopDevice)
{
    AI_LOG_FN_ENTRY();

    bool status = false;

    // step 1 - create directories within the rootfs
    if (!DobbyRdkPluginUtils::mkdirRecursive(mTempMountPointOutsideContainer, 0700))
    {
        // logging already provided by mkdirRecursive
        return false;
    }
    else if (!DobbyRdkPluginUtils::mkdirRecursive(mMountPointOutsideContainer, 0700))
    {
        // logging already provided by mkdirRecursive
        return false;
    }

    // step 2 - create the mount options data string, which is just the
    // options separated by a comma (,)
    std::string mountData;
    std::list<std::string>::const_iterator it = mMount.mountOptions.begin();
    for (; it != mMount.mountOptions.end(); ++it)
    {
        if (it != mMount.mountOptions.begin())
            mountData += ",";

        mountData += *it;
    }

    // step 3 - mount loop device to temporary location
    if (mount(loopDevice.c_str(), mTempMountPointOutsideContainer.c_str(),
            mMount.fsImageType.c_str(), mMount.mountFlags, mountData.data()) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to mount '%s' @ '%s'",
                        loopDevice.c_str(), mTempMountPointOutsideContainer.c_str());
    }
    else
    {
        status = true;
    }

    // We should always clean anything out of the lost+found directory
    // of the loopback mounts, if we don't, cruft could build up
    // in there and consume all the available space for the apps
    StorageHelper::cleanMountLostAndFound(mTempMountPointOutsideContainer, "");

    AI_LOG_FN_EXIT();

    return status;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets permissions for container to access directories
 *
 *  @return true on success, false on failure.
 */
bool LoopMountDetails::setPermissions()
{
    AI_LOG_FN_ENTRY();

    bool success = false;

    // We need to check the permissions on the image root dir, they
    // should allow full read/write by the user inside the container.
    // In an ideal world we wouldn't do this here, instead when the fs
    // data.img is created it should be passed '-E root_owner=uid:gid',
    // however currently our version of mke2fs doesn't support that.
    if (mUserId != 0 && mGroupId != 0)
    {
        // proper value of mUserId and mGroupId
        if (chown(mTempMountPointOutsideContainer.c_str(), mUserId, mGroupId) != 0)
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "failed to chown '%s' to %u:%u",
                                mTempMountPointOutsideContainer.c_str(),
                                mUserId, mGroupId);
        }
        else
        {
            success = true;
        }
    }
    else
    {
        AI_LOG_WARN("Config does not contain proper ID/GID to set file permissions");

        // config.json has not set mUserId/mGroupId so give access to everyone
        // so the container could use this mount point
        if (chmod(mTempMountPointOutsideContainer.c_str(), 0777) != 0)
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "failed to set dir '%s' perms to 0%03o",
                                mTempMountPointOutsideContainer.c_str(), 0777);
        }
        else
        {
            success = true;
        }
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Mounts temp directory into desired one, then cleans old files
 *
 *  @return true on success, false on failure.
 */
bool LoopMountDetails::remountTempDirectory()
{
    AI_LOG_FN_ENTRY();

    bool success = false;

    if (mount(mTempMountPointOutsideContainer.c_str(),
                mMountPointOutsideContainer.c_str(),
                "", MS_BIND, nullptr) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to bind mount '%s' -> '%s'",
                            mTempMountPointOutsideContainer.c_str(),
                            mMountPointOutsideContainer.c_str());
    }
    else
    {
        success = true;
    }

    AI_LOG_FN_EXIT();
    return success;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Cleans up temp mount and directory
 *
 *  @return true on success, false on failure.
 */
bool LoopMountDetails::cleanupTempDirectory()
{
    AI_LOG_FN_ENTRY();

    bool success = false;

    // now that the namespaces have forked off, unmount this from outside of the
    // container namespace won't affect it's mounting inside the container.
    if (umount2(mTempMountPointOutsideContainer.c_str(), UMOUNT_NOFOLLOW) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to unmount '%s'",
                         mTempMountPointOutsideContainer.c_str());
    }
    else
    {
        AI_LOG_DEBUG("unmounted temp loop mount @ '%s', now deleting mount point",
                    mTempMountPointOutsideContainer.c_str());

        // can now delete the temporary mount point
        if (rmdir(mTempMountPointOutsideContainer.c_str()) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to delete temp mount point @ '%s'",
                             mTempMountPointOutsideContainer.c_str());
        }
        else
        {
            success = true;
        }
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Checks if image should be non persistent and if so remove it.
 *
 *  @return true on success, false on failure.
 */
bool LoopMountDetails::removeNonPersistentImage()
{
    AI_LOG_FN_ENTRY();

    bool success = true;

    if (!mMount.persistent)
    {
        if (unlink(mMount.fsImagePath.c_str()))
        {
            AI_LOG_SYS_ERROR(errno, "failed to delete image file @ '%s'",
                             mMount.fsImagePath.c_str());
            success = false;
        }
        else
        {
            AI_LOG_DEBUG("Unlinked successfully @ '%s'", mMount.fsImagePath.c_str());
        }
    }

    AI_LOG_FN_EXIT();
    return success;
}
