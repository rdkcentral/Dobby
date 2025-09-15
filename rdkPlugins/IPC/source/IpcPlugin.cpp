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

#include "IpcPlugin.h"
#include "sys/types.h"
#include "sys/stat.h"
#include "sys/mount.h"
#include <fcntl.h>

#include "DobbyRdkPluginUtils.h"

/**
 * Need to do this at the start of every plugin to make sure the correct
 * C methods are visible to allow PluginLauncher to find the plugin
 */
REGISTER_RDK_PLUGIN(IpcPlugin);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
IpcPlugin::IpcPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                             const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                             const std::string &rootfsPath)
    : mName("ipc"),
      mContainerConfig(containerConfig),
      mRootfsPath(rootfsPath),
      mUtils(utils),
#if defined(RDK)
      mDbusRunDir("var/run/dbus"),
#else
      mDbusRunDir("DBUS/var/run/dbus"),
#endif
      mDbusSystemSocketPath(mDbusRunDir + "/system_bus_socket"),
      mDbusSessionSocketPath(mDbusRunDir + "/session_bus_socket"),
      mDbusDebugSocketPath(mDbusRunDir + "/debug_bus_socket"),
      mDbusSystemEnvVar("DBUS_SYSTEM_BUS_ADDRESS=unix:path=/" + mDbusSystemSocketPath),
      mDbusSessionEnvVar("DBUS_SESSION_BUS_ADDRESS=unix:path=/" + mDbusSessionSocketPath),
      mDbusDebugEnvVar("DBUS_DEBUG_BUS_ADDRESS=unix:path=/" + mDbusDebugSocketPath)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 *
 * This plugin uses all the hooks so set all the flags
 */
unsigned IpcPlugin::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PostInstallationFlag);
}

// Begin Hook Methods

/**
 * @brief OCI Hook - Run in host namespace
 */
