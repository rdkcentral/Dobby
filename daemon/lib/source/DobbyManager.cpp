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
 * File:   DobbyManager.cpp
 *
 */

#include "DobbyManager.h"
#include "DobbyContainer.h"
#include "DobbyEnv.h"
#include "DobbyLegacyPluginManager.h"
#include "DobbyRdkPluginManager.h"
#include "DobbyRunC.h"
#include "DobbyRootfs.h"
#include "DobbyBundleConfig.h"
#include "DobbyBundle.h"
#include "DobbyStartState.h"
#include "DobbyStream.h"
#include "DobbyStats.h"
#include "DobbyFileAccessFixer.h"
#include "DobbyAsync.h"
#include "DobbyHibernate.h"

#if defined(LEGACY_COMPONENTS)
#  include "DobbySpecConfig.h"
#endif // defined(LEGACY_COMPONENTS)

#include <DobbyProtocol.h>
#include <Logging.h>
#include <Tracing.h>

#include <rt_dobby_schema.h>

#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <fstream>
#include <unordered_map>
#include <chrono>
#include <thread>

#if defined(USE_SYSTEMD)
    #include <systemd/sd-daemon.h>
#endif

// The following are supported by all sky kernels, but some toolchains (ST)
// aren't built against the correct kernel headers, hence need to define these
#ifndef PR_SET_CHILD_SUBREAPER
#  define PR_SET_CHILD_SUBREAPER 36
#endif
#ifndef PR_GET_CHILD_SUBREAPER
#  define PR_GET_CHILD_SUBREAPER 37
#endif

// Can override the plugin path at build time by setting -DPLUGIN_PATH=/path/to/plugins/
#ifndef PLUGIN_PATH
    #define PLUGIN_PATH     "/usr/lib/plugins/dobby"
#endif

DobbyManager::DobbyManager(const std::shared_ptr<IDobbyEnv> &env,
                           const std::shared_ptr<IDobbyUtils> &utils,
                           const std::shared_ptr<IDobbyIPCUtils> &ipcUtils,
                           const std::shared_ptr<const IDobbySettings> &settings,
                           const ContainerStartedFunc &containerStartedCb,
                           const ContainerStoppedFunc &containerStoppedCb,
                           const ContainerHibernatedFunc& containerHibernatedCb,
                           const ContainerHibernatedFunc& containerAwokenCb)
    : mContainerStartedCb(containerStartedCb)
    , mContainerStoppedCb(containerStoppedCb)
    , mContainerHibernatedCb(containerHibernatedCb)
    , mContainerAwokenCb(containerAwokenCb)
    , mEnvironment(env)
    , mUtilities(utils)
    , mIPCUtilities(ipcUtils)
    , mSettings(settings)
    , mLogger(std::make_unique<DobbyLogger>(settings))
    , mRunc(std::make_unique<DobbyRunC>(utils, settings))
    , mRuncMonitorTerminate(false)
    , mCleanupTaskTimerId(0)
#if defined(LEGACY_COMPONENTS)
    , mLegacyPlugins(new DobbyLegacyPluginManager(env, utils))
#endif // defined(LEGACY_COMPONENTS)
{
    AI_LOG_FN_ENTRY();

    setupSystem();

    setupWorkspace(env);

    cleanupContainers();

    startRuncMonitorThread();

    AI_LOG_FN_EXIT();
}

