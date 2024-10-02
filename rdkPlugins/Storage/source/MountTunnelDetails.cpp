/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2022 Sky UK
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

#include "MountTunnelDetails.h"
#include "StorageHelper.h"
#include "DobbyRdkPluginUtils.h"
#include "RefCountFile.h"
#include "RefCountFileLock.h"

#include <fstream>
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

// Mount tunnel details class
MountTunnelDetails::MountTunnelDetails(const std::string& rootfsPath,
                                   const MountTunnelProperties& mount,
                                   const uid_t& userId,
                                   const gid_t& groupId,
                                   const std::shared_ptr<DobbyRdkPluginUtils> &utils)
    : mMount(mount),
    mUserId(userId),
    mGroupId(groupId),
    mUtils(utils)
{
    AI_LOG_FN_ENTRY();

    mMountPointInsideContainer = rootfsPath + mount.destination;
    mTempMountPointOutsideContainer = mount.source;

    AI_LOG_FN_EXIT();
}

MountTunnelDetails::~MountTunnelDetails()
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
bool MountTunnelDetails::onPreCreate()
{
    AI_LOG_FN_ENTRY();

    if (!DobbyRdkPluginUtils::mkdirRecursive(mTempMountPointOutsideContainer, 0755))
    {
        AI_LOG_WARN("failed to create dir '%s'", mTempMountPointOutsideContainer.c_str());
        return false;
    }

    if (!DobbyRdkPluginUtils::mkdirRecursive(mMountPointInsideContainer, 0755))
    {
        AI_LOG_WARN("failed to create dir '%s'", mMountPointInsideContainer.c_str());
        return false;
    }

    if(mount(mTempMountPointOutsideContainer.c_str(), mTempMountPointOutsideContainer.c_str(), NULL, MS_BIND, NULL) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to bind mount '%s'", mTempMountPointOutsideContainer.c_str());
        return false;
    }

    if(mount(NULL, mTempMountPointOutsideContainer.c_str(), NULL, MS_PRIVATE, NULL) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to mount MS_PRIVATE @ '%s'", mTempMountPointOutsideContainer.c_str());
        return false;
    }

    if(mount(NULL, mTempMountPointOutsideContainer.c_str(), NULL, MS_SHARED, NULL) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to mount MS_SHARED @ '%s'", mTempMountPointOutsideContainer.c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets permissions for container to access directories
 *
 *  @return true on success, false on failure.
 */
bool MountTunnelDetails::setPermissions()
{
    AI_LOG_FN_ENTRY();

    bool success = true;
#if 0
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
#endif
    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Mounts temp directory into desired one
 *
 *  @return true on success, false on failure.
 */
bool MountTunnelDetails::remountTempDirectory()
{
    AI_LOG_FN_ENTRY();

    bool success = false;

    if (mount(mTempMountPointOutsideContainer.c_str(),
                mMountPointInsideContainer.c_str(),
                "", MS_BIND, nullptr) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to bind mount '%s' -> '%s'",
                            mTempMountPointOutsideContainer.c_str(),
                            mMountPointInsideContainer.c_str());
    }
    else
    {
        AI_LOG_INFO("created mount tunnel '%s' -> '%s'",
                    mTempMountPointOutsideContainer.c_str(),
                    mMountPointInsideContainer.c_str());
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
bool MountTunnelDetails::removeMountTunnel()
{
    AI_LOG_FN_ENTRY();

    bool success = false;

    if (umount2(mTempMountPointOutsideContainer.c_str(), UMOUNT_NOFOLLOW) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to unmount '%s'",
                         mTempMountPointOutsideContainer.c_str());
    }
    else
    {
        AI_LOG_DEBUG("unmounted temp mount @ '%s', now deleting mount point",
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
