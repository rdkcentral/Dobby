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

#include "PortForwarding.h"

#include <algorithm>
#include <Logging.h>


// -----------------------------------------------------------------------------
/**
 *  @brief Adds the two iptables firewall rules to enable port forwarding.
 *
 *  The 'protocol' field can be omitted in which case TCP will be specified.
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  portForwards        libocispec structs containing ports to
 *                                  forward.
 *
 *  @return true on success, otherwise false.
 */
bool PortForwarding::addPortForwards(const std::shared_ptr<Netfilter> &netfilter,
                                     const std::shared_ptr<NetworkingHelper> &helper,
                                     const std::string &containerId,
                                     rt_defs_plugins_networking_data_port_forwarding *portForwards)
{
    AI_LOG_FN_ENTRY();

    // add IPv4 rules to iptables if needed
    if (helper->ipv4())
    {
        std::vector<Netfilter::RuleSet> ipv4Rules = constructRules(helper,
                                                                   containerId,
                                                                   portForwards,
                                                                   AF_INET);
        if (ipv4Rules.empty())
        {
            AI_LOG_ERROR_EXIT("failed to construct port forward iptables rules");
            return false;
        }

        // insert vector index 0 of constructed rules
        if (!netfilter->insertRules(ipv4Rules[0], AF_INET))
        {
            AI_LOG_ERROR_EXIT("failed to insert port forward rules to iptables");
            return false;
        }

        // append potential rules from vector index 1 of constructed rules
        if (ipv4Rules.size() > 1)
        {
            if (!netfilter->appendRules(ipv4Rules[1], AF_INET))
            {
                AI_LOG_ERROR_EXIT("failed to append port forward rules to iptables");
                return false;
            }
        }
    }

    // add IPv6 rules to iptables if needed
    if (helper->ipv6())
    {
        std::vector<Netfilter::RuleSet> ipv6Rules = constructRules(helper,
                                                                   containerId,
                                                                   portForwards,
                                                                   AF_INET6);
        if (ipv6Rules.empty())
        {
            AI_LOG_ERROR_EXIT("failed to construct port forward ip6tables rules");
            return false;
        }

        // insert vector index 0 of constructed rules
        if (!netfilter->insertRules(ipv6Rules[0], AF_INET6))
        {
            AI_LOG_ERROR_EXIT("failed to insert port forward rules to ip6tables");
            return false;
        }

        // append potential rules from vector index 1 of constructed rules
        if (ipv6Rules.size() > 1)
        {
            if (!netfilter->appendRules(ipv6Rules[1], AF_INET6))
            {
                AI_LOG_ERROR_EXIT("failed to append port forward rules to ip6tables");
                return false;
            }
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Removes port forwarding rules assigned to the container.
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  portForwards        libocispec structs containing ports to
 *                                  forward.
 *
 *  @return true on success, otherwise false.
 */
bool PortForwarding::removePortForwards(const std::shared_ptr<Netfilter> &netfilter,
                                        const std::shared_ptr<NetworkingHelper> &helper,
                                        const std::string &containerId,
                                        rt_defs_plugins_networking_data_port_forwarding *portForwards)
{
    AI_LOG_FN_ENTRY();

    // delete IPv4 rules from ip6tables if needed
    if (helper->ipv4())
    {
        std::vector<Netfilter::RuleSet> ipv4Rules = constructRules(helper,
                                                                   containerId,
                                                                   portForwards,
                                                                   AF_INET);
        if (ipv4Rules.empty())
        {
            AI_LOG_ERROR_EXIT("failed to construct iptables rules to remove");
            return false;
        }

        // delete constructed rulesets
        for (int i = 0; i < ipv4Rules.size(); i++)
        {
            if (!netfilter->deleteRules(ipv4Rules[i], AF_INET))
            {
                AI_LOG_ERROR_EXIT("failed to delete port forwarding ip6tables rule"
                                  "at index %d", i);
                return false;
            }
        }

    }

    // delete IPv6 rules from ip6tables if needed
    if (helper->ipv6())
    {
        std::vector<Netfilter::RuleSet> ipv6Rules = constructRules(helper,
                                                                   containerId,
                                                                   portForwards,
                                                                   AF_INET6);
        if (ipv6Rules.empty())
        {
            AI_LOG_ERROR_EXIT("failed to construct ip6tables rules to remove");
            return false;
        }

        // delete constructed rulesets
        for (int i = 0; i < ipv6Rules.size(); i++)
        {
            if (!netfilter->deleteRules(ipv6Rules[i], AF_INET6))
            {
                AI_LOG_ERROR_EXIT("failed to delete port forwarding ip6tables rule"
                                  "at index %d", i);
                return false;
            }
        }

    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Construct the rules based on input in the bundle config.
 *
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  portForwards        libocispec structs containing ports to
 *                                  forward.
 *  @param[in]  ipVersion           IPv family version (AF_INET/AF_INET6).
 *
 *  @return always returns true.
 */
std::vector<Netfilter::RuleSet> constructRules(const std::shared_ptr<NetworkingHelper> &helper,
                                               const std::string &containerId,
                                               rt_defs_plugins_networking_data_port_forwarding *portForwards,
                                               const int ipVersion)
{
    std::vector<Netfilter::RuleSet> ruleSets;

    std::string containerAddress;
    if (ipVersion == AF_INET)
    {
        containerAddress = helper->ipv4AddrStr();
    }
    else if (ipVersion == AF_INET6)
    {
        containerAddress = helper->ipv6AddrStr();
    }
    else
    {
        AI_LOG_ERROR("supported ip address families are AF_INET or AF_INET6");
        return std::vector<Netfilter::RuleSet>();
    }


    // check if we have ports to forward from host to container
    if (portForwards->host_to_container != nullptr)
    {
        if (!constructHostToContainerRules(ruleSets, containerId,
                                           containerAddress,
                                           portForwards->host_to_container,
                                           portForwards->host_to_container_len,
                                           ipVersion))
        {
            AI_LOG_ERROR("failed to construct host to container rules");
            return std::vector<Netfilter::RuleSet>();
        }
    }

    // check if we have ports to forward from container to host
    if (portForwards->container_to_host != nullptr)
    {
        if (!constructContainerToHostRules(ruleSets, containerId,
                                           containerAddress,
                                           helper->vethName(),
                                           portForwards->container_to_host,
                                           portForwards->container_to_host_len,
                                           ipVersion))
        {
            AI_LOG_ERROR("failed to construct host to container rules");
            return std::vector<Netfilter::RuleSet>();
        }
    }

    return ruleSets;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Construct rules to port forward from host to container.
 *
 *  NB: The forwarding rule should be inserted to the end of the iptables rule
 *  chain, so we put it in the 'ruleSets' vector index 0. The prerouting rule
 *  should be appended to the start of its chain, so we put it in vector index
 *  1.
 *
 *  @param[in]  ruleSets            Vector to place new rulesets in.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  containerAddress    IP address of the container.
 *  @param[in]  ports               ports to forward
 *  @param[in]  len                 Number of ports/protocol combinations.
 *  @param[in]  ipVersion           IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns true on success, otherwise false.
 */
bool constructHostToContainerRules(std::vector<Netfilter::RuleSet> &ruleSets,
                                   const std::string &containerId,
                                   const std::string &containerAddress,
                                   rt_defs_plugins_networking_data_port_forwarding_host_to_container_element **ports,
                                   size_t len, const int ipVersion)
{
    std::list<std::string> appendRules;
    std::list<std::string> insertRules;

    // construct rules for each port
    for (int i = 0; i < len; i++)
    {
        std::string port = std::to_string(ports[i]->port);

        // default to tcp if no protocol is set
        std::string protocol;

        if (ports[i]->protocol)
        {
            protocol = ports[i]->protocol;

            // transform to lower case to allow both upper and lower case entries
            std::transform(protocol.begin(), protocol.end(), protocol.begin(), ::tolower);

            // check for accepted protocol values
            if (strcmp(protocol.c_str(), "tcp") != 0 && strcmp(protocol.c_str(), "udp") != 0)
            {
                AI_LOG_ERROR("invalid protocol value '%s' for port at index %d",
                             ports[i]->protocol, i);
                return false;
            }
        }
        else
        {
            protocol = "tcp";
        }

        // construct forwarding rule to insert to iptables
        std::string forwardingRule =
            createForwardingRule(containerId, protocol,
                                 containerAddress, port, ipVersion);
        insertRules.emplace_back(forwardingRule);

        // construct prerouting rule to append to iptables
        const std::string preroutingRule =
            createPreroutingRule(containerId, protocol,
                                 containerAddress, port, ipVersion);
        appendRules.emplace_back(preroutingRule);
    }

    // we can emplace these rules directly into the ruleSet vector because this
    // function is always called before constructContainerToHostRules()
    Netfilter::RuleSet insertRuleSet = {{ Netfilter::TableType::Filter, insertRules }};
    ruleSets.emplace_back(insertRuleSet);
    Netfilter::RuleSet appendRuleSet = {{ Netfilter::TableType::Nat, appendRules }};
    ruleSets.emplace_back(appendRuleSet);

    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the iptables rule to set up pre-routing so that incoming
 *  packets have their destination ip address and port number changed to match
 *  the container.
 *
 *      iptables -t nat -A PREROUTING ! -i <BRIDGE_NAME> -p <PROTOCOL>
 *               --dport <PORT_NUMBER> -j DNAT --to <CONTAINER_IP>:<PORT_NUMBER>
 *
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  protocol    The name of protocol for the rule.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  portNumber  The port number to forward.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 */
std::string createPreroutingRule(const std::string &id,
                                 const std::string &protocol,
                                 const std::string &ipAddress,
                                 const std::string &portNumber,
                                 const int ipVersion)
{
    char buf[256];

    std::string baseRule("PREROUTING "
                         "! -i " BRIDGE_NAME " "
                         "-p %s "                           // protocol
                         "-m %s "                           // protocol
                         "--dport %s "                      // port number
                         "-m comment --comment %s "         // container id
                         "-j DNAT --to-destination %s");    // target address

    std::string destination;

    // populate '%s' fields in base rule
    if (ipVersion == AF_INET)
    {
        destination = ipAddress + ":" + portNumber;
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                 protocol.c_str(),
                 protocol.c_str(),
                 portNumber.c_str(),
                 id.c_str(),
                 destination.c_str());
    }
    else if (ipVersion == AF_INET6)
    {
        destination = "[" + ipAddress + "]:" + portNumber;
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                 protocol.c_str(),
                 protocol.c_str(),
                 portNumber.c_str(),
                 id.c_str(),
                 destination.c_str());
    }
    else
    {
        return std::string();
    }

    return buf;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Constructs an iptables rule for the FORWARD chain to enable
 *  forwarding to the bridge and then on into the container.
 *
 *      iptables -I FORWARD 1 ! -i <BRIDGE_NAME> -o <BRIDGE_NAME>
 *               --destination <CONTAINER_IP> -p <PROTOCOL> --dport <PORT_NUMBER>
 *               -j ACCEPT
 *
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  protocol    The name of protocol for the rule.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  portNumber  The port number to forward.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 */
std::string createForwardingRule(const std::string &id,
                                 const std::string &protocol,
                                 const std::string &ipAddress,
                                 const std::string &portNumber,
                                 const int ipVersion)
{
    char buf[256];

    std::string baseRule("FORWARD "
                         "-d %s/%s "                // container ip address/mask
                         "! -i " BRIDGE_NAME " "
                         "-o " BRIDGE_NAME " "
                         "-p %s "                   // protocol
                         "-m %s "                   // protocol
                         "--dport %s "              // port number
                         "-m comment --comment %s " // container id
                         "-j ACCEPT");

    // populate '%s' fields in base rule
    if (ipVersion == AF_INET)
    {
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                 ipAddress.c_str(), "32",
                 protocol.c_str(),
                 protocol.c_str(),
                 portNumber.c_str(),
                 id.c_str());
    }
    else if (ipVersion == AF_INET6)
    {
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                 ipAddress.c_str(), "128",
                 protocol.c_str(),
                 protocol.c_str(),
                 portNumber.c_str(),
                 id.c_str());
    }
    else
    {
        return std::string();
    }

    return buf;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Construct rules to port forward from container to host and add them
 *  to the 'ruleSets' vector.
 *
 *  NB: these rules should be inserted to the end of their respective tables,
 *  so we put the new rules into the ruleSets vector's index 0 reserved for
 *  insert(-I) rules.
 *
 *  @param[in]  ruleSets            Vector to place new rulesets in.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  containerAddress    IP address of the container.
 *  @param[in]  vethName            The container's assigned veth device name.
 *  @param[in]  ports               Ports to forward.
 *  @param[in]  len                 Number of ports/protocol combinations.
 *  @param[in]  ipVersion           IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns true on success, otherwise false.
 */
bool constructContainerToHostRules(std::vector<Netfilter::RuleSet> &ruleSets,
                                   const std::string &containerId,
                                   const std::string &containerAddress,
                                   const std::string &vethName,
                                   rt_defs_plugins_networking_data_port_forwarding_container_to_host_element **ports,
                                   size_t len, const int ipVersion)
{
    std::list<std::string> natRules;
    std::list<std::string> filterRules;

    // construct rules for each port to enable forwarding on
    for (int i = 0; i < len; i++)
    {
        std::string port = std::to_string(ports[i]->port);

        // default to tcp if no protocol is set
        std::string protocol;
        if (ports[i]->protocol)
        {
            protocol = ports[i]->protocol;

            // transform to lower case to allow both upper and lower case entries
            std::transform(protocol.begin(), protocol.end(), protocol.begin(), ::tolower);

            // check for accepted protocol values
            if (strcmp(protocol.c_str(), "tcp") != 0 && strcmp(protocol.c_str(), "udp") != 0)
            {
                AI_LOG_ERROR("invalid protocol value '%s' for port at index %d",
                             ports[i]->protocol, i);
                return false;
            }
        }
        else
        {
            protocol = "tcp";
        }

        // construct dnat rule to insert to iptables
        const std::string dnatRule =
            createDnatRule(containerId, protocol, containerAddress,
                           port, ipVersion);
        natRules.emplace_back(dnatRule);

        // construct accept rule to insert to iptables
        const std::string acceptRule =
            createAcceptRule(containerId, protocol, containerAddress,
                             vethName, port, ipVersion);
        filterRules.emplace_back(acceptRule);
    }


    // We want to insert(-I) these rules, so we put them in vector index 0.
    if (ruleSets.empty())
    {
        // ruleSets vector is empty, so we can just emplace our rules directly.
        Netfilter::RuleSet rules = {
            { Netfilter::TableType::Nat, natRules },
            { Netfilter::TableType::Filter, filterRules }
        };
        ruleSets.emplace_back(rules);
    }
    else
    {
        // ruleSets vector isn't empty, so we need to merge our rulesets into
        // the ones already in there.
        auto natRuleset = ruleSets[0].find(Netfilter::TableType::Nat);
        auto filterRuleset = ruleSets[0].find(Netfilter::TableType::Filter);

        natRuleset->second.merge(natRules);
        filterRuleset->second.merge(filterRules);
    }

    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Constructs a DNAT PREROUTING rule to send anything from the container
 *  on the given port to localhost outside the container.
 *
 *      iptables -t nat -I PREROUTING -s <CONTAINER_IP> -d <BRIDGE_ADDRESS>
 *               -i <BRIDGE_NAME> -p <PROTOCOL> -m <PROTOCOL>
 *               --dport <PORT_NUMBER> -j DNAT
 *               --to-destination 127.0.0.1:<PORT_NUMBER>
 *
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  protocol    The name of protocol to create the rule for.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  portNumber  The port number to forward.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
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
                         "-s %s "                       // container address
                         "-d %s "                       // bridge address
                         "-i " BRIDGE_NAME " "
                         "-p %s "                       // protocol
                         "-m %s "                       // protocol
                         "--dport %s "                  // port number
                         "-m comment --comment %s "     // container id
                         "-j DNAT --to-destination %s"  // localhost address
                         );

    // create addresses based on IP version
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

    // populate '%s' fields in base rule
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
 *  @brief Constructs an INPUT ACCEPT rule to allow packets from the container
 *  over the dobby bridge to localhost.
 *
 *      iptables -I DobbyInputChain -s <CONTAINER_IP> -d 127.0.0.1/32
 *               -i <BRIDGE_NAME> -p <PROTOCOL> -m <PROTOCOL>
 *               --dport <PORT_NUMBER> -m physdev --physdev-in <VETH_NAME>
 *               -j ACCEPT
 *
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  protocol    The string name of protocol to create the rule for.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  vethName    The name of the veth device that belongs to the
 *                          container.
 *  @param[in]  portNumber  The port number to forward.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
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
                         "-s %s "                       // container address
                         "-d %s "                       // localhost address
                         "-i " BRIDGE_NAME " "
                         "-p %s "                       // protocol
                         "-m %s "                       // protocol
                         "--dport %s "                  // port number
                         "-m physdev --physdev-in %s "  // veth name
                         "-m comment --comment %s "     // container id
                         "-j ACCEPT");

    // create addresses based on IP version
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

    // populate '%s' fields in base rule
    snprintf(buf, sizeof(buf), baseRule.c_str(),
             sourceAddr.c_str(),
             loAddr.c_str(),
             protocol.c_str(), protocol.c_str(),
             portNumber.c_str(),
             vethName.c_str(),
             id.c_str());

    return std::string(buf);
}
