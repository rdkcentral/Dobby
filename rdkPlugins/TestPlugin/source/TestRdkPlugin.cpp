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

#include "TestRdkPlugin.h"

/**
 * Need to do this at the start of every plugin to make sure the correct
 * C methods are visible to allow PluginLauncher to find the plugin
 */
REGISTER_RDK_PLUGIN(TestRdkPlugin);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
TestRdkPlugin::TestRdkPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                             const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                             const std::string &rootfsPath)
    : mName("TestRdkPlugin"),
      mContainerConfig(containerConfig),
      mRootfsPath(rootfsPath),
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 *
 * This plugin uses all the hooks so set all the flags
 */
unsigned TestRdkPlugin::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PostInstallationFlag |
        IDobbyRdkPlugin::HintFlags::PreCreationFlag |
        IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag |
        IDobbyRdkPlugin::HintFlags::CreateContainerFlag |
        IDobbyRdkPlugin::HintFlags::StartContainerFlag |
        IDobbyRdkPlugin::HintFlags::PostStartFlag |
        IDobbyRdkPlugin::HintFlags::PostHaltFlag |
        IDobbyRdkPlugin::HintFlags::PostStopFlag);
}

// Begin Hook Methods

/**
 * @brief Dobby Hook - run in host namespace *once* when container bundle is downloaded
 */
bool TestRdkPlugin::postInstallation()
{
    AI_LOG_INFO("Hello world, this is the %s hook", __func__);

    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    AI_LOG_INFO("This hook is running for container with hostname %s", mContainerConfig->hostname);
    return true;
}

/**
 * @brief Dobby Hook - run in host namespace before container creation process
 */
bool TestRdkPlugin::preCreation()
{
    AI_LOG_INFO("Hello world, this is the %s hook", __func__);

    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    AI_LOG_INFO("This hook is running for container with hostname %s", mContainerConfig->hostname);
    return true;
}

/**
 * @brief OCI Hook - Run in host namespace
 */
bool TestRdkPlugin::createRuntime()
{
    AI_LOG_INFO("Hello world, this is the %s hook", __func__);

    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    AI_LOG_INFO("This hook is running for container with hostname %s", mContainerConfig->hostname);
    return true;
}

/**
 * @brief OCI Hook - Run in container namespace. Paths resolve to host namespace
 */
bool TestRdkPlugin::createContainer()
{
    AI_LOG_INFO("Hello world, this is the %s hook", __func__);

    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    AI_LOG_INFO("This hook is running for container with hostname %s", mContainerConfig->hostname);
    return true;
}

/**
 * @brief OCI Hook - Run in container namespace
 */
bool TestRdkPlugin::startContainer()
{
    AI_LOG_INFO("Hello world, this is the %s hook", __func__);

    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    AI_LOG_INFO("This hook is running for container with hostname %s", mContainerConfig->hostname);
    return true;
}

/**
 * @brief OCI Hook - Run in host namespace once container has started
 */
bool TestRdkPlugin::postStart()
{
    AI_LOG_INFO("Hello world, this is the %s hook", __func__);

    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    AI_LOG_INFO("This hook is running for container with hostname %s", mContainerConfig->hostname);
    return true;
}

/**
 * @brief Dobby Hook - Run in host namespace when container terminates
 */
bool TestRdkPlugin::postHalt()
{
    AI_LOG_INFO("Hello world, this is the %s hook", __func__);

    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    AI_LOG_INFO("This hook is running for container with hostname %s", mContainerConfig->hostname);
    return true;
}

/**
 * @brief OCI Hook - Run in host namespace. Confusing name - is run when a container is DELETED
 */
bool TestRdkPlugin::postStop()
{
    AI_LOG_INFO("Hello world, this is the %s hook", __func__);

    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    AI_LOG_INFO("This hook is running for container with hostname %s", mContainerConfig->hostname);
    return true;
}

// End hook methods

// Begin private methods
