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
 * File:   DobbyPluginManager.cpp
 *
 */
#include "DobbyPluginManager.h"
#include "IDobbyPlugin.h"
#include "IDobbyEnv.h"
#include "IDobbyUtils.h"
#include "DobbyAsync.h"

#include <Logging.h>
#include <Tracing.h>

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



DobbyPluginManager::DobbyPluginManager(const std::shared_ptr<IDobbyEnv>& env,
                                       const std::shared_ptr<IDobbyUtils>& utils,
                                       const std::string& path /*= std::string(DEFAULT_PLUGIN_PATH)*/)
    : mRwLock(PTHREAD_RWLOCK_INITIALIZER)
    , mEnvironment(env)
    , mUtilities(utils)
{
    AI_LOG_FN_ENTRY();

    pthread_rwlock_init(&mRwLock, nullptr);

    loadPlugins(path);

    AI_LOG_FN_EXIT();
}

DobbyPluginManager::~DobbyPluginManager()
{
    AI_LOG_FN_ENTRY();

    // destruct the plugins; note you need to take care to destruct the pointer
    // first then close the library as the destructor needs to be called from
    // the library.
    std::map<std::string,
             std::pair<void*,
                       std::shared_ptr<IDobbyPlugin>>>::iterator it = mPlugins.begin();
    for (; it != mPlugins.end(); ++it)
    {
        it->second.second.reset();
        dlclose(it->second.first);
    }

    mPlugins.clear();

    pthread_rwlock_destroy(&mRwLock);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Scans the given path for any shared objects that implement the
 *  plugin entry points.
 *
 *  This calls dlopen() on all the executable files in the given path (although
 *  it doesn't recurse into subdirs), if the file has symbols createIDobbyPlugin
 *  and destroyIDobbyPlugin then it's deemed to be a 'dobby' plugin.
 *
 *  If loaded successfully the plugins are stored in an internal map, keyed
 *  off the plugin name.
 *
 *  @param[in]  path            The path to scan for hook libraries.
 *
 */
void DobbyPluginManager::loadPlugins(const std::string& path)
{
    AI_LOG_FN_ENTRY();

    int dirFd = open(path.c_str(), O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open dir '%s'", path.c_str());
        return;
    }

    DIR* dir = fdopendir(dirFd);
    if (dir == nullptr)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open dir '%s'", path.c_str());
        close(dirFd);
        return;
    }

    // iterate through all the files in the directory
    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // if a symlink verify that the thing we're pointing to is a file
        if (entry->d_type == DT_LNK)
        {
            struct stat buf;
            if (fstatat(dirfd(dir), entry->d_name, &buf, AT_NO_AUTOMOUNT) != 0)
            {
                AI_LOG_SYS_ERROR(errno, "failed to stat '%s'", entry->d_name);
                continue;
            }

            if (!S_ISREG(buf.st_mode))
            {
                // symlink doesn't point to a regular file so skip it
                continue;
            }
        }
        else if (entry->d_type != DT_REG)
        {
            // the entry is not a regular file so skip it
            continue;
        }

        // check if the file is executable
        if (faccessat(dirfd(dir), entry->d_name, X_OK, 0) != 0)
        {
            continue;
        }

        // try and dlopen it
        char libPath[PATH_MAX];
        snprintf(libPath, sizeof(libPath), "%s/%s", path.c_str(), entry->d_name);

        void* libHandle = dlopen(libPath, RTLD_LAZY | RTLD_LOCAL);
        if (libHandle == nullptr)
        {
            continue;
        }

        // check if it contains the register function
        void* libCreateFn = dlsym(libHandle, "createIDobbyPlugin");
        void* libDestroyFn = dlsym(libHandle, "destroyIDobbyPlugin");
        if ((libCreateFn == nullptr) || (libDestroyFn == nullptr))
        {
            dlclose(libHandle);
            continue;
        }



        // execute the register function ... fingers crossed
        typedef IDobbyPlugin*(*CreatePluginFunction)(const std::shared_ptr<IDobbyEnv>& env,
                                                     const std::shared_ptr<IDobbyUtils>& utils);
        typedef void(*DestroyPluginFunction)(IDobbyPlugin*);

        CreatePluginFunction createFunc = reinterpret_cast<CreatePluginFunction>(libCreateFn);
        DestroyPluginFunction destroyFunc = reinterpret_cast<DestroyPluginFunction>(libDestroyFn);

        std::shared_ptr<IDobbyPlugin> plugin(createFunc(mEnvironment, mUtilities),
                                             destroyFunc);
        if (!plugin)
        {
            AI_LOG_WARN("plugin for library '%s' failed to register", libPath);
            dlclose(libHandle);
            continue;
        }

        const std::string pluginName = plugin->name();
        if (pluginName.empty())
        {
            AI_LOG_WARN("plugin for library '%s' returned an invalid name", libPath);
            dlclose(libHandle);
            continue;
        }


        // it's all good in the hood, so add the library handle and plugin to
        // the internal map
        std::map<std::string,
                 std::pair<void*,
                           std::shared_ptr<IDobbyPlugin>>>::iterator it = mPlugins.find(pluginName);
        if (it != mPlugins.end())
        {
            AI_LOG_INFO("already had a plugin called '%s', replacing with new "
                        "one from '%s'", pluginName.c_str(), libPath);

            it->second.second.reset();
            dlclose(it->second.first);
            mPlugins.erase(it);
        }

        mPlugins[pluginName] = std::make_pair(libHandle, plugin);

        AI_LOG_INFO("loaded plugin '%s' from '%s'", pluginName.c_str(), libPath);
    }

    closedir(dir);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief (re)loads all the plugin libraries found at the given path
 *
 *  Intended for debugging / developing plugins as it allows for reloading
 *  libraries.
 *
 *  @param[in]  path        The path to scan for plugins libraries.
 *
 */
void DobbyPluginManager::refreshPlugins(const std::string& path /*= std::string(DEFAULT_PLUGIN_PATH)*/)
{
    pthread_rwlock_wrlock(&mRwLock);

    loadPlugins(path);

    pthread_rwlock_unlock(&mRwLock);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Get the plugin with the name, or nullptr if no plugin
 *
 *  @param[in]  name        The name of the plugin.
 *
 *  @return The plugin interface shared pointer.
 */
std::shared_ptr<IDobbyPlugin> DobbyPluginManager::getPlugin(const std::string& name) const
{
    std::shared_ptr<IDobbyPlugin> plugin;

    pthread_rwlock_rdlock(&mRwLock);

    std::map<std::string,
             std::pair<void*,
                       std::shared_ptr<IDobbyPlugin>>>::const_iterator it = mPlugins.find(name);
    if (it != mPlugins.end())
    {
        plugin = it->second.second;
    }

    pthread_rwlock_unlock(&mRwLock);

    return plugin;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Calls the supplied hook function for the plugins in the list.
 *
 *  Because the process of executing plugin hooks is the same for all hooks,
 *  we use this function to do the work, supplying it with (effectively) a
 *  function pointer of the hook we want to execute.
 *
 *  This function will iterate over the plugins and call the function pointer
 *  on each plugin object.
 *
 *  The plugins themselves provide a bit-field indicating which hooks need to
 *  be executed, and if they should be executed whether they run in a separate
 *  thread or in the calling thread.  These hints are queried before executing
 *  the supplied hook function.
 *
 *  @param[in]  plugins     A map of plugin names and their data to execute
 *  @param[in]  hookFn      The hook method to call on the plugin
 *  @param[in]  asyncFlag   The bit flag that if the plugin has set indicates
 *                          the hook method should be called asynchronously
 *  @param[in]  syncFlag    The bit flag that if the plugin has set indicates
 *                          the hook method should be called synchronously
 *
 *  @return true if all plugins executed the hook method without failure,
 *  otherwise false.
 */
bool DobbyPluginManager::executeHooks(const std::map<std::string, Json::Value>& plugins,
                                      const HookFn& hookFn,
                                      const unsigned asyncFlag,
                                      const unsigned syncFlag) const
{
    AI_TRACE_EVENT("Plugins", "executeHooks");

    AI_LOG_FN_ENTRY();

    std::list<std::shared_ptr<DobbyAsyncResult>> hookResults;

    // take the lock while iterating over the plugins
    pthread_rwlock_rdlock(&mRwLock);

    // iterate through the plugin list supplied by the caller
    std::map<std::string, Json::Value>::const_iterator it = plugins.begin();
    for (; it != plugins.end(); ++it)
    {
        // get the plugin name and data from the config (originally the json spec)
        const std::string& pluginName = it->first;
        const Json::Value& pluginData = it->second;

        // find the plugin
        std::map<std::string,
            std::pair<void*,
                std::shared_ptr<IDobbyPlugin>>>::const_iterator jt = mPlugins.find(pluginName);
        if (jt == mPlugins.end())
        {
            AI_LOG_WARN("no plugin named '%s'", pluginName.c_str());
            continue;
        }

        // get the plugin pointer
        IDobbyPlugin* plugin = jt->second.second.get();

        // check if the hints indicate we should be running the plugin hook;
        // at all, synchronously or asynchronously
        unsigned hints = plugin->hookHints();
        if (hints & asyncFlag)
        {
            std::shared_ptr<DobbyAsyncResult> result = DobbyAsync(pluginName, hookFn, plugin, pluginData);
            hookResults.emplace_back(std::move(result));
        }
        else if (hints & syncFlag)
        {
            std::shared_ptr<DobbyAsyncResult> result = DobbyDeferred(hookFn, plugin, pluginData);
            hookResults.emplace_front(std::move(result));
        }
    }

    pthread_rwlock_unlock(&mRwLock);


    // the hookResults list contains all the outstanding hook operations, so we
    // now need to wait till they all finish.  If any returns false then we
    // return false (which in some cases will cause runc to abort the container).
    bool result = true;

    for (const std::shared_ptr<DobbyAsyncResult> &hookResult : hookResults)
    {
        // NB deliberately no timeout as we don't have any way to tell a plugin
        // to abort what it's doing and therefore we don't have any recovery
        // mechanism ... so just patiently wait and trust the plugins to do
        // sensible stuff
        if (hookResult->getResult() == false)
            result = false;
    }

    // TODO: add some checks on the total time it took to execute all hooks


    AI_LOG_FN_EXIT();
    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Calls the postConstruction method for the given plugins
 *
 *  The function iterates over the list of plugins and their data and calls
 *  the plugin's postConstruction method.  Each plugin provides hints as to
 *  whether the hook method should be executed at all or synchronously or
 *  asynchronously.
 *
 *  @param[in]  plugins         A map of plugin names and their data to execute.
 *  @param[in]  id              The id of the container.
 *  @param[in]  startupState    The start-up state of the container.
 *  @param[in]  rootfsPath      The path of the rootfs of the container.
 *
 *  @return true if all plugins executed the hook method without failure,
 *  otherwise false.
 */
bool DobbyPluginManager::executePostConstructionHooks(const std::map<std::string, Json::Value>& plugins,
                                                      const ContainerId& id,
                                                      const std::shared_ptr<IDobbyStartState>& startupState,
                                                      const std::string& rootfsPath) const
{
    HookFn hookFn =
        [id, startupState, rootfsPath](IDobbyPlugin *plugin, const Json::Value &data)
        {
            AI_TRACE_EVENT("Plugins", "plugin::PostConstruction",
                           "name", plugin->name());

            return plugin->postConstruction(id, startupState, rootfsPath, data);
        };

    return executeHooks(plugins, hookFn,
                        IDobbyPlugin::PostConstructionAsync,
                        IDobbyPlugin::PostConstructionSync);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Calls the preStart method for the given plugins
 *
 *  The function iterates over the list of plugins and their data and calls
 *  the plugin's preStart method.  Each plugin provides hints as to
 *  whether the hook method should be executed; at all or synchronously or
 *  asynchronously.
 *
 *  @param[in]  plugins         A map of plugin names and their data to execute.
 *  @param[in]  id              The id of the container.
 *  @param[in]  pid             The pid of the init process inside the container.
 *  @param[in]  rootfsPath      The path of the rootfs of the container.
 *
 *  @return true if all plugins executed the hook method without failure,
 *  otherwise false.
 */
bool DobbyPluginManager::executePreStartHooks(const std::map<std::string, Json::Value>& plugins,
                                              const ContainerId& id,
                                              pid_t pid,
                                              const std::string& rootfsPath) const
{
    HookFn hookFn =
        [id, pid, rootfsPath](IDobbyPlugin *plugin, const Json::Value &data)
        {
            AI_TRACE_EVENT("Plugins", "plugin::PreStart",
                           "name", plugin->name());

            return plugin->preStart(id, pid, rootfsPath, data);
        };

    return executeHooks(plugins, hookFn,
                        IDobbyPlugin::PreStartAsync,
                        IDobbyPlugin::PreStartSync);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Calls the postStart method for the given plugins
 *
 *  The function iterates over the list of plugins and their data and calls
 *  the plugin's postStart method.  Each plugin provides hints as to
 *  whether the hook method should be executed; at all or synchronously or
 *  asynchronously.
 *
 *  @param[in]  plugins         A map of plugin names and their data to execute.
 *  @param[in]  id              The id of the container.
 *  @param[in]  pid             The pid of the init process inside the container.
 *  @param[in]  rootfsPath      The path of the rootfs of the container.
 *
 *  @return true if all plugins executed the hook method without failure,
 *  otherwise false.
 */
bool DobbyPluginManager::executePostStartHooks(const std::map<std::string, Json::Value>& plugins,
                                               const ContainerId& id,
                                               pid_t pid,
                                               const std::string& rootfsPath) const
{
    HookFn hookFn =
        [id, pid, rootfsPath](IDobbyPlugin *plugin, const Json::Value &data)
        {
            AI_TRACE_EVENT("Plugins", "plugin::PostStart",
                           "name", plugin->name());

            return plugin->postStart(id, pid, rootfsPath, data);
        };

    return executeHooks(plugins, hookFn,
                        IDobbyPlugin::PostStartAsync,
                        IDobbyPlugin::PostStartSync);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Calls the postStop method for the given plugins
 *
 *  The function iterates over the list of plugins and their data and calls
 *  the plugin's postStop method.  Each plugin provides hints as to
 *  whether the hook method should be executed; at all or synchronously or
 *  asynchronously.
 *
 *  @param[in]  plugins         A map of plugin names and their data to execute.
 *  @param[in]  id              The id of the container.
 *  @param[in]  rootfsPath      The path of the rootfs of the container.
 *
 *  @return true if all plugins executed the hook method without failure,
 *  otherwise false.
 */
bool DobbyPluginManager::executePostStopHooks(const std::map<std::string, Json::Value>& plugins,
                                              const ContainerId& id,
                                              const std::string& rootfsPath) const
{
    HookFn hookFn =
        [id, rootfsPath](IDobbyPlugin *plugin, const Json::Value &data)
        {
            AI_TRACE_EVENT("Plugins", "plugin::PostStop",
                           "name", plugin->name());

            return plugin->postStop(id, rootfsPath, data);
        };

    return executeHooks(plugins, hookFn,
                        IDobbyPlugin::PostStopAsync,
                        IDobbyPlugin::PostStopSync);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Calls the preDestruction method for the given plugins
 *
 *  The function iterates over the list of plugins and their data and calls
 *  the plugin's preDestruction method.  Each plugin provides hints as to
 *  whether the hook method should be executed; at all or synchronously or
 *  asynchronously.
 *
 *  @param[in]  plugins         A map of plugin names and their data to execute.
 *  @param[in]  id              The id of the container.
 *  @param[in]  rootfsPath      The path of the rootfs of the container.
 *
 *  @return true if all plugins executed the hook method without failure,
 *  otherwise false.
 */
bool DobbyPluginManager::executePreDestructionHooks(const std::map<std::string, Json::Value>& plugins,
                                                    const ContainerId& id,
                                                    const std::string& rootfsPath) const
{
    HookFn hookFn =
        [id, rootfsPath](IDobbyPlugin *plugin, const Json::Value &data)
        {
            AI_TRACE_EVENT("Plugins", "plugin::PreDestruction",
                           "name", plugin->name());

            return plugin->preDestruction(id, rootfsPath, data);
        };

    return executeHooks(plugins, hookFn,
                        IDobbyPlugin::PreDestructionAsync,
                        IDobbyPlugin::PreDestructionSync);
}
