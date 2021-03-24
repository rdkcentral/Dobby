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

#include "NetworkingPlugin.h"
#include "DnsmasqSetup.h"
#include "PortForwarding.h"
#include "MulticastForwarder.h"
#include "NetworkSetup.h"

#include <fcntl.h>
#include <unistd.h>


REGISTER_RDK_PLUGIN(NetworkingPlugin);

static std::string gDBusService(DOBBY_SERVICE ".plugin.networking");


NetworkingPlugin::NetworkingPlugin(std::shared_ptr<rt_dobby_schema> &cfg,
                                   const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                   const std::string &rootfsPath)
    : mName("Networking"),
      mNetworkType(NetworkType::None),
      mContainerConfig(cfg),
      mUtils(utils),
      mRootfsPath(rootfsPath),
      mIpcService(nullptr),
      mDobbyProxy(nullptr),
      mNetfilter(std::make_shared<Netfilter>())
{
    AI_LOG_FN_ENTRY();

    if (!mContainerConfig || !cfg->rdk_plugins->networking ||
        !cfg->rdk_plugins->networking->data)
    {
        mValid = false;
    }
    else
    {
        mPluginData = cfg->rdk_plugins->networking->data;
        mHelper = std::make_shared<NetworkingHelper>(mPluginData->ipv4, mPluginData->ipv6);

        std::string networkType = mPluginData->type;
        if (networkType == "nat")
        {
            mNetworkType = NetworkType::Nat;
        }
        else if (networkType == "none")
        {
            mNetworkType = NetworkType::None;
        }
        else if (networkType == "open")
        {
            mNetworkType = NetworkType::Open;
        }
        else
        {
            AI_LOG_WARN("Unexpected network type '%s', defaulting to 'none'",
                        networkType.c_str());
            mNetworkType = NetworkType::None;
        }
        mValid = true;
    }

    AI_LOG_FN_EXIT();
}

NetworkingPlugin::~NetworkingPlugin()
{
    AI_LOG_FN_ENTRY();

    // stop the remote service if it was initialised
    if (mIpcService)
    {
        mIpcService->stop();
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 *
 * This plugin uses all the hooks so set all the flags
 */
unsigned NetworkingPlugin::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PostInstallationFlag |
        IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag |
        IDobbyRdkPlugin::HintFlags::PostHaltFlag
    );
}

// Begin Hook Methods

/**
 * @brief Dobby Hook - run in host namespace *once* when container bundle is downloaded
 */
bool NetworkingPlugin::postInstallation()
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_ERROR_EXIT("invalid config file");
        return false;
    }

    // if the network type is not 'open', enable network namespacing in OCI config
    if (mNetworkType != NetworkType::Open)
    {
        // add /etc/resolv.conf mount if not using dnsmasq. If dnsmasq is enabled,
        // a new /etc/resolv.conf is created rather than mounting the host's
        if (!mPluginData->dnsmasq)
        {
            NetworkSetup::addResolvMount(mUtils, mContainerConfig);
        }

        // add network namespacing to the OCI config
        NetworkSetup::addNetworkNamespace(mContainerConfig);
    }

    AI_LOG_FN_EXIT();
    return true;
}


/**
 * @brief OCI Hook - Run in host namespace
 */
bool NetworkingPlugin::createRuntime()
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_ERROR_EXIT("invalid config file");
        return false;
    }

    // nothing to do for containers configured for an open network
    if (mNetworkType == NetworkType::Open)
    {
        AI_LOG_FN_EXIT();
        return true;
    }

    // set up communication interface with DobbyDaemon
    if (!createRemoteService())
    {
        AI_LOG_ERROR_EXIT("failed to create remote service");
        return false;
    }

    // get external interfaces from daemon
    const std::vector<std::string> extIfaces = mDobbyProxy->getExternalInterfaces();
    if (extIfaces.empty())
    {
        AI_LOG_ERROR_EXIT("no external network interfaces defined in settings");
        return false;
    }

    // check if another container has already initialised the bridge device for us
    int32_t bridgeConnections = mDobbyProxy->getBridgeConnections();
    if (bridgeConnections < 0)
    {
        AI_LOG_ERROR_EXIT("failed to get response from daemon");
        return false;
    }
    else if (!bridgeConnections)
    {
        AI_LOG_DEBUG("No connections to dobby network bridge found, setting it up");

        // setup the bridge device
        if (!NetworkSetup::setupBridgeDevice(mUtils, mNetfilter, extIfaces))
        {
            AI_LOG_ERROR_EXIT("failed to setup Dobby bridge device");
            return false;
        }
    }

    // setup veth, ip address and iptables rules for container
    if (!NetworkSetup::setupVeth(mUtils, mNetfilter, mDobbyProxy, mHelper,
                                 mRootfsPath, mUtils->getContainerId(), mNetworkType))
    {
        AI_LOG_ERROR_EXIT("failed to setup virtual ethernet device");
        return false;
    }

    // setup dnsmasq rules if enabled
    if (mNetworkType != NetworkType::None && mPluginData->dnsmasq)
    {
        if (!DnsmasqSetup::set(mUtils, mNetfilter, mHelper, mRootfsPath,
                               mUtils->getContainerId(), mNetworkType))
        {
            AI_LOG_ERROR_EXIT("failed to setup container for dnsmasq use");
            return false;
        }
    }

    // add port forwards if any have been configured
    if (mPluginData->port_forwarding != nullptr)
    {
        if (!PortForwarding::addPortForwards(mNetfilter, mHelper, mUtils->getContainerId(), mPluginData->port_forwarding))
        {
            AI_LOG_ERROR_EXIT("failed to add port forwards");
            return false;
        }
    }

    // add port forwards if any have been configured
    if (mPluginData->multicast_forwarding != nullptr)
    {
        if (!MulticastForwarder::set(mNetfilter, mPluginData, mHelper->vethName(), mUtils->getContainerId(), extIfaces))
        {
            AI_LOG_ERROR_EXIT("failed to add multicast forwards");
            return false;
        }
    }

    // apply iptables changes
    if (!mNetfilter->applyRules(AF_INET) || !mNetfilter->applyRules(AF_INET6))
    {
        AI_LOG_ERROR_EXIT("failed to apply iptables rules");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}


