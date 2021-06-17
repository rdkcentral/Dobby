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
    : mName("Storage")
    , mContainerConfig(containerSpec)
    , mMappedId(containerSpec)
    , mRootfsPath(rootfsPath)
    , mUtils(utils)
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
    loopDetails.reserve(mounts.size());

    // loop though all the mounts for the given container and create individual
    // DobbyLoopMount objects for each
    for (const LoopMountDetails::LoopMount &mount : mounts)
    {
        uid_t uid = mMappedId.forUser();
        gid_t gid = mMappedId.forGroup();

        // create the loop mount and make sure it was constructed
        auto loopMount = std::make_unique<LoopMountDetails>(mRootfsPath, mount, uid, gid, mUtils);
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
    mounts.reserve(mContainerConfig->rdk_plugins->storage->data->loopback_len);

    // Check if container has mount data
    if (mContainerConfig->rdk_plugins->storage->data)
    {
        // loop though all the mounts for the given container and create individual
        // LoopMountDetails::LoopMount objects for each
        for (size_t i = 0; i < mContainerConfig->rdk_plugins->storage->data->loopback_len; i++)
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
