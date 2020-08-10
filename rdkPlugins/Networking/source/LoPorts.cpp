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

#include "LoPorts.h"

#include <algorithm>
#include <Logging.h>
#include <arpa/inet.h>


// -----------------------------------------------------------------------------
/**
 *  @brief Adds the two iptables firewall rules to enable port forwarding.
 *
 *  The 'protocol' field can be omitted in which case TCP will be specified.
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  loPorts             libocispec structs containing ports to
 *                                  enable port forwarding on.
 *  @param[in]  len                 Number of ports to forward.
 *
 *  @return true on success, false on failure.
 */
bool LoPorts::addLoPorts(const std::shared_ptr<Netfilter> &netfilter,
                         const std::shared_ptr<NetworkingHelper> &helper,
                         const std::string &containerId,
                         rt_defs_plugins_networking_data_loports_element **loPorts,
                         const size_t len)
{
    AI_LOG_FN_ENTRY();

    // add IPv4 rules to iptables if needed
    if (helper->ipv4())
    {
        Netfilter::RuleSet ipv4Rules = constructRules(helper, containerId, loPorts, len, AF_INET);

        // insert constructed rules to iptables
        if (!netfilter->insertRules(ipv4Rules, AF_INET))
        {
            AI_LOG_ERROR_EXIT("failed to insert port forwarding rule in iptables");
            return false;
        }
    }

    // add IPv6 rules to iptables if needed
    if (helper->ipv6())
    {
        Netfilter::RuleSet ipv6Rules = constructRules(helper, containerId, loPorts, len, AF_INET6);

        // insert constructed rules to ip6tables
        if (!netfilter->insertRules(ipv6Rules, AF_INET6))
        {
            AI_LOG_ERROR_EXIT("failed to insert port forwarding rule in ip6tables");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes the port forwarding rules added at container launch.
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  loPorts             libocispec structs containing ports to
 *                                  enable port forwarding on.
 *  @param[in]  len                 Number of ports to forward.
 *
 *  @return always returns true.
 */
bool LoPorts::removeLoPorts(const std::shared_ptr<Netfilter> &netfilter,
                            const std::shared_ptr<NetworkingHelper> &helper,
                            const std::string &containerId,
                            rt_defs_plugins_networking_data_loports_element **loPorts,
                            const size_t len)
{
    AI_LOG_FN_ENTRY();

    // delete IPv4 rules from iptables if needed
    if (helper->ipv4())
    {
        Netfilter::RuleSet ipv4Rules = constructRules(helper, containerId, loPorts, len, AF_INET);

        // delete constructed rulesets
        if (!netfilter->deleteRules(ipv4Rules, AF_INET))
        {
            AI_LOG_ERROR_EXIT("failed to delete port forwarding rules");
            return false;
        }

    }

    // delete IPv6 rules from ip6tables if needed
    if (helper->ipv6())
    {
        Netfilter::RuleSet ipv6Rules = constructRules(helper, containerId, loPorts, len, AF_INET6);

        // delete constructed rulesets
        if (!netfilter->deleteRules(ipv6Rules, AF_INET6))
        {
            AI_LOG_ERROR_EXIT("failed to delete port forwarding rules");
            return false;
        }

    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Construct localhost port forwarding rules based on bundle config.
 *
 *  The first rule will send anything from the container on the given port to
 *  localhost outside the container.
 *
 *      iptables -t nat -I PREROUTING -s <CONTAINER_IP> -d <BRIDGE_ADDRESS>
 *               -i <BRIDGE_NAME> -p tcp -m tcp --dport <PORT_NUMBER>
 *               -j DNAT --to-destination 127.0.0.1:<PORT_NUMBER>
 *
 *  And the second rule allows packets from the container over the dobby bridge
 *  to localhost.
 *
 *      iptables -I DobbyInputChain -s <CONTAINER_IP> -d 127.0.0.1/32
 *               -i <BRIDGE_NAME> -p tcp -m tcp --dport <PORT_NUMBER>
 *               -m physdev --physdev-in <VETH_NAME> -j ACCEPT
 *
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  loPorts             ports to enable forwarding on.
 *  @param[in]  len                 Number of ports/protocol combinations.
 *  @param[in]  ipVersion           IPv family version (AF_INET/AF_INET6).
 *
 *  @return always returns true.
 */
Netfilter::RuleSet constructRules(const std::shared_ptr<NetworkingHelper> &helper,
                                  const std::string &containerId,
                                  rt_defs_plugins_networking_data_loports_element **loPorts,
                                  const size_t len,
                                  const int ipVersion)
{
    std::string address;
    if (ipVersion == AF_INET)
    {
        address = helper->ipv4AddrStr();
    }
    else if (ipVersion == AF_INET6)
    {
        address = helper->ipv6AddrStr();
    }
    else
    {
        AI_LOG_ERROR_EXIT("supported ip address families are AF_INET or AF_INET6");
        return Netfilter::RuleSet();
    }

    std::list<std::string> dnatRules;
    std::list<std::string> acceptRules;

    // construct rules for each port to enable forwarding on
    for (int i = 0; i < len; i++)
    {
        std::string port = std::to_string(loPorts[i]->port);

        // default to tcp if no protocol is set
        std::string protocol;
        if (loPorts[i]->protocol)
        {
            protocol = loPorts[i]->protocol;

            // transform to lower case to allow both upper and lower case entries
            std::transform(protocol.begin(), protocol.end(), protocol.begin(), ::tolower);

            // check for accepted protocol values
            if (strcmp(protocol.c_str(), "tcp") != 0 && strcmp(protocol.c_str(), "udp") != 0)
            {
                AI_LOG_ERROR("invalid protocol value '%s' for loPort %d",
                             loPorts[i]->protocol, i);
                return Netfilter::RuleSet();
            }
        }
        else
        {
            protocol = "tcp";
        }

        // construct dnat rule to append to iptables
        const std::string dnatRule =
            createDnatRule(containerId, protocol, address, port, ipVersion);
        dnatRules.emplace_back(dnatRule);

        // construct accept rule to insert to iptables
        const std::string acceptRule =
            createAcceptRule(containerId, protocol, address,
                             helper->vethName(), port, ipVersion);
        acceptRules.emplace_back(acceptRule);
    }

    Netfilter::RuleSet ruleSet =
    {
        { Netfilter::TableType::Nat, dnatRules },
        { Netfilter::TableType::Filter, acceptRules }
    };

    return ruleSet;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Constructs a DNAT PREROUTING rule to send anything from the container
 *  on the given port to localhost outside the container.
 *
 *  @param[in]  id          The id of the container.
 *  @param[in]  protocol    The string name of protocol to create the rule for.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  portNumber  The port number to forward.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET).
 *
 *  @return the iptables formatted string.
 */
std::string createDnatRule(const std::string &id,
                           const std::string &protocol,
                           const std::string &ipAddress,
                           const std::string &portNumber,
                           const int ipVersion)
{
    char buf[256];

    std::string sourceAddr;
    std::string bridgeAddr;
    std::string destination;

    std::string baseRule("PREROUTING "
                         "-s %s "
                         "-d %s "
                         "-i " BRIDGE_NAME " -p %s -m %s "
                         "--dport %s "
                         "-m comment --comment %s "
                         "-j DNAT --to-destination %s");

    if (ipVersion == AF_INET)
    {
        sourceAddr = std::string() + ipAddress + "/32";
        bridgeAddr = std::string() + BRIDGE_ADDRESS + "/32";
        destination = "127.0.0.1:" + portNumber;
    }
    else
    {
        sourceAddr = std::string() + ipAddress + "/128";
        bridgeAddr = std::string() + BRIDGE_ADDRESS_IPV6 + "/128";
        destination = "[::1]:" + portNumber;
    }

    snprintf(buf, sizeof(buf), baseRule.c_str(),
             sourceAddr.c_str(),
             bridgeAddr.c_str(),
             protocol.c_str(), protocol.c_str(),
             portNumber.c_str(),
             id.c_str(),
             destination.c_str());

    return std::string(buf);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs a INPUT ACCEPT rule to allow packets from the container
 *  over the dobby0 bridge to localhost.
 *
 *  @param[in]  id          The id of the container.
 *  @param[in]  protocol    The string name of protocol to create the rule for.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  vethName    The name of the veth device that belongs to the container.
 *  @param[in]  portNumber  The port number to forward.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET).
 *
 *  @return the iptables formatted string.
 */
std::string createAcceptRule(const std::string &id,
                             const std::string &protocol,
                             const std::string &ipAddress,
                             const std::string &vethName,
                             const std::string &portNumber,
                             const int ipVersion)
{
    char buf[256];

    std::string sourceAddr;
    std::string loAddr;

    std::string baseRule("DobbyInputChain "
                         "-s %s "
                         "-d %s "
                         "-i " BRIDGE_NAME " -p %s -m %s "
                         "--dport %s "
                         "-m physdev --physdev-in %s "
                         "-m comment --comment %s "
                         "-j ACCEPT");

    if (ipVersion == AF_INET)
    {
        sourceAddr = std::string() + ipAddress + "/32";
        loAddr = "127.0.0.1/32";
    }
    else
    {
        sourceAddr = std::string() + ipAddress + "/128";
        loAddr = "::1/128";
    }

    snprintf(buf, sizeof(buf), baseRule.c_str(),
             sourceAddr.c_str(),
             loAddr.c_str(),
             protocol.c_str(), protocol.c_str(),
             portNumber.c_str(),
             vethName.c_str(),
             id.c_str());

    return std::string(buf);
}