DobbyManager::~DobbyManager()
{
    // Intentially stop monitoring for container termination before cleaning up
    // so we can force container cleanup to be synchronous and deterministic
    stopRuncMonitorThread();

    cleanupContainersShutdown();

    if (mCleanupTaskTimerId > 0)
    {
        mUtilities->cancelTimer(mCleanupTaskTimerId);
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Configures the linux system for enabling features needed for runc
 *
 *  This method is equivalent to performing the following on the cmdline
 *
 *      ulimit -c unlimited
 *
 *      echo "1" > /proc/sys/net/ipv4/ip_forward
 *
 *
 *  See the comments in the code for why each step is needed
 */
void DobbyManager::setupSystem()
{
    AI_LOG_FN_ENTRY();

    // make us a subreaper SUBREAPER!! which means we get the SIGCHLD signal
    // for the fork/exec descendants (i.e. runc cmdline tool) we've spawned
    if (prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to set PR_SET_CHILD_SUBREAPER");
    }

    // set the core dump ulimit to un-limited, this is needed to get core
    // dumps from apps within containers ... and for if this daemon dies.
    // note that is can be overridden by the 'rlimits' field in the OCI
    // json spec file
    struct rlimit coreLimit;
    coreLimit.rlim_cur = coreLimit.rlim_max = RLIM_INFINITY;
    if (setrlimit(RLIMIT_CORE, &coreLimit) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to set RLIMIT_CORE");
    }

    // globally enable ipv4 forwarding, this is what libvirt does and it
    // seems selectively enabling forwarding on only the interfaces we
    // control doesn't seem to work (intermittently)
    if (!mUtilities->writeTextFile("/proc/sys/net/ipv4/ip_forward", "1\n",
                                   O_TRUNC | O_WRONLY, 0))
    {
        AI_LOG_FATAL("failed to write to ip_forward file, you may have issues"
                     " with container networking");
    }

    // finally cisco, in their infinite hardening wisdom, keep monkeying around
    // with access permissions, so here we reset everything to sensible values
    DobbyFileAccessFixer fileFixer;
    fileFixer.fixIt();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Configures the workspace directory
 *
 *  The supplied path will be created if it doesn't exist.  It should be on
 *  writable mount point with an adequate amount of space available.
 *
 *  The workspace is where bundle directories are created for each container
 *  and also where various temporary files are created.  However the runc
 *  tool doesn't use it and log files are written to /var/log/...
 *
 *  TODO: show tree of workspace layout
 *
 *  @param[in]  path        The absolute path to the workspace
 */
void DobbyManager::setupWorkspace(const std::shared_ptr<IDobbyEnv> &env)
{
    AI_LOG_FN_ENTRY();

    // the workspace path in the environment is the top level tmpfs mount, we
    // want to create a subdirectory under that for all dobby stuff
    std::string path = env->workspaceMountPath();
    path += "/dobby";

    if ((mkdir(path.c_str(), 0755) != 0) && (errno != EEXIST))
    {
        AI_LOG_SYS_FATAL_EXIT(errno, "failed to create workspace dir '%s'",
                              path.c_str());
        return;
    }
    else if (chmod(path.c_str(), 0755) != 0)
    {
        AI_LOG_SYS_WARN(errno, "failed to set the workspace mode to 0755");
    }

    // create a directory for the bundles
    path += "/bundles";
    if ((mkdir(path.c_str(), 0755) != 0) && (errno != EEXIST))
    {
        AI_LOG_SYS_ERROR(errno, "failed to make '%s' directory", path.c_str());
    }
    else if (chmod(path.c_str(), 0755) != 0)
    {
        AI_LOG_SYS_WARN(errno, "failed to set 0755 mode on '%s' dir",
                        path.c_str());
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Cleans up a container that is in an unknown state - used at Dobby startup to ensure the box
 * is in a clean state with no leftover containers
 *
 * @param[in]   container   Container info returned by crun list command
 */
bool DobbyManager::cleanupContainer(const DobbyRunC::ContainerListItem &container)
{
    DobbyRunC::ContainerStatus status = container.status;

    if (status == DobbyRunC::ContainerStatus::Paused ||
        status == DobbyRunC::ContainerStatus::Pausing ||
        status == DobbyRunC::ContainerStatus::Running)
    {
        /*
        There have been scenarios where SIGKILL doesn't work. Retry killing the container a
        few times. If the container is still running, then we can't attempt to destroy
        it (destroy will just hang forever)

        Seems to occur when a process gets stuck in an uninterruptible sleep
        */
        const int maxRetry = 4;
        int retryCount = 1;
        auto retryTime = std::chrono::milliseconds(50);

        while (retryCount <= maxRetry)
        {
            AI_LOG_INFO("attempting to kill old container '%s' (attempt %d/%d)", container.id.c_str(), retryCount, maxRetry);
            mRunc->killCont(container.id, SIGKILL, true);

            // Did we actually kill it? Give it some time, then check the status
            std::this_thread::sleep_for(retryTime * retryCount);
            DobbyRunC::ContainerStatus state = mRunc->state(container.id);

            if (state != DobbyRunC::ContainerStatus::Running)
            {
                // Managed to kill the container, mark it as stopped so we destroy it next
                AI_LOG_INFO("Successfully killed old container '%s", container.id.c_str());
                status = DobbyRunC::ContainerStatus::Stopped;
                // Quit the retry loop
                break;
            }

            AI_LOG_WARN("Failed to kill container '%s' (attempt %d/%d)", container.id.c_str(), retryCount, maxRetry);

            if (retryCount >= maxRetry)
            {
                // We can't kill the container. This will leave dobby in a potentially bad state since there is a container
                // running that is stuck somewhere between life and death. However this is better than the whole daemon
                // locking up completely (and being killed by watchdog repeatedly)
                return false;
            }

            retryCount++;
        }
    }

    if (status == DobbyRunC::ContainerStatus::Created ||
        status == DobbyRunC::ContainerStatus::Stopped ||
        status == DobbyRunC::ContainerStatus::Unknown)
    {
        // Attempt to run the postHalt hook to clean up anything done by the container plugins
        // Since the bundle may not exist, load the config file from the crun copy
        char configPath[PATH_MAX];
        snprintf(configPath, sizeof(configPath), "%s/%s/config.json", mRunc->getWorkingDir().c_str(), container.id.c_str());

        parser_error err;
        auto containerConfig = std::shared_ptr<rt_dobby_schema>(rt_dobby_schema_parse_file(configPath, nullptr, &err), free_rt_dobby_schema);
        if (containerConfig.get() == nullptr || err)
        {
            AI_LOG_WARN("Couldn't load container confirm from %s, cannot run postHalt hook for %s", configPath, container.id.c_str());
        }
        else
        {
            // Got a good config
            // Work out the rootfs path (if it exists)
            std::string rootfsDirPath;
            if (containerConfig->root->path[0] == '/')
            {
                rootfsDirPath = std::string(containerConfig->root->path) + "/";
            }
            else
            {
                // relative path to rootfs
                rootfsDirPath = std::string(container.bundlePath) + "/" + containerConfig->root->path + "/";
            }

            if (access(rootfsDirPath.c_str(), R_OK) != 0)
            {
                AI_LOG_WARN("Cannot access container rootfs @ '%s' - postHalt hooks may fail", rootfsDirPath.c_str());
            }

            auto rdkPluginUtils = std::make_shared<DobbyRdkPluginUtils>(containerConfig, container.id.str());
            auto rdkPluginManager = std::make_shared<DobbyRdkPluginManager>(containerConfig, rootfsDirPath.c_str(), PLUGIN_PATH, rdkPluginUtils);

            // Attempt to run the postHalt hook for the container
            if (!rdkPluginManager->runPlugins(IDobbyRdkPlugin::HintFlags::PostHaltFlag, 4000))
            {
                AI_LOG_ERROR("Failure in postHalt hook");
            }
        }

        // Now attempt to actually delete the container
        std::shared_ptr<DobbyBufferStream> buffer = std::make_shared<DobbyBufferStream>();
        AI_LOG_INFO("attempting to destroy old container '%s'", container.id.c_str());
        // Dobby will try a normal delete, then a force delete
        // Force delete may hang on old crun versions if process in uninterruptible sleep: https://github.com/containers/crun/issues/868
        if (!mRunc->destroy(container.id, buffer))
        {
            AI_LOG_ERROR_EXIT("Could not destroy container %s with error %s", container.id.c_str(), buffer->getBuffer().data());
            return false;
        }
        else
        {
            AI_LOG_INFO("Successfully destroyed old container '%s", container.id.c_str());
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets a list of running containers and tries to kill and delete them.
 *
 *  Will run the postHalt hook for the container where possible (some hooks might
 *  fail as the container bundle cannot be guaranteed to exist at this time)
 *
 *  Designed as a crash-recovery mechanism as we should clean up all our containers
 *  if the daemon shut down gracefully
 *
 */
void DobbyManager::cleanupContainers()
{
    AI_LOG_FN_ENTRY();

    // Do a manual check for leftover containers ourselves to improve startup performance
    // For consistency, we should really call out to crun list here, but in some scenarios
    // that can fail. Since we're only interested in finding out if the container is running
    // or stopped, do it ourselves
    const std::string workDir = mRunc->getWorkingDir();
    DIR *d = opendir(workDir.c_str());
    if (!d)
    {
        AI_LOG_SYS_WARN(errno, "Could not access %s dir", workDir.c_str());
    }

    int count = 0;
    struct dirent *entry = {};
    while ((entry = readdir(d)) != nullptr)
    {
        // Skip "." and ".." dirs
        if ((entry->d_name[0] == '.') && ((entry->d_name[1] == '\0') ||
                                          ((entry->d_name[1] == '.') && (entry->d_name[2] == '\0'))))
        {
            continue;
        }

        if (entry->d_type == DT_DIR)
        {
            count++;
        }
    }
    closedir(d);

    // No old containers - return
    if (count == 0)
    {
        return;
    }

    AI_LOG_INFO("%d old containers found - attempting to clean up", count);

    /* We've got some old containers, try to clean them up

    There are a few important caveats here since we are likely recovering from a crash situation at this point.
    The main consideration is that we can't guarantee the container bundle will actually still exist on disk. We'll
    attempt to run the postHalt and postStop plugins, but they might throw errors if they try to do
    anything with the rootfs. */

    int stuckContainerCount = 0;
    const std::list<DobbyRunC::ContainerListItem> containers = mRunc->list();
    for (const auto &container : containers)
    {
#if defined(USE_SYSTEMD)
        // Wag the watchdog each time we go round here, since we haven't started the watchdog wagging thread
        // yet and if we have many containers this could take some time...
        sd_notify(0, "WATCHDOG=1");
#endif

        AI_LOG_WARN("found old container '%s' with pid %d in state %d, cleaning it up",
                    container.id.c_str(), container.pid, int(container.status));

        bool cleanupSuccess = cleanupContainer(container);

        // If the container is stuck (i.e. we can't kill or destroy it), then
        // add it in the Unknown state so we can't attempt to start a container with
        // the same ID again
        if (!cleanupSuccess)
        {
            AI_LOG_FATAL("Failed to clean up container '%s'. We may be unable to launch app until next reboot!", container.id.c_str());

            // Track the container so we can't start a container with the same name again
            // A background thread will handle cleaning it up if/when it eventually dies
            stuckContainerCount++;

            std::unique_ptr<DobbyContainer> dobbyContainer(new DobbyContainer(nullptr, nullptr, nullptr));
            dobbyContainer->state = DobbyContainer::State::Unknown;
            dobbyContainer->containerPid = container.pid;

            mContainers.emplace(container.id, std::move(dobbyContainer));
        }
    }

    if (stuckContainerCount > 0)
    {
        // Try to clean up the container later so the user can restart the app again
        AI_LOG_INFO("%d containers are stuck and can't be destroyed. Starting regular cleanup job", stuckContainerCount);
        mCleanupTaskTimerId = mUtilities->startTimer(std::chrono::seconds(10),
                                false,
                                std::bind(&DobbyManager::invalidContainerCleanupTask, this));
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Gracefully stops and cleans up any running containers. Will emit the container stop
 * event when a container stops.
 *
 * Designed to be called when the daemon is going down (e.g. SIGTERM)
 *
 */
void DobbyManager::cleanupContainersShutdown()
{
    AI_LOG_FN_ENTRY();

    AI_LOG_INFO("Dobby shutting down - stopping %lu containers", mContainers.size());

    auto it = mContainers.begin();
    while (it != mContainers.end())
    {
        if ((it->second->state == DobbyContainer::State::Running) || \
            (it->second->state == DobbyContainer::State::Paused))
        {
            AI_LOG_INFO("Stopping container %s", it->first.c_str());
            // By calling the "proper" stop method here, any listening services will be
            // notified of the container stop event
            if (!stopContainer(it->second->descriptor, false))
            {
                // As DobbyRunC::killCont already handles problem of masked SIGTERM in
                // case we failed to stop it means that it tried to SIGKILL too, so
                // container must be in uninterrable sleep and we cannot do anything
                // Remove it container from the list (even though it wasn't clean up)
                // to avoid repeating indefinitely. It will be cleaned on boot-up
                it = mContainers.erase(it);
                AI_LOG_ERROR("Failed to stop container %s. Will attempt to clean up at daemon restart", it->first.c_str());
            }
            else
            {
                // This would normally be done async by the runc monitor thread, but we're
                // shutting down so we want to run synchronously
                handleContainerTerminate(it->first, it->second, 0);
                it = mContainers.erase(it);
            }
        }
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Get the instance of the logging plugin for the current container (if
 * one is loaded)
 *
 */
std::shared_ptr<IDobbyRdkLoggingPlugin> DobbyManager::GetContainerLogger(const std::unique_ptr<DobbyContainer> &container)
{
    if (container->rdkPluginManager)
    {
        auto loggingPlugin = container->rdkPluginManager->getContainerLogger();

        if (!loggingPlugin)
        {
            AI_LOG_WARN("No logging plugin is specified in the container config"
                        " - container logs will not be handled");
        }
        return loggingPlugin;
    }
    return nullptr;
}

/**
 * @brief Create and start a container. Set up and capture logs from all
 * container hooks if an RDK logging plugin is loaded.
 *
 * If container->customConfigFilePath is set, the container will use that
 * config.json file instead of the onein the bundle
 *
 * @param[in] id          id of the container
 * @param[in] container   The object that wraps up the container details.
 * @param[in] files       List of fds to preserve inside the container
 */
bool DobbyManager::createAndStart(const ContainerId &id,
                                  const std::unique_ptr<DobbyContainer> &container,
                                  const std::list<int> &files)
{
    AI_LOG_FN_ENTRY();

    // Create the container, but don't start it yet
    auto loggingPlugin = GetContainerLogger(container);
    std::shared_ptr<DobbyBufferStream> createBuffer = std::make_shared<DobbyBufferStream>();

    auto pids = mRunc->create(id,
                              container->bundle,
                              createBuffer,
                              files,
                              container->customConfigFilePath);


    // First PID = crun
    // Second PID = DobbyInit (same as container.pid)
    if (pids.second < 0)
    {
        AI_LOG_ERROR("Failed to create container - see crun log for more details");

        // Dump the runtime output to a new file even if the container failed to start
        if (loggingPlugin)
        {
            mLogger->DumpBuffer(createBuffer->getMemFd(), -1, loggingPlugin);
        }

        container->containerPid = -1;
        return false;
    }
    container->containerPid = pids.second;

#if defined(LEGACY_COMPONENTS)
    // Run the legacy Dobby PreStart hooks (to be removed once RDK plugin work is complete)
    if (!onPreStartHook(id, container))
    {
        AI_LOG_ERROR("failure in one of the PreStart hooks");
        return false;
    }
#endif //defined(LEGACY_COMPONENTS)

    // if we've survived to this point then the container is pretty much
    // already to go, so move it's state to Running
    container->state = DobbyContainer::State::Running;

    // Attempt to start the container
    std::shared_ptr<DobbyBufferStream> startBuffer = std::make_shared<DobbyBufferStream>();
    bool started = mRunc->start(id, startBuffer);

    if (!started)
    {
        AI_LOG_ERROR("Failed to start container '%s'", id.c_str());
    }

    // Dump the hook logs and start the main logging thread if started. Have to wait until
    // now since the startContainer logs are tied to the create process
    if (loggingPlugin)
    {
        // Dump the contents of the buffers even if the container didn't start
        mLogger->DumpBuffer(createBuffer->getMemFd(), container->containerPid, loggingPlugin);
        mLogger->DumpBuffer(startBuffer->getMemFd(), container->containerPid, loggingPlugin);

        if (started)
        {
            mLogger->StartContainerLogging(id.str(), pids.first, pids.second, loggingPlugin);
        }
    }

    AI_LOG_FN_EXIT();
    return started;
}

/**
 * @brief Updates the container config with custom options provided by the start
 * command
 *
 * @param[in] command       The custom command to run instead of the args in the
 *                          config file
 * @param[in] displaySocket Path to a westeros socket to mount into the container
 * @param[in] envVars       Custom env vars to add to the container
 *
 * @return true if modifications were made, false if no changes made
 */
bool DobbyManager::customiseConfig(const std::shared_ptr<DobbyConfig> &config,
                                    const std::string &command,
                                    const std::string &displaySocket,
                                    const std::vector<std::string>& envVars)
{
    AI_LOG_FN_ENTRY();

    bool changesMade = false;

    // If we've been given a custom command, replace args[] with the custom command
    if (!command.empty())
    {
       config->changeProcessArgs(command);
       changesMade = true;
    }

    // If we've been given a displaySocket, then add the mount it into the container
    // Will always be mounted to /tmp/westeros in container
    if (!displaySocket.empty())
    {
        config->addWesterosMount(displaySocket);
        changesMade = true;
    }

    // Add any extra environment variables
    if (envVars.size() > 0)
    {
        for (const auto &var : envVars)
        {
            config->addEnvironmentVar(var);
        }
        changesMade = true;
    }

    if (shouldEnableSTrace(config))
    {
        // Start container with strace.
        // It should be done in customiseConfig so that change in strace params
        // would not require to reinstall container bundle.
        config->enableSTrace(mSettings->straceSettings().logsDir);
        changesMade = true;
    }

    AI_LOG_FN_EXIT();
    return changesMade;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates and attempts to start the container.
 *
 *
 *  @param[in]  id          The id string for the container.
 *  @param[in]  container   The object that wraps up the container details.
 *  @param[in]  startState  The start state object of the files.
 *  @param[in]   command    The custom command to run instead of the args in the
 *                          config file
 *
 *  @return true on success, false on failure.
 */
bool DobbyManager::createAndStartContainer(const ContainerId &id,
                                           const std::unique_ptr<DobbyContainer> &container,
                                           const std::list<int> &files)
{
    AI_LOG_FN_ENTRY();

    if (createAndStart(id, container, files))
    {
        AI_LOG_INFO("container '%s' started, controller process pid %d",
                    id.c_str(), container->containerPid);

#if defined(LEGACY_COMPONENTS)
        // call the postStart hook, don't care about the return code
        // for now
        onPostStartHook(id, container);
#endif //defined(LEGACY_COMPONENTS)

        // signal that the container has started
        if (mContainerStartedCb)
        {
            mContainerStartedCb(container->descriptor, id);
        }

        AI_LOG_FN_EXIT();
        return true;
    }

    // If the PID is < 0, something went wrong during container creation and
    // start was never attempted
    if (container->containerPid < 0)
    {
        AI_LOG_WARN("Something went wrong when creating '%s'", id.c_str());
    }
    else
    {
        // PID > 0 so container was created but failed to start
        AI_LOG_WARN("Something went wrong when starting '%s', cleaning up", id.c_str());

        // Something went wrong during container start, clean up everything
        // kill the container created
        if (!mRunc->killCont(id, SIGKILL))
        {
            AI_LOG_ERROR("failed to kill (non-running) container for '%s'",
                        id.c_str());
        }


        // wait for the half-started container to terminate
        if (waitpid(container->containerPid, nullptr, 0) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "error waiting for the container '%s' to terminate",
                            id.c_str());
        }

#if defined(LEGACY_COMPONENTS)
        // either the container failed to start, or one of the preStart hooks
        // failed, either way we want to call the postStop hook
        onPostStopHook(id, container);
#endif //defined(LEGACY_COMPONENTS)

        // once we're here we mark the container as Stopping, however the container
        // object is not removed from the list until the crun parent process has
        // actually terminated
        container->state = DobbyContainer::State::Stopping;

        // if we dropped out here it means something has gone wrong, but the
        // container was created, so destroy it
        std::shared_ptr<DobbyBufferStream> destroyBuffer = std::make_shared<DobbyBufferStream>();
        if (!mRunc->destroy(id, destroyBuffer))
        {
            AI_LOG_ERROR("failed to destroy '%s'", id.c_str());
        }

        auto loggingPlugin = GetContainerLogger(container);
        if (loggingPlugin)
        {
            mLogger->DumpBuffer(destroyBuffer->getMemFd(), container->containerPid, loggingPlugin);
        }

        // clear the pid now it's been killed
        container->containerPid = -1;
    }

    // Call the postHalt hook to clean up from the creation (preCreation,
    // createRuntime, createContainer) hooks
    if (container->config->rdkPlugins().size() > 0)
    {
        onPostHaltHook(container);
    }

    AI_LOG_FN_EXIT();
    return false;
}

#if defined(LEGACY_COMPONENTS)
// -----------------------------------------------------------------------------
/**
 *  @brief Where the magic begins .... attempts to create a container
 *  from a Dobby spec file.
 *
 *  @param[in]  id          The id string for the container
 *  @param[in]  jsonSpec    The sky json spec with the container details
 *  @param[in]  files       A list of file descriptors to pass into the
 *                          container, can be empty.
 *  @param[in]  command     The custom command to run instead of the args in the
 *                          config file (optional)
 *
 *  @return a container descriptor, which is just a unique number that
 *  identifies the container.
 */
int32_t DobbyManager::startContainerFromSpec(const ContainerId &id,
                                             const std::string &jsonSpec,
                                             const std::list<int> &files,
                                             const std::string &command,
                                             const std::string &displaySocket,
                                             const std::vector<std::string>& envVars = std::vector<std::string>())
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // the first step is to check we don't already have a container with the
    // given id
    if (mContainers.count(id) > 0)
    {
        AI_LOG_ERROR_EXIT("trying to start a container for '%s' that is already running",
                          id.c_str());
        return -1;
    }

    // create a bundle directory
    std::shared_ptr<DobbyBundle> bundle =
        std::make_shared<DobbyBundle>(mUtilities, mEnvironment, id);
    if (!bundle || !bundle->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create bundle");
        return -1;
    }

    // parse the json config
    std::shared_ptr<DobbySpecConfig> config =
        std::make_shared<DobbySpecConfig>(mUtilities, mSettings, id, bundle, jsonSpec);
    if (!config || !config->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create config object from OCI bundle config");
        return -1;
    }

    // create a (populated) rootfs directory within the bundle from the config
    std::shared_ptr<DobbyRootfs> rootfs =
        std::make_shared<DobbyRootfs>(mUtilities, bundle, config);
    if (!rootfs || !rootfs->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create rootfs");
        return -1;
    }

    // create a 'start state' object that wraps the file descriptors
    std::shared_ptr<DobbyStartState> startState =
        std::make_shared<DobbyStartState>(config, files);
    if (!startState || !startState->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create 'start state' object");
        return -1;
    }

    // Set Apparmor profile
    if (mSettings->apparmorSettings().enabled)
    {
        config->setApparmorProfile(mSettings->apparmorSettings().profileName);
    }

    // Set pids limit
    if (mSettings->pidsSettings().enabled)
    {
        config->setPidsLimit(mSettings->pidsSettings().limit);
    }

    // Load the RDK plugins from disk (if necessary)
    std::map<std::string, Json::Value> rdkPlugins = config->rdkPlugins();
    AI_LOG_DEBUG("There are %zd rdk plugins to run", rdkPlugins.size());

    std::unique_ptr<DobbyContainer> container;
    if (rdkPlugins.size() > 0)
    {
        const std::string rootfsPath = rootfs->path();

        std::shared_ptr<rt_dobby_schema> containerConfig(config->config());
        auto rdkPluginUtils = std::make_shared<DobbyRdkPluginUtils>(config->config(), startState, id.str());
        auto rdkPluginManager = std::make_shared<DobbyRdkPluginManager>(containerConfig, rootfsPath, PLUGIN_PATH, rdkPluginUtils);

        std::vector<std::string> loadedPlugins = rdkPluginManager->listLoadedPlugins();
        AI_LOG_DEBUG("Loaded %zd RDK plugins\n", loadedPlugins.size());

        // Create the container wrapper
        std::unique_ptr<DobbyContainer> dobbyContainer(new DobbyContainer(bundle, config, rootfs, rdkPluginManager));
        container = std::move(dobbyContainer);
    }
    else
    {
        // Create the container wrapper
        std::unique_ptr<DobbyContainer> dobbyContainer(new DobbyContainer(bundle, config, rootfs));
        container = std::move(dobbyContainer);
    }

    // If we have legacy plugins, run their postConstruction hooks before
    // executing crun
    bool pluginFailure = false;
    if (!onPostConstructionHook(id, startState, container))
    {
        AI_LOG_ERROR("failure in one of the PostConstruction hooks");
        pluginFailure = true;
    }

    // If we have RDK plugins, run their postInstallation hooks. Other
    // hooks (excluding preCreate) will be run automatically by crun
    if (!pluginFailure && rdkPlugins.size() > 0)
    {
        if (!onPostInstallationHook(container))
        {
            pluginFailure = true;
        }

        // Run any pre-creation hooks
        // Note: running the hooks here allows these hooks to also modify the
        // config. This is necessary to add envvars etc, but can cause issues
        // when launching multiple containers from the same bundle where the plugin
        // could add duplicate data to the config
        if (!onPreCreationHook(container))
        {
            pluginFailure = true;
        }
    }

    // Don't start if necessary plugins have failed
    if (!pluginFailure)
    {
        // Customise the config if necessary
        customiseConfig(config, command, displaySocket, envVars);

        if (!config->writeConfigJson(bundle->path() + "/config.json"))
        {
            AI_LOG_ERROR("failed to create config.json file");
        }
        else
        {
            // if the respawn flag is set in the spec file then we need to store
            // any file descriptors for use at respawn time
            if (config->restartOnCrash())
            {
                container->setRestartOnCrash(startState->files());
            }

            // try and create and start the container
            if (createAndStartContainer(id, container, startState->files()))
            {
                // get the descriptor of the container and return that to the
                // caller (need to do this before we move into the map)
                int32_t cd = container->descriptor;

                // woo - she's off and running, so move the container object
                // into the map and then we're done
                mContainers.emplace(id, std::move(container));

                AI_LOG_FN_EXIT();
                return cd;
            }
        }
    }

    // not required, but tidy up the start state object so all the file
    // descriptors will be released now
    startState.reset();

    // something went wrong, however we still want to call the preDestruction
    // hook, in case a hook setup some stuff the post-construction phase above
    onPreDestructionHook(id, container);

    AI_LOG_FN_EXIT();
    return -1;
}
#endif //defined(LEGACY_COMPONENTS)

// -----------------------------------------------------------------------------
/**
 *  @brief Where the magic begins ... attempts to create a container from
 *         an OCI bundle*
 *
 *
 *  @param[in]  id          The id string for the container
 *  @param[in]  bundlePath  The absolute path to the OCI bundle*
 *  @param[in]  files       A list of file descriptors to pass into the
 *                          container, can be empty.
 * @param[in]   command     The custom command to run instead of the args in the
 *                          config file (optional)
 *
 *  @return a container descriptor, which is just a unique number that
 *  identifies the container.
 */
int32_t DobbyManager::startContainerFromBundle(const ContainerId &id,
                                               const std::string &bundlePath,
                                               const std::list<int> &files,
                                               const std::string &command,
                                               const std::string &displaySocket,
                                               const std::vector<std::string>& envVars)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // The first step is to check we don't already have a container with the given id
    if (mContainers.count(id) > 0)
    {
        AI_LOG_ERROR_EXIT("trying to start a container for '%s' that is already running",
                          id.c_str());
        return -1;
    }

    // Parse the bundle's json config
    std::shared_ptr<DobbyBundleConfig> config =
        std::make_shared<DobbyBundleConfig>(mUtilities, mSettings, id, bundlePath);
    if (!config || !config->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create config object from OCI bundle config");
        return -1;
    }

    // Populate DobbyBundle object with path to the bundle
    std::shared_ptr<DobbyBundle> bundle =
        std::make_shared<DobbyBundle>(mUtilities, mEnvironment, bundlePath);
    if (!bundle || !bundle->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to populate DobbyBundle");
        return -1;
    }

    // Populate DobbyRootfs object with rootfs path
    std::shared_ptr<DobbyRootfs> rootfs =
        std::make_shared<DobbyRootfs>(mUtilities, bundle, config);
    if (!rootfs || !rootfs->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create rootfs");
        return -1;
    }
    rootfs->setPersistence(true);

    // Create a 'start state' object that wraps the file descriptors
    std::shared_ptr<DobbyStartState> startState =
        std::make_shared<DobbyStartState>(config, files);
    if (!startState || !startState->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create 'start state' object");
        return -1;
    }

    // Set Apparmor profile
    if (mSettings->apparmorSettings().enabled)
    {
        config->setApparmorProfile(mSettings->apparmorSettings().profileName);
    }

    // Set pids limit
    if (mSettings->pidsSettings().enabled)
    {
        config->setPidsLimit(mSettings->pidsSettings().limit);
    }

    // Load the RDK plugins from disk (if necessary)
    std::map<std::string, Json::Value> rdkPlugins = config->rdkPlugins();
    AI_LOG_DEBUG("There are %zd rdk plugins to run", rdkPlugins.size());

    std::unique_ptr<DobbyContainer> container;
    if (rdkPlugins.size() > 0)
    {
        const std::string rootfsPath = rootfs->path();

        std::shared_ptr<rt_dobby_schema> containerConfig(config->config());
        auto rdkPluginUtils = std::make_shared<DobbyRdkPluginUtils>(config->config(), startState, id.str());
        auto rdkPluginManager = std::make_shared<DobbyRdkPluginManager>(containerConfig, rootfsPath, PLUGIN_PATH, rdkPluginUtils);

        std::vector<std::string> loadedPlugins = rdkPluginManager->listLoadedPlugins();
        AI_LOG_DEBUG("Loaded %zd RDK plugins\n", loadedPlugins.size());

        // Create the container wrapper
        std::unique_ptr<DobbyContainer> dobbyContainer(new DobbyContainer(bundle, config, rootfs, rdkPluginManager));
        container = std::move(dobbyContainer);
    }
    else
    {
        // Create the container wrapper
        std::unique_ptr<DobbyContainer> dobbyContainer(new DobbyContainer(bundle, config, rootfs));
        container = std::move(dobbyContainer);
    }

    bool pluginFailure = false;

#if defined(LEGACY_COMPONENTS)
    // If we have legacy plugins, run their postConstruction hooks before
    // executing crun
    if (!onPostConstructionHook(id, startState, container))
    {
        AI_LOG_ERROR("failure in one of the PostConstruction hooks");
        pluginFailure = true;
    }
#endif // defined(LEGACY_COMPONENTS)

    // If we have RDK plugins, run their postInstallation hooks. Other
    // hooks (excluding preCreate) will be run automatically by crun
    if (!pluginFailure && rdkPlugins.size() > 0)
    {
        if (!onPostInstallationHook(container))
        {
            pluginFailure = true;
        }

        // Run any pre-creation hooks
        // Note: running the hooks here allows these hooks to also modify the
        // config. This is necessary to add envvars etc, but can cause issues
        // when launching multiple containers from the same bundle where the plugin
        // could add duplicate data to the config
        if (!onPreCreationHook(container))
        {
            pluginFailure = true;
        }
    }

    // Don't start if necessary plugins have failed
    if (!pluginFailure)
    {
        // can now write the config.json file into the bundle directory
        if (!config->writeConfigJson(bundle->path() + "/config.json"))
        {
            AI_LOG_ERROR("failed to create config.json file");
        }
        else
        {
            // Create a file to mark that preinstallation hooks have run for
            // this container and config.json has been updated accordingly
            if (rdkPlugins.size() > 0)
            {
                const std::string successFlagPath = container->bundle->path() + "/postinstallhooksuccess";
                std::ofstream flag(successFlagPath);
            }

            // if the respawn flag is set in the spec file then we need to store
            // any file descriptors for use at respawn time
            if (config->restartOnCrash())
            {
                container->setRestartOnCrash(startState->files());
            }

            // Create a custom config file for this container with custom options
            if (customiseConfig(config, command, displaySocket, envVars))
            {
                // Write the config to a temp file that is only used for this container launch
                // Will be deleted when the container is destroyed
                std::string tmpConfigPath = container->bundle->path() + "/config-" +
                                            std::to_string(container->descriptor) + ".json";

                if (!config->writeConfigJson(tmpConfigPath))
                {
                    AI_LOG_ERROR_EXIT("Failed to write custom config file to '%s'",
                                tmpConfigPath.c_str());
                    return false;
                }

                container->customConfigFilePath = tmpConfigPath;
                AI_LOG_DEBUG("Created custom config for container '%s' at %s", id.c_str(), container->customConfigFilePath.c_str());
            }

            // try and create and start the container
            if (createAndStartContainer(id, container, startState->files()))
            {
                // get the descriptor of the container and return that to the
                // caller (need to do this before we move into the map)
                int32_t cd = container->descriptor;

                // woo - she's off and running, so move the container object
                // into the map and then we're done
                mContainers.emplace(id, std::move(container));

                AI_LOG_FN_EXIT();
                return cd;
            }
            else
            {
                // If the container was launched from a custom config, delete
                // the custom config, if we succeed to start then cleanup will
                // be done by onChildExit.
                if (!container->customConfigFilePath.empty())
                {
                    if (remove(container->customConfigFilePath.c_str()) != 0)
                    {
                        AI_LOG_SYS_ERROR(errno, "Failed to remove custom config '%s'",
                        container->customConfigFilePath.c_str());
                    }
                }
            }
        }
    }
    else
    {
        // plugin failure detected, postInstallation hook did not run successfully
        // return config file to original state
        std::ifstream src(bundlePath + "/config-dobby.json", std::ios::binary);
        std::ofstream dst(bundlePath + "/config.json", std::ios::binary);
        dst << src.rdbuf();
    }

    // not required, but tidy up the start state object so all the file
    // descriptors will be released now
    startState.reset();

#if defined(LEGACY_COMPONENTS)
    // something went wrong, however we still want to call the preDestruction
    // hook, in case a hook setup some stuff the post-construction phase above
    onPreDestructionHook(id, container);
#endif //defined(LEGACY_COMPONENTS)

    AI_LOG_FN_EXIT();
    return -1;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to restart the container
 *
 *  Called internally when we've detected a container shutdown and the config
 *  has indicated we should try and auto-restart the container.
 *
 *  From the runc tool's POV this is start of a new container.
 *
 *  Note when restarting, we don't regenerate the rootfs or config.json files
 *  for the bundle and the postConstruction hook is not called.  However
 *  the preStart, postStart and postStop hooks will be called.
 *
 *  Any file descriptors that were passed into the container when it was first
 *  launched will be passed in again.  The DobbyContainer object will have
 *  stored them when the container was created the first time.
 *
 *  @warning this function is called from the thread that monitors the child
 *  pids, not the main thread, however it is called with mLock held.
 *
 *  @param[in]  id          The id of the container to re-start.
 *  @param[in]  container   The container object storing the container details.
 *
 *  @return true if runc executed the request, however this doesn't necessarily
 *  mean the container is up and running successfully.
 */
bool DobbyManager::restartContainer(const ContainerId &id,
                                    const std::unique_ptr<DobbyContainer> &container)
{
    AI_LOG_FN_ENTRY();

    // no need to take the mLock, it should already be held

    std::shared_ptr<DobbyBufferStream> bufferStream =
        std::make_shared<DobbyBufferStream>();

    // ask the runc tool to clean up anything it may have left over from the
    // previous run
    if (!mRunc->destroy(id, bufferStream))
    {
        AI_LOG_ERROR("failed to destroy '%s'", id.c_str());
    }
    else
    {
        // Same logic as on container stop
        if (container->rdkPluginManager)
        {
            auto loggingPlugin = container->rdkPluginManager->getContainerLogger();
            // No point trying to stop logging if there was never a
            // logging plugin to run in the first place
            if (loggingPlugin)
            {
                // If main container logging thread is still running,
                // wait for it to finish before we dump the hook output
                // to the log
                mLogger->DumpBuffer(bufferStream->getMemFd(), container->containerPid, loggingPlugin);
            }
        }
    }

    // give everything to runC to try and start the container again
    if (!createAndStartContainer(id, container, container->files()))
    {
        AI_LOG_ERROR_EXIT("failed to restart container");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Stops a running container
 *
 *  If withPrejudice is not specified (the default) then we send the init
 *  process within the container a SIGTERM.
 *
 *  If the withPrejudice is true then we use the SIGKILL signal.
 *
 *  This call is asynchronous, i.e. it is a request to stop rather than a
 *  blocking call that ensures the container is stopped before returning.
 *
 *  The @a mContainerStoppedCb callback will be called when the container
 *  has actually been torn down.
 *
 *  @param[in]  cd              The descriptor of the container to stop.
 *  @param[in]  withPrejudice   If true the container process is killed with
 *                              SIGKILL, otherwise SIGTERM is used.
 *
 *  @return true if a container with a matching id was found and a signal
 *  sent successfully to it.
 */
bool DobbyManager::stopContainer(int32_t cd, bool withPrejudice)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // find the container
    auto it = mContainers.cbegin();
    for (; it != mContainers.cend(); ++it)
    {
        if (it->second && (it->second->descriptor == cd))
            break;
    }

    if (it == mContainers.end())
    {
        AI_LOG_WARN("failed to find container with descriptor %d", cd);
        AI_LOG_FN_EXIT();
        return false;
    }

    const ContainerId &id = it->first;
    const std::unique_ptr<DobbyContainer> &container = it->second;

    // this is an explicit stop request by the user so clear the 'restartOnCrash'
    // flag so the container doesn't auto-respawn
    container->clearRestartOnCrash();

    if (container->state == DobbyContainer::State::Unknown)
    {
        // Container is in an unknown (i.e. bad) state. Don't attempt to stop it
        // (should be cleaned up automatically by background cleanup task)
        AI_LOG_WARN("Container %s is in an unknown state - cannot stop", id.c_str());
        return false;
    }

    // check the state, if we're in the Starting phase then pre-start hasn't run
    // and we just need to set a flag to indicate the pre-start hooks should
    // fail, this is the quickest way to terminate a container in this state
    if (container->state == DobbyContainer::State::Starting)
    {
        container->hasCurseOfDeath = true;
    }

    // if in Running/Hibernated/Hibernating/Awakening state then use runc to send
    // the container's process a signal.  We could just send a kill
    // signal ourselves, but this way we are consistent with the tools
    else if (container->state == DobbyContainer::State::Running ||
             container->state == DobbyContainer::State::Hibernating ||
             container->state == DobbyContainer::State::Hibernated ||
             container->state == DobbyContainer::State::Awakening)
    {
        if (!mRunc->killCont(id, withPrejudice ? SIGKILL : SIGTERM))
        {
            AI_LOG_WARN("failed to send signal to '%s'", id.c_str());
            AI_LOG_FN_EXIT();
            return false;
        }
    }

    // if in the the Stopping state then we don't need to do anything, death
    // is imminent and we just need to let nature take it's course
    else if (container->state == DobbyContainer::State::Stopping)
    {
    }

    // If a container is paused, it must be resumed before it can be stopped
    // Calling runc kill on a paused container won't do anything.
    // As per the OCI spec: "Attempting to send a signal to a container that is
    // neither "created" nor "running" MUST have no effect on the container"
    else if (container->state == DobbyContainer::State::Paused)
    {
        // If we're force stopping, resume the container so it can be stopped
        if (withPrejudice)
        {
            if (!mRunc->resume(id))
            {
                // If we failed to resume the container, we can't stop it, so
                // give up :(
                AI_LOG_WARN("Failed to resume container '%s' so cannot kill it", id.c_str());
                AI_LOG_FN_EXIT();
                return false;
            }

            // Container has been resumed, so kill it now
            if (!mRunc->killCont(id, withPrejudice ? SIGKILL : SIGTERM))
            {
                AI_LOG_WARN("failed to send signal to '%s'", id.c_str());
                AI_LOG_FN_EXIT();
                return false;
            }
        }
        else
        {
            AI_LOG_WARN("'%s' is paused and cannot be killed. Resume it first, or force a stop", id.c_str());
            AI_LOG_FN_EXIT();
            return false;
        }
    }
    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Freezes a running container
 *
 *  Currently we have no use case for pause/resume containers so the method
 *  hasn't been implemented, however when testing manually I've discovered it
 *  actually works quite well.
 *
 *  If wanting to have a play you can run the following on the command line
 *
 *      runc --root /var/run/runc pause <id>
 *
 *  @param[in]  cd      The descriptor of the container to pause.
 *
 *  @return true if a container with a matching descriptor was found and it was
 *  frozen.
 */
bool DobbyManager::pauseContainer(int32_t cd)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // find the container
    auto it = mContainers.cbegin();
    for (; it != mContainers.cend(); ++it)
    {
        if (it->second && (it->second->descriptor == cd))
            break;
    }

    if (it == mContainers.end())
    {
        AI_LOG_WARN("failed to find container with descriptor %d", cd);
        AI_LOG_FN_EXIT();
        return false;
    }

    const ContainerId &id = it->first;
    const std::unique_ptr<DobbyContainer> &container = it->second;

    // We can only pause a container that's currently running
    if (container->state == DobbyContainer::State::Running)
    {
        if (mRunc->pause(id))
        {
            // Set the container state to paused
            container->state = DobbyContainer::State::Paused;
            AI_LOG_FN_EXIT();
            return true;
        }
        AI_LOG_WARN("Failed to pause container '%s'", id.c_str());
        AI_LOG_FN_EXIT();
        return false;
    }

    if (container->state == DobbyContainer::State::Paused)
    {
        AI_LOG_WARN("Container '%s' is already paused", id.c_str());
    }
    else
    {
        AI_LOG_WARN("Container '%s' is not running so could not be paused", id.c_str());
    }

    AI_LOG_FN_EXIT();
    return false;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Thaws a frozen container
 *
 *  @param[in]  cd      The descriptor of the container to resume.
 *
 *  @return true if a container with a matching descriptor was found and it was
 *  resumed.
 */
bool DobbyManager::resumeContainer(int32_t cd)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // Find the container
    auto it = mContainers.cbegin();
    for (; it != mContainers.cend(); ++it)
    {
        if (it->second && (it->second->descriptor == cd))
            break;
    }

    if (it == mContainers.end())
    {
        AI_LOG_WARN("failed to find container with descriptor %d", cd);
        AI_LOG_FN_EXIT();
        return false;
    }

    const ContainerId &id = it->first;
    const std::unique_ptr<DobbyContainer> &container = it->second;

    // We can only resume a container that's currently paused
    if (container->state == DobbyContainer::State::Paused)
    {
        if (mRunc->resume(id))
        {
            // Set the container state to running
            container->state = DobbyContainer::State::Running;
            AI_LOG_FN_EXIT();
            return true;
        }
        AI_LOG_WARN("Failed to resume container '%s'", id.c_str());
        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_WARN("Container '%s' is not paused so could not be resumed", id.c_str());
    AI_LOG_FN_EXIT();
    return false;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Dumps a running container's processes
 *
 *  @param[in]  cd      The descriptor of the container to checkpoint.
 *  @param[in]  options Additional options
 *
 *  @return true if a container with a matching descriptor was found and its
 *  processes were dumped.
 */
bool DobbyManager::hibernateContainer(int32_t cd, const std::string& options)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // find the container
    auto it = mContainers.cbegin();
    for (; it != mContainers.cend(); ++it)
    {
        if (it->second && (it->second->descriptor == cd))
            break;
    }

    if (it == mContainers.end())
    {
        AI_LOG_WARN("failed to find container with descriptor %d", cd);
        AI_LOG_FN_EXIT();
        return false;
    }

    const ContainerId &id = it->first;

    // only 'running' container can be hibernated
    if (it->second->state != DobbyContainer::State::Running)
    {
        AI_LOG_WARN("Container '%s' is not running so could not be hibernated", id.c_str());
        AI_LOG_FN_EXIT();
        return false;
    }

    std::thread hibernateThread =
        std::thread([=]()
        {
            //TODO: --delay support is temporary and should be removed
            int delayMs = 0;
            size_t delayMsPos = options.find("--delay=");
            if (delayMsPos != std::string::npos)
            {
                delayMs = std::stoi(&options[delayMsPos + std::string("--delay=").length()], nullptr, 10);
            }
            DobbyHibernate::Error ret = DobbyHibernate::Error::ErrorNone;

            int delayChunkMs = 100;
            while (delayMs > 0)
            {
                int sleepTime = delayMs > delayChunkMs? delayChunkMs : delayMs;
                delayMs -= sleepTime;
                std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
                {
                    std::lock_guard<std::mutex> locker(mLock);
                    if (mContainers.find(id) == mContainers.end() ||
                        mContainers[id]->descriptor != cd ||
                        mContainers[id]->state != DobbyContainer::State::Hibernating)
                    {
                        AI_LOG_WARN("Hibernation of: %s with descriptor %d aborted", id.c_str(), cd);
                        AI_LOG_FN_EXIT();
                        return;
                    }
                }
            }
            //TODO: --delay support end

            // create a stats object for the container to get list of PIDs
            std::unique_lock<std::mutex> locker(mLock);
            DobbyStats stats(it->first, mEnvironment, mUtilities);
            Json::Value jsonPids = DobbyStats(it->first, mEnvironment, mUtilities).stats()["pids"];
            locker.unlock();

            for (auto pidIt = jsonPids.begin(); pidIt != jsonPids.end(); ++pidIt)
            {
                locker.lock();
                if (mContainers.find(id) == mContainers.end() ||
                    mContainers[id]->descriptor != cd ||
                    mContainers[id]->state != DobbyContainer::State::Hibernating)
                {
                    AI_LOG_WARN("Hibernation of: %s with descriptor %d aborted", id.c_str(), cd);
                    AI_LOG_FN_EXIT();
                    return;
                }
                locker.unlock();

                uint32_t pid = pidIt->asUInt();
                ret  = DobbyHibernate::HibernateProcess(pid);
                if (ret != DobbyHibernate::Error::ErrorNone)
                {
                    AI_LOG_WARN("Error hibernating pid: '%d'", pid);
                    // revert previous Hibernations and break
                    while (pidIt != jsonPids.begin())
                    {
                        --pidIt;
                        pid = pidIt->asUInt();
                        DobbyHibernate::WakeupProcess(pid);
                    }

                    break;
                }
            }

            // update state
            locker.lock();
            if (mContainers.find(id) == mContainers.end() ||
                mContainers[id]->descriptor != cd)
            {
                AI_LOG_WARN("failed to find container: %s with descriptor %d", id.c_str(), cd);
                AI_LOG_FN_EXIT();
                return;
            }

            if (mContainers[id]->state != DobbyContainer::State::Hibernating)
            {
                AI_LOG_WARN("container state (%s) is not hibernating", id.c_str());
                AI_LOG_FN_EXIT();
                return;
            }

            if (ret == DobbyHibernate::Error::ErrorNone)
            {
                mContainers[id]->state = DobbyContainer::State::Hibernated;
                if (mContainerHibernatedCb)
                {
                    mContainerHibernatedCb(cd, id);
                }
            }
            else
            {
                mContainers[id]->state = DobbyContainer::State::Running;
            }
            AI_LOG_FN_EXIT();
        });

    it->second->state = DobbyContainer::State::Hibernating;
    hibernateThread.detach();
    AI_LOG_INFO("Hibernation of: %s triggered", id.c_str());
    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Wakeup a checkpointed container from existing dump.
 *
 *  @param[in]  cd      The descriptor of the container to checkpoint.
 *
 *  @return true if a container was successfully restored.
 */
bool DobbyManager::wakeupContainer(int32_t cd)
{
     AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // find the container
    auto it = mContainers.cbegin();
    for (; it != mContainers.cend(); ++it)
    {
        if (it->second && (it->second->descriptor == cd))
            break;
    }

    if (it == mContainers.cend())
    {
        AI_LOG_WARN("failed to find container with descriptor %d", cd);
        AI_LOG_FN_EXIT();
        return false;
    }

    const ContainerId &id = it->first;

    // only 'hibernated/hibernating' container can be woke up
    if (it->second->state != DobbyContainer::State::Hibernated &&
        it->second->state != DobbyContainer::State::Hibernating)
    {
        AI_LOG_WARN("Container '%s' is not hibernated/hibernating so could not be wakeup", id.c_str());
        AI_LOG_FN_EXIT();
        return false;
    }

    // Awakening state will abort hibernation thread if still running
    it->second->state = DobbyContainer::State::Awakening;

    std::thread wakeupThread =
    std::thread([=]()
    {
        // create a stats object for the container to get list of PIDs
        std::unique_lock<std::mutex> locker(mLock);
        DobbyStats stats(it->first, mEnvironment, mUtilities);
        Json::Value jsonPids = DobbyStats(it->first, mEnvironment, mUtilities).stats()["pids"];
        locker.unlock();
        // try to Wakeup all processes to be sure all is cleaned up
        // and wakeup in revers order
        auto pidIt = jsonPids.end();
        while (pidIt != jsonPids.begin())
        {
            --pidIt;
            uint32_t pid = pidIt->asUInt();
            DobbyHibernate::WakeupProcess(pid);
        }

        // update state
        locker.lock();
        if (mContainers.find(id) == mContainers.end() ||
            mContainers[id]->descriptor != cd)
        {
            AI_LOG_WARN("failed to find container: %s with descriptor %d", id.c_str(), cd);
            AI_LOG_FN_EXIT();
            return;
        }

        if (mContainers[id]->state != DobbyContainer::State::Awakening)
        {
            AI_LOG_WARN("container state (%s) is not awakening", id.c_str());
            AI_LOG_FN_EXIT();
            return;
        }

        mContainers[id]->state = DobbyContainer::State::Running;
        if (mContainerAwokenCb)
        {
            mContainerAwokenCb(cd, id);
        }
        AI_LOG_FN_EXIT();
    });

    wakeupThread.detach();
    AI_LOG_INFO("Wakeup of: %s triggered", id.c_str());
    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Executes a command in a running container
 *
 *  @param[in]  cd          The descriptor of the container to execute the command in.
 *  @param[in]  command     Command to be executed.
 *  @param[in]  options     Options to execute the command with.
 *
 *  @return true if a container with a matching descriptor was found and the command
 *  was run
 */
bool DobbyManager::execInContainer(int32_t cd, const std::string &options, const std::string &command)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // Find the container
    auto it = mContainers.cbegin();
    for (; it != mContainers.cend(); ++it)
    {
        if (it->second && (it->second->descriptor == cd))
            break;
    }

    if (it == mContainers.end())
    {
        AI_LOG_WARN("failed to find container with descriptor %d", cd);
        AI_LOG_FN_EXIT();
        return false;
    }

    const ContainerId &id = it->first;
    const std::unique_ptr<DobbyContainer> &container = it->second;

    // We can only execute in a container that's running
    if (container->state == DobbyContainer::State::Running)
    {
        auto pids = mRunc->exec(id, options, command);
        if (pids.second > 0)
        {
            // If we have a plugin to capture logs, send the output of exec to the
            // plugin
            if (container->rdkPluginManager)
            {
                auto loggingPlugin = container->rdkPluginManager->getContainerLogger();

                if (!loggingPlugin)
                {
                    AI_LOG_WARN("No logging plugin is specified in the container config - exec output will not be captured");
                }
                else
                {
                    // Spin up thread to capture output from the exec command (could be long running)
                    mLogger->StartContainerLogging(id.str(), pids.first, container->containerPid, loggingPlugin);
                }
            }

            // Dobby needs to track this newly launched process so it can
            // clean up after it exists to avoid a zombie
            mContainerExecPids.insert(std::make_pair(id, pids.second));

            AI_LOG_FN_EXIT();
            return true;
        }
        AI_LOG_WARN("Failed to execute the command in container '%s'", id.c_str());
        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_WARN("Container '%s' was not running, command could not be executed", id.c_str());
    AI_LOG_FN_EXIT();
    return false;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns a list of all the containers
 *
 *  The returned list contains the id of all the containers we know about in
 *  their various states.  Just because a container id is in the list it
 *  doesn't necessarily mean it's actually running, it could be in either
 *  the starting or stopping phase.
 *
 *  @see DobbyManager::stateOfContainer for a way to retrieve the
 *  status of the container.
 *
 *  @return a list of all the containers.
 */
std::list<std::pair<int32_t, ContainerId>> DobbyManager::listContainers() const
{
    std::lock_guard<std::mutex> locker(mLock);

    std::list<std::pair<int32_t, ContainerId>> ids;

    auto it = mContainers.cbegin();
    for (; it != mContainers.cend(); ++it)
    {
        ids.emplace_back(it->second->descriptor, it->first);
    }

    return ids;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the state of a given container
 *
 *
 *
 *  @param[in]  cd      The descriptor of the container to get the state of.
 *
 *  @return one of the possible state values.
 */
int32_t DobbyManager::stateOfContainer(int32_t cd) const
{
    std::lock_guard<std::mutex> locker(mLock);

    // find the container
    auto it = mContainers.cbegin();
    for (; it != mContainers.cend(); ++it)
    {
        if (it->second && (it->second->descriptor == cd))
            break;
    }

    if (it == mContainers.end())
    {
        // TODO: is this really a warning ? should we just return a 'stopped'
        // status.
        AI_LOG_WARN("failed to find container with descriptor %d", cd);
    }
    else
    {
        const std::unique_ptr<DobbyContainer> &container = it->second;
        switch (container->state)
        {
            case DobbyContainer::State::Starting:
                return CONTAINER_STATE_STARTING;
            case DobbyContainer::State::Running:
                return CONTAINER_STATE_RUNNING;
            case DobbyContainer::State::Paused:
                return CONTAINER_STATE_PAUSED;
            case DobbyContainer::State::Hibernated:
                return CONTAINER_STATE_HIBERNATED;
            case DobbyContainer::State::Hibernating:
                return CONTAINER_STATE_HIBERNATING;
            case DobbyContainer::State::Awakening:
                return CONTAINER_STATE_AWAKENING;
            case DobbyContainer::State::Stopping:
                return CONTAINER_STATE_STOPPING;
            default:
                return CONTAINER_STATE_INVALID;
        }
    }

    return CONTAINER_STATE_INVALID;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets the stats for the container
 *
 *  This is primarily a debugging method, used to get statistics on the
 *  container and roughly correlates to the 'runc events --stats <id>' call.
 *
 *  The reply is a json formatted string containing some info, it's form may
 *  change over time.
 *
 *      {
 *          "id": "blah",
 *          "state": "running",
 *          "timestamp": 348134887768,
 *          "pids": [ 1234, 1245 ],
 *          "cpu": {
 *              "usage": {
 *                  "total":734236982,
 *                  "percpu":[348134887,386102095]
 *              }
 *          },
 *          "memory":{
 *              "user": {
 *                  "limit":41943040,
 *                  "usage":356352,
 *                  "max":524288,
 *                  "failcnt":0
 *              }
 *          }
 *          "gpu":{
 *              "memory": {
 *                  "limit":41943040,
 *                  "usage":356352,
 *                  "max":524288,
 *                  "failcnt":0
 *              }
 *          }
 *          ...
 *      }
 *
 *  @param[in]  cd      The container descriptor
 *
 *  @return Json formatted string with the info for the container, on failure an
 *  empty string.
 */
std::string DobbyManager::statsOfContainer(int32_t cd) const
{
    std::lock_guard<std::mutex> locker(mLock);

    // find the container
    auto it = mContainers.cbegin();
    for (; it != mContainers.cend(); ++it)
    {
        if (it->second && (it->second->descriptor == cd))
            break;
    }

    if (it == mContainers.end())
    {
        AI_LOG_WARN("failed to find container with descriptor %d", cd);
    }
    else
    {
        // create a stats object for the container
        DobbyStats stats(it->first, mEnvironment, mUtilities);

        // get the raw stats and add the "id" and "state" fields
        Json::Value jsonStats = stats.stats();
        jsonStats["id"] = it->first.str();
        switch (it->second->state)
        {
            case DobbyContainer::State::Starting:
                jsonStats["state"] = "starting";
                break;
            case DobbyContainer::State::Running:
                jsonStats["state"] = "running";
                break;
            case DobbyContainer::State::Stopping:
                jsonStats["state"] = "stopping";
                break;
            case DobbyContainer::State::Paused:
                jsonStats["state"] = "paused";
                break;
            case DobbyContainer::State::Unknown:
                jsonStats["state"] = "unknown";
                break;
            case DobbyContainer::State::Hibernating:
                jsonStats["state"] = "hibernating";
                break;
            case DobbyContainer::State::Hibernated:
                jsonStats["state"] = "hibernated";
                break;
            case DobbyContainer::State::Awakening:
                jsonStats["state"] = "awakening";
                break;
        }

        // convert the json stats to a string and return
        Json::StreamWriterBuilder builder;
        builder["indentation"] = " ";
        return Json::writeString(builder, jsonStats);
    }

    return std::string();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Debugging method to allow you to retrieve the OCI config.json spec
 *  used to create the container
 *
 *
 *  @param[in]  cd      The descriptor of the container to get the config.json of.
 *
 *  @return the config.json string.
 */
std::string DobbyManager::ociConfigOfContainer(int32_t cd) const
{
    std::lock_guard<std::mutex> locker(mLock);

    // find the container
    auto it = mContainers.cbegin();
    for (; it != mContainers.cend(); ++it)
    {
        if (it->second && (it->second->descriptor == cd))
            break;
    }

    if (it == mContainers.end())
    {
        AI_LOG_WARN("failed to find container with descriptor %d", cd);
    }
    else
    {
        const std::unique_ptr<DobbyContainer> &container = it->second;
        return container->config->configJson();
    }

    return std::string();
}

#if defined(LEGACY_COMPONENTS)
// -----------------------------------------------------------------------------
/**
 *  @brief Debugging method to allow you to retrieve the json spec used to
 *  create the container
 *
 *
 *  @param[in]  cd      The descriptor of the container to get the spec of.
 *
 *  @return the json spec string.
 */
std::string DobbyManager::specOfContainer(int32_t cd) const
{
    std::lock_guard<std::mutex> locker(mLock);

    // find the container
    auto it = mContainers.begin();
    for (; it != mContainers.end(); ++it)
    {
        if (it->second && (it->second->descriptor == cd))
            break;
    }

    if (it == mContainers.end())
    {
        AI_LOG_WARN("failed to find container with descriptor %d", cd);
    }
    else
    {
        const std::unique_ptr<DobbyContainer> &container = it->second;

        return container->config->spec();
    }

    return std::string();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Debugging method to allow you to create a bundle with rootfs and
 *  config.json without actually running runc on it.
 *
 *
 *  @param[in]  id          The id of the new bundle to create.
 *  @param[in]  jsonSpec    The spec file to use to generate the rootfs and
 *                          config.json within the bundle.
 *
 *  @return true on success, false on failure.
 */
bool DobbyManager::createBundle(const ContainerId &id,
                                const std::string &jsonSpec)
{
    AI_LOG_FN_ENTRY();

    // create a bundle directory
    std::shared_ptr<DobbyBundle> bundle =
        std::make_shared<DobbyBundle>(mUtilities, mEnvironment, id);
    if (!bundle || !bundle->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create bundle");
        return false;
    }

    // parse the json config
    std::shared_ptr<DobbySpecConfig> config =
        std::make_shared<DobbySpecConfig>(mUtilities, mSettings, bundle, jsonSpec);
    if (!config || !config->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create config object from OCI bundle config");
        return false;
    }

    // create a (populated) rootfs directory within the bundle from the config
    std::shared_ptr<DobbyRootfs> rootfs =
        std::make_shared<DobbyRootfs>(mUtilities, bundle, config);
    if (!rootfs || !rootfs->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create rootfs");
        return false;
    }

    // write out the OCI config json file
    config->writeConfigJson(bundle->path() + "/config.json");

    // set both the bundle and rootfs as persistent so they're not deleted when
    // they are destructed
    rootfs->setPersistence(true);
    bundle->setPersistence(true);

    AI_LOG_FN_EXIT();
    return true;
}
#endif //defined(LEGACY_COMPONENTS)

// -----------------------------------------------------------------------------
/**
 *  @brief Called at the post-installation stage of container startup
 *
 *  Uses the map of rdkPlugin names/data in the container config to determine
 *  which plugins to execute. RDK plugins are loaded from disk in the constructor
 *  of DobbyContainer if any rdk plugins are required
 *
 *  @param[in]  container   Container wrapper object for the container to execute
 *                          the plugins on
 *
 *  @return true if all required postInstallation hooks were successful
 */
bool DobbyManager::onPostInstallationHook(const std::unique_ptr<DobbyContainer> &container)
{
    AI_LOG_FN_ENTRY();

    // Make sure we've initialised rdkPluginManager
    if (container->rdkPluginManager == nullptr)
    {
        AI_LOG_ERROR("Could not run postInstallation hook as plugin manager is null");
        return false;
    }

    // Check if we've run the plugins before
    const std::string successFlagPath = container->bundle->path() + "/postinstallhooksuccess";
    struct stat sb;
    if (stat(successFlagPath.c_str(), &sb) == 0)
    {
        AI_LOG_INFO("PostInstallation hooks have already run - not running again");
        return true;
    }

    // Attempt to run the plugins specified in the config file
    if (!container->rdkPluginManager->runPlugins(IDobbyRdkPlugin::HintFlags::PostInstallationFlag))
    {
        AI_LOG_ERROR("Failure in postInstallation hook");
        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_INFO("Successfully ran postInstallation hook");
    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Called at the pre-create stage of container startup
 *
 *  Uses the map of rdkPlugin names/data in the container config to determine
 *  which plugins to execute. RDK plugins are loaded from disk in the constructor
 *  of DobbyContainer if any rdk plugins are required
 *
 *  @param[in]  container   Container wrapper object for the container to execute
 *                          the plugins on
 *
 *  @return true if all required preCreate hooks were successful
 */
bool DobbyManager::onPreCreationHook(const std::unique_ptr<DobbyContainer> &container)
{
    AI_LOG_FN_ENTRY();

    // Make sure we've initialised rdkPluginManager
    if (container->rdkPluginManager == nullptr)
    {
        AI_LOG_ERROR("Could not run preCreation hook as plugin manager is null");
        return false;
    }

    // Attempt to run the plugins specified in the config file
    if (!container->rdkPluginManager->runPlugins(IDobbyRdkPlugin::HintFlags::PreCreationFlag))
    {
        AI_LOG_ERROR("Failure in preCreation hook");
        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_INFO("Successfully ran preCreation hook");
    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called at the post-halt stage of container startup
 *
 *  Uses the map of rdkPlugin names/data in the container config to determine
 *  which plugins to execute. RDK plugins are loaded from disk in the ctor
 *  of DobbyContainer if any rdk plugins are required
 *
 *  @param[in]  container   Container wrapper object for the container to execute
 *                          the plugins on
 *
 *  @return true if all required postInstallation hooks were successful
 */
bool DobbyManager::onPostHaltHook(const std::unique_ptr<DobbyContainer> &container)
{
    AI_LOG_FN_ENTRY();

    // Make sure we've initialised rdkPluginManager
    if (container->rdkPluginManager == nullptr)
    {
        AI_LOG_ERROR("Could not run postHalt hook as plugin manager is null");
        return false;
    }

    // Attempt to run the plugins specified in the config file. PostHalt hooks cannot modify
    // the config struct so we should be safe to run in the forked process.
    if (!container->rdkPluginManager->runPlugins(IDobbyRdkPlugin::HintFlags::PostHaltFlag, 4000))
    {
        AI_LOG_ERROR("Failure in postHalt hook");
        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_INFO("Successfully ran postHalt hook");
    AI_LOG_FN_EXIT();
    return true;
}

#if defined(LEGACY_COMPONENTS)
// -----------------------------------------------------------------------------
/**
 *  @brief Called after the rootfs is created but before runc has been executed
 *
 *  @warning this function is called with the lock already held.
 *
 *  Here we go though each plugin and ask them to execute their
 *  postConstruction hooks.
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  startState      The object that represents the start-up state.
 *  @param[in]  container       The container wrapper object, stores things
 *                              like the container config and rootfs objects.
 *
 *  @return true if all postConstruction callbacks were called AND all of them
 *  returned true.  If any callback fails then false is returned.
 */
bool DobbyManager::onPostConstructionHook(const ContainerId &id,
                                          const std::shared_ptr<DobbyStartState> &startState,
                                          const std::unique_ptr<DobbyContainer> &container)
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "postConstruction");

    // optimist
    bool success = true;

    AI_LOG_DEBUG("executing plugins postConstruction hooks");

    // execute the plugin hooks
    if (mLegacyPlugins->executePostConstructionHooks(container->config->legacyPlugins(),
                                                     id,
                                                     startState,
                                                     container->rootfs->path()) == false)
    {
        AI_LOG_ERROR("one or more post-construction plugins failed for '%s'",
                     id.c_str());
        success = false;
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called at the pre-start phase of the container start-up
 *
 *  We use the @a id to find the container spec, using that we can determine
 *  what plugin libraries need to be called.
 *
 *  Then we go though each plugin and pass it it's data from the spec file.
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  pid             The pid of the init process within the container.
 *  @param[in]  rootfs          The path to the rootfs of the container.
 *
 *  @return true if all preStart callbacks were called AND all of them returned
 *  true.  If any callback fails then false is returned.
 */
bool DobbyManager::onPreStartHook(const ContainerId &id,
                                  const std::unique_ptr<DobbyContainer> &container)
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "preStart");

    // the first thing to check is if the container has got the curse of death,
    // this can happen if DobbyManager::stop was called after the
    // container was constructed but before we hit this point.  In such cases
    // we just need to return false here to abort the container start-up
    if (container->hasCurseOfDeath)
    {
        return false;
    }

    // optimist
    bool success = true;

    // execute the plugin hooks
    if (mLegacyPlugins->executePreStartHooks(container->config->legacyPlugins(),
                                             id,
                                             container->containerPid,
                                             container->rootfs->path()) == false)
    {
        AI_LOG_ERROR("one or more pre-start plugins failed for '%s'",
                     id.c_str());
        success = false;
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called at the post-start phase of the container start-up
 *
 *  We use the @a id to find the container spec, using that we can determine
 *  what plugin libraries need to be called.
 *
 *  Then we go though each plugin and pass it it's data from the spec file.
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  pid             The pid of the init process within the container.
 *  @param[in]  rootfs          The path to the rootfs of the container.
 *
 *  @return true if all postStart callbacks were called AND all of them returned
 *  true.  If any callback fails then false is returned.
 */
bool DobbyManager::onPostStartHook(const ContainerId &id,
                                   const std::unique_ptr<DobbyContainer> &container)
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "postStart");

    // execute the plugin hooks
    if (mLegacyPlugins->executePostStartHooks(container->config->legacyPlugins(),
                                              id,
                                              container->containerPid,
                                              container->rootfs->path()) == false)
    {
        AI_LOG_ERROR("one or more post-start hooks failed for '%s'",
                     id.c_str());
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called at the post-stop phase of the container tear-down
 *
 *  @warning this method must be called with the @a mLock already held.
 *
 *  We use the @a id to find the container spec, using that we can determine
 *  what plugin libraries need to be called.
 *
 *  Then we go though each plugin and pass it it's data from the
 *  spec file.
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  rootfs          The path to the rootfs of the container.
 *
 *  @return true if all postStop callbacks were called AND all of them returned
 *  true.  If any callback fails then false is returned.
 */
bool DobbyManager::onPostStopHook(const ContainerId &id,
                                  const std::unique_ptr<DobbyContainer> &container)
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "postStop");

    // execute the plugin hooks
    if (mLegacyPlugins->executePostStopHooks(container->config->legacyPlugins(),
                                             id,
                                             container->rootfs->path()) == false)
    {
        AI_LOG_ERROR("one or more post-stop hooks failed for '%s'",
                     id.c_str());
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called after the container is stopped but just before the rootfs
 *  is deleted.
 *
 *  @warning this method must be called with the @a mLock already held.
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  container       The container wrapper object, stores things
 *                              like the container config and rootfs objects.
 *
 *  @return always returns true.
 */
bool DobbyManager::onPreDestructionHook(const ContainerId &id,
                                        const std::unique_ptr<DobbyContainer> &container)
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "preDestruction");

    // execute the plugin hooks
    if (mLegacyPlugins->executePreDestructionHooks(container->config->legacyPlugins(),
                                                   id,
                                                   container->rootfs->path()) == false)
    {
        AI_LOG_ERROR("one or more pre-destruction hooks failed for '%s'",
                     id.c_str());
    }

    AI_LOG_FN_EXIT();
    return true;
}
#endif //defined(LEGACY_COMPONENTS)

/**
 * @brief Perform all the necessary cleanup and run plugins required when
 * a container has terminated.
 *
 * Will also delete the container so the ID can be re-used
 *
 * @param[in]   id          ID of the container that has terminated
 * @param[in]   container   Information about the container that has terminated (rootfs, config etc)
 * @param[in]   status      Exit status of the container runtime
 */
void DobbyManager::handleContainerTerminate(const ContainerId &id, const std::unique_ptr<DobbyContainer>& container, const int status)
{
    AI_LOG_FN_ENTRY();

    // this function is called when the runc process dies, what this
    // boils down to is that if we're in the Running state it
    // means that the preStart hook has been called but postStop hasn't
    // therefore we should call the postStop here as well as the
    // preDestruction hook
    if (container->state == DobbyContainer::State::Running)
    {
#if defined(LEGACY_COMPONENTS)
        // this will internally change the state to 'stopping'
        onPostStopHook(id, container);
#endif // defined(LEGACY_COMPONENTS)

        // change the container state to 'stopping'
        container->state = DobbyContainer::State::Stopping;
    }

    // check if the container has the respawn flag, if so attempt to
    // restart the container now, this skips the preDestruction /
    // postConstruction hooks
    if (!container->shouldRestart(status) || !restartContainer(id, container))
    {
#if defined(LEGACY_COMPONENTS)
        // either the respawn flag isn't set, or we failed to restart
        // the container, so call any pre-destruction hooks before
        // tearing down the roots and bundle directories
        onPreDestructionHook(id, container);
#endif // defined(LEGACY_COMPONENTS)

        // Also run any postHalt hooks in RDK plugins
        if (container->config->rdkPlugins().size() > 0)
        {
            container->rdkPluginManager->setExitStatus(status);
            onPostHaltHook(container);
        }

        // Dump the logs from the postStop hook
        std::shared_ptr<DobbyBufferStream> bufferStream =
            std::make_shared<DobbyBufferStream>();

        // ask the runc tool to clean up anything it may have left over
        // for it's own use
        if (!mRunc->destroy(id, bufferStream))
        {
            AI_LOG_ERROR("failed to destroy '%s'", id.c_str());
        }

        if (container->rdkPluginManager)
        {
            auto loggingPlugin = container->rdkPluginManager->getContainerLogger();
            // No point trying to stop logging if there was never a
            // logging plugin to run in the first place
            if (loggingPlugin)
            {
                // If main container logging thread is still running,
                // wait for it to finish before we dump the hook output
                // to the log
                mLogger->DumpBuffer(bufferStream->getMemFd(), container->containerPid, loggingPlugin);
            }
        }

        // clear the runc pid just in case it accidentally gets re-used
        container->containerPid = -1;

        // remove any metadata stored for the container
        mUtilities->clearContainerMetaData(id);

        // If the container was launched from a custom config, delete
        // the custom config
        if (!container->customConfigFilePath.empty())
        {
            if (remove(container->customConfigFilePath.c_str()) != 0)
            {
                AI_LOG_SYS_ERROR(errno, "Failed to remove custom config '%s'",
                                 container->customConfigFilePath.c_str());
            }
        }
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called when we detect a child process has terminated.
 *
 *  The child process will typically be a runc instance for a container. To
 *  check we need to iterate over the pids we know about and check if they
 *  have terminated.
 *
 *  This is how we determine when a container dies, we look for the child
 *  process death signal.
 *
 */
void DobbyManager::onChildExit()
{
    struct ContainerStoppedEvent {
        int32_t descriptor;
        ContainerId id;
        int status;
    };

    AI_LOG_FN_ENTRY();

    AI_LOG_DEBUG("detected child terminated signal");

    // take the lock as we're being called from the signal monitor thread
    std::lock_guard<std::mutex> locker(mLock);
    std::vector<ContainerStoppedEvent> containerStoppedEvents;

    // find the container which has been launched by the given runc (use pid
    // to match it).
    auto it = mContainers.begin();
    while (it != mContainers.end())
    {
        const std::unique_ptr<DobbyContainer> &container = it->second;
        const pid_t containerPid = container->containerPid;

        // If container has invalid pid or is in an unknown state, nothing we can do
        // so move on
        if (containerPid <= 0 || container->state == DobbyContainer::State::Unknown)
        {
            ++it;
            continue;
        }

        // check if the runc process has exited
        int status = 0;
        int rc = waitpid(containerPid, &status, WNOHANG);
        if (rc < 0)
        {
            // Sometimes waitpid fails even though container is already dead
            // we can check if it is running by sending "dummy" kill (it will
            // not perform kill, just check if it CAN)
            if (kill(containerPid, 0) == -1)
            {
                // Cannot kill process, probably already dead
                // treat it as if it would return proper waitpid
                status = 0;
                rc = container->containerPid;
            }
            else
            {
                // Process still exists
                AI_LOG_ERROR("waitpid failed for pid %d", containerPid);
            }
        }

        if (rc == container->containerPid)
        {
            const ContainerId &id = it->first;

            AI_LOG_INFO("runc for container '%s' has quit (pid:%d status:0x%04x)",
                        id.c_str(), containerPid, status);

            handleContainerTerminate(id, container, status);

            // signal the higher layers that a container has died, later
            if (mContainerStoppedCb)
            {
                containerStoppedEvents.push_back({container->descriptor, id, status});
            }

            if (!container->shouldRestart(status) || !restartContainer(id, container))
            {
                // remove the container, this should free all the resources
                // associated with it
                it = mContainers.erase(it);

                mContainerExecPids.erase(id);
            }

            // on to the next container
            // [ nb: we do this even if the container was restarted
            //   (i.e. mContainers.erase not called), as we also want to check
            //   that the newly restarted container (which has the same iterator)
            //   has not also just died. ]
            continue;
        }

        ++it;
    }

    // We're also tracking any executed processes inside the container
    // If one of the exec'd processes dies, we need to wait on it to avoid
    // a zombie process. This check is fairly rudimentary and could be made more
    // robust if needed.

    // FIXME: If the exec'd process is longer running than the main container
    // process, Dobby will get confused and won't clean up the container
    auto execit = mContainerExecPids.begin();
    while (execit != mContainerExecPids.end())
    {
        int status = 0;
        int rc = waitpid(execit->second, &status, WNOHANG);
        if (rc < 0)
        {
            AI_LOG_SYS_ERROR(errno, "waitpid failed for pid %d", execit->second);
        }

        if (rc == execit->second)
        {
            // Exec'd process has exited - remove from the map
            // as erase invalidates iterator we must use its
            // return value instead of simple increment
            execit = mContainerExecPids.erase(execit);
        }
        else
        {
            ++execit;
        }
    }

    if (mContainerStoppedCb)
    {
        for (const auto &ev: containerStoppedEvents)
        {
            // signal the higher layers that a container has died, now
            mContainerStoppedCb(ev.descriptor, ev.id, ev.status);
        }
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Starts a thread that monitors for SIGCHILD signals
 *
 *  This thread is used to determine when one of our spawned child runc
 *  processes has died.
 *
 *  Failure to create the thread should be considered fatal.
 *
 *  @return true on success, false on failure.
 */
void DobbyManager::startRuncMonitorThread()
{
    AI_LOG_FN_ENTRY();

    // clear the terminate flag
    mRuncMonitorTerminate = false;

    int result = sem_init(&mRuncMonitorThreadStartedSem,0,0);
    assert(0 == result);

    //spawn the thread
    mRuncMonitorThread = std::thread(&DobbyManager::runcMonitorThread, this);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Stops the monitor thread and cleans up it's resources
 *
 *
 */
void DobbyManager::stopRuncMonitorThread()
{
    AI_LOG_FN_ENTRY();

    // attempt to terminate the thread
    if (mRuncMonitorThread.joinable())
    {
        // Wait for RuncMonitorThread to be running first
        sem_wait(&mRuncMonitorThreadStartedSem);

        // set the terminate flag
        mRuncMonitorTerminate = true;

        // send a signal to wake up the blocking signalwait
        int rc = pthread_kill(mRuncMonitorThread.native_handle(), SIGUSR1);
        if (rc != 0)
        {
            AI_LOG_SYS_ERROR(rc, "failed to send signal to terminate thread");
        }
        else
        {
            mRuncMonitorThread.join();
        }
    }

    int result = sem_destroy(&mRuncMonitorThreadStartedSem);
    assert(0 == result);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Thread function that monitors for any SIGCHILD signals and if
 *  detected loops through the running containers to see if it was the
 *  runc process that spawned it (i.e. a containered init process)
 *
 *
 */
void DobbyManager::runcMonitorThread()
{
    AI_LOG_FN_ENTRY();

    AI_LOG_INFO("started SIGCHLD monitor thread");

    // set the name of the thread for debugging
    pthread_setname_np(pthread_self(), "AI_SIGMONITOR");

    // monitor both SIGCHLD & SIGUSR1
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    sigaddset(&mask, SIGUSR1);

    // don't know if this is needed, but doesn't hurt and is part of the
    // belt and braces approach to signals
    sigprocmask(SIG_BLOCK, &mask, nullptr);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    sem_post(&mRuncMonitorThreadStartedSem);
    while (!mRuncMonitorTerminate)
    {
        // wait for both SIGCHLD and SIGUSR1
        int sig = TEMP_FAILURE_RETRY(sigwaitinfo(&mask, nullptr));
        if (sig == SIGCHLD)
        {
            // inform the the manager that a child has died, note that although
            // the supplied signal info contains fields to tell us which process
            // has died, the kernel can compressed multiple SIGCHLD signals into
            // a single siginfo, therefore if two processes die at the same time
            // only one pid will be stored in the siginfo. So the only to solve
            // this is to iterate over all pids and call waitpid(..., WNOHANG)
            //
            //   https://ldpreload.com/blog/signalfd-is-useless
            //   http://stackoverflow.com/questions/8398298/handling-sigchld
            //
            onChildExit();
        }
        else if (sig != SIGUSR1)
        {
            AI_LOG_SYS_ERROR(errno, "sigwaitinfo failed with result %d", sig);
        }
    }

    AI_LOG_INFO("stopped SIGCHLD monitor thread");

    AI_LOG_FN_EXIT();
}

/**
 * @brief Task that will try and cleanup invalid/unknown state containers
 * periodically - if the container can be killed then kill it and release the id
 * back to the pool so it can be restarted
 *
 */
bool DobbyManager::invalidContainerCleanupTask()
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // Find out how many containers are in an unknown state
    const int stuckCount = std::count_if(mContainers.begin(), mContainers.end(), [](const std::pair<const ContainerId, std::unique_ptr<DobbyContainer>>& c)
    {
        return c.second->state == DobbyContainer::State::Unknown;
    });
    if (stuckCount == 0)
    {
        // No more stuck containers, our job here is done
        mCleanupTaskTimerId = -1;
        return false;
    }

    auto it = mContainers.begin();
    std::shared_ptr<DobbyDevNullStream> devNull = std::make_shared<DobbyDevNullStream>();
    while (it != mContainers.end())
    {
        // Only care about containers in an unknown state
        if (it->second->state == DobbyContainer::State::Unknown)
        {
            // Check if container PID is still valid
            if (kill(it->second->containerPid, 0) != 0 && errno == ESRCH)
            {
                // Pid no longer exists, attempt to destroy container
                if (mRunc->destroy(it->first, devNull))
                {
                    // Container destroyed successfully, stop tracking it
                    AI_LOG_INFO("Previously stuck container '%s' has  been destroyed - releasing id back to the pool", it->first.c_str());
                    it = mContainers.erase(it);
                }
            }
            else
            {
                // Pid is still valid. Attempt to send SIGKILL
                mRunc->killCont(it->first, SIGKILL, true);

                // Did we actually kill it? Give it some time, then check the status
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                DobbyRunC::ContainerStatus state = mRunc->state(it->first);

                if (state != DobbyRunC::ContainerStatus::Running)
                {
                    // We killed it! Destroy it and remove from our list
                    if (mRunc->destroy(it->first, devNull))
                    {
                        // Container destroyed successfully, stop tracking it
                        AI_LOG_INFO("Previously stuck container %d has been destroyed - releasing id back to the pool", it->second->descriptor);
                        it = mContainers.erase(it);
                    }
                }
                else
                {
                    ++it;
                }
            }
        }
        else
        {
            ++it;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

bool DobbyManager::shouldEnableSTrace(const std::shared_ptr<DobbyConfig> &config) const
{
    std::shared_ptr<rt_dobby_schema> containerConfig(config->config());
    if (containerConfig == nullptr)
        return false;

    const std::string hostName{containerConfig->hostname};
    const std::vector<std::string>& apps = mSettings->straceSettings().apps;

    return std::find(apps.begin(), apps.end(), hostName) != apps.end();
}