bool IpcPlugin::postInstallation()
{
    AI_LOG_FN_ENTRY();

    // get all buses from config
    std::string systemBus;
    std::string sessionBus;
    std::string debugBus;

    if (mContainerConfig->rdk_plugins->ipc->data)
    {
        // determine which buses we need to add to the container
        if(mContainerConfig->rdk_plugins->ipc->data->system)
        {
            systemBus = std::string(mContainerConfig->rdk_plugins->ipc->data->system);
        }

        if(mContainerConfig->rdk_plugins->ipc->data->session)
        {
            sessionBus = std::string(mContainerConfig->rdk_plugins->ipc->data->session);
        }

        if(mContainerConfig->rdk_plugins->ipc->data->debug)
        {
            debugBus = std::string(mContainerConfig->rdk_plugins->ipc->data->debug);
        }
    }

    AI_LOG_INFO("dbus config : system=%s",
                systemBus.empty() ? "none" : systemBus.c_str());

    if (systemBus.empty() && sessionBus.empty() && debugBus.empty())
    {
        // No buses provided
        AI_LOG_WARN("No buses provided in IPC plugin");
        return true;
    }

    // set the environment vars for dbus to fix issues with userns and
    // the dbus AUTH EXTERNAL protocol
#if defined(RDK)
    mUtils->addEnvironmentVar("SKY_DBUS_DISABLE_UID_IN_EXTERNAL_AUTH=1");
#else
    mUtils->addEnvironmentVar("DBUS_ID_MAPPING=1");
#endif

    // create the directory in the rootfs for the mount
    bool success = DobbyRdkPluginUtils::mkdirRecursive(mRootfsPath + mDbusRunDir, 0755);

    // perform bind mounts into the rootfs of the container
    if (!systemBus.empty() && success)
    {
        success = addSocketAndEnv(mUtils,
                                  mRootfsPath,
                                  mContainerConfig,
                                  std::move(systemBus),
                                  mDbusSystemSocketPath,
                                  mDbusSystemEnvVar);
    }


    if (!sessionBus.empty() && success)
    {
        success = addSocketAndEnv(mUtils,
                                  mRootfsPath,
                                  mContainerConfig,
                                  std::move(sessionBus),
                                  mDbusSessionSocketPath,
                                  mDbusSessionEnvVar);
    }

    if (!debugBus.empty() && success)
    {
        success = addSocketAndEnv(mUtils,
                                  mRootfsPath,
                                  mContainerConfig,
                                  std::move(debugBus),
                                  mDbusDebugSocketPath,
                                  mDbusDebugEnvVar);
    }

    AI_LOG_FN_EXIT();
    return success;
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
std::vector<std::string> IpcPlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_ipc* pluginConfig = mContainerConfig->rdk_plugins->ipc;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

// Begin private methods


// -----------------------------------------------------------------------------
/**
 *  @brief Adds the bind mount of the socket.
 *
 *  This also creates the mount point and sets the environment variables for
 *  the dbus code running inside the container.
 *
 */
bool IpcPlugin::addSocketAndEnv(const std::shared_ptr<DobbyRdkPluginUtils> utils,
                                const std::string& rootfsPath,
                                std::shared_ptr<rt_dobby_schema> containerConfig,
                                std::string busStr,
                                const std::string &socketPath,
                                const std::string &envVar) const
{
    // open socket
    const std::string socketInsideContainer = rootfsPath + socketPath;
    int fd = open(socketInsideContainer.c_str(),
                    O_CLOEXEC | O_CREAT | O_WRONLY | O_TRUNC,
                    0644);
    if (fd < 0)
    {
        if (errno != EEXIST)
        {
            AI_LOG_SYS_ERROR(errno, "failed to create file @ '%s'",
                             socketInsideContainer.c_str());
            return false;
        }
    }
    else if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close file @ '%s'", socketInsideContainer.c_str());
    }


    // for legacy purposes we are supporting buses which are not path but special
    // names. If one of this special name is used then find it path
    if ((busStr == "system") ||
        (busStr == "ai-private") ||
        (busStr == "ai-public"))
    {
        std::string tmpBusStr = DBUS_SYSTEM_ADDRESS;

        // get the dbus socket paths (outside the container)
        if (busStr == "system")
        {
            tmpBusStr = socketPathFromAddressSimple(tmpBusStr);
        }
        else if(busStr == "ai-private")
        {
            // These are possible addresses of private from initial implementation, non of
            // them is valid on current platform, but they are left in case we would bring
            // back support for them
            //tmpBusStr = "/tmp/ai_workspace.*/dbus/socket/private/serverfd";
            //tmpBusStr = "/mnt/nds/tmpfs/APPLICATIONS_WORKSPACE/dbus/socket/private/serverfd";
            tmpBusStr = "";

            AI_LOG_WARN("Option %s is no longer supported on this platform", busStr.c_str());
            return true;
        }
        else
        {
            // These are possible addresses of ai-public from initial implementation, non of
            // them is valid on current platform, but they are left in case we would bring
            // back support for them
            //tmpBusStr = "/tmp/ai_workspace.*/dbus/socket/public/serverfd";
            //tmpBusStr = "/mnt/nds/tmpfs/APPLICATIONS_WORKSPACE/dbus/socket/public/serverfd";
            tmpBusStr = "";

            AI_LOG_WARN("Option %s is no longer supported on this platform", busStr.c_str());
            return true;
        }

        if (tmpBusStr.empty())
        {
            AI_LOG_WARN("no dbus socket address for %s bus", busStr.c_str());
            return false;
        }

        busStr = std::move(tmpBusStr);
    }

    // create a mount point for the socket
    if (!utils->addMount(busStr,
                        "/" + socketPath,
                        "bind",
                        {"bind", "nodev","nosuid", "noexec" }))
    {
        AI_LOG_WARN("failed to add bind mount for '%s' bus socket",
                    busStr.c_str());
        return false;
    }

    utils->addEnvironmentVar(envVar);
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Utility function to extract the socket path from the dbus address
 *  string.
 *
 *  This uses basic string operations
 *
 *  @param[in]  address     The dbus address trying to parse
 *
 *  @return on success the path to the dbus socket, on failure an empty string.
 */
std::string IpcPlugin::socketPathFromAddressSimple(const std::string& address) const
{
    AI_LOG_FN_ENTRY();

    if (address.empty())
    {
        return std::string();
    }

    std::string socketPath;

    const std::string unixPathStr = "unix:path=";

    std::size_t position = address.find(unixPathStr);

    if (position != 0)
    {
        return std::string();
    }

    socketPath = address.substr(unixPathStr.length());

    AI_LOG_DEBUG("Socket path is '%s'", socketPath.c_str());

    AI_LOG_FN_EXIT();
    return socketPath;
}