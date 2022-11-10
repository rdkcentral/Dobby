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

#include "EnvironmentVariablePlugin.h"

/**
 * Need to do this at the start of every plugin to make sure the correct
 * C methods are visible to allow PluginLauncher to find the plugin
 */
REGISTER_RDK_PLUGIN(EnvironmentVariablePlugin);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
EnvironmentVariablePlugin::EnvironmentVariablePlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                                                    const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                                    const std::string &rootfsPath)
    : mName("EnvironmentVariable"),
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
unsigned EnvironmentVariablePlugin::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PreCreationFlag);
}

// Begin Hook Methods

/**
 * @brief Dobby Hook - run in host namespace before container creation process
 */
bool EnvironmentVariablePlugin::preCreation()
{
    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    std::vector<std::string> variables_list;
    for (size_t i = 0; i < mContainerConfig->rdk_plugins->environmentvariable->data->variables_len; i++)
    {
        variables_list.push_back(std::string(mContainerConfig->rdk_plugins->environmentvariable->data->variables[i]));
    }

    for (std::string item : variables_list)
    {
        AI_LOG_DEBUG("Expecting variable '%s'", item.c_str());
        const char* value = getenv(item.c_str());
        if (value)
        {
            std::string fullVariable = item + "=" + std::string(value);
            AI_LOG_DEBUG("Adding environment variable '%s'", fullVariable.c_str());

            mUtils->addEnvironmentVar(fullVariable);
        }
        else
        {
            AI_LOG_DEBUG("Variable '%s' not found", item.c_str());
        }
    }
    return true;
}

// End hook methods

/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> EnvironmentVariablePlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_environment_variable* pluginConfig = mContainerConfig->rdk_plugins->environmentvariable;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

// Begin private methods
