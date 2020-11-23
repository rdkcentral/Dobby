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

#include "MulticastForwarder.h"
#include "NetworkingPluginCommon.h"

#include <Logging.h>
#include <sstream>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>

#define SMCROUTE_PATH "/usr/sbin/smcroutectl"
#define EBTABLES_PATH "/sbin/ebtables"

// -----------------------------------------------------------------------------
/**
 *  @brief Adds iptables rules, ebtables rules and smcroute to enable multicast
 *  forwarding.
 *
 *  @param[in]  netfilter       Instance of the Netfilter class.
 *  @param[in]  pluginData      Networking plugin data structure.
 *  @param[in]  vethName        The container's veth device name
 *  @param[in]  containerId     The container's identifier.
 *  @param[in]  extIfaces       External interfaces on the device.
 *
 *  @return true on success, false on failure.
 */
bool MulticastForwarder::set(const std::shared_ptr<Netfilter> &netfilter,
                             const rt_defs_plugins_networking_data *pluginData,
                             const std::string &vethName,
                             const std::string &containerId,
                             const std::vector<std::string> &extIfaces)
{
    AI_LOG_FN_ENTRY();

    // before attempting to add rules, check that the required programs exist
    if (!checkCompatibility())
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    std::mutex lock;
    std::lock_guard<std::mutex> locker(lock);

    for (int i = 0; i < pluginData->multicast_forwarding_len; i++)
    {
        const std::string address = pluginData->multicast_forwarding[i]->ip;
        const uint16_t port = pluginData->multicast_forwarding[i]->port;

        // check ip address family type
        const int addrFamily = checkAddressFamily(address);
        if (addrFamily < 0)
        {
            AI_LOG_ERROR_EXIT("MulticastForwarder address %s family is not"
                              " IPv4 or IPv6", address.c_str());
            return false;
        }

        // construct iptables ruleset
        Netfilter::RuleSet rules =
        {
            {
                Netfilter::TableType::Filter,
                { constructForwardingIptablesRule(containerId, address, port, addrFamily) }
            },
            {
                Netfilter::TableType::Mangle,
                { constructPreRoutingIptablesRule(containerId, address, port, addrFamily) }
            }
        };

        // add ruleset to be inserted to iptables
        if (!netfilter->addRules(rules, addrFamily, Netfilter::Operation::Insert))
        {
            AI_LOG_ERROR_EXIT("failed to add MulticastForwarder iptables rules"
                              " %s:%d for insertion", address.c_str(), port);
            return false;
        }


        // insert ebtables rules
        if (!executeCommand(EBTABLES_PATH " -I " + constructEbtablesRule(address, vethName, addrFamily)))
        {
            AI_LOG_ERROR_EXIT("failed to insert MulticastForwarder ebtables "
                              "rules for '%s', group %s", containerId.c_str(),
                              address.c_str());
            return false;
        }

        // add smcroute rules
        if (!addSmcrouteRules(extIfaces, address))
        {
            AI_LOG_ERROR_EXIT("failed to apply MulticastForwarder smcroute "
                              "rules for '%s', group %s", containerId.c_str(),
                              address.c_str());
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Removes the iptables rules, ebtables rules and smcroute that were
 *  added with addMulticastRules().
 *
 *  @param[in]  netfilter       Instance of the Netfilter class.
 *  @param[in]  pluginData      Networking plugin data structure.
 *  @param[in]  vethName        The container's veth device name
 *  @param[in]  containerId     The container's identifier.
 *  @param[in]  extIfaces       External interfaces on the device.
 *
 *  @return true on success, false on failure.
 */
bool MulticastForwarder::removeRules(const std::shared_ptr<Netfilter> &netfilter,
                                     const rt_defs_plugins_networking_data *pluginData,
                                     const std::string &vethName,
                                     const std::string &containerId,
                                     const std::vector<std::string> &extIfaces)
{
    AI_LOG_FN_ENTRY();

    // before attempting to remove rules, check that the required programs exist
    if (!checkCompatibility())
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    std::mutex lock;
    std::lock_guard<std::mutex> locker(lock);

    for (int i = 0; i < pluginData->multicast_forwarding_len; i++)
    {
        const std::string address = pluginData->multicast_forwarding[i]->ip;
        const uint16_t port = pluginData->multicast_forwarding[i]->port;

        // check ip address family type
        const int addrFamily = checkAddressFamily(address);
        if (addrFamily < 0)
        {
            AI_LOG_ERROR_EXIT("MulticastForwarder address %s family is not"
                              " IPv4 or IPv6", address.c_str());
            return false;
        }

        // construct iptables ruleset
        Netfilter::RuleSet rules =
        {
            {
                Netfilter::TableType::Filter,
                { constructForwardingIptablesRule(containerId, address, port, addrFamily) }
            },
            {
                Netfilter::TableType::Mangle,
                { constructPreRoutingIptablesRule(containerId, address, port, addrFamily) }
            }
        };

        // add ruleset to be inserted to iptables
        if (!netfilter->addRules(rules, addrFamily, Netfilter::Operation::Delete))
        {
            AI_LOG_ERROR_EXIT("failed to add MulticastForwarder iptables rules"
                              " %s:%d for deletion", address.c_str(), port);
            return false;
        }


        // insert ebtables rules
        if (!executeCommand(EBTABLES_PATH " -D " + constructEbtablesRule(address, vethName, addrFamily)))
        {
            AI_LOG_ERROR_EXIT("failed to delete MulticastForwarder ebtables "
                              "rules for '%s', group %s", containerId.c_str(),
                              address.c_str());
            return false;
        }

        // add smcroute rules
        if (!removeSmcrouteRules(extIfaces, address))
        {
            AI_LOG_ERROR_EXIT("failed to remove MulticastForwarder smcroute "
                              "rules for '%s', group %s", containerId.c_str(),
                              address.c_str());
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Simply checks that ebtables and smcroutectl are available.
 *
 *  iptables isn't checked, because it's generally available on all builds.
 *
 *  @return true if success, otherwise false.
 */
bool checkCompatibility()
{
    struct stat buffer;

    if (stat (EBTABLES_PATH, &buffer) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Multicast forwarding not supported - ebtables not found in PATH");
        return false;
    }

    memset(&buffer, 0, sizeof(buffer));

    if (stat (SMCROUTE_PATH, &buffer) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Multicast forwarding not supported - smcroutectl not found in PATH");
        return false;
    }

    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Checks IP address family.
 *
 *  Checks input address's address family if it's IPv4 or IPv6. If the address
 *  is anything else, -1 is returned to indicate an error.
 *
 *  @param[in]  address         Address to be checked for family.
 *
 *  @return address family AF_INET/AF_INET6 or -1 on failure.
 */
int checkAddressFamily(const std::string &address)
{
    struct addrinfo *res = nullptr;
    struct addrinfo hint;
    memset(&hint, '\0', sizeof(hint));

    // configure hints - check for AF_INET/AF_INET6 from string
    hint.ai_family = PF_UNSPEC;
    hint.ai_flags = AI_NUMERICHOST;

    int ret = getaddrinfo(address.c_str(), nullptr, &hint, &res);
    if (ret < 0 || res->ai_family != AF_INET && res->ai_family != AF_INET6)
    {
        return -1;
    }

    return res->ai_family;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Simply executes the given command
 *
 *  @param[in]  command         Command to be executed.
 *
 *  @return true on success, otherwise false.
 */
bool executeCommand(const std::string &command)
{
    std::string noOutputCommand = command + " &> /dev/null";
    FILE* pipe = popen(noOutputCommand.c_str(), "r");
    if (!pipe)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "popen failed");
        return false;
    }

    int returnCode = pclose(pipe);
    if (returnCode < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to exec command `%s`",
                              noOutputCommand.c_str());
        return false;
    }
    else if(returnCode > 0)
    {
        AI_LOG_ERROR_EXIT("failed to exec command `%s`, command returned code %d",
                          noOutputCommand.c_str(), returnCode);
        return false;
    }

    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Adds the smcroute rule to route multicast traffic from the specified
 *  group to the Dobby bridge device.
 *
 *  This is equivalent to the following on a command line:
 *
 *      smcroutectl add <IF_DEVICE_NAME> <ADDRESS> <BRIDGE_NAME>
 *
 *  @param[in]  extIfaces       External interfaces on the device.
 *  @param[in]  address         Address of the multicast group to forward.
 *
 *  @return true on success, otherwise false.
 */
bool addSmcrouteRules(const std::vector<std::string> &extIfaces, const std::string &address)
{
    for (const auto &extIface : extIfaces)
    {
        if (!executeCommand(SMCROUTE_PATH " add " + extIface + " " + address + " " + BRIDGE_NAME))
        {
            AI_LOG_ERROR_EXIT("failed to add smcroute rule for ip %s, interface"
                              " %s", address.c_str(), extIface.c_str());
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes the smcroute rule.
 *
 *  This is equivalent to the following on a command line:
 *
 *      smcroutectl remove <IF_DEVICE_NAME> <ADDRESS>
 *
 *  @param[in]  extIfaces       External interfaces on the device.
 *  @param[in]  address         Address of the multicast group to forward.
 *
 *  @return true on success, otherwise false.
 */
bool removeSmcrouteRules(const std::vector<std::string> &extIfaces, const std::string &address)
{
    for (const auto &extIface : extIfaces)
    {
        if (!executeCommand(SMCROUTE_PATH " remove " + extIface + " " + address))
        {
            AI_LOG_ERROR_EXIT("failed to remove MulticastForwarder smcroute rule "
                              "for ip %s, inbound interface %s",
                              address.c_str(), extIface.c_str());
            return false;
        }
    }

    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Constructs a TTL PREROUTING rule.
 *
 *  Increase TTL by one to allow multicast routing.
 *
 *  @see https://github.com/troglobit/smcroute (Usage)
 *
 *  This is equivalent to the following on a command line:
 *
 *      iptables -t mangle -I PREROUTING -d <ADDRESS/MASK> ! -i <BRIDGE_NAME>
 *               -p udp -m udp --dport <PORT>
 *               -m comment --comment <CONTAINER_ID> -j TTL -ttl-inc 1
 *
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  address             Destination IP address.
 *  @param[in]  port                Destination port.
 *  @param[in]  addressFamily       IP address family.
 *
 *  @return TTL PREROUTING iptables rule.
 */
std::string constructPreRoutingIptablesRule(const std::string &containerId,
                                            const std::string &address,
                                            const in_port_t port,
                                            const int addressFamily)
{
    std::string baseRule("PREROUTING "
                         "-d %s "                       // address/mask
                         "! -i " BRIDGE_NAME " "
                         "-p udp -m udp --dport %hu "   // port
                         "-m comment --comment %s "     // container id
                         "-j TTL --ttl-inc 1");

    // create addresses based on IP family
    std::string destAddr = (addressFamily == AF_INET) ?
                            address + "/32" : address + "/128";

    char buf[256];
    snprintf(buf, sizeof(buf), baseRule.c_str(),
             address.c_str(),
             port,
             containerId.c_str());

    return std::string(buf);
}


// -----------------------------------------------------------------------------
/**
 *  @brief Constructs a FORWARD ACCEPT rule to allow traffic to the given
 *  address/port combination via the bridge device.
 *
 *  This is equivalent to the following on a command line:
 *
 *      iptables -I FORWARD -d <ADDRESS/MASK> ! -i <BRIDGE_NAME>
 *               -o <BRIDGE_NAME> -p udp -m udp --dport <PORT>
 *               -m comment --comment <CONTAINER_ID> -j ACCEPT
 *
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  address             Destination IP address.
 *  @param[in]  port                Destination port.
 *  @param[in]  addressFamily       IP address family.
 *
 *  @return FORWARD ACCEPT iptables rule.
 */
std::string constructForwardingIptablesRule(const std::string &containerId,
                                            const std::string &address,
                                            const in_port_t port,
                                            const int addressFamily)
{
    std::string baseRule("FORWARD "
                         "-d %s "                       // address/mask
                         "! -i " BRIDGE_NAME " "
                         "-o " BRIDGE_NAME " "
                         "-p udp -m udp --dport %hu "   // port
                         "-m comment --comment %s "     // container id
                         "-j ACCEPT");

    // create addresses based on IP family
    std::string destAddr = (addressFamily == AF_INET) ?
                            address + "/32" : address + "/128";

    char buf[256];
    snprintf(buf, sizeof(buf), baseRule.c_str(),
             address.c_str(),
             port,
             containerId.c_str());

    return std::string(buf);
}


// -----------------------------------------------------------------------------
/**
 *  @brief Constructs ebtables arguments for insertion or removal.
 *
 *  This is equivalent to the following on a command line:
 *
 *      ebtables -I OUTPUT -o <VETH_NAME> -p <ADDRESS_FAMILY>
 *               --ip-dst <ADDRESS> -j ACCEPT
 *
 *  @param[in]  address             IP address.
 *  @param[in]  vethName            Name of the container's veth device.
 *  @param[in]  addressFamily       IP address family.
 *
 *  @return ebtables argument string.
 */
std::string constructEbtablesRule(const std::string &address,
                                  const std::string &vethName,
                                  const int addressFamily)
{
    char buf[256];

    const std::string addrFamily = (addressFamily == AF_INET) ? "IPv4 --ip-dst" : "IPv6 --ip6-dst";

    snprintf(buf, sizeof(buf), "OUTPUT -o %s -p %s %s -j ACCEPT",
             vethName.c_str(),
             addrFamily.c_str(),
             address.c_str());

    return std::string(buf);
}