/**
 * @brief Dobby Hook - Run in host namespace when container terminates
 */
bool NetworkingPlugin::postHalt()
{
    AI_LOG_FN_ENTRY();

    bool success = true;

    if (!mValid)
    {
        AI_LOG_ERROR_EXIT("invalid config file");
        return false;
    }

    // nothing to do for containers configured for an open network
    if (mNetworkType == NetworkType::Open)
    {
        AI_LOG_FN_EXIT();
        return true;
    }

    // set up communication interface with DobbyDaemon
    if (!createRemoteService())
    {
        AI_LOG_ERROR_EXIT("failed to create remote service");
        return false;
    }

    ContainerNetworkInfo networkInfo;
    if (!mUtils->getContainerNetworkInfo(networkInfo))
    {
        AI_LOG_WARN("Failed to get container network info");
        success = false;
    }
    else
    {
        in_addr_t ipAddress;
        inet_pton(AF_INET, networkInfo.ipAddress.c_str(), &ipAddress);
        mHelper->storeContainerInterface(htonl(ipAddress), networkInfo.vethName);

        // delete the veth pair for the container
        if (!NetworkSetup::removeVethPair(mNetfilter, mHelper, networkInfo.vethName, mNetworkType, mUtils->getContainerId()))
        {
            AI_LOG_WARN("failed to remove veth pair %s", networkInfo.vethName.c_str());
            success = false;
        }

        // return the container's ip address back to the address pool
        if (!mDobbyProxy->freeIpAddress(ipAddress))
        {
            AI_LOG_WARN("failed to return address %s of container %s back to the"
                        "address pool", mHelper->ipv4AddrStr().c_str(), mUtils->getContainerId().c_str());
            success = false;
        }
    }

    // remove the address file from the host
    const std::string addressFilePath = ADDRESS_FILE_PREFIX + mUtils->getContainerId();
    if (unlink(addressFilePath.c_str()) == -1)
    {
        AI_LOG_WARN("failed to remove address file for container %s at %s",
                    mUtils->getContainerId().c_str(), addressFilePath.c_str());
        success = false;
    }

    // get external interfaces from daemon
    const std::vector<std::string> extIfaces = mDobbyProxy->getExternalInterfaces();
    if (extIfaces.empty())
    {
        AI_LOG_WARN("couldn't find external network interfaces in settings,"
                    "unable to remove bridge device");
        success = false;
    }
    else
    {
        // if there are no containers using the bridge device left, remove bridge device
        if (!mDobbyProxy->getBridgeConnections())
        {
            if (!NetworkSetup::removeBridgeDevice(mNetfilter, extIfaces))
            {
                success = false;
            }
        }
    }

    // if dnsmasq iptables rules were set up for container, "uninstall" them
    if (mNetworkType != NetworkType::None && mPluginData->dnsmasq)
    {
        if (!DnsmasqSetup::removeRules(mNetfilter, mHelper, mUtils->getContainerId()))
        {
            success = false;
        }
    }

    // remove port forwards if any have been configured
    if (mPluginData->port_forwarding != nullptr)
    {
        if (!PortForwarding::removePortForwards(mNetfilter, mHelper, mUtils->getContainerId(), mPluginData->port_forwarding))
        {
            success = false;
        }
    }

    // add port forwards if any have been configured
    if (mPluginData->multicast_forwarding != nullptr)
    {
        if (!MulticastForwarder::removeRules(mNetfilter, mPluginData, mHelper->vethName(), mUtils->getContainerId(), extIfaces))
        {
            AI_LOG_ERROR_EXIT("failed to remove multicast forwards");
            return false;
        }
    }

    // apply iptables changes
    if (!mNetfilter->applyRules(AF_INET) || !mNetfilter->applyRules(AF_INET6))
    {
        AI_LOG_ERROR_EXIT("failed to apply iptables rules");
        return false;
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
std::vector<std::string> NetworkingPlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_networking* pluginConfig = mContainerConfig->rdk_plugins->networking;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

// Begin private methods

/**
 * @brief Creates a remote dbus service to communicate with the Dobby daemon
 *
 * @return true if successfully connected, otherwise false
 */
bool NetworkingPlugin::createRemoteService()
{
    AI_LOG_FN_ENTRY();

    // Append the pid onto the end of the service name so we can run multiple
    // clients
    char strPid[32];
    sprintf(strPid, ".pid%d", getpid());
    gDBusService += strPid;

    try
    {
        mIpcService = AI_IPC::createIpcService(DBUS_SYSTEM_ADDRESS, gDBusService);
        if(!mIpcService->start())
        {
            AI_LOG_ERROR_EXIT("failed to create IPC service");
            return false;
        }

        // create a DobbyRdkPluginProxy remote service that wraps up the dbus API calls to the Dobby daemon
        mDobbyProxy = std::make_shared<DobbyRdkPluginProxy>(mIpcService, DOBBY_SERVICE, DOBBY_OBJECT);
    }
    catch (const std::exception& e)
    {
        AI_LOG_ERROR_EXIT("failed to create IPC service: %s", e.what());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}
