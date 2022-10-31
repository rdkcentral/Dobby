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

#include "MemCheckpointRestore.h"

/**
 * Need to do this at the start of every plugin to make sure the correct
 * C methods are visible to allow PluginLauncher to find the plugin
 */
REGISTER_RDK_PLUGIN(MemCheckpointRestore);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
MemCheckpointRestore::MemCheckpointRestore(std::shared_ptr<rt_dobby_schema> &containerConfig,
                                           const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                           const std::string &rootfsPath)
    : mName("MemCheckpointRestore"),
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
unsigned MemCheckpointRestore::hookHints() const
{
    return (IDobbyRdkPlugin::HintFlags::PostInstallationFlag);
}

// Begin Hook Methods

/**
 * @brief Dobby Hook - run in host namespace *once* when container bundle is downloaded
 */
bool MemCheckpointRestore::postInstallation()
{
    AI_LOG_FN_ENTRY();

    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    if (mContainerConfig->rdk_plugins->memcheckpointrestore)
    {
        const rt_defs_plugins_mem_checkpoint_restore *config = mContainerConfig->rdk_plugins->memcheckpointrestore;
        rt_defs_mount **mountpoints = config->data->mountpoints;

        for (size_t i = 0; i < config->data->mountpoints_len; ++i)
        {
            std::list<std::string> options;
            for (size_t j = 0; j < mountpoints[i]->options_len; ++j)
            {
                options.push_back(mountpoints[i]->options[j]);
            }

            AI_LOG_INFO("Adding bind mount: source(%s), dest(%s)",
                        mountpoints[i]->source, mountpoints[i]->destination);

            if (!mUtils->addMount(mountpoints[i]->source,
                                  mountpoints[i]->destination,
                                  mountpoints[i]->type,
                                  options))
            {
                AI_LOG_WARN("failed to add bind mount for source '%s' ",
                            mountpoints[i]->source);
                return false;
            }
        }
    }

    AI_LOG_FN_EXIT();
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
std::vector<std::string> MemCheckpointRestore::getDependencies() const
{
    std::vector<std::string> dependencies;

    return dependencies;
}

// Begin private methods
