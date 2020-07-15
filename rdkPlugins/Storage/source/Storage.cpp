/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
    AI_LOG_INFO("Storage Plugin loaded");
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
    std::vector<std::unique_ptr<LoopMountDetails>> loopDetails = getLoopDetails();
    for(auto it = loopDetails.begin(); it != loopDetails.end(); it++)
    {
        // Creating loop mount and attaching it to temp mount inside container
        if(!(*it)->onPreCreate())
        {
            AI_LOG_ERROR_EXIT("failed to execute preCreation loop hook");
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
    std::vector<std::unique_ptr<LoopMountDetails>> loopDetails = getLoopDetails();
    for(auto it = loopDetails.begin(); it != loopDetails.end(); it++)
    {
        // Setting permissions for generated directories
        if(!(*it)->setPermissions())
        {
            AI_LOG_ERROR_EXIT("failed to execute createRuntime loop hook");
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
    std::vector<std::unique_ptr<LoopMountDetails>> loopDetails = getLoopDetails();
    for(auto it = loopDetails.begin(); it != loopDetails.end(); it++)
    {
        // Remount temp directory into proper place
        if(!(*it)->remountTempDirectory())
        {
            AI_LOG_ERROR_EXIT("failed to execute createRuntime loop hook");
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

    std::vector<std::unique_ptr<LoopMountDetails>> loopDetails = getLoopDetails();
    for(auto it = loopDetails.begin(); it != loopDetails.end(); it++)
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

    std::vector<std::unique_ptr<LoopMountDetails>> loopDetails = getLoopDetails();
    for(auto it = loopDetails.begin(); it != loopDetails.end(); it++)
    {
        // Clean up temp mount points
        if(!(*it)->removeNonPersistentImage())
        {
            AI_LOG_ERROR_EXIT("failed to clean up non persistent image");
            // This is probably to late to fail but do it either way
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// End hook methods

// Begin private methods

// -----------------------------------------------------------------------------
/**
 *  @brief Create loop mount details vector from all mounts in config.
 *
 *
 *  @return vector of LoopMountDetails's that were in the config
 */
std::vector<std::unique_ptr<LoopMountDetails>> Storage::getLoopDetails()
{
    AI_LOG_FN_ENTRY();

    const std::vector<LoopMountDetails::LoopMount> mounts = getLoopMounts();

    std::vector<std::unique_ptr<LoopMountDetails>> loopDetails;
    // loop though all the mounts for the given container and create individual
    // DobbyLoopMount objects for each
    for (const LoopMountDetails::LoopMount &mount : mounts)
    {
        uid_t tmp_uid = 0;
        gid_t tmp_gid = 0;

        // Firstly get uid/gid from process
        if(mContainerConfig->process &&
           mContainerConfig->process->user)
        {
            if (mContainerConfig->process->user->uid_present)
            {
                tmp_uid = mContainerConfig->process->user->uid;
            }

            if (mContainerConfig->process->user->gid_present)
            {
                tmp_gid = mContainerConfig->process->user->gid;
            }
        }

        // Then map it inside container
        tmp_uid = getMappedId(tmp_uid,
                                mContainerConfig->linux->uid_mappings,
                                mContainerConfig->linux->uid_mappings_len);

        tmp_gid = getMappedId(tmp_gid,
                                mContainerConfig->linux->gid_mappings,
                                mContainerConfig->linux->gid_mappings_len);


        // create the loop mount and make sure it was constructed
        std::unique_ptr<LoopMountDetails> loopMount(
            std::make_unique<LoopMountDetails>(
                mRootfsPath,
                mount,
                tmp_uid,
                tmp_gid,
                mUtils)
            );

        if (loopMount)
        {
            loopDetails.emplace_back(std::move(loopMount));
        }
    }

    AI_LOG_FN_EXIT();
    return loopDetails;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Reads container config and creates all mount information in LoopMount
 *  type objects.
 *
 *
 *  @return vector of LoopMount's that were in the config
 */
std::vector<LoopMountDetails::LoopMount> Storage::getLoopMounts()
{
    AI_LOG_FN_ENTRY();

    std::vector<LoopMountDetails::LoopMount> mounts;

    // Check if container has mount data
    if (mContainerConfig->rdk_plugins->storage->data)
    {
        // loop though all the mounts for the given container and create individual
        // LoopMountDetails::LoopMount objects for each
        for (int i = 0; i < mContainerConfig->rdk_plugins->storage->data->loopback_len; i++)
        {
            auto loopback = mContainerConfig->rdk_plugins->storage->data->loopback[i];
            LoopMountDetails::LoopMount mount;

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
                // default image size = 12 MB
                mount.imgSize = 12 * 1024 * 1024;
            }

            for (int j = 0; j < loopback->options_len; j++)
            {
                mount.mountOptions.push_back(std::string(loopback->options[j]));
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
 *  @brief Gets userId or groupId based on mappings
 *
 *  @param[in]  id          Id we want to map
 *  @param[in]  mapping     Mapping that should be used
 *  @param[in]  mapping_len Length of mapping
 *
 *  @return if found mapped id, if not found initial id
 */
uint32_t Storage::getMappedId(uint32_t id, rt_defs_id_mapping **mapping, size_t mapping_len)
{
    AI_LOG_FN_ENTRY();

    uint32_t tmp_id = id;

    // get id of the container inside host
    for (int i = 0; i < mapping_len; i++)
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