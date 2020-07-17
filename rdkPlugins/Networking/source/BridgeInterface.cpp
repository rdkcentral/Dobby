/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
 * File:   BridgeInterface.cpp
 *
 * Copyright (C) Sky UK 2016+
 */

#include "BridgeInterface.h"

#include <Logging.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <net/if.h>
#include <sys/ioctl.h>


// -----------------------------------------------------------------------------
/**
 *  @brief Creates the Dobby bridge device.
 *
 *  @param[in]  netlink     Instance of the Netlink class.
 *
 *  @see Netlink::createBridge()
 *
 *  @return true on success, false on failure.
 */
bool BridgeInterface::createBridge(const std::shared_ptr<Netlink> &netlink)
{
    return netlink->createBridge(BRIDGE_NAME);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Destroys the Dobby bridge device.
 *
 *  @param[in]  netlink     Instance of the Netlink class.
 *
 *  @see Netlink::destroyBridge()
 *
 *  @return true on success, false on failure.
 */
bool BridgeInterface::destroyBridge(const std::shared_ptr<Netlink> &netlink)
{
    return netlink->destroyBridge(BRIDGE_NAME);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Brings the Dobby bridge device up.
 *
 *  Uses a workaround function if needed (libnl v3.3.x-3.4.0).
 *
 *  @param[in]  netlink     Instance of the Netlink class.
 *
 *  @see Netlink::ifaceUp()
 *
 *  @return true on success, false on failure.
 */
bool BridgeInterface::up(const std::shared_ptr<Netlink> &netlink)
{
#if ENABLE_LIBNL_BRIDGE_WORKAROUND
    return netlinkFlagsWorkaround(IFF_UP, IFF_UP);
#else
    return netlink->ifaceUp(BRIDGE_NAME);
#endif
}

// -----------------------------------------------------------------------------
/**
 *  @brief Brings the Dobby bridge device down.
 *
 *  Uses a workaround function if needed (libnl v3.3.x-3.4.0).
 *
 *  @param[in]  netlink     Instance of the Netlink class.
 *
 *  @see Netlink::ifaceDown()
 *
 *  @return true on success, false on failure.
 */
bool BridgeInterface::down(const std::shared_ptr<Netlink> &netlink)
{
#if ENABLE_LIBNL_BRIDGE_WORKAROUND
    return netlinkFlagsWorkaround(IFF_UP, 0);
#else
    return netlink->ifaceDown(BRIDGE_NAME);
#endif
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets forwarding on the Dobby bridge device.
 *
 *  Uses a workaround function if needed (libnl v3.3.x-3.4.0).
 *
 *  @param[in]  utils       Instance of the DobbyRdkPluginUtils class
 *  @param[in]  netlink     Instance of the Netlink class
 *  @param[in]  enable      true to enable, false to disable.
 *
 *  @see Netlink::setIfaceForwarding()
 *
 *  @return true on success, false on failure.
 */
bool BridgeInterface::setIfaceForwarding(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                         const std::shared_ptr<Netlink> &netlink,
                                         bool enable)
{
#if ENABLE_LIBNL_BRIDGE_WORKAROUND
    return netlinkForwardingWorkaround(utils, enable);
#else
    return netlink->setIfaceForwarding(BRIDGE_NAME, enable);
#endif
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the route_localnet on the Dobby bridge device.
 *
 *  Uses a workaround function if needed (libnl v3.3.x-3.4.0).
 *
 *  @param[in]  utils       Instance of the DobbyRdkPluginUtils class
 *  @param[in]  netlink     Instance of the Netlink class
 *  @param[in]  enable      true to enable, false to disable.
 *
 *  @see Netlink::setIfaceRouteLocalNet()
 *
 *  @return true on success, false on failure.
 */
bool BridgeInterface::setIfaceRouteLocalNet(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                            const std::shared_ptr<Netlink> &netlink,
                                            bool enable)
{
#if ENABLE_LIBNL_BRIDGE_WORKAROUND
    return netlinkRouteLocalNetWorkaround(utils, enable);
#else
    return netlink->setIfaceRouteLocalNet(BRIDGE_NAME, enable);
#endif
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the ip address and netmask of the bridge interface
 *
 *  This is the equivalent of the following on the command line
 *
 *      ifconfig <BRIDGE_NAME> <address> netmask <netmask>
 *
 *  @param[in]  netlink     Instance of the Netlink class
 *  @param[in]  address     The address to set, the netmask will be applied to
 *                          this before setting on the iface
 *  @param[in]  netmask     The netmask to apply.
 *
 *  @return true on success, false on failure.
 */
bool BridgeInterface::setAddress(const std::shared_ptr<Netlink> &netlink, in_addr_t address, in_addr_t netmask)
{
    return netlink->setIfaceAddress(BRIDGE_NAME, address, netmask);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Disables Spanning Tree Protocol in sysfs file
 *
 *  @param[in]  utils           Instance of DobbyRdkPluginUtils
 *
 *  @return true on success, false on failure.
 */
bool BridgeInterface::disableStp(const std::shared_ptr<DobbyRdkPluginUtils> &utils)
{
    std::string path = "/sys/class/net/" BRIDGE_NAME "/bridge/stp_state";

    return utils->writeTextFile(path, "0\n", O_TRUNC, 0);
}

#if defined(ENABLE_LIBNL_BRIDGE_WORKAROUND)

// -----------------------------------------------------------------------------
/**
 *  @brief Brings up or takes down the bridge interface.
 *
 *  This is the equivalent of the following on the command line
 *
 *      ifconfig <BRIDGE_NAME> up | down
 *
 *  This is a workaround needed for certain versions of netlink which have a bug
 *  when setting / clearing the IFF_UP flag.  See the following:
 *
 *  https://stackoverflow.com/questions/56535754/change-bridge-flags-with-libnl
 *  http://lists.infradead.org/pipermail/libnl/2017-November/thread.html#2384
 *
 *
 *  @param[in]  mask    The interface flags mask.
 *  @param[in]  flags   The flags to set.
 *
 *  @return true on success, false on failure.
 */
bool netlinkFlagsWorkaround(short int mask, short int flags)
{
    int sock = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (sock < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open socket");
        return false;
    }

    bool success = false;

    struct ifreq ifr;
    bzero(&ifr, sizeof(ifr));
    strncpy(ifr.ifr_name, BRIDGE_NAME, IFNAMSIZ);

    if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to get flags for interface '%s'",
                        BRIDGE_NAME);
    }
    else
    {
        ifr.ifr_flags &= ~mask;
        ifr.ifr_flags |= (mask & flags);

        success = (ioctl(sock, SIOCSIFFLAGS, &ifr) >= 0);
        if (!success)
        {
            AI_LOG_SYS_ERROR(errno, "failed to set flags for interface '%s'",
                             BRIDGE_NAME);
        }
    }

    if (close(sock) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close the socket");
    }

    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Enables or disables forwarding on the given interface
 *
 *  This is the equivalent of the following on the command line
 *
 *      echo "1" > /proc/sys/net/ipv4/conf/<BRIDGE_NAME>/forwarding
 *  Or
 *      echo "0" > /proc/sys/net/ipv4/conf/<BRIDGE_NAME>/forwarding
 *
 * This is a workaround needed for certain versions of netlink which have a bug
 * when setting / clearing the interface flags.  See the following:
 *
 *  https://stackoverflow.com/questions/56535754/change-bridge-flags-with-libnl
 *  http://lists.infradead.org/pipermail/libnl/2017-November/thread.html#2384
 *
 *  @param[in]  utils       instance of the DobbyRdkPluginUtils class
 *  @param[in]  enable      true to enable forwarding, false to disable.
 *
 *  @return true on success, false on failure.
 */
bool netlinkForwardingWorkaround(const std::shared_ptr<DobbyRdkPluginUtils> &utils, bool enable)
{
    std::string path = "/proc/sys/net/ipv4/conf/" BRIDGE_NAME "/forwarding";

    return utils->writeTextFile(path, enable ? "1" : "0", O_TRUNC, 0);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Enables or disables route_localnet on the given interface
 *
 *  This is the equivalent of the following on the command line
 *
 *      echo "1" > /proc/sys/net/ipv4/conf/<BRIDGE_NAME>/route_localnet
 *  Or
 *      echo "0" > /proc/sys/net/ipv4/conf/<BRIDGE_NAME>/route_localnet
 *
 * This is a workaround needed for certain versions of netlink which have a bug
 * when setting / clearing the interface flags.  See the following:
 *
 *  https://stackoverflow.com/questions/56535754/change-bridge-flags-with-libnl
 *  http://lists.infradead.org/pipermail/libnl/2017-November/thread.html#2384
 *
 *  @param[in]  utils       instance of the DobbyRdkPluginUtils class
 *  @param[in]  enable      true to enable route_localnet, false to disable.
 *
 *  @return true on success, false on failure.
 */
bool netlinkRouteLocalNetWorkaround(const std::shared_ptr<DobbyRdkPluginUtils> &utils, bool enable)
{
    std::string path = "/proc/sys/net/ipv4/conf/" BRIDGE_NAME "/route_localnet";

    return utils->writeTextFile(path, enable ? "1" : "0", O_TRUNC, 0);
}

#endif // ENABLE_LIBNL_BRIDGE_WORKAROUND