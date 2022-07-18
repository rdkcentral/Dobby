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

#include "Storage.h"
#include "StorageHelper.h"

#include <stdio.h>
#include <errno.h>
#include <string>
#include <memory>

/**
 * Need to do this at the start of every plugin to make sure the correct
 * C methods are visible to allow PluginLauncher to find the plugin
 */
REGISTER_RDK_PLUGIN(Storage);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
Storage::Storage(std::shared_ptr<rt_dobby_schema> &containerSpec,
                const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                const std::string &rootfsPath)
    : mName("Storage"),
      mContainerConfig(containerSpec),
      mRootfsPath(rootfsPath),
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 *
 */
unsigned Storage::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PreCreationFlag |
        IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag |
        IDobbyRdkPlugin::HintFlags::CreateContainerFlag |
#ifdef ENABLE_TESTS
        IDobbyRdkPlugin::HintFlags::StartContainerFlag |
#endif //#ifdef ENABLE_TESTS
        IDobbyRdkPlugin::HintFlags::PostStartFlag |
        IDobbyRdkPlugin::HintFlags::PostStopFlag);
}

// Begin Hook Methods

/**
 * @brief OCI Hook - Run in host namespace
 */
bool Storage::preCreation()
{
    AI_LOG_FN_ENTRY();

    // create loopmount for every point
    std::vector<std::unique_ptr<LoopMountDetails>> mountDetails = getLoopMountDetails();

    for(auto it = mountDetails.begin(); it != mountDetails.end(); it++)
    {
        // Creating loop mount and attaching it to temp mount inside container
        if(!(*it)->onPreCreate())
        {
            AI_LOG_ERROR_EXIT("failed to execute preCreation hook for loop mount");
            return false;
        }
    }
    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief OCI Hook - Run in host namespace
 */
bool Storage::createRuntime()
{
    AI_LOG_FN_ENTRY();

    // Set permissions for every loop point directory
    std::vector<std::unique_ptr<LoopMountDetails>> mountDetails = getLoopMountDetails();
    for(auto it = mountDetails.begin(); it != mountDetails.end(); it++)
    {
        // Setting permissions for generated directories
        if(!(*it)->setPermissions())
        {
            AI_LOG_ERROR_EXIT("failed to execute createRuntime loop hook");
            return false;
        }
    }

    // create destination paths for each dynamic mount
    std::vector<std::unique_ptr<DynamicMountDetails>> dynamicMountDetails = getDynamicMountDetails();
    for(auto it = dynamicMountDetails.begin(); it != dynamicMountDetails.end(); it++)
    {
        // Creating destination paths inside container
        if(!(*it)->onCreateRuntime())
        {
            AI_LOG_ERROR_EXIT("failed to execute createRuntime hook for dynamic mount");
            return false;
        }
    }

    // Change mount ownership for all configured items
    std::vector<std::unique_ptr<MountOwnerDetails>> mountOwnerDetails = getMountOwnerDetails();

    for(auto it = mountOwnerDetails.begin(); it != mountOwnerDetails.end(); it++)
    {
        // Change ownership of mount
        if(!(*it)->onCreateRuntime())
        {
            AI_LOG_ERROR_EXIT("failed to execute createRuntime hook for mount owner");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief OCI Hook - Run in container namespace. Paths resolve to host namespace
 */
bool Storage::createContainer()
{
    AI_LOG_FN_ENTRY();

    // Mount temp directory in proper place
    std::vector<std::unique_ptr<LoopMountDetails>> loopMountDetails = getLoopMountDetails();
    for(auto it = loopMountDetails.begin(); it != loopMountDetails.end(); it++)
    {
        // Remount temp directory into proper place
        if(!(*it)->remountTempDirectory())
        {
            AI_LOG_ERROR_EXIT("failed to execute createRuntime loop hook");
            return false;
        }
    }

    // Create dynamic mounts for every point
    std::vector<std::unique_ptr<DynamicMountDetails>> dynamicMountDetails = getDynamicMountDetails();

    for(auto it = dynamicMountDetails.begin(); it != dynamicMountDetails.end(); it++)
    {
        // Creating dynamic mounts inside container where source exists on the host
        if(!(*it)->onCreateContainer())
        {
            AI_LOG_ERROR_EXIT("failed to execute createContainer hook for dynamic mount");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief OCI Hook - Run in container namespace
 */
#ifdef ENABLE_TESTS
bool Storage::startContainer()
{
    AI_LOG_FN_ENTRY();

    // This is test only
    StorageHelper::Test_checkWriteReadMount("/home/private/startContainerWorks.txt");

    AI_LOG_FN_EXIT();
    return true;
}
#endif // ENABLE_TESTS

/**
 * @brief OCI Hook - Run in host namespace once container has started
 */
bool Storage::postStart()
{
    AI_LOG_FN_ENTRY();

    std::vector<std::unique_ptr<LoopMountDetails>> mountDetails = getLoopMountDetails();
    for(auto it = mountDetails.begin(); it != mountDetails.end(); it++)
    {
        // Clean up temp mount points
        if(!(*it)->cleanupTempDirectory())
        {
            AI_LOG_WARN("failed to clean up inside postStart");
            // We are not failing this step as even if temp is not cleaned up it doesn't
            // harm to run container.
            // return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief OCI Hook - Run in host namespace. Confusing name - is run when a container is DELETED
 */
bool Storage::postStop()
{
    AI_LOG_FN_ENTRY();

    // here should be deleting the data.img file when non persistent option selected

    std::vector<std::unique_ptr<LoopMountDetails>> loopMountDetails = getLoopMountDetails();
    for(auto it = loopMountDetails.begin(); it != loopMountDetails.end(); it++)
    {
        // Clean up temp mount points
        if(!(*it)->removeNonPersistentImage())
        {
            AI_LOG_ERROR_EXIT("failed to clean up non persistent image");
            // This is probably to late to fail but do it either way
            return false;
        }
    }

    std::vector<std::unique_ptr<DynamicMountDetails>> dynamicMountDetails = getDynamicMountDetails();
    for(auto it = dynamicMountDetails.begin(); it != dynamicMountDetails.end(); it++)
    {
        // Clean up temp mount points
        if(!(*it)->onPostStop())
        {
            AI_LOG_ERROR_EXIT("failed to remove dynamic mounts");
            // This is probably to late to fail but do it either way
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// End hook methods

// -----------------------------------------------------------------------------
/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> Storage::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_storage* pluginConfig = mContainerConfig->rdk_plugins->storage;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

// Begin private methods

// -----------------------------------------------------------------------------
/**
 *  @brief Create loop mount details vector from all loopback mounts in config.
 *
 *
 *  @return vector of LoopMountDetails that were in the config
 */
std::vector<std::unique_ptr<LoopMountDetails>> Storage::getLoopMountDetails() const
{
    AI_LOG_FN_ENTRY();

    const std::vector<LoopMountProperties> loopMounts = getLoopMounts();
    std::vector<std::unique_ptr<LoopMountDetails>> mountDetails;

    // loop though all the loop mounts for the given container and create individual
    // LoopMountDetails objects for each
    for (const LoopMountProperties &properties : loopMounts)
    {
        // Setup the user/group IDs
        uid_t uid = 0;
        gid_t gid = 0;
        setupOwnerIds(uid, gid);

        // create the loop mount and make sure it was constructed
        auto loopMount = std::make_unique<LoopMountDetails>(mRootfsPath,
                                                            properties,
                                                            uid,
                                                            gid,
                                                            mUtils);

        if (loopMount)
        {
            mountDetails.emplace_back(std::move(loopMount));
        }
    }

    AI_LOG_FN_EXIT();
    return mountDetails;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Reads container config and creates all loop mounts in LoopMountProperties
 *  type objects.
 *
 *
 *  @return vector of LoopMountProperties that were in the config
 */
std::vector<LoopMountProperties> Storage::getLoopMounts() const
{
    AI_LOG_FN_ENTRY();

    std::vector<LoopMountProperties> mounts;

    // Check if container has mount data
    if (mContainerConfig->rdk_plugins->storage->data)
    {
        // loop though all the loopback mounts for the given container and create individual
        // LoopMountDetails::LoopMount objects for each
        for (size_t i = 0; i < mContainerConfig->rdk_plugins->storage->data->loopback_len; i++)
        {
            auto loopback = mContainerConfig->rdk_plugins->storage->data->loopback[i];

            LoopMountProperties mount;
            mount.fsImagePath = std::string(loopback->source);
            mount.destination = std::string(loopback->destination);
            mount.mountFlags  = loopback->flags;

            // Optional parameters
            if (loopback->fstype)
            {
                mount.fsImageType = std::string(loopback->fstype);
            }
            else
            {
                // default image type = ext4
                mount.fsImageType = "ext4";
            }

            if (loopback->persistent_present)
            {
                mount.persistent = loopback->persistent;
            }
            else
            {
                // default persistent = true
                mount.persistent = true;
            }

            if (loopback->imgsize_present)
            {
                mount.imgSize = loopback->imgsize;
            }
            else
            {
                if (mount.fsImageType.compare("xfs") == 0)
                {
                    // default image size = 16 MB
                    mount.imgSize = 16 * 1024 * 1024;
                }
                else
                {
                    // default image size = 12 MB
                    mount.imgSize = 12 * 1024 * 1024;
                }
            }

            for (size_t j = 0; j < loopback->options_len; j++)
            {
                mount.mountOptions.push_back(std::string(loopback->options[j]));
            }

            if (loopback->imgmanagement_present)
            {
                mount.imgManagement = loopback->imgmanagement;
            }
            else
            {
                // default imgManagement = true
                mount.imgManagement = true;
            }

            mounts.push_back(mount);
        }
    }
    else
    {
        AI_LOG_ERROR("No storage data in config file");
    }

    AI_LOG_FN_EXIT();
    return mounts;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Create dynamic mount details vector from all dynamic mounts in config.
 *
 *
 *  @return vector of DynamicMountDetails that were in the config
 */
std::vector<std::unique_ptr<DynamicMountDetails>> Storage::getDynamicMountDetails() const
{
    AI_LOG_FN_ENTRY();

    const std::vector<DynamicMountProperties> dynamicMounts = getDynamicMounts();

    std::vector<std::unique_ptr<DynamicMountDetails>> mountDetails;
    // loop though all the dynamic mounts for the given container and create individual
    // DynamicMountDetails objects for each
    for (const DynamicMountProperties &properties : dynamicMounts)
    {
        // create the dynamic mount and make sure it was constructed
        auto dynamicMount = std::make_unique<DynamicMountDetails>(mRootfsPath,
                                                                  properties,
                                                                  mUtils);

        if (dynamicMount)
        {
            mountDetails.emplace_back(std::move(dynamicMount));
        }
    }

    AI_LOG_FN_EXIT();

    return mountDetails;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Reads container config and creates all dynamic mounts in DynamicMountProperties
 *  type objects.
 *
 *
 *  @return vector of DynamicMountProperties that were in the config
 */
std::vector<DynamicMountProperties> Storage::getDynamicMounts() const
{
    AI_LOG_FN_ENTRY();

    std::vector<DynamicMountProperties> mounts;

    // Check if container has mount data
    if (mContainerConfig->rdk_plugins->storage->data)
    {
        // loop though all the dynamic mounts for the given container and create individual
        // DynamicMountProperties objects for each
        for (size_t i = 0; i < mContainerConfig->rdk_plugins->storage->data->dynamic_len; i++)
        {
            auto dynamic = mContainerConfig->rdk_plugins->storage->data->dynamic[i];

            DynamicMountProperties mount;
            mount.source = std::string(dynamic->source);
            mount.destination = std::string(dynamic->destination);
            mount.mountFlags  = dynamic->flags;

            for (size_t j = 0; j < dynamic->options_len; j++)
            {
                mount.mountOptions.push_back(std::string(dynamic->options[j]));
            }

            mounts.push_back(mount);
        }
    }
    else
    {
        AI_LOG_ERROR("No storage data in config file");
    }

    AI_LOG_FN_EXIT();
    return mounts;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Create mount owner details vector from all mount owners in config.
 *
 *
 *  @return vector of MountOwnerDetails that were in the config
 */
std::vector<std::unique_ptr<MountOwnerDetails>> Storage::getMountOwnerDetails() const
{
    AI_LOG_FN_ENTRY();

    const std::vector<MountOwnerProperties> mountOwners = getMountOwners();
    std::vector<std::unique_ptr<MountOwnerDetails>> ownerDetails;

    // Setup the user/group IDs
    uid_t uid = 0;
    gid_t gid = 0;
    setupOwnerIds(uid, gid);

    // loop though all the mount owners for the given container and create individual
    // MountOwnerDetails objects for each
    for (const MountOwnerProperties &properties : mountOwners)
    {
        // create the mount owner details and make sure it was constructed
        auto mountOwner = std::make_unique<MountOwnerDetails>(mRootfsPath,
                                                              properties,
                                                              uid,
                                                              gid,
                                                              mUtils);

        if (mountOwner)
        {
            ownerDetails.emplace_back(std::move(mountOwner));
        }
    }

    AI_LOG_FN_EXIT();

    return ownerDetails;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Reads container config to obtain source path on host, userId, groupId
 * and recursive options. These will be used later to change ownership of the
 * source path based on userId and groupId within the host namespace.
 *
 *  @return vector of MountOwnerProperties that were in the config
 */
std::vector<MountOwnerProperties> Storage::getMountOwners() const
{
    AI_LOG_FN_ENTRY();

    std::vector<MountOwnerProperties> mountOwners;

    // Check if container has mount data
    if (mContainerConfig->rdk_plugins->storage->data)
    {
        // loop though all the mount owners for the given container and create individual
        // MountOwnerProperties objects for each
        for (size_t i = 0; i < mContainerConfig->rdk_plugins->storage->data->mount_owner_len; i++)
        {
            auto mountOwner = mContainerConfig->rdk_plugins->storage->data->mount_owner[i];

            MountOwnerProperties mountOwnerProps;
            mountOwnerProps.source = std::string(mountOwner->source);
            if (mountOwner->user)
            {
                mountOwnerProps.user = std::string(mountOwner->user);
            }
            if (mountOwner->group)
            {
                mountOwnerProps.group  = std::string(mountOwner->group);
            }
            mountOwnerProps.recursive = mountOwner->recursive_present ? mountOwner->recursive : false;
            mountOwners.push_back(mountOwnerProps);
        }
    }
    else
    {
        AI_LOG_ERROR("No storage data in config file");
    }

    AI_LOG_FN_EXIT();
    return mountOwners;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets userId and groupId
 *
 *  @param[in]  id          Id we want to map
 *  @param[in]  mapping     Mapping that should be used
 *  @param[in]  mapping_len Length of mapping
 *
 *  @return if found mapped id, if not found initial id
 */
void Storage::setupOwnerIds(uid_t& uid, gid_t& gid) const
{
    uid = 0;
    gid = 0;

    // Get uid/gid from process
    if(mContainerConfig->process && mContainerConfig->process->user)
    {
        if (mContainerConfig->process->user->uid_present)
        {
            uid = mContainerConfig->process->user->uid;
        }

        if (mContainerConfig->process->user->gid_present)
        {
            gid = mContainerConfig->process->user->gid;
        }
    }

    // Then map it inside container
    uid = getMappedId(uid,
                      mContainerConfig->linux->uid_mappings,
                      mContainerConfig->linux->uid_mappings_len);

    gid = getMappedId(gid,
                      mContainerConfig->linux->gid_mappings,
                      mContainerConfig->linux->gid_mappings_len);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets userId or groupId based on mappings
 *
 *  @param[in]  id          Id we want to map
 *  @param[in]  mapping     Mapping that should be used
 *  @param[in]  mapping_len Length of mapping
 *
 *  @return if found mapped id, if not found initial id
 */
uint32_t Storage::getMappedId(uint32_t id, rt_defs_id_mapping **mapping, size_t mapping_len) const
{
    AI_LOG_FN_ENTRY();

    uint32_t tmp_id = id;

    // get id of the container inside host
    for (size_t i = 0; i < mapping_len; i++)
    {
        // No need to check if container_id, size or host_id is present as all those fields
        // are required ones, this means that if mapping point exists it has all 3 of those

        // Check if id is higher than mapping one, if not it is not the mapping we are looking for
        if (id >= mapping[i]->container_id)
        {
            uint32_t shift = id - mapping[i]->container_id;
            // Check if id is inside this mapping
            if (shift < mapping[i]->size)
            {
                // Shift host as much as ID was shifted
                tmp_id = mapping[i]->host_id + shift;
            }
        }
    }

    if (tmp_id == id)
    {
        AI_LOG_WARN("Mapping not found for id '%d'", id);
    }

    AI_LOG_FN_EXIT();
    return tmp_id;
}
