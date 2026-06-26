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
#include "DobbyRdkPluginDependencySolver.h"
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
#include <sys/wait.h>
#include <sys/mman.h>
#include <signal.h>

#include <functional>
#include <list>
#include <thread>

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
                                             const std::string &pluginPath,
                                             const std::shared_ptr<DobbyRdkPluginUtils> &utils)
    : mContainerConfig(std::move(containerConfig)),
      mRootfsPath(rootfsPath),
      mPluginPath(pluginPath),
      mUtils(utils),
      mDependencySolver(std::make_unique<DobbyRdkPluginDependencySolver>())
{
    AI_LOG_FN_ENTRY();

    mValid = loadPlugins() && preprocessPlugins();

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

    // FIXME:: During daemon shutdown, due to a race condition in rare circumstances something can still
    // hold a reference to the logging plugin, meaning use_count() is > 1 at this point.
    // If we attempt to dlclose() the plugin in this scenario the daemon will segfault.

    // This shouldn't happen, but we make some safety checks here just in case until
    // the root case is fixed. Don't attempt to dlclose() the plugin whilst a reference
    // is still held elsewhere.
    std::vector<std::string> doNotUnload = {};

    // All loggers are also plugins
    auto itl = mLoggers.begin();
    for (; itl != mLoggers.end(); ++itl)
    {
        if (itl->second.second.use_count() > 1)
        {
            doNotUnload.emplace_back(itl->first);
        }

        itl->second.second.reset();
    }

    mLoggers.clear();

    auto it = mPlugins.begin();
    for (; it != mPlugins.end(); ++it)
    {
        if (it->second.second.use_count() > 1)
        {
            doNotUnload.emplace_back(it->first);
        }

        it->second.second.reset();

        if (doNotUnload.size() > 0)
        {
            // If the plugin is not in the doNotUnload list, close it
            if (std::find(doNotUnload.begin(), doNotUnload.end(), it->first) == doNotUnload.end())
            {
                dlclose(it->second.first);
            }
            else
            {
                AI_LOG_ERROR("Cannot unload plugin %s due to reference still being held", it->first.c_str());
            }
        }
        else
        {
            dlclose(it->second.first);
        }
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
 * @return False if unable to open the given directory, true otherwise.
 */
bool DobbyRdkPluginManager::loadPlugins()
{
    AI_LOG_FN_ENTRY();

    // Check we can access the directory and open it
    int dirFd = open(mPluginPath.c_str(), O_DIRECTORY | O_CLOEXEC);
    if (dirFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open dir '%s'", mPluginPath.c_str());
        return false;
    }

    DIR *dir = fdopendir(dirFd);
    if (dir == nullptr)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open dir '%s'", mPluginPath.c_str());
        close(dirFd);
        return false;
    }

    int directoriesCount = 0;
    struct dirent **namelist = nullptr;

    // Need to sort directories with versionsort so lib.12 would be greater than lib.2
    directoriesCount = scandir(mPluginPath.c_str(), &namelist, 0, versionsort);
    if (directoriesCount < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to scan directory '%s'", mPluginPath.c_str());
        closedir(dir);
        return false;
    }
    
    for(int i=0; i<directoriesCount;i++)
    {
        // if a symlink verify that the thing we're pointing to is a file
        if (namelist[i]->d_type == DT_LNK)
        {
            struct stat buf;
            if (fstatat(dirfd(dir), namelist[i]->d_name, &buf, AT_NO_AUTOMOUNT) != 0)
            {
                AI_LOG_SYS_ERROR(errno, "failed to stat '%s'", namelist[i]->d_name);
                free(namelist[i]);
                continue;
            }

            if (!S_ISREG(buf.st_mode))
            {
                // symlink doesn't point to a regular file so skip it
                free(namelist[i]);
                continue;
            }
        }
        else if (namelist[i]->d_type != DT_REG)
        {
            // the entry is not a regular file so skip it
            free(namelist[i]);
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
            free(namelist[i]);
            continue;
        }


        std::shared_ptr<IDobbyRdkPlugin> plugin;
        std::shared_ptr<IDobbyRdkLoggingPlugin> logger;
        if (isPlugin)
        {
            // execute the register function ... fingers crossed
            typedef IDobbyRdkPlugin *(*CreatePluginFunction)(std::shared_ptr<rt_dobby_schema>& containerConfig,
                                                            const std::shared_ptr<DobbyRdkPluginUtils> &util,
                                                            const std::string &rootfsPath);
            typedef void (*DestroyPluginFunction)(IDobbyRdkPlugin *);

            CreatePluginFunction createFunc = reinterpret_cast<CreatePluginFunction>(libCreateFn);
            DestroyPluginFunction destroyFunc = reinterpret_cast<DestroyPluginFunction>(libDestroyFn);

            std::shared_ptr<IDobbyRdkPlugin> loadedPlugin(createFunc(mContainerConfig, mUtils, mRootfsPath),
                                                    destroyFunc);

            plugin = std::move(loadedPlugin);
        }
        else if (isLogger)
        {
            // execute the register function ... fingers crossed
            typedef IDobbyRdkLoggingPlugin *(*CreateLoggerFunction)(std::shared_ptr<rt_dobby_schema>& containerConfig,
                                                            const std::shared_ptr<DobbyRdkPluginUtils> &util,
                                                            const std::string &rootfsPath);
            typedef void (*DestroyLoggerFunction)(IDobbyRdkLoggingPlugin *);

            CreateLoggerFunction createFunc = reinterpret_cast<CreateLoggerFunction>(libCreateLoggerFn);
            DestroyLoggerFunction destroyFunc = reinterpret_cast<DestroyLoggerFunction>(libDestroyLoggerFn);

            std::shared_ptr<IDobbyRdkPlugin> loadedPlugin(createFunc(mContainerConfig, mUtils, mRootfsPath),
                                                    destroyFunc);

            std::shared_ptr<IDobbyRdkLoggingPlugin> loadedLogger(createFunc(mContainerConfig, mUtils, mRootfsPath),
                                                     destroyFunc);

            logger = std::move(loadedLogger);
            plugin = std::move(loadedPlugin);
        }


        if (!plugin)
        {
            AI_LOG_WARN("plugin for library '%s' failed to register", libPath);
            dlclose(libHandle);
            free(namelist[i]);
            continue;
        }

        std::string pluginName = plugin->name();
        if (pluginName.empty())
        {
            AI_LOG_WARN("plugin for library '%s' returned an invalid name", libPath);
            plugin.reset();
            dlclose(libHandle);
            free(namelist[i]);
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

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Prepares the dependency solver and required plugins data structures.
 *
 *  This method scans the container config and based on its contents:
 *  1. Adds all the plugins, along with their dependencies, to the plugin dependency solver,
 *  2. Creates a list of the required plugins,
 *  3. Checks if the required plugins are loaded.
 *
 *  @return False if a required plugin is not loaded or if one of the dependencies is not a known plugin.
 *  True otherwise.
 */
bool DobbyRdkPluginManager::preprocessPlugins()
{
    AI_LOG_FN_ENTRY();

    // Get all the plugins listed in the container config
    if (mContainerConfig == nullptr || mContainerConfig->rdk_plugins == nullptr)
    {
        AI_LOG_ERROR_EXIT("Container spec is null");
        return false;
    }

    const auto pluginsInConfig = mContainerConfig->rdk_plugins->names_of_plugins;
    const size_t rdkPluginCount = mContainerConfig->rdk_plugins->plugins_count;

    // Add plugins to the solver, remember which ones are required.
    for (size_t i = 0; i < rdkPluginCount; i++)
    {
        const std::string pluginName = pluginsInConfig[i];
        const bool required = mContainerConfig->rdk_plugins->required_plugins[i];
        if (required)
        {
            mRequiredPlugins.insert(pluginName);
        }

        mDependencySolver->addPlugin(pluginName);
    }

    // Check if required plugins are loaded, store plugin dependencies.
    for (size_t i = 0; i < rdkPluginCount; i++)
    {
        const std::string pluginName = pluginsInConfig[i];
        if (!isLoaded(pluginName))
        {
            if (isRequired(pluginName))
            {
                AI_LOG_ERROR_EXIT("Required plugin %s isn't loaded, but present in the container config - aborting", pluginName.c_str());
                return false;
            }
            else
            {
                AI_LOG_WARN("Plugin %s isn't loaded, but present in the container config", pluginName.c_str());
                continue;
            }
        }

        std::shared_ptr<IDobbyRdkPlugin> plugin = getPlugin(pluginName);
        const std::vector<std::string> pluginDependencies = plugin->getDependencies();
        for (const std::string &dependencyName : pluginDependencies)
        {
            if (!mDependencySolver->addDependency(pluginName, dependencyName))
            {
                // This can happen if the name of the dependency is not a name of a plugin defined in the container spec.
                // The spec is invalid. Abort.
                AI_LOG_ERROR_EXIT("Failed to register dependency %s->%s - aborting", pluginName.c_str(), dependencyName.c_str());
                return false;
            }
        }
    }

    AI_LOG_FN_EXIT();
    return true;
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
        AI_LOG_WARN("No suitable logging plugin found for container '%s'", mUtils->getContainerId().c_str());
    }
    return containerLogger;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Set the exit status of the container
 *
 *  @param[in]  status        The exit status of the container.
 *
 *  @return void
 */
void DobbyRdkPluginManager::setExitStatus(int status) const
{
    mUtils->exitStatus = status;
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

/**
 * @brief Check if a plugin is loaded.
 *
 * @param[in]   pluginName The name of the plugin to check.
 *
 * @return True if a plugin is loaded, false if not.
 */
bool DobbyRdkPluginManager::isLoaded(const std::string &pluginName) const
{
    return getPlugin(pluginName) != nullptr;
}

/**
 * @brief Check if a plugin is required.
 *
 * @param[in]   pluginName The name of the plugin to check.
 *
 * @return True if a plugin is required, false if not.
 */
bool DobbyRdkPluginManager::isRequired(const std::string &pluginName) const
{
    return mRequiredPlugins.count(pluginName) != 0;
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

    unsigned pluginHookHints = plugin->hookHints();
    return pluginHookHints & hook;
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
 * Runs the specified hook for a given plugin, checks if execution takes
 * less than timeoutMs value, and if so kills the process.
 *
 * @param[in]   pluginName      Name of the plugin to run
 * @param[in]   hook            Which hook to execute
 * @param[in]   timeoutMs       Timeout value in miliseconds
 *
 * @return True if the hook executed successfully
 */
bool DobbyRdkPluginManager::executeHookTimeout(const std::string &pluginName,
                                                const IDobbyRdkPlugin::HintFlags hook,
                                                const uint timeoutMs) const
{
    int status;
    pid_t exitedPid;
    pid_t workerPid;
    pid_t timeoutPid;
    const size_t sharedMemorySize = 1;

    // Create shared memory to get result from hook execution
    void* sharedMemory = mmap(NULL, sharedMemorySize, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (sharedMemory == MAP_FAILED)
    {
        AI_LOG_ERROR_EXIT("Failed to allocate shared memory");
        return false;
    }

    // Set result as fail in case we need to kill worker
  
    *static_cast<char*>(sharedMemory) = 0;

    workerPid = fork();
    if (workerPid == 0)
    {
        // Create a new SID for the child process
        if (setsid() < 0)
            _exit(EXIT_FAILURE);

        char result = static_cast<char>(executeHook(pluginName, hook));
         // Set result in shared memory
        *static_cast<char*>(sharedMemory) = result;
        _exit(0);
    }
    else if (workerPid < 0)
    {
        AI_LOG_ERROR_EXIT("Failed to fork for executeHookTimeout");
        munmap(sharedMemory, sharedMemorySize);
        return false;
    }

    timeoutPid = fork();
    if (timeoutPid == 0)
    {
        struct timespec timeout_val, remaining;
        timeout_val.tv_nsec = (long)(timeoutMs % 1000) * 1000000;
        timeout_val.tv_sec = timeoutMs / 1000;
        // In case signal comes during wait
        while (nanosleep(&timeout_val, &remaining) && errno == EINTR)
        {
            timeout_val = remaining;
        }

        _exit(0);
    }
    
    // Wait for either worker or timeout to finish
    do
    {
        exitedPid = TEMP_FAILURE_RETRY(wait(&status));
        if (exitedPid >= 0 &&
            exitedPid != timeoutPid &&
            exitedPid != workerPid)
        {
            AI_LOG_DEBUG("Found non-waited process with pid %d", exitedPid);
        }
    } while (exitedPid >= 0 &&
             exitedPid != timeoutPid &&
             exitedPid != workerPid);

    if (exitedPid == timeoutPid)
    {
        // Timeout occurred
        AI_LOG_ERROR("Timeout executing plugin %s hookpoint %s",
                     pluginName.c_str(), HookPointToString(hook).c_str());

      // Check if we can kill workerPid (if it ended already
      // then we will be unable to kill)
      if (kill(workerPid, 0) == -1)
      {
        // Cannot kill process, probably already dead
        // treat it as if it would return proper waitpid
        AI_LOG_DEBUG("Cannot kill after timeout");
        exitedPid = waitpid(workerPid, &status, WNOHANG);
      }
      else
      {
        // Worker is stuck, we need to kill whole group
        // in case any child process was stuck too
        AI_LOG_DEBUG("Can kill after timeout");
        killpg(workerPid, SIGKILL);
        // Collect the worker process
        pid_t waited = waitpid(workerPid, &status, 0);
        if (waited == -1)
        {
          AI_LOG_SYS_WARN(errno, "Failed to wait for workerPid after killing");
        }
        // Collect child of worker if any
        wait(nullptr);
      }
    }
    else if (exitedPid == workerPid)
    {
        kill(timeoutPid, SIGKILL);
        wait(nullptr);
    }
    // get result and free shared memory
    bool result = static_cast<bool>(*static_cast<char*>(sharedMemory));
    munmap(sharedMemory, sharedMemorySize);
    return result;
}

// -----------------------------------------------------------------------------
/**
 * Converts hook point into human readable string
 *
 * @param[in]   hook            Which hook to translate
 *
 * @return std::string with representation, empty if not found.
 */
std::string DobbyRdkPluginManager::HookPointToString(const IDobbyRdkPlugin::HintFlags &hookPoint) const
{
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
    }
    return hookName;
}

// -----------------------------------------------------------------------------
/**
 * @brief Run the plugins specified in the container config at the given hook point.
 * Returns true if all required plugins execute successfully. If non-required plugins
 * fail or are not loaded, then it logs an error but continues running other plugins
 *
 * @param[in]   hookPoint   Which hook point to execute
 * @param[in]   timeoutMs   Timeout in miliseconds, if 0 (default value) then there
 *                          will be no timeout.
 *
 * @return True if all required plugins ran successfully
 *
 */
bool DobbyRdkPluginManager::runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint,
                                       const uint timeoutMs /*=0*/) const
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_ERROR_EXIT("Container config invalid. Plugins will not be run");
        return false;
    }

    // Get the hook name as string, mostly just for logging purposes
    std::string hookName = HookPointToString(hookPoint);
    if(hookName.empty())
    {
        return false;
    }

    // Determine the order of launching based on the dependencies.
    std::vector<std::string> launchOrder;
    if (hookPoint < IDobbyRdkPlugin::HintFlags::PostHaltFlag)
    {
        launchOrder = mDependencySolver->getOrderOfDependency();
    }
    else
    {
        // Reverse the order for the shutdown hooks, so that the plugins
        // on which other plugins depend are shut down later.
        launchOrder = mDependencySolver->getReversedOrderOfDependency();
    }
    if (launchOrder.empty())
    {
        const bool pluginsRequested = (mContainerConfig->rdk_plugins->plugins_count != 0);
        if (pluginsRequested)
        {
            // There are plugins in the container spec, but no plugin names in the launch order vector.
            // This means the solver has detected wrong dependencies (cycles).
            AI_LOG_ERROR_EXIT("Plugin dependency errors detected. Aborting");
            return false;
        }
        else
        {
            AI_LOG_WARN("No plugins to run");
            return true;
        }
    }

    // Run all the plugins
    for (const std::string &pluginName : launchOrder)
    {
        if (!implementsHook(pluginName, hookPoint))
        {
            // If the plugin doesn't need to do anything at this hook point, skip
            AI_LOG_INFO("Plugin %s has nothing to do at %s", pluginName.c_str(), hookName.c_str());
            continue;
        }

        bool success = false;
        // Everything looks good, run the plugin
        AI_LOG_INFO("Running %s plugin", pluginName.c_str());
        if (timeoutMs != 0)
        {
            success = executeHookTimeout(pluginName, hookPoint, timeoutMs);
        }
        else
        {
            success = executeHook(pluginName, hookPoint);
        }

        // If the plugin has failed and is required, don't bother running any
        // other plugins. If it's not required, just log it
        const bool required = isRequired(pluginName);
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
