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

#include "MountOwnerDetails.h"
#include "StorageHelper.h"
#include "DobbyRdkPluginUtils.h"

#include <unistd.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

// -----------------------------------------------------------------------------
/**
    @class MountOwnerDetails
 *  @brief Class that represents mount ownership and whether to apply recursively
 *
 *  This class is only intended to be used internally by Storage plugin
 *  do not use from external code.
 *
 *  @param[in]  rootfsPath      Root FS path to apply ownership
 *  @param[in]  mountProperties Structure holding mount ownership configuration
 *  @param[in]  utils           Useful Dobby utilities
 *
 *  @see Storage
 */
MountOwnerDetails::MountOwnerDetails(const std::string& rootfsPath,
                                     const MountOwnerProperties& mountOwnerProperties,
                                     const uid_t& defaultUserId,
                                     const gid_t& defaultGroupId,
                                     const std::shared_ptr<DobbyRdkPluginUtils> &utils)
    : mRootfsPath(rootfsPath),
      mMountOwnerProperties(mountOwnerProperties),
      mDefaultUserId(defaultUserId),
      mDefaultGroupId(defaultGroupId),
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

MountOwnerDetails::~MountOwnerDetails()
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Changes ownership of mount source according to MountOwnerProperties
 * during the createRuntime hook
 *
 *  @return true on success, false on failure
 */
bool MountOwnerDetails::onCreateRuntime() const
{
    AI_LOG_FN_ENTRY();

    bool success = false;
    struct stat buffer;
    if (stat(mMountOwnerProperties.source.c_str(), &buffer) == 0)
    {
        success = processOwners();
    }
    else
    {
        // Mount not found to ignore request to change ownership
        success = true;
        AI_LOG_INFO("Mount '%s' does not exist, change ownership skipped", mMountOwnerProperties.source.c_str());
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Get user and group IDs based on their configured
 *
 *  @param[out]  userId  ID corresponding to the configured user name
 *  @param[out]  groupId  ID corresponding to the configured group name
 *  @return true on success, false on failure
 */
bool MountOwnerDetails::getOwnerIds(uid_t& userId, gid_t& groupId) const
{
    struct passwd *pwd;
    struct group *grp;
    if (mMountOwnerProperties.user.empty())
    {
        AI_LOG_INFO("Using default user '%d'", mDefaultUserId);
        userId = mDefaultUserId;
    }
    else
    {
        pwd = getpwnam(mMountOwnerProperties.user.c_str());
        if (pwd == NULL)
        {
            AI_LOG_SYS_ERROR(errno, "User '%s' not found", mMountOwnerProperties.user.c_str());
            return false;
        }
        else
        {
            userId = pwd->pw_uid;
        }
    }

    if (mMountOwnerProperties.group.empty())
    {
        AI_LOG_INFO("Using default group '%d'", mDefaultGroupId);
        groupId = mDefaultGroupId;
    }
    else
    {
        grp = getgrnam(mMountOwnerProperties.group.c_str());
        if (grp == NULL)
        {
            AI_LOG_SYS_ERROR(errno, "Group '%s' not found", mMountOwnerProperties.group.c_str());
            return false;
        }
        else
        {
            groupId = grp->gr_gid;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Process individual mount owner and change ownership either singularly
 * or recursively
 *
 *  @return true on success, false on failure
 */
bool MountOwnerDetails::processOwners() const
{
    AI_LOG_FN_ENTRY();

    bool success = false;
    uid_t userId = 0;
    gid_t groupId = 0;

    if (getOwnerIds(userId, groupId))
    {
        if (mMountOwnerProperties.recursive)
        {
            success = changeOwnerRecursive(mMountOwnerProperties.source, userId, groupId);
            if (!success)
            {
                AI_LOG_ERROR("Failed to change owner recursively of '%s' to '%d:%d",
                             mMountOwnerProperties.source.c_str(),
                             userId,
                             groupId);
            }
        }
        else
        {
            success = changeOwner(mMountOwnerProperties.source, userId, groupId);
        }
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Change ownership recursively from the given path
 *
 *  @param[out]  path    Path to recurse from
 *  @param[out]  userId  ID corresponding to the configured user name
 *  @param[out]  groupId ID corresponding to the configured group name
 *  @return true on success, false on failure
 */
bool MountOwnerDetails::changeOwnerRecursive(const std::string& path, uid_t userId, gid_t groupId) const
{
    DIR* dir = opendir(path.c_str());
    if (!dir) {
        return false;
    }

    bool success = true;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string nextPath = path + "/" + std::string(entry->d_name);
        if (entry->d_type == DT_DIR)
        {
            if ((strcmp(entry->d_name, ".") == 0) || (strcmp(entry->d_name, "..") == 0))
            {
                continue;
            }
            success &= changeOwnerRecursive(nextPath, userId, groupId);
        }
        success &= changeOwner(nextPath, userId, groupId);
    }
    success &= changeOwner(path, userId, groupId);

    closedir(dir);
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Change ownership of mount according to properties structure
 *
 *  @param[in]  path    Path to change ownership of
 *  @param[in]  userId  User ID to set
 *  @param[in]  groupId Group ID to set
 *  @return true on success, false on failure
 */
bool MountOwnerDetails::changeOwner(const std::string& path, uid_t userId, gid_t groupId) const
{
    AI_LOG_FN_ENTRY();

    bool success = (chown(path.c_str(), userId, groupId) == 0);
    if (!success)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to change owner of '%s' to '%d:%d", path.c_str(), userId, groupId);
    }

    AI_LOG_FN_EXIT();
    return success;
}
