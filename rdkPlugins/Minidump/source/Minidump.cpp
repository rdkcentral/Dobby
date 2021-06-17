/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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

#include "Minidump.h"
#include "StorageHelper.h"

#include <dirent.h>

/**
 * Need to do this at the start of every plugin to make sure the correct
 * C methods are visible to allow PluginLauncher to find the plugin
 */
REGISTER_RDK_PLUGIN(Minidump);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
Minidump::Minidump(std::shared_ptr<rt_dobby_schema> &containerConfig,
                   const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                   const std::string &rootfsPath)
    : mName("Minidump")
    , mContainerConfig(containerConfig)
    , mMappedId(containerConfig)
    , mRootfsPath(rootfsPath)
    , mUtils(utils)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 */
unsigned Minidump::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PreCreationFlag |
        IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag |
        IDobbyRdkPlugin::HintFlags::CreateContainerFlag |
        IDobbyRdkPlugin::HintFlags::PostStartFlag |
        IDobbyRdkPlugin::HintFlags::PostHaltFlag |
        IDobbyRdkPlugin::HintFlags::PostStopFlag);
}

/**
 * @brief OCI Hook - Run in host namespace
 */
bool Minidump::preCreation()
{
    AI_LOG_FN_ENTRY();

    const auto pathsData = getPathsData();
    for (const auto& pathData : pathsData)
    {
        const auto loopMountDetails = convert(pathData);
        if (loopMountDetails)
        {
            if (!loopMountDetails->onPreCreate())
            {
                AI_LOG_ERROR_EXIT("failed to execute preCreation loop hook");
                return false;
            }
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief OCI Hook - Run in host namespace
 */
bool Minidump::createRuntime()
{
    AI_LOG_FN_ENTRY();

    const auto pathsData = getPathsData();
    for (const auto& pathData : pathsData)
    {
        const auto loopMountDetails = convert(pathData);
        if (loopMountDetails)
        {
            if (!loopMountDetails->setPermissions())
            {
                AI_LOG_ERROR_EXIT("failed to execute createRuntime loop hook");
                return false;
            }
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief OCI Hook - Run in container namespace. Paths resolve to host namespace
 */
bool Minidump::createContainer()
{
    AI_LOG_FN_ENTRY();

    const auto pathsData = getPathsData();
    for (const auto& pathData : pathsData)
    {
        const auto loopMountDetails = convert(pathData);
        if (loopMountDetails)
        {
            if (!loopMountDetails->remountTempDirectory())
            {
                AI_LOG_ERROR_EXIT("failed to execute createContainer loop hook");
                return false;
            }
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief OCI Hook - Run in host namespace once container has started
 */
bool Minidump::postStart()
{
    AI_LOG_FN_ENTRY();

    const auto pathsData = getPathsData();
    for (const auto& pathData : pathsData)
    {
        const auto loopMountDetails = convert(pathData);
        if (loopMountDetails)
        {
            if (!loopMountDetails->cleanupTempDirectory())
            {
                AI_LOG_ERROR_EXIT("failed to execute postStart loop hook");
                return false;
            }
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief OCI Hook - Run in host namespace. Confusing name - is run when a container is DELETED
 */
bool Minidump::postStop()
{
    AI_LOG_FN_ENTRY();

    const auto pathsData = getPathsData();
    for (const auto& pathData : pathsData)
    {
        const auto loopMountDetails = convert(pathData);
        if (loopMountDetails)
        {
            if (!loopMountDetails->removeNonPersistentImage())
            {
                AI_LOG_ERROR_EXIT("failed to execute postStop loop hook");
                return false;
            }
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief Dobby Hook - Run in host namespace when container terminates
 */
bool Minidump::postHalt()
{
    AI_LOG_FN_ENTRY();

    const auto pathsData = getPathsData();
    for (const auto& pathData : pathsData)
    {
        const auto loopMountDetails = convert(pathData);
        if (loopMountDetails)
        {
            if (!loopMountDetails->copyToHost(".dmp", "MDMP", pathData.hostDestination))
            {
                AI_LOG_ERROR_EXIT("failed to execute postHalt loop hook");
                return false;
            }
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Create path details vector from all paths in config.
 *
 *
 *  @return vector of PathData's that were in the config
 */
std::vector<Minidump::PathData> Minidump::getPathsData()
{
    AI_LOG_FN_ENTRY();

    if (not mContainerConfig->rdk_plugins->minidump->data)
    {
        AI_LOG_ERROR("No minidump data in config file");
        return {};
    }

    std::vector<PathData> pathsData;
    pathsData.reserve(mContainerConfig->rdk_plugins->minidump->data->paths_len);

    for (size_t i = 0; i < mContainerConfig->rdk_plugins->minidump->data->paths_len; i++)
    {
        pathsData.emplace_back(
            std::string(mContainerConfig->rdk_plugins->minidump->data->paths[i]->image),
            std::string(mContainerConfig->rdk_plugins->minidump->data->paths[i]->container_source),
            std::string(mContainerConfig->rdk_plugins->minidump->data->paths[i]->host_destination),
            mContainerConfig->rdk_plugins->minidump->data->paths[i]->imgsize_present
                ? mContainerConfig->rdk_plugins->minidump->data->paths[i]->imgsize
                : 8 * 1024 * 1024 // fallback 8MB
        );
    }

    AI_LOG_FN_EXIT();
    return pathsData;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Convert path details object to loop mount details object
 *
 *
 *  @return unique_ptr of LoopMountDetails object with injected values from the
 *          config that are needed for mounting writable disk space purposes
 */
std::unique_ptr<LoopMountDetails> Minidump::convert(const PathData& pathData)
{
    AI_LOG_FN_ENTRY();

    LoopMountDetails::LoopMount loopMount;

    loopMount.fsImagePath = pathData.image;
    loopMount.destination = pathData.containerSource;
    loopMount.imgSize = pathData.imgSize;
    loopMount.fsImageType = "ext4";
    loopMount.persistent = false;
    loopMount.imgManagement = true;
    loopMount.mountFlags = 14;
    uid_t uid = mMappedId.forUser();
    gid_t gid = mMappedId.forGroup();

    AI_LOG_FN_EXIT();
    return std::make_unique<LoopMountDetails>(mRootfsPath, loopMount, uid, gid, mUtils);
}

/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> Minidump::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_minidump* pluginConfig = mContainerConfig->rdk_plugins->minidump;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}
