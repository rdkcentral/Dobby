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
 * File:   Netlink.h
 *
 */
#include "Netlink.h"

#include <Logging.h>
#include "NetworkingHelper.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sstream>
#include <linux/if.h>
#include <netlink/netlink.h>
#include <netlink/socket.h>
#include <netlink/errno.h>
#include <netlink/route/rule.h>
#include <netlink/route/link.h>
#include <netlink/route/addr.h>
#include <netlink/route/route.h>
#include <netlink/route/nexthop.h>
#include <netlink/route/link/inet.h>
#include <netlink/route/link/veth.h>
#include <netlink/route/link/bridge.h>
#include <netlink/route/neighbour.h>

#define AI_LOG_NL_WARN(err, fmt, args...) \
    AI_LOG_WARN(fmt " (%d - %s)", ##args, -err, nl_geterror(err))

#define AI_LOG_NL_ERROR(err, fmt, args...) \
    AI_LOG_ERROR(fmt " (%d - %s)", ##args, -err, nl_geterror(err))

#define AI_LOG_NL_ERROR_EXIT(err, fmt, args...) \
    AI_LOG_ERROR_EXIT(fmt " (%d - %s)", ##args, -err, nl_geterror(err))



// -----------------------------------------------------------------------------
/**
 *  @class NlAddress
 *  @brief Wrapper around the nl_addr object
 *
 *  Simple wrapper used to handle construction and safe destruction of a nl
 *  addr object.
 */
class NlAddress
{
public:
    NlAddress()
        : mAddress(nl_addr_alloc(0)),
        mAddressFamily(AF_UNSPEC)
    { }

    explicit NlAddress(in_addr_t address, in_addr_t netmask = 0xffffffff)
        : mAddress(fromIpv4(address, netmask)),
        mAddressFamily(AF_INET)
    { }

    explicit NlAddress(struct in6_addr address, int netmask = 128)
        : mAddress(fromIpv6(address, netmask)),
        mAddressFamily(AF_INET6)
    { }

    explicit NlAddress(const std::array<uint8_t, 6> &mac)
        : mAddress(nl_addr_build(AF_LLC, mac.data(), mac.size())),
          mAddressFamily(AF_LLC)
    { }

    ~NlAddress()
    {
        if (mAddress)
            nl_addr_put(mAddress);
    }

public:
    explicit operator bool() const noexcept
    {
        return (mAddress != nullptr);
    }

    operator struct nl_addr*() const noexcept
    {
        return mAddress;
    }

public:
    std::string toString() const
    {
        char buf[64];
        return std::string(nl_addr2str(mAddress, buf, sizeof(buf)));
    }

private:
    static struct nl_addr* fromIpv4(in_addr_t address, in_addr_t netmask)
    {
        struct nl_addr* addr = nullptr;

        // if the netmask is zero then just create an empty addr
        if (netmask == 0)
        {
            addr = nl_addr_alloc(0);
            if (addr != nullptr)
            {
                nl_addr_set_family(addr, AF_INET);
            }
        }
        else
        {
            // netlink stores ip addresses in network order, not host order
            struct in_addr ip;
            ip.s_addr = htonl(address);

            addr = nl_addr_build(AF_INET, &ip, sizeof(struct in_addr));
            if (addr != nullptr)
            {
                nl_addr_set_prefixlen(addr, (33 - __builtin_ffs(netmask)));
            }
        }

        return addr;
    }

private:
    static struct nl_addr* fromIpv6(struct in6_addr address, int netmask)
    {
        struct nl_addr* addr = nullptr;

        // if the netmask is zero then just create an empty addr
        if (netmask == 0)
        {
            addr = nl_addr_alloc(0);
            if (addr != nullptr)
            {
                nl_addr_set_family(addr, AF_INET6);
            }
        }
        else
        {
            addr = nl_addr_build(AF_INET6, &address, sizeof(struct in6_addr));
            if (addr != nullptr)
            {
                nl_addr_set_prefixlen(addr, netmask);
            }
        }

        return addr;
    }

private:
    struct nl_addr* const mAddress;
    int mAddressFamily;
};


// -----------------------------------------------------------------------------
/**
 *  @class NlRouteAddress
 *  @brief Wrapper around the rtnl_addr object
 *
 *  Simple wrapper used to handle construction and safe destruction of a rtnl
 *  addr object.
 */
class NlRouteAddress
{
public:
    explicit NlRouteAddress(in_addr_t address, in_addr_t netmask = 0xffffffff)
        : mAddress(fromIpv4(address, netmask)),
        mAddressFamily(AF_INET)
    { }

    explicit NlRouteAddress(struct in6_addr address, int netmask = 128)
        : mAddress(fromIpv6(address, netmask)),
        mAddressFamily(AF_INET6)
    { }

    ~NlRouteAddress()
    {
        if (mAddress)
            rtnl_addr_put(mAddress);
    }

public:
    explicit operator bool() const noexcept
    {
        return (mAddress != nullptr);
    }

    operator struct rtnl_addr*() const noexcept
    {
        return mAddress;
    }

public:
    std::string toString() const
    {
        if (mAddress == nullptr)
            return std::string("null");

        char buf[128];
        nl_object_dump_buf(OBJ_CAST(mAddress), buf, sizeof(buf));

        std::string str(buf);
        if (str.back() == '\n')
            str.pop_back();

        return str;
    }

private:
    static struct rtnl_addr* fromIpv4(in_addr_t address, in_addr_t netmask)
    {
        // sanity check we have a valid netmask
        if (netmask == 0)
        {
            AI_LOG_ERROR("invalid netmask");
            return nullptr;
        }

        NlAddress local(address);
        if (!local)
        {
            AI_LOG_ERROR("failed to create ipv4 nl address");
            return nullptr;
        }
        NlAddress bcast(address | ~netmask);
        if (!bcast)
        {
            AI_LOG_ERROR("failed to create ipv4 nl broadcast address");
            return nullptr;
        }

        // create the actual route address
        struct rtnl_addr* addr = rtnl_addr_alloc();
        if (addr == nullptr)
        {
            AI_LOG_ERROR("failed to create route address");
            return nullptr;
        }

        rtnl_addr_set_family(addr, AF_INET);
        rtnl_addr_set_local(addr, local);
        rtnl_addr_set_broadcast(addr, bcast);
        rtnl_addr_set_prefixlen(addr, (33 - __builtin_ffs(netmask)));

        return addr;
    }

    static struct rtnl_addr* fromIpv6(struct in6_addr address, int netmask)
    {
        // sanity check we have a valid netmask
        if (netmask == 0)
        {
            AI_LOG_ERROR("invalid netmask");
            return nullptr;
        }

        NlAddress local(address);
        if (!local)
        {
            AI_LOG_ERROR("failed to create ipv6 nl address");
            return nullptr;
        }

        // create the actual route address
        struct rtnl_addr* addr = rtnl_addr_alloc();
        if (addr == nullptr)
        {
            AI_LOG_ERROR("failed to create route address");
            return nullptr;
        }

        rtnl_addr_set_family(addr, AF_INET6);
        rtnl_addr_set_local(addr, local);
        rtnl_addr_set_prefixlen(addr, netmask);

        return addr;
    }

private:
    struct rtnl_addr* const mAddress;
    int mAddressFamily;
};


// -----------------------------------------------------------------------------
/**
 *  @class NlRoute
 *  @brief Wrapper around the rtnl_route object
 *
 *  Simple wrapper used to handle construction and safe destruction of a rtnl
 *  route object.
 */
class NlRoute
{
public:
    NlRoute()
        : mRoute(rtnl_route_alloc())
    { }

    ~NlRoute()
    {
        if (mRoute != nullptr)
            rtnl_route_put(mRoute);
    }

public:
    explicit operator bool() const noexcept
    {
        return (mRoute != nullptr);
    }

    operator struct rtnl_route*() const noexcept
    {
        return mRoute;
    }

public:
    std::string toString() const
    {
        if (mRoute == nullptr)
            return std::string("null");

        char buf[128];
        nl_object_dump_buf(OBJ_CAST(mRoute), buf, sizeof(buf));

        std::string str(buf);
        if (str.back() == '\n')
            str.pop_back();

        return str;
    }

private:
    struct rtnl_route* const mRoute;
};


// -----------------------------------------------------------------------------
/**
 *  @class NlNeigh
 *  @brief Wrapper around the rtnl_neigh object
 *
 *  Simple wrapper used to handle construction and safe destruction of a rtnl
 *  neigh object.
 */
class NlNeigh
{
public:
    NlNeigh()
        : mNeigh(rtnl_neigh_alloc())
    { }

    ~NlNeigh()
    {
        if (mNeigh != nullptr)
            rtnl_neigh_put(mNeigh);
    }

public:
    explicit operator bool() const noexcept
    {
        return (mNeigh != nullptr);
    }

    operator struct rtnl_neigh*() const noexcept
    {
        return mNeigh;
    }

public:
    std::string toString() const
    {
        if (mNeigh == nullptr)
            return std::string("null");

        char buf[128];
        nl_object_dump_buf(OBJ_CAST(mNeigh), buf, sizeof(buf));

        std::string str(buf);
        if (str.back() == '\n')
            str.pop_back();

        return str;
    }

private:
    struct rtnl_neigh* const mNeigh;
};


// -----------------------------------------------------------------------------
/**
 *  @class NlLink
 *  @brief Wrapper around the rtnl_link object
 *
 *  Simple wrapper used to handle construction and safe destruction of a rtnl
 *  link object.
 */
class NlLink
{
public:
    NlLink()
        : mLink(rtnl_link_alloc())
    { }

    explicit NlLink(struct rtnl_link* link)
        : mLink(link)
    { }

    NlLink(struct nl_sock* nl, const std::string& name)
        : mLink(fromName(nl, name))
    { }

    ~NlLink()
    {
        if (mLink != nullptr)
            rtnl_link_put(mLink);
    }

public:
    explicit operator bool() const noexcept
    {
        return (mLink != nullptr);
    }

    operator struct rtnl_link*() const noexcept
    {
        return mLink;
    }

private:
    static struct rtnl_link* fromName(struct nl_sock* nl, const std::string& name)
    {
        struct rtnl_link* link = nullptr;

        int ret = rtnl_link_get_kernel(nl, -1, name.c_str(), &link);
        if (ret != 0)
        {
            AI_LOG_NL_WARN(ret, "failed to get interface with name '%s'",
                            name.c_str());
            return nullptr;
        }

        return link;
    }

private:
    struct rtnl_link* const mLink;
};


Netlink::Netlink()
    : mSocket(nullptr)
    , mSysClassNetDirFd(-1)
{
    AI_LOG_FN_ENTRY();

    // create the netlink socket
    mSocket = nl_socket_alloc();
    if (!mSocket)
    {
        AI_LOG_ERROR_EXIT("failed to create netlink socket");
        return;
    }

    // try and connect to the kernel
    int ret = nl_connect(mSocket, NETLINK_ROUTE);
    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "unable to connect to netlink socket");
        nl_socket_free(mSocket);
        mSocket = nullptr;
        return;
    }

    // set the FD_CLOEXEC flag on the socket
    int fd = nl_socket_get_fd(mSocket);
    if (fd < 0)
    {
        AI_LOG_ERROR("invalid socket fd");
        nl_socket_free(mSocket);
        mSocket = nullptr;
    }
    else
    {
        int flags = fcntl(fd, F_GETFD, 0);
        if (flags < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to get socket flags");
            nl_socket_free(mSocket);
            mSocket = nullptr;
        }
        else if (fcntl(fd, F_SETFD, flags | FD_CLOEXEC) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to set FD_CLOEXEC");
            nl_socket_free(mSocket);
            mSocket = nullptr;
        }
    }

    // open the "/sys/class/net" directory, we use this to scan for free names
    // of veth devices, so makes sense to open it just once
    mSysClassNetDirFd = open("/sys/class/net", O_CLOEXEC | O_DIRECTORY);
    if (mSysClassNetDirFd < 0)
    {
        AI_LOG_SYS_FATAL(errno, "failed to open '/sys/class/net'");
    }

    AI_LOG_FN_EXIT();
}

Netlink::~Netlink()
{
    AI_LOG_FN_ENTRY();

    if (mSocket != nullptr)
    {
        nl_socket_free(mSocket);
        mSocket = nullptr;
    }

    if ((mSysClassNetDirFd >= 0) && (close(mSysClassNetDirFd) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close fd");
    }

    AI_LOG_FN_EXIT();
}

bool Netlink::isValid() const
{
    std::lock_guard<std::mutex> locker(mLock);
    return (mSocket != nullptr);
}

bool Netlink::applyChangesToLink(const std::string& ifaceName,
                                 const NlLink& changes)
{
    AI_LOG_FN_ENTRY();

    // get the link with the given name
    NlLink link(mSocket, ifaceName);
    if (!link)
    {
        return false;
    }

    // apply the changes
    int ret = rtnl_link_change(mSocket, link, changes, 0);
    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to apply changes");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the ip address and netmask of an interface (IPv4)
 *
 *  This is the equivalent of the following on the command line
 *
 *      ifconfig <ifaceName> <address> netmask <netmask>
 *
 *  @param[in]  link        Instance of NlLink (rtnl_link wrapper)
 *  @param[in]  address     The address to set, the netmask will be applied to
 *                          this before setting on the iface
 *  @param[in]  netmask     The netmask to apply.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::setLinkAddress(const NlLink& link, const in_addr_t address,
                             const in_addr_t netmask)
{
    AI_LOG_FN_ENTRY();

    // create the link route address
    NlRouteAddress addr(address, netmask);
    if (!addr)
    {
        AI_LOG_ERROR_EXIT("failed to create route address object");
        return false;
    }

    AI_LOG_INFO("setting link address to '%s'", addr.toString().c_str());

    // set the link index
    rtnl_addr_set_link(addr, link);

    // add the address
    int ret = rtnl_addr_add(mSocket, addr, 0);
    if ((ret != 0) && (ret != -NLE_EXIST))
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to add new link address");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the ip address and netmask of an interface (IPv6)
 *
 *  This is the equivalent of the following on the command line
 *
 *      ifconfig <ifaceName> inet6 add <address>/<netmask>
 *
 *  @param[in]  link        Instance of NlLink (rtnl_link wrapper)
 *  @param[in]  address     The address to set, the netmask will be applied to
 *                          this before setting on the iface
 *  @param[in]  netmask     The netmask to apply.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::setLinkAddress(const NlLink& link, const struct in6_addr address,
                             const int netmask)
{
    AI_LOG_FN_ENTRY();

    // create the link route address
    NlRouteAddress addr(address, netmask);
    if (!addr)
    {
        AI_LOG_ERROR_EXIT("failed to create route address object");
        return false;
    }

    AI_LOG_INFO("setting link address to '%s'", addr.toString().c_str());

    // set the link index
    rtnl_addr_set_link(addr, link);

    // add the address
    int ret = rtnl_addr_add(mSocket, addr, 0);
    if ((ret != 0) && (ret != -NLE_EXIST))
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to add new link address");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the ip address and netmask of an interface (IPv4)
 *
 *  This is the equivalent of the following on the command line
 *
 *      ifconfig <ifaceName> <address> netmask <netmask>
 *
 *  @param[in]  ifaceName   The name of the interface to set the address on
 *  @param[in]  address     The address to set, the netmask will be applied to
 *                          this before setting on the iface
 *  @param[in]  netmask     The netmask to apply.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::setIfaceAddress(const std::string& ifaceName,
                              const in_addr_t address, const in_addr_t netmask)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // get the link with the given name
    NlLink link(mSocket, ifaceName);
    if (!link)
    {
        AI_LOG_ERROR_EXIT("failed to get link with name '%s'", ifaceName.c_str());
        return false;
    }

    // set the address on the link
    bool success = setLinkAddress(link, address, netmask);

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the ip address and netmask of an interface (IPv6)
 *
 *  This is the equivalent of the following on the command line
 *
 *      ifconfig <ifaceName> inet6 add <address>/<netmask>
 *
 *  @param[in]  ifaceName   The name of the interface to set the address on
 *  @param[in]  address     The IPv6 address to set, the netmask will be applied
 *                          to this before setting on the iface
 *  @param[in]  netmask     The netmask to apply.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::setIfaceAddress(const std::string& ifaceName,
                              const struct in6_addr address, const int netmask)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // get the link with the given name
    NlLink link(mSocket, ifaceName);
    if (!link)
    {
        AI_LOG_ERROR_EXIT("failed to get link with name '%s'", ifaceName.c_str());
        return false;
    }

    // set the address on the link
    bool success = setLinkAddress(link, address, netmask);

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the config on a given interface.
 *
 *  Used by setIfaceForwarding() and setIfaceRouteLocalNet().
 *
 *  @param[in]  ifaceName   The name of the interface to set the address on
 *  @param[in]  configId    The config to set.
 *  @param[in]  value       The value to set.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::setIfaceConfig(const std::string& ifaceName, const unsigned int configId,
                             const uint32_t value)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // create an empty link object with just the flags changed
    NlLink changes;
    if (!changes)
    {
        AI_LOG_ERROR_EXIT("failed to create changes object");
        return false;
    }

    // set the forwarding conf
    int ret = rtnl_link_inet_set_conf(changes, configId, value);
    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to set forwarding conf");
        return false;
    }

    // apply the changes
    bool success = applyChangesToLink(ifaceName, changes);

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Enables or disables IPv4 forwarding on the given interface
 *
 *  This is the equivalent of the following on the command line
 *
 *      echo "1" > /proc/sys/net/ipv4/conf/<ifaceName>/forwarding
 *  Or
 *      echo "0" > /proc/sys/net/ipv4/conf/<ifaceName>/forwarding
 *
 *  @param[in]  ifaceName   The name of the interface to set the config on
 *  @param[in]  enable      true to enable, false to disable.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::setIfaceForwarding(const std::string& ifaceName, bool enable)
{
    // the IPV4_DEVCONF_FORWARDING value is not defined in our toolchain,
    // however happily libnl has provided us a rtnl_link_inet_str2devconf()
    // function which given a string returns the devconf number
    int devConf = rtnl_link_inet_str2devconf("forwarding");
    if (devConf < 0)
    {
        AI_LOG_ERROR_EXIT("failed to get config id for 'forwarding'");
        return false;
    }

    return setIfaceConfig(ifaceName, devConf, enable ? 1 : 0);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Enables or disables IPv6 forwarding on the given interface
 *
 *  This is the equivalent of the following on the command line
 *
 *      echo "1" > /proc/sys/net/ipv6/conf/<ifaceName>/forwarding
 *  Or
 *      echo "0" > /proc/sys/net/ipv6/conf/<ifaceName>/forwarding
 *
 *  @param[in]  utils       Instance of the DobbyRdkPluginUtils class.
 *  @param[in]  ifaceName   The name of the interface to set the config on.
 *  @param[in]  enable      true to enable, false to disable.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::setIfaceForwarding6(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                  const std::string &ifaceName, bool enable)
{
    // IPv6 forwarding enable/disable has no API in libnl, change manually
    const std::string ipv6FwdPath = "/proc/sys/net/ipv6/conf/" + ifaceName + "/forwarding";

    return utils->writeTextFile(ipv6FwdPath, enable ? "1" : "0", O_TRUNC, 0);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the route_localnet flag on the interface.
 *
 *  Which means:
 *      "Do not consider loopback addresses as martian source or destination
 *       while routing. This enables the use of 127/8 for local routing purposes.
 *       default FALSE"
 *
 *  This is the equivalent of the following on the command line
 *
 *      echo "1" > /proc/sys/net/ipv4/conf/<ifaceName>/route_localnet
 *  Or
 *      echo "0" > /proc/sys/net/ipv4/conf/<ifaceName>/route_localnet
 *
 *  This is used so we can use iptables to route packets on the bridge interface
 *  to local host.  The main usage is for connecting specific ports like dns, as
 *  to the localhost interface.
 *
 *  @param[in]  ifaceName   The name of the interface to set the config on.
 *  @param[in]  enable      true to enable, false to disable.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::setIfaceRouteLocalNet(const std::string& ifaceName, bool enable)
{
    // the IPV4_DEVCONF_ROUTE_LOCALNET value is not defined in our toolchain,
    // however happily libnl has provided us a rtnl_link_inet_str2devconf()
    // function which given a string returns the devconf number
    int devConf = rtnl_link_inet_str2devconf("route_localnet");
    if (devConf < 0)
    {
        AI_LOG_ERROR_EXIT("failed to get config id for 'route_localnet'");
        return false;
    }

    return setIfaceConfig(ifaceName, devConf, enable ? 1 : 0);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the accept_ra flag on the interface.

 *  This is the equivalent of the following on the command line
 *
 *      echo "2" > /proc/sys/net/ipv6/conf/<ifaceName>/accept_ra
 *  Or
 *      echo "1" > /proc/sys/net/ipv6/conf/<ifaceName>/accept_ra
 *  Or
 *      echo "0" > /proc/sys/net/ipv6/conf/<ifaceName>/accept_ra
 *
 *  This is used to set accept_ra to "2" so that router advertisements are
 *  accepted on the interface even with forwarding enabled.
 *
 *  @param[in]  utils       Instance of the DobbyRdkPluginUtils class.
 *  @param[in]  ifaceName   The name of the interface to set the config on.
 *  @param[in]  enable      true to enable, false to disable.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::setIfaceAcceptRa(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                               const std::string& ifaceName, int value)
{
    // libnl doesn't have an API for editing IPv6 devconf values, so we have
    // to write it manually
    std::string path = "/proc/sys/net/ipv6/conf/" + ifaceName + "/accept_ra";
    std::string writeValue;

    switch (value)
    {
        case 2:
            writeValue = "2";
            break;
        case 1:
            writeValue = "1";
            break;
        case 0:
            writeValue = "0";
            break;
        default:
            AI_LOG_ERROR("accept_ra can only be set to values 2, 1 or 0");
            return false;
    }

    return utils->writeTextFile(path, writeValue, O_TRUNC, 0);;
}
// -----------------------------------------------------------------------------
/**
 *  @brief Brings an interface up
 *
 *  @param[in]  ifaceName   The name of the interface to bring up.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::ifaceUp(const std::string& ifaceName)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // create an empty link object with just the flags changed
    NlLink changes;
    if (!changes)
    {
        AI_LOG_ERROR_EXIT("failed to create changes object");
        return false;
    }

    // set the link state to up
    rtnl_link_set_flags(changes, IFF_UP);

    // apply the changes
    bool success = applyChangesToLink(ifaceName, changes);

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Takes an interface down
 *
 *  @param[in]  ifaceName   The name of the interface to take down.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::ifaceDown(const std::string& ifaceName)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // create an empty link object with just the flags changed
    NlLink changes;
    if (!changes)
    {
        AI_LOG_ERROR_EXIT("failed to create changes object");
        return false;
    }

    // set the link state to up
    rtnl_link_unset_flags(changes, IFF_UP);

    // apply the changes
    bool success = applyChangesToLink(ifaceName, changes);

    AI_LOG_FN_EXIT();
    return success;
}

 // -----------------------------------------------------------------------------
 /**
 *  @brief Sets the MAC address of the given interface.
 *
 *  This is primarily used to set a fixed MAC address for the bridge device.
 *
 *  @param[in]  ifaceName       The name of the interface to set.
 *  @param[in]  address         The MAC address to set.
 *
 *  @return true if successfully set.
 */
bool Netlink::setIfaceMAC(const std::string& ifaceName,
                          const std::array<uint8_t, 6>& address)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // get the current link
    NlLink current(mSocket, ifaceName);
    if (!current)
    {
        AI_LOG_ERROR_EXIT("failed to get link '%s'", ifaceName.c_str());
        return false;
    }

    AI_LOG_INFO("setting '%s' MAC address to %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                ifaceName.c_str(), address[0], address[1], address[2],
                                   address[3], address[4], address[5]);

    // create a new link with just mac changes
    NlLink newLink;

    // create a mac address object
    struct nl_addr *mac = nl_addr_build(AF_LLC, address.data(), address.size());
    if (!mac)
    {
        AI_LOG_ERROR_EXIT("failed to create MAC address object");
        return false;
    }

    // set the link address
    rtnl_link_set_addr(newLink, mac);
    nl_addr_put(mac);

    // and apply the change
    int err = rtnl_link_change(mSocket, current, newLink, 0);
    if (err != 0)
    {
        AI_LOG_NL_ERROR_EXIT(err, "failed to change MAC address on '%s'",
                             ifaceName.c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets the MAC address of the given interface.
 *
 *
 *  @param[in]  ifaceName       The name of the interface to get.
 *
 *  @return the mac address.
 */
std::array<uint8_t, 6> Netlink::getIfaceMAC(const std::string& ifaceName)
{
    AI_LOG_FN_ENTRY();

    std::array<uint8_t, 6> mac = { 0 };

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return mac;
    }

    // get the current link
    NlLink iface(mSocket, ifaceName);
    if (!iface)
    {
        AI_LOG_ERROR_EXIT("failed to get link '%s'", ifaceName.c_str());
        return mac;
    }

    // get the mac address of the link
    struct nl_addr *addr = rtnl_link_get_addr(iface);
    if (!addr)
    {
        AI_LOG_ERROR("failed to get MAC address of '%s'", ifaceName.c_str());
    }
    else if (nl_addr_get_len(addr) != mac.size())
    {
        AI_LOG_ERROR("invalid length of MAC address (%u bytes)",
                     nl_addr_get_len(addr));
    }
    else
    {
        const uint8_t *data = reinterpret_cast<const uint8_t*>(nl_addr_get_binary_addr(addr));
        std::copy(data, data + mac.size(), mac.begin());

        AI_LOG_INFO("'%s' MAC address is %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
                    ifaceName.c_str(), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    AI_LOG_FN_EXIT();
    return mac;
}

 // -----------------------------------------------------------------------------
 /**
 *  @brief Gets the set of interfaces currently enslaved to a given bridge device.
 *
 *
 *  @param[in]  bridgeName  The name of the bridge device
 *
 *  @return a list of interfaces attached to the bridge, or empty list on failure.
 */
std::list<Netlink::BridgePortDetails> Netlink::getAttachedIfaces(const std::string& bridgeName)
{
    AI_LOG_FN_ENTRY();

    std::list<Netlink::BridgePortDetails> ports;

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return ports;
    }

    // get a list of all bridge devices, plus all the links attached to them
    struct nl_cache *cache = nullptr;
    if (rtnl_link_alloc_cache(mSocket, AF_BRIDGE, &cache) < 0)
    {
        AI_LOG_ERROR_EXIT("failed to create cache of bridge devices");
        return ports;
    }

    // get the bridge interface ifindex from the cache
    int bridgeIFindex = rtnl_link_name2i(cache, bridgeName.c_str());
    if (bridgeIFindex <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to find bridge device with name '%s'",
                          bridgeName.c_str());
        nl_cache_free(cache);
        return ports;
    }


    // iterate through all bridges and links and get the ones enslaved to our
    // bridge
    struct nl_object *object = nl_cache_get_first(cache);
    while (object)
    {
        struct rtnl_link *iface = reinterpret_cast<struct rtnl_link *>(object);

        // get the name and the ifindex
        int linkIndex = rtnl_link_get_ifindex(iface);
        int masterIndex = rtnl_link_get_master(iface);

        // skip the actual bridge itself and links not enslaved to the bridge
        if ((linkIndex == bridgeIFindex) || (masterIndex != bridgeIFindex))
        {
            object = nl_cache_get_next(object);
            continue;
        }


        BridgePortDetails details;
        bzero(&details, sizeof(details));

        // get the mac address of the link
        struct nl_addr *mac = rtnl_link_get_addr(iface);
        if (!mac)
        {
            AI_LOG_ERROR("failed to get link MAC address");
        }
        else if (nl_addr_get_len(mac) != 6)
        {
            AI_LOG_ERROR("invalid length of MAC address (%u bytes)",
                         nl_addr_get_len(mac));
        }
        else
        {
            memcpy(details.mac, nl_addr_get_binary_addr(mac), 6);
        }

        // get the name and the ifindex
        details.index = linkIndex;
        strncpy(details.name, rtnl_link_get_name(iface), sizeof(details.name) - 1);

        // store in the set
        ports.emplace_back(details);

        AI_LOG_INFO("found iface %d: '%s' (%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx) enslaved to '%s''",
                    details.index, details.name,
                    details.mac[0], details.mac[1], details.mac[2], details.mac[3], details.mac[4], details.mac[5],
                    bridgeName.c_str());

        object = nl_cache_get_next(object);
    }

    nl_cache_free(cache);

    AI_LOG_FN_EXIT();
    return ports;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Queries the interface to determine if it's up or not
 *
 *
 *  @param[in]  ifaceName       The name of the interface to check
 *
 *  @return true if the interface is up, otherwise false.
 */
bool Netlink::ifaceIsUp(const std::string& ifaceName) const
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // get the link
    NlLink link(mSocket, ifaceName);
    if (!link)
    {
        AI_LOG_ERROR_EXIT("failed to get link '%s'", ifaceName.c_str());
        return false;
    }

    // get the link flags
    unsigned int flags = rtnl_link_get_flags(link);

    AI_LOG_FN_EXIT();
    return (flags & IFF_UP) ? true : false;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Checks if an interface with a given name exists (interface could be
 *  either up or down)
 *
 *
 *  @param[in]  ifaceName       The name of the interface to check
 *
 *  @return true if the interface exists, otherwise false.
 */
bool Netlink::ifaceExists(const std::string& ifaceName) const
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // get the link
    NlLink link(mSocket, ifaceName);
    if (!link)
    {
        AI_LOG_INFO("Interface %s does not exist", ifaceName.c_str());
        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Returns the number of the next free veth device.
 *
 *  This works by scanning /sys/class/net/ for devices with names "veth%d",
 *  the first one not found is returned.
 *
 *  @param[in]  startIndex      Starting index for veth names
 *
 *  @return the name of a free veth device, if all names are used (possible?) we
 *  return an empty string.
 */
std::string Netlink::getAvailableVethName(const int startIndex) const
{
    if (mSysClassNetDirFd < 0)
    {
        AI_LOG_ERROR("missing fd for '/sys/class/net' directory");
        return std::string();
    }

    char vethName[32];
    for (unsigned n = startIndex; n < 1024; n++)
    {
        snprintf(vethName, sizeof(vethName), "veth%u", n);
        if (faccessat(mSysClassNetDirFd, vethName, F_OK, AT_SYMLINK_NOFOLLOW) != 0)
        {
            return std::string(vethName);
        }
    }

    AI_LOG_ERROR("no available veth device names");
    return std::string();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates a veth pair for the netns attached to the given pid
 *
 *  @param[in]  peerVethName    The name of the veth interface inside the net
 *                              namespace (container), typically this will be
 *                              "eth0".
 *  @param[in]  peerPid         The pid of the process which has the netns we
 *                              want to create the veth in (i.e. the pid of
 *                              init process within the container).
 *  @param[in]  takenVeths      Veth devices reserved by other containers.
 *                              We want to check that in case of races.
 *
 *  @return on success the interface name of the veth pair, this is the name
 *  outside the container and will be of the form veth%d, ie veth0, veth1, etc.
 *  On failure an empty string is returned.
 */
std::string Netlink::createVeth(const std::string& peerVethName,
                                const pid_t peerPid,
                                std::vector<std::string> &takenVeths)
{
    AI_LOG_FN_ENTRY();

    if (peerVethName.empty() || (peerVethName.size() >= IFNAMSIZ))
    {
        AI_LOG_ERROR_EXIT("invalid peer veth name");
        return std::string();
    }

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return std::string();
    }

    // frustratingly choosing a name for a veth device is not straight forward,
    // we have to get the list of existing devices and then pick one not in
    // that list, if the creation fails we have to go back through the loop.
    // We can ask the kernel to generate a unique name itself, however there
    // is no way that it can signal that name back to us ... which seems stupid
    // but that's the way it is.
    std::string vethName;
    int vethNameStartIndex = 0;
    while (true)
    {
        // get an available interface name for the veth
        vethName = getAvailableVethName(vethNameStartIndex);
        if (vethName.empty())
        {
            AI_LOG_ERROR("no free veth%%d names available");
            break;
        }

        // check if some other container doesn't already claims this veth
        bool already_taken = false;
        for (auto & taken : takenVeths)
        {
            if(vethName.compare(taken) == 0)
            {
                already_taken = true;
                break;
            }
        }

        if (already_taken)
        {
            AI_LOG_WARN("Tried to use already taken vethName '%s', continue looking", vethName.c_str());
            // if more than one container is on we can "jump" to the next free one, we don't need
            // to iterate one by one
            vethNameStartIndex = std::stoi(vethName.erase(0, 4)) + 1;
            continue;
        }

        // create the veth pair
        int ret = rtnl_link_veth_add(mSocket, vethName.c_str(),
                                     peerVethName.c_str(), peerPid);
        if (ret == -NLE_EXIST)
        {
            AI_LOG_WARN("'%s' already exists, trying again to get free veth"
                        " name", vethName.c_str());

            // Sometimes when containers are killed outside of Dobby, the veth device is not properly
            // cleared. In this case getFreeVethName() will not find a veth device in /sys/class/net,
            // but rtnl_link_veth_add returns NLE_EXIST, indicating that the peer <-> veth device
            // link already exists. To combat this, we'll skip the not so free veth name.
            // Alternatively, we could try to use rtnl_link_veth_release to release the link that
            // exists even though the device doesn't, but that seems risky...
            vethNameStartIndex = std::stoi(vethName.erase(0, 4)) + 1;

            // surely if we've tried over 300 names by now, we won't find one
            if (vethNameStartIndex > 300)
            {
                AI_LOG_ERROR_EXIT("failed to find free veth device");
                return std::string();
            }

            continue;
        }
        else if (ret != 0)
        {
            AI_LOG_NL_ERROR(ret, "failed to create veth pair ('%s' : '%s')",
                            vethName.c_str(), peerVethName.c_str());
            vethName.clear();
            break;
        }

#if (AI_BUILD_TYPE == AI_DEBUG)
        // get the newly created veth link and ensure it is actually a veth type
        NlLink veth(mSocket, vethName);
        if (!veth)
        {
            AI_LOG_ERROR("failed to get newly created veth link '%s'",
                         vethName.c_str());
        }
        if (!rtnl_link_is_veth(veth))
        {
            AI_LOG_ERROR("odd, apparently link '%s' is not a veth type",
                         vethName.c_str());
        }
#endif // (AI_BUILD_TYPE == AI_DEBUG)

        AI_LOG_INFO("created veth pair ('%s' <-> '%s')", vethName.c_str(),
                    peerVethName.c_str());
        break;
    }

    AI_LOG_FN_EXIT();
    return vethName;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates a new bridge device
 *
 *  Note that it is not an error if a bridge device already exists with the
 *  same name, this method will return true in that case.
 *
 *  This is equivalent of the performing the following on the command line
 *
 *      brctl addbr <bridgeName>
 *
 *  @param[in]  bridgeName      The name of the new bridge device.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::createBridge(const std::string& bridgeName)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    int ret = rtnl_link_bridge_add(mSocket, (bridgeName.empty() ? nullptr : bridgeName.c_str()));
    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to create bridge named '%s'",
                             bridgeName.c_str());
        return false;
    }

    // oddly - if the bridge already exists the above function doesn't return
    // an error. However this is the behaviour we want, happy days.

    AI_LOG_INFO("created bridge device name '%s'", bridgeName.c_str());

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Deletes an existing bridge device.
 *
 *  This is equivalent of the performing the following on the command line
 *
 *      brctl delbr <bridgeName>
 *
 *  @param[in]  bridgeName      The name of the new bridge to delete.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::destroyBridge(const std::string& bridgeName)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // get the bridge link
    NlLink link(mSocket, bridgeName);
    if (!link)
    {
        AI_LOG_ERROR_EXIT("failed to get link '%s'", bridgeName.c_str());
        return false;
    }

    // sanity check the link is a bridge
    if (!rtnl_link_is_bridge(link))
    {
        AI_LOG_ERROR_EXIT("link '%s' is not a bridge", bridgeName.c_str());
        return false;
    }

    int ret = rtnl_link_delete(mSocket, link);
    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to delete link '%s'",
                             bridgeName.c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds an interface to the bridge device
 *
 *  This is equivalent of the performing the following on the command line
 *
 *      brctl addif <bridgeName> <ifaceName>
 *
 *  @return true on success, false on failure.
 */
bool Netlink::addIfaceToBridge(const std::string& bridgeName,
                               const std::string& ifaceName)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // get the bridge interface
    NlLink bridge(mSocket, bridgeName);
    if (!bridge)
    {
        AI_LOG_ERROR_EXIT("failed to get bridge '%s'", bridgeName.c_str());
        return false;
    }

    // sanity check the link is a bridge
    if (!rtnl_link_is_bridge(bridge))
    {
        AI_LOG_ERROR_EXIT("link '%s' is not a bridge", bridgeName.c_str());
        return false;
    }

    // get the interface we're trying to enslave
    NlLink iface(mSocket, ifaceName);
    if (!iface)
    {
        AI_LOG_ERROR_EXIT("failed to get interface '%s'", ifaceName.c_str());
        return false;
    }

    // try and enslave the interface to the bridge
    int ret = rtnl_link_enslave(mSocket, bridge, iface);
    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to enslave '%s' to bridge '%s'",
                             ifaceName.c_str(), bridgeName.c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes an interface from a bridge
 *
 *  This is equivalent of the performing the following on the command line
 *
 *      brctl delif <bridgeName> <ifaceName>
 *
 *  @return true on success, false on failure.
 */
bool Netlink::delIfaceFromBridge(const std::string& bridgeName,
                                 const std::string& ifaceName)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // get the interface we're trying to release
    struct rtnl_link* iface = nullptr;
    int ret = rtnl_link_get_kernel(mSocket, -1, ifaceName.c_str(), &iface);
    if ((ret != 0) || (iface == nullptr))
    {
        // couldn't find interface, no need to delete
        return true;
    }

    // pessimistic
    bool success = false;

    // sanity check the master of the iface is the bridge object
    int masterIndex = rtnl_link_get_master(iface);
    if (masterIndex < 1)
    {
        AI_LOG_ERROR("interface '%s' is not enslaved to any bridge",
                     ifaceName.c_str());
    }
    else
    {
        // get the master device, which should be the bridge
        struct rtnl_link* master = nullptr;
        ret = rtnl_link_get_kernel(mSocket, masterIndex, nullptr, &master);
        if ((ret != 0) || (master == nullptr))
        {
            AI_LOG_NL_ERROR(ret, "failed to get master device at index %d",
                         masterIndex);
        }
        else
        {
            const char* masterName = rtnl_link_get_name(master);
            if ((masterName == nullptr) || (masterName != bridgeName))
            {
                AI_LOG_ERROR("interface '%s' is ensalved to '%s', not '%s'",
                             ifaceName.c_str(), masterName, bridgeName.c_str());
            }
            else
            {
                // finally we can now release the interface from the bridge
                ret = rtnl_link_release(mSocket, iface);
                if (ret != 0)
                {
                    // If device not found, ignore - the veth must have been
                    // automatically cleaned up in the time it took to get here...
                    if (-ret != NLE_NODEV)
                    {
                        AI_LOG_NL_ERROR(ret, "failed to release '%s' from bridge '%s'",
                                 ifaceName.c_str(), bridgeName.c_str());
                    }
                }
                else
                {
                    success = true;
                }
            }

            // free the master (bridge) link
            rtnl_link_put(master);
        }
    }

    rtnl_link_put(iface);

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds a new route to the routing table (IPv4)
 *
 *  This is equivalent of the performing the following on the command line
 *
 *      ip route add <destination>/<netmask> via <gateway> dev <ifname>
 *
 *  @param[in]  iface       The name of the iface to route to
 *  @param[in]  destination The destination ip address
 *  @param[in]  netmask     The netmask for the destination ip address
 *  @param[in]  gateway     The ip address of the gateway
 *
 *  @return true on success, false on failure.
 */
bool Netlink::addRoute(const std::string &iface, const in_addr_t destination,
                       const in_addr_t netmask, const in_addr_t gateway)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // create the destination and gateway addresses
    NlAddress dstAddress(destination, netmask);
    NlAddress gwAddress(gateway);
    if (!dstAddress || !gwAddress)
    {
        AI_LOG_ERROR_EXIT("failed to create destination or gateway address");
        return false;
    }

    // get the link we want to route to
    NlLink link(mSocket, iface);
    if (!link)
    {
        AI_LOG_ERROR_EXIT("failed to get link '%s'", iface.c_str());
        return false;
    }

    // create the route
    NlRoute route;
    if (!route)
    {
        AI_LOG_ERROR_EXIT("failed to create empty route");
        return false;
    }

    // set parameters
    rtnl_route_set_scope(route, RT_SCOPE_UNIVERSE);
    rtnl_route_set_table(route, RT_TABLE_MAIN);
    rtnl_route_set_protocol(route, RTPROT_STATIC);

    int ret = rtnl_route_set_family(route, AF_INET);
    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to set the route family");
        return false;
    }
    ret = rtnl_route_set_dst(route, dstAddress);
    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to set the route destination");
        return false;
    }

    // create a 'next hop' object (nb once assigned to the route it'll be
    // freed when the route is destructed).
    struct rtnl_nexthop* nextHop = rtnl_route_nh_alloc();
    if (nextHop == nullptr)
    {
        AI_LOG_ERROR_EXIT("failed to create empty next hop");
        return false;
    }

    // set the next hop parameters
    rtnl_route_nh_set_gateway(nextHop, gwAddress);
    rtnl_route_nh_set_ifindex(nextHop, rtnl_link_get_ifindex(link));

    // add the next hop to the route
    rtnl_route_add_nexthop(route, nextHop);

    // and finally add the route to the table
    AI_LOG_INFO("adding route '%s'", route.toString().c_str());
    ret = rtnl_route_add(mSocket, route, 0);
    if (ret == -NLE_EXIST)
    {
        // failing to add a route that already exists isn't harmful for
        // operation, but indicates that there may have been a failed cleanup
        AI_LOG_WARN("failed to add route because it already exists");
        AI_LOG_FN_EXIT();
        return true;
    }

    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to add route");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds a new route to the routing table (IPv6)
 *
 *  This is equivalent of the performing the following on the command line
 *
 *      ip -6 route add <destination>/<netmask> via <gateway> dev <ifname>
 *
 *  @param[in]  iface       The name of the iface to route to
 *  @param[in]  destination The destination ip address
 *  @param[in]  netmask     The netmask for the destination ip address
 *  @param[in]  gateway     The ip address of the gateway, if null, we don't set
 *                          a gateway for the route.
 *
 *  @return true on success, false on failure.
 */
bool Netlink::addRoute(const std::string &iface, const struct in6_addr destination,
                       const int netmask, const struct in6_addr gateway)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // create the destination and gateway addresses
    NlAddress dstAddress(destination, netmask);
    if (!dstAddress)
    {
        AI_LOG_ERROR_EXIT("failed to create destination address");
        return false;
    }

    // get the link we want to route to
    NlLink link(mSocket, iface);
    if (!link)
    {
        AI_LOG_ERROR_EXIT("failed to get link '%s'", iface.c_str());
        return false;
    }

    // create the route
    NlRoute route;
    if (!route)
    {
        AI_LOG_ERROR_EXIT("failed to create empty route");
        return false;
    }

    // set parameters
    rtnl_route_set_scope(route, RT_SCOPE_UNIVERSE);
    rtnl_route_set_table(route, RT_TABLE_MAIN);
    rtnl_route_set_protocol(route, RTPROT_STATIC);

    int ret = rtnl_route_set_family(route, AF_INET6);
    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to set the route family");
        return false;
    }
    ret = rtnl_route_set_dst(route, dstAddress);
    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to set the route destination");
        return false;
    }

    // create a 'next hop' object (nb once assigned to the route it'll be
    // freed when the route is destructed).
    struct rtnl_nexthop* nextHop = rtnl_route_nh_alloc();
    if (nextHop == nullptr)
    {
        AI_LOG_ERROR_EXIT("failed to create empty next hop");
        return false;
    }

    // set the next hop interface
    rtnl_route_nh_set_ifindex(nextHop, rtnl_link_get_ifindex(link));

    // add nexthop gateway only if it's not ::0
    if (memcmp(&gateway, &IN6ADDR_ANY, sizeof(struct in6_addr)) != 0)
    {
        NlAddress gwAddress(gateway);
        if (!gwAddress)
        {
            AI_LOG_ERROR_EXIT("failed to create gateway address");
            rtnl_route_nh_free(nextHop);
            return false;
        }

        // set the next hop gateway
        rtnl_route_nh_set_gateway(nextHop, gwAddress);
    }

    // add the next hop to the route
    rtnl_route_add_nexthop(route, nextHop);

    // and finally add the route to the table
    AI_LOG_INFO("adding route '%s'", route.toString().c_str());
    ret = rtnl_route_add(mSocket, route, 0);


    if (ret == -NLE_EXIST)
    {
        // failing to add a route that already exists isn't harmful for
        // operation, but indicates that there may have been a failed cleanup
        AI_LOG_WARN("failed to add route because it already exists");
        AI_LOG_FN_EXIT();
        return true;
    }

    if (ret != 0)
    {
        AI_LOG_NL_ERROR_EXIT(ret, "failed to add route");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets an entry in the ARP table.
 *
 *  This is equivalent of the performing the following on the command line
 *
 *      arp -i <iface> -s <address> <mac>
 *
 *  @param[in]  iface       The name of the iface containing the ARP cache
 *  @param[in]  address     The destination ip address
 *  @param[in]  mac         The hardware address of the destination
 *
 *  @return true on success, false on failure.
 */
bool Netlink::addArpEntry(const std::string &iface, const in_addr_t address,
                          const std::array<uint8_t, 6> &mac)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // sanity check the MAC address is locally assigned
    if ((mac[0] & 0x02) == 0)
    {
        AI_LOG_ERROR_EXIT("invalid MAC address - not locally assigned, won't add to ARP table");
        return false;
    }

    // get the link we want to update the arp table for
    NlLink link(mSocket, iface);
    if (!link)
    {
        AI_LOG_ERROR_EXIT("failed to get link '%s'", iface.c_str());
        return false;
    }

    // allocate a new neighbor object
    NlNeigh neigh;
    if (!neigh)
    {
        AI_LOG_ERROR_EXIT("failed to allocate ARP table entry object");
        return false;
    }

    rtnl_neigh_set_ifindex(neigh, rtnl_link_get_ifindex(link));

    // set the destination IP address
    NlAddress dst(address);
    if (!dst)
    {
        AI_LOG_ERROR_EXIT("failed to build ARP destination address");
        return false;
    }
    rtnl_neigh_set_dst(neigh, dst);

    // set the MAC address
    NlAddress lladdr(mac);
    if (!lladdr)
    {
        AI_LOG_ERROR_EXIT("failed to build MAC address for ARP table");
        return false;
    }
    rtnl_neigh_set_lladdr(neigh, lladdr);

    // set the state to permanent
    rtnl_neigh_set_state(neigh, NUD_PERMANENT);

    // add the entry to the ARP table
    int err = rtnl_neigh_add(mSocket, neigh, NLM_F_CREATE | NLM_F_REPLACE);
    if (err < 0)
    {
        AI_LOG_NL_ERROR_EXIT(err, "failed to add ARP entry");
        return false;
    }

    AI_LOG_INFO("added ARP entry for %s -> %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx on %s",
                dst.toString().c_str(), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], iface.c_str());

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes (invalidates) an entry in the ARP table.
 *
 *  This is equivalent of the performing the following on the command line
 *
 *      arp -i <iface> -d <address>
 *
 *  @param[in]  iface       The name of the iface containing the ARP cache
 *  @param[in]  address     The destination ip address
 *
 *  @return true on success, false on failure.
 */
bool Netlink::delArpEntry(const std::string &iface, const in_addr_t address)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mSocket == nullptr)
    {
        AI_LOG_ERROR_EXIT("invalid socket");
        return false;
    }

    // get the link we want to update the arp table for
    NlLink link(mSocket, iface);
    if (!link)
    {
        AI_LOG_ERROR_EXIT("failed to get link '%s'", iface.c_str());
        return false;
    }

    // allocate a new neighbor object
    NlNeigh neigh;
    if (!neigh)
    {
        AI_LOG_ERROR_EXIT("failed to allocate ARP table entry object");
        return false;
    }

    rtnl_neigh_set_ifindex(neigh, rtnl_link_get_ifindex(link));

    // set the destination IP address
    NlAddress dst(address);
    if (!dst)
    {
        AI_LOG_ERROR_EXIT("failed to build ARP destination address");
        return false;
    }
    rtnl_neigh_set_dst(neigh, dst);

    // attempt to remove the actual entry
    int err = rtnl_neigh_delete(mSocket, neigh, 0);
    if (err < 0)
    {
        AI_LOG_NL_ERROR_EXIT(err, "failed to delete ARP entry");
        return false;
    }

    AI_LOG_INFO("deleted ARP entry for %s on %s", dst.toString().c_str(), iface.c_str());

    AI_LOG_FN_EXIT();
    return true;
}
