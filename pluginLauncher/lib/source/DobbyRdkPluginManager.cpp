/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
/*
 * File:   DobbyRdkPluginManager.cpp
 *
 */
#include "DobbyRdkPluginManager.h"
#include "IDobbyRdkPlugin.h"

#include <Logging.h>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <list>
#include <thread>
#include <functional>

// -----------------------------------------------------------------------------
/**
 * @brief Create instance of DobbyRdkPlugin Manager and load all plugins that
 * can be found in pluginPath
 *
 * @param[in] containerConfig     Pointer to the libocispec struct for the container config
 * @param[in] pluginPath        Where to search for plugins
 */
DobbyRdkPluginManager::DobbyRdkPluginManager(std::shared_ptr<rt_dobby_schema> containerConfig,
                                             const std::string &rootfsPath,
                                             const std::string &hookStdin,
                                             const std::string &pluginPath,
                                             const std::shared_ptr<DobbyRdkPluginUtils> &utils)
    : mContainerConfig(containerConfig),
      mRootfsPath(rootfsPath),
      mHookStdin(hookStdin),
      mPluginPath(pluginPath),
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();

    loadPlugins();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 * Unload all plugins on destruction
 */
DobbyRdkPluginManager::~DobbyRdkPluginManager()
{
    AI_LOG_FN_ENTRY();

    // destruct the plugins; note you need to take care to destruct the pointer
    // first then close the library as the destructor needs to be called from
    // the library.

    // All loggers are also plugins
    auto itl = mLoggers.begin();
    for (; itl != mLoggers.end(); ++itl)
    {
        itl->second.second.reset();
        // Don't close, as need to clean up plugin list
    }

    mLoggers.clear();

    auto it = mPlugins.begin();
    for (; it != mPlugins.end(); ++it)
    {
        it->second.second.reset();
        dlclose(it->second.first);
    }

    mPlugins.clear();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Scans the given path for any shared objects that implement the
 *  plugin entry points.
 *
 *  This calls dlopen() on all the executable files in the given path (although
 *  it doesn't recurse into subdirs), if the file has symbols createIDobbyRdkPlugin
 *  and destroyIDobbyRdkPlugin then it's deemed to be a 'rdk' plugin.
 *
 *  If loaded successfully the plugins are stored in an internal map, keyed
 *  off the plugin name.
 *
 */
void DobbyRdkPluginManager::loadPlugins()
{
    AI_LOG_FN_ENTRY();

    // Check we can access the directory and open it
    int dirFd = open(mPluginPath.c_str(), O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open dir '%s'", mPluginPath.c_str());
        return;
    }

    DIR *dir = fdopendir(dirFd);
    if (dir == nullptr)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open dir '%s'", mPluginPath.c_str());
        close(dirFd);
        return;
    }

    int directoriesCount = 0;
    struct dirent **namelist;

    // Need to sort directories with versionsort so lib.12 would be greater than lib.2
    directoriesCount = scandir(mPluginPath.c_str(), &namelist, 0, versionsort);
    for(int i=0; i<directoriesCount;i++)
    {
        // if a symlink verify that the thing we're pointing to is a file
        if (namelist[i]->d_type == DT_LNK)
        {
            struct stat buf;
            if (fstatat(dirfd(dir), namelist[i]->d_name, &buf, AT_NO_AUTOMOUNT) != 0)
            {
                AI_LOG_SYS_ERROR(errno, "failed to stat '%s'", namelist[i]->d_name);
                continue;
            }

            if (!S_ISREG(buf.st_mode))
            {
                // symlink doesn't point to a regular file so skip it
                continue;
            }
        }
        else if (namelist[i]->d_type != DT_REG)
        {
            // the entry is not a regular file so skip it
            continue;
        }

        // try and dlopen it
        char libPath[PATH_MAX];
        snprintf(libPath, sizeof(libPath), "%s/%s", mPluginPath.c_str(), namelist[i]->d_name);

        void *libHandle = dlopen(libPath, RTLD_LAZY | RTLD_LOCAL);
        if (libHandle == nullptr)
        {
            AI_LOG_ERROR("Plugin %s failed to load with error %s\n", namelist[i]->d_name, dlerror());
            continue;
        }

        // check if it contains the register function

        // These are both the same signature, but exist so we can determine
        // quickly if it's a plugin or logger (sub-class of plugin)
        void *libCreateFn = dlsym(libHandle, "createIDobbyRdkPlugin");
        void *libDestroyFn = dlsym(libHandle, "destroyIDobbyRdkPlugin");

        void *libCreateLoggerFn = dlsym(libHandle, "createIDobbyRdkLogger");
        void *libDestroyLoggerFn = dlsym(libHandle, "destroyIDobbyRdkLogger");

        const bool isPlugin = (libCreateFn != nullptr) && (libDestroyFn != nullptr);
        const bool isLogger = (libCreateLoggerFn != nullptr) && (libDestroyLoggerFn != nullptr);

        if (!isPlugin && !isLogger)
        {
            dlclose(libHandle);
            AI_LOG_DEBUG("%s does not contain create/destroy functions, skipping...\n", namelist[i]->d_name);
            continue;
        }


        std::shared_ptr<IDobbyRdkPlugin> plugin;
        std::shared_ptr<IDobbyRdkLoggingPlugin> logger;
        if (isPlugin)
        {
            // execute the register function ... fingers crossed
            typedef IDobbyRdkPlugin *(*CreatePluginFunction)(std::shared_ptr<rt_dobby_schema>& containerConfig,
                                                            const std::shared_ptr<DobbyRdkPluginUtils> &util,
                                                            const std::string &rootfsPath,
                                                            const std::string &hookStdin);
            typedef void (*DestroyPluginFunction)(IDobbyRdkPlugin *);

            CreatePluginFunction createFunc = reinterpret_cast<CreatePluginFunction>(libCreateFn);
            DestroyPluginFunction destroyFunc = reinterpret_cast<DestroyPluginFunction>(libDestroyFn);

            std::shared_ptr<IDobbyRdkPlugin> loadedPlugin(createFunc(mContainerConfig, mUtils, mRootfsPath, mHookStdin),
                                                    destroyFunc);

            plugin = std::move(loadedPlugin);
        }
        else if (isLogger)
        {
            // execute the register function ... fingers crossed
            typedef IDobbyRdkLoggingPlugin *(*CreateLoggerFunction)(std::shared_ptr<rt_dobby_schema>& containerConfig,
                                                            const std::shared_ptr<DobbyRdkPluginUtils> &util,
                                                            const std::string &rootfsPath,
                                                            const std::string &hookStdin);
            typedef void (*DestroyLoggerFunction)(IDobbyRdkLoggingPlugin *);

            CreateLoggerFunction createFunc = reinterpret_cast<CreateLoggerFunction>(libCreateLoggerFn);
            DestroyLoggerFunction destroyFunc = reinterpret_cast<DestroyLoggerFunction>(libDestroyLoggerFn);

            std::shared_ptr<IDobbyRdkPlugin> loadedPlugin(createFunc(mContainerConfig, mUtils, mRootfsPath, mHookStdin),
                                                    destroyFunc);

            std::shared_ptr<IDobbyRdkLoggingPlugin> loadedLogger(createFunc(mContainerConfig, mUtils, mRootfsPath, mHookStdin),
                                                     destroyFunc);

            logger = std::move(loadedLogger);
            plugin = std::move(loadedPlugin);
        }


        if (!plugin)
        {
            AI_LOG_WARN("plugin for library '%s' failed to register", libPath);
            dlclose(libHandle);
            continue;
        }

        std::string pluginName = plugin->name();
        if (pluginName.empty())
        {
            AI_LOG_WARN("plugin for library '%s' returned an invalid name", libPath);
            plugin.reset();
            dlclose(libHandle);
            continue;
        }

        // Plugin names aren't case sensitive, so convert to lowercase
        std::transform(pluginName.begin(), pluginName.end(), pluginName.begin(), ::tolower);

        // it's all good in the hood, so add the library handle and plugin to
        // the internal maps
        std::map<std::string,
                 std::pair<void *,
                 std::shared_ptr<IDobbyRdkPlugin>>>::iterator it = mPlugins.find(pluginName);
        if (it != mPlugins.end())
        {
            AI_LOG_WARN("already had a plugin called '%s', replacing with new "
                        "one from '%s'",
                        pluginName.c_str(), libPath);

            it->second.second.reset();

            if (isLogger)
            {
                auto iter = mLoggers.find(pluginName);
                if(iter != mLoggers.end())
                {
                    iter->second.second.reset();
                    mLoggers.erase(iter);
                }
            }

            //  destruct the pointer first then close the library as
            // the destructor needs to be called from the library.
            dlclose(it->second.first);
            mPlugins.erase(it);
        }

        mPlugins[pluginName] = std::make_pair(libHandle, plugin);
        if (isLogger)
        {
            mLoggers[pluginName] = std::make_pair(libHandle, logger);
        }

        AI_LOG_INFO("Loaded plugin '%s' from '%s'\n", pluginName.c_str(), libPath);

        free(namelist[i]);
    }

    free(namelist);
    closedir(dir);
    close(dirFd);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Get the logger with the name, or nullptr if no plugin
 *
 *  @param[in]  name        The name of the plugin.
 *
 *  @return The plugin interface shared pointer.
 */
std::shared_ptr<IDobbyRdkLoggingPlugin> DobbyRdkPluginManager::getLogger(const std::string &name) const
{
    // Plugins names are case-insensitive - use lowercase
    std::string lowercaseName;
    lowercaseName.resize(name.length());
    std::transform(name.begin(), name.end(), lowercaseName.begin(), ::tolower);

    std::shared_ptr<IDobbyRdkLoggingPlugin> logger;

    auto it = mLoggers.find(lowercaseName);
    if (it != mLoggers.end())
    {
        logger = it->second.second;
    }

    return logger;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Get the logging plugin specified in the container config. Each
 *  container can only have a single plugin for logging, otherwise there is
 *  a chance of conflicts whilst reading the container stdout/err
 *
 *  @param[in]  name        The name of the plugin.
 *
 *  @return The logging plugin
 */
std::shared_ptr<IDobbyRdkLoggingPlugin> DobbyRdkPluginManager::getContainerLogger() const
{
    AI_LOG_FN_ENTRY();

    // Get a list of all the logging plugins loaded from disk
    auto loadedLoggers = listLoadedLoggers();

     // Get all the plugins listed in the container config
    if (mContainerConfig == nullptr || mContainerConfig->rdk_plugins == nullptr)
    {
        AI_LOG_ERROR("Container config is null");
        AI_LOG_FN_EXIT();
        return nullptr;
    }

    const auto pluginsInConfig = mContainerConfig->rdk_plugins->names_of_plugins;
    const int rdkPluginCount = mContainerConfig->rdk_plugins->plugins_count;

    // Work out which plugins specified in the config are loggers
    std::shared_ptr<IDobbyRdkLoggingPlugin> containerLogger;
    std::string pluginName;
    for (int i = 0; i < rdkPluginCount; i++)
    {
        pluginName = pluginsInConfig[i];

        auto it = std::find(loadedLoggers.begin(), loadedLoggers.end(), pluginName);
        if (it != loadedLoggers.end())
        {
            containerLogger = getLogger(pluginName);
            // Break as we've found a logger - if there are others, we can't use
            // them so ignore them
            break;
        }
    }

    if (!containerLogger)
    {
        AI_LOG_WARN("No suitable logging plugin found for container '%s'", mContainerConfig->hostname);
    }
    return containerLogger;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Get the plugin with the name, or nullptr if no plugin
 *
 *  @param[in]  name        The name of the plugin.
 *
 *  @return The plugin interface shared pointer.
 */
std::shared_ptr<IDobbyRdkPlugin> DobbyRdkPluginManager::getPlugin(const std::string &name) const
{
    // Plugins names are case-insensitive - use lowercase
    std::string lowercaseName;
    lowercaseName.resize(name.length());
    std::transform(name.begin(), name.end(), lowercaseName.begin(), ::tolower);

    std::shared_ptr<IDobbyRdkPlugin> plugin;

    auto it = mPlugins.find(lowercaseName);
    if (it != mPlugins.end())
    {
        plugin = it->second.second;
    }

    return plugin;
}

// -----------------------------------------------------------------------------
/**
 * @brief Just return a list of all loaded logging plugin names
 *
 */
const std::vector<std::string> DobbyRdkPluginManager::listLoadedLoggers() const
{
    std::vector<std::string> loggerNames;
    loggerNames.reserve(mLoggers.size());
    for (auto const &plugin : mLoggers)
    {
        loggerNames.push_back(plugin.first);
    }
    return loggerNames;
}


// -----------------------------------------------------------------------------
/**
 * @brief Just return a list of all loaded plugin names
 *
 */
const std::vector<std::string> DobbyRdkPluginManager::listLoadedPlugins() const
{
    std::vector<std::string> pluginNames;
    pluginNames.reserve(mPlugins.size());
    for (auto const &plugin : mPlugins)
    {
        pluginNames.push_back(plugin.first);
    }
    return pluginNames;
}

bool DobbyRdkPluginManager::isLoaded(const std::string &pluginName) const
{
    return getPlugin(pluginName) != nullptr;
}

// -----------------------------------------------------------------------------
/**
 * @brief Check if a plugin implements the specified hook
 *
 * @param[in]   pluginName  The name of the plugin to check
 * @param[in]   hook        The hook to check if the plugin implements
 *
 * @return True if plugin implements specified hook
 */
bool DobbyRdkPluginManager::implementsHook(const std::string &pluginName,
                                           const IDobbyRdkPlugin::HintFlags hook) const
{
    // If plugin isn't loaded, we don't know if it implements hook
    std::shared_ptr<IDobbyRdkPlugin> plugin = getPlugin(pluginName);
    if (!plugin)
    {
        AI_LOG_ERROR("Plugin %s isn't loaded", pluginName.c_str());
        return false;
    }

    if (plugin)
    {
        unsigned pluginHookHints = plugin->hookHints();
        return pluginHookHints & hook;
    }
    return false;
}

// -----------------------------------------------------------------------------
/**
 * Runs the specified hook for a given plugin
 *
 * @param[in]   pluginName      Name of the plugin to run
 * @param[in]   hook            Which hook to execute
 *
 * @return True if the hook executed successfully
 */
bool DobbyRdkPluginManager::executeHook(const std::string &pluginName,
                                        const IDobbyRdkPlugin::HintFlags hook) const
{
    AI_LOG_FN_ENTRY();

    // If plugin isn't loaded, then we can't run it!
    std::shared_ptr<IDobbyRdkPlugin> plugin = getPlugin(pluginName);
    if (!plugin)
    {
        AI_LOG_ERROR("Cannot execute hook as plugin %s isn't loaded", pluginName.c_str());
        AI_LOG_FN_EXIT();
        return false;
    }

    // We know that plugins are derived from RdkPluginBase which includes
    // base implementations of all hooks, so even if the hint flags are wrong,
    // it's safe to call any hook
    switch (hook)
    {
    case IDobbyRdkPlugin::HintFlags::PostInstallationFlag:
        return plugin->postInstallation();
    case IDobbyRdkPlugin::HintFlags::PreCreationFlag:
        return plugin->preCreation();
    case IDobbyRdkPlugin::HintFlags::CreateContainerFlag:
        return plugin->createContainer();
    case IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag:
        return plugin->createRuntime();
#ifdef USE_STARTCONTAINER_HOOK
    case IDobbyRdkPlugin::HintFlags::StartContainerFlag:
        return plugin->startContainer();
#endif
    case IDobbyRdkPlugin::HintFlags::PostStartFlag:
        return plugin->postStart();
    case IDobbyRdkPlugin::HintFlags::PostHaltFlag:
        return plugin->postHalt();
    case IDobbyRdkPlugin::HintFlags::PostStopFlag:
        return plugin->postStop();
    default:
        AI_LOG_ERROR_EXIT("Could not work out which hook method to call");
        return false;
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief Run the plugins specified in the container config at the given hook point.
 * Returns true if all required plugins execute successfully. If non-required plugins
 * fail or are not loaded, then it logs an error but continues running other plugins
 *
 * @param[in]   hookPoint   Which hook point to execute
 *
 * @return True if all required plugins ran successfully
 *
 */
bool DobbyRdkPluginManager::runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint) const
{
    AI_LOG_FN_ENTRY();

    // Get the hook name as string, mostly just for logging purposes
    std::string hookName;
    switch (hookPoint)
    {
    case IDobbyRdkPlugin::HintFlags::PostInstallationFlag:
        hookName = "postInstallation";
        break;
    case IDobbyRdkPlugin::HintFlags::PreCreationFlag:
        hookName = "preCreation";
        break;
    case IDobbyRdkPlugin::HintFlags::CreateContainerFlag:
        hookName = "createContainer";
        break;
    case IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag:
        hookName = "createRuntime";
        break;
#ifdef USE_STARTCONTAINER_HOOK
    case IDobbyRdkPlugin::HintFlags::StartContainerFlag:
        hookName = "startContainer";
        break;
#endif
    case IDobbyRdkPlugin::HintFlags::PostStartFlag:
        hookName = "postStart";
        break;
    case IDobbyRdkPlugin::HintFlags::PostHaltFlag:
        hookName = "postHalt";
        break;
    case IDobbyRdkPlugin::HintFlags::PostStopFlag:
        hookName = "postStop";
        break;
    default:
        AI_LOG_ERROR_EXIT("Unknown Hook Point");
        return false;
    }

    bool success = true;

    // Get all the plugins listed in the container config
    if (mContainerConfig == nullptr || mContainerConfig->rdk_plugins == nullptr)
    {
        AI_LOG_ERROR_EXIT("Container spec is null");
        return false;
    }

    const auto pluginsInConfig = mContainerConfig->rdk_plugins->names_of_plugins;
    const int rdkPluginCount = mContainerConfig->rdk_plugins->plugins_count;

    std::string pluginName;

    // Run all the plugins
    for (size_t i = 0; i < rdkPluginCount; i++)
    {
        pluginName = pluginsInConfig[i];

        const bool required = mContainerConfig->rdk_plugins->required_plugins[i];

        if (required && !isLoaded(pluginName))
        {
            // If the plugin is required, but isn't loaded, then fail early and don't
            // run any more plugins
            // TODO:: Implement a more graceful fallback to default plugin implementation
            AI_LOG_ERROR_EXIT("Required plugin %s isn't loaded", pluginName.c_str());
            return false;
        }
        else if (!isLoaded(pluginName))
        {
            // If it's not required, but isn't loaded, then log but carry on
            AI_LOG_WARN("Non-required plugin %s isn't loaded. Continuing running other plugins.", pluginName.c_str());
            continue;
        }
        else if (!implementsHook(pluginName, hookPoint))
        {
            // If the plugin doesn't need to do anything at this hook point, skip
            AI_LOG_INFO("Plugin %s has nothing to do at %s", pluginName.c_str(), hookName.c_str());
            continue;
        }

        // Everything looks good, run the plugin
        AI_LOG_INFO("Running %s plugin", pluginName.c_str());
        success = executeHook(pluginName,
                              hookPoint);

        // If the plugin has failed and is required, don't bother running any
        // other plugins. If it's not required, just log it
        if (!success && required)
        {
            AI_LOG_ERROR("Required plugin %s %s hook has failed", pluginName.c_str(), hookName.c_str());
            AI_LOG_FN_EXIT();
            return false;
        }
        else if (!success && !required)
        {
            AI_LOG_WARN("Non-required plugin %s %s hook has failed. Continuing running other plugins.",
                        pluginName.c_str(), hookName.c_str());
        }
        else
        {
            AI_LOG_INFO("Plugin %s has %s hook run successfully", pluginName.c_str(), hookName.c_str());
        }
    }
    AI_LOG_FN_EXIT();
    return true;
}