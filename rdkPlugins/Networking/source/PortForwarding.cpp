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
#include <fcntl.h>


// -----------------------------------------------------------------------------
/**
 *  @brief Adds the two iptables firewall rules to enable port forwarding.
 *
 *  The 'protocol' field can be omitted in which case TCP will be specified.
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  portsConfig         libocispec structs containing ports to
 *                                  forward.
 *
 *  @return true on success, otherwise false.
 */
bool PortForwarding::addPortForwards(const std::shared_ptr<Netfilter> &netfilter,
                                     const std::shared_ptr<NetworkingHelper> &helper,
                                     const std::string &containerId,
                                     rt_defs_plugins_networking_data_port_forwarding *portsConfig)
{
    AI_LOG_FN_ENTRY();

    // parse the libocispec struct data
    PortForwards portForwards = parsePortsConfig(portsConfig);
    if (!portForwards.isValid)
    {
        AI_LOG_ERROR_EXIT("failed to parse port configurations");
        return false;
    }

    // add IPv4 rules to iptables if needed
    if (helper->ipv4())
    {
        std::vector<Netfilter::RuleSet> ipv4Rules = constructPortForwardingRules(helper,
                                                                   containerId,
                                                                   portForwards,
                                                                   AF_INET);
        if (ipv4Rules.empty())
        {
            AI_LOG_ERROR_EXIT("failed to construct port forward iptables rules");
            return false;
        }

        // insert vector index 0 of constructed rules
        if (!netfilter->addRules(ipv4Rules[0], AF_INET, Netfilter::Operation::Insert))
        {
            AI_LOG_ERROR_EXIT("failed to insert port forward rules to iptables");
            return false;
        }

        // append potential rules from vector index 1 of constructed rules
        if (ipv4Rules.size() > 1)
        {
            if (!netfilter->addRules(ipv4Rules[1], AF_INET, Netfilter::Operation::Append))
            {
                AI_LOG_ERROR_EXIT("failed to append port forward rules to iptables");
                return false;
            }
        }
    }

    // add IPv6 rules to iptables if needed
    if (helper->ipv6())
    {
        std::vector<Netfilter::RuleSet> ipv6Rules = constructPortForwardingRules(helper,
                                                                   containerId,
                                                                   portForwards,
                                                                   AF_INET6);
        if (ipv6Rules.empty())
        {
            AI_LOG_ERROR_EXIT("failed to construct port forward ip6tables rules");
            return false;
        }

        // insert vector index 0 of constructed rules
        if (!netfilter->addRules(ipv6Rules[0], AF_INET6, Netfilter::Operation::Insert))
        {
            AI_LOG_ERROR_EXIT("failed to insert port forward rules to ip6tables");
            return false;
        }

        // append potential rules from vector index 1 of constructed rules
        if (ipv6Rules.size() > 1)
        {
            if (!netfilter->addRules(ipv6Rules[1], AF_INET6, Netfilter::Operation::Append))
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
 *  @param[in]  portsConfig         libocispec structs containing ports to
 *                                  forward.
 *
 *  @return true on success, otherwise false.
 */
bool PortForwarding::removePortForwards(const std::shared_ptr<Netfilter> &netfilter,
                                        const std::shared_ptr<NetworkingHelper> &helper,
                                        const std::string &containerId,
                                        rt_defs_plugins_networking_data_port_forwarding *portsConfig)
{
    AI_LOG_FN_ENTRY();

    // parse the libocispec struct data
    PortForwards portForwards = parsePortsConfig(portsConfig);
    if (!portForwards.isValid)
    {
        AI_LOG_ERROR_EXIT("failed to parse port configurations");
        return false;
    }

    // delete IPv4 rules from ip6tables if needed
    if (helper->ipv4())
    {
        std::vector<Netfilter::RuleSet> ipv4Rules = constructPortForwardingRules(helper,
                                                                   containerId,
                                                                   portForwards,
                                                                   AF_INET);
        if (ipv4Rules.empty())
        {
            AI_LOG_ERROR_EXIT("failed to construct iptables rules to remove");
            return false;
        }

        // delete constructed rulesets
        for (size_t i = 0; i < ipv4Rules.size(); i++)
        {
            if (!netfilter->addRules(ipv4Rules[i], AF_INET, Netfilter::Operation::Delete))
            {
                AI_LOG_ERROR_EXIT("failed to delete port forwarding ip6tables rule"
                                  "at index %zu", i);
                return false;
            }
        }

    }

    // delete IPv6 rules from ip6tables if needed
    if (helper->ipv6())
    {
        std::vector<Netfilter::RuleSet> ipv6Rules = constructPortForwardingRules(helper,
                                                                   containerId,
                                                                   portForwards,
                                                                   AF_INET6);
        if (ipv6Rules.empty())
        {
            AI_LOG_ERROR_EXIT("failed to construct ip6tables rules to remove");
            return false;
        }

        // delete constructed rulesets
        for (size_t i = 0; i < ipv6Rules.size(); i++)
        {
            if (!netfilter->addRules(ipv6Rules[i], AF_INET6, Netfilter::Operation::Delete))
            {
                AI_LOG_ERROR_EXIT("failed to delete port forwarding ip6tables rule"
                                  "at index %zu", i);
                return false;
            }
        }

    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 * @brief Adds iptables rules to forward packets from the container localhost to
 * the host's localhost on specific ports. This removes the need to edit code to
 * point to the bridge IP directly.
 *
 * This must be run inside the container's network namespace
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  portsConfig         libocispec structs containing ports to
 *                                  forward.
 *
 *  @return true on success, otherwise false.
 */
bool PortForwarding::addLocalhostMasquerading(const std::shared_ptr<NetworkingHelper> &helper,
                                              const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                              rt_defs_plugins_networking_data_port_forwarding *portsConfig)
{
    AI_LOG_FN_ENTRY();

    const std::string containerId = utils->getContainerId();

    // Version of netfilter for inside the container namespace
    std::shared_ptr<Netfilter> nsNetfilter = std::make_shared<Netfilter>();

    // parse the libocispec struct data
    PortForwards portForwards = parsePortsConfig(portsConfig);
    if (!portForwards.isValid)
    {
        AI_LOG_ERROR_EXIT("failed to parse port configurations");
        return false;
    }

    // add IPv4 rules to iptables if needed
    if (helper->ipv4())
    {
        std::vector<Netfilter::RuleSet> ipv4Rules = constructMasqueradeRules(helper,
                                                                             containerId,
                                                                             portForwards,
                                                                             AF_INET);
        if (ipv4Rules.empty())
        {
            AI_LOG_ERROR_EXIT("failed to construct localhost masquerade iptables rules");
            return false;
        }

        // insert vector index 0 of constructed rules
        if (!nsNetfilter->addRules(ipv4Rules[0], AF_INET, Netfilter::Operation::Insert))
        {
            AI_LOG_ERROR_EXIT("failed to insert localhost masquerade rules to iptables");
            return false;
        }
    }

    // add IPv6 rules to iptables if needed
    if (helper->ipv6())
    {
        std::vector<Netfilter::RuleSet> ipv6Rules = constructMasqueradeRules(helper,
                                                                             containerId,
                                                                             portForwards,
                                                                             AF_INET6);
        if (ipv6Rules.empty())
        {
            AI_LOG_ERROR_EXIT("failed to construct localhost masquerade iptables rules");
            return false;
        }

        // insert vector index 0 of constructed rules
        if (!nsNetfilter->addRules(ipv6Rules[0], AF_INET6, Netfilter::Operation::Insert))
        {
            AI_LOG_ERROR_EXIT("failed to insert localhost masquerade rules to iptables");
            return false;
        }
    }

    // Apply the iptables rules
    if (!nsNetfilter->applyRules(AF_INET) || !nsNetfilter->applyRules(AF_INET6))
    {
        AI_LOG_ERROR_EXIT("failed to apply iptables rules");
        return false;
    }

    // Enable route_localnet inside the container
    const std::string routingFilename = "/proc/sys/net/ipv4/conf/eth0/route_localnet";
    utils->writeTextFile(routingFilename, "1", O_TRUNC | O_WRONLY, 0);

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Takes the 'protocol' string from a port forwarding entry in the
 *  bundle config, transforms it to lower case and checks for validity. If the
 *  'protocol' field was empty, we default to tcp.
 *
 *  @param[in]  protocol            Protocol string from config.
 *
 *  @return returns protocol string, or empty on failure.
 */
std::string parseProtocol(char *protocol)
{
    // if no protocol was set, default to tcp
    if (protocol == nullptr || std::string(protocol).empty())
    {
        return std::string("tcp");
    }

    std::string protocolStr = std::string(protocol);

    // transform to lower case to allow both upper and lower case entries
    std::transform(protocolStr.begin(), protocolStr.end(),
                   protocolStr.begin(), ::tolower);

    // check for accepted protocol values
    if (strcmp(protocolStr.c_str(), "tcp") != 0 &&
        strcmp(protocolStr.c_str(), "udp") != 0)
    {
        // not an accepted protocol value, return empty string
        return std::string();
    }

    return protocolStr;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Parse the libocispec struct formatted port forwarding data into a
 *  PortForwards type struct.
 *
 *  @param[in]  portsConfig         port forwarding configuration data structs.
 *
 *  @return parsed data structure.
 */
PortForwards parsePortsConfig(rt_defs_plugins_networking_data_port_forwarding *portsConfig)
{
    PortForwards portForwards;
    portForwards.isValid = false;

    for (size_t i = 0; i < portsConfig->host_to_container_len; i++)
    {
        PortForward pf;
        pf.port = std::to_string(portsConfig->host_to_container[i]->port);

        // validate the protocol input
        pf.protocol = parseProtocol(portsConfig->host_to_container[i]->protocol);
        if (pf.protocol.empty())
        {
            AI_LOG_ERROR("invalid protocol value '%s' for port at index %zu",
                        portsConfig->host_to_container[i]->protocol, i);
            return portForwards;
        }

        portForwards.hostToContainer.emplace_back(pf);
    }

    for (size_t i = 0; i < portsConfig->container_to_host_len; i++)
    {
        PortForward pf;
        pf.port = std::to_string(portsConfig->container_to_host[i]->port);

        // validate the protocol input
        pf.protocol = parseProtocol(portsConfig->container_to_host[i]->protocol);
        if (pf.protocol.empty())
        {
            AI_LOG_ERROR("invalid protocol value '%s' for port at index %zu",
                        portsConfig->container_to_host[i]->protocol, i);
            return portForwards;
        }
        portForwards.containerToHost.emplace_back(pf);
    }

    // parsed all port configurations correctly, set valid object
    portForwards.isValid = true;

    return portForwards;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Construct the rules based on input in the bundle config.
 *
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  portForwards        structs containing ports to forward.
 *  @param[in]  ipVersion           IPv family version (AF_INET/AF_INET6).
 *
 *  @return always returns true.
 */
std::vector<Netfilter::RuleSet> constructPortForwardingRules(const std::shared_ptr<NetworkingHelper> &helper,
                                                             const std::string &containerId,
                                                             const PortForwards &portForwards,
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
    if (!portForwards.hostToContainer.empty())
    {
        if (!constructHostToContainerRules(ruleSets, containerId,
                                           containerAddress,
                                           portForwards.hostToContainer,
                                           ipVersion))
        {
            AI_LOG_ERROR("failed to construct host to container rules");
            return std::vector<Netfilter::RuleSet>();
        }
    }

    // check if we have ports to forward from container to host
    if (!portForwards.containerToHost.empty())
    {
        if (!constructContainerToHostRules(ruleSets, containerId,
                                           containerAddress,
                                           helper->vethName(),
                                           portForwards.containerToHost,
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
 * @brief Constructs rules to allow requests to the container localhost on certain
 * ports to be automatically forwarded to the host's localhost.
 *
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  portForwards        structs containing ports to forward.
 *  @param[in]  ipVersion           IPv family version (AF_INET/AF_INET6).
 *
 * @return RuleSet to configure iptables
 */
std::vector<Netfilter::RuleSet> constructMasqueradeRules(const std::shared_ptr<NetworkingHelper> &helper,
                                                         const std::string &containerId,
                                                         const PortForwards &portForwards,
                                                         const int ipVersion)
{
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

    std::vector<Netfilter::RuleSet> ruleSets;

    // We can only setup masquerading in one direction (container to access host ports)
    if (!portForwards.containerToHost.empty())
    {
        std::list<std::string> natRules;
        std::list<std::string> filterRules;
        std::vector<struct PortForward> ports = portForwards.containerToHost;

        for (size_t i = 0; i < ports.size(); i++)
        {
            const std::string snatRule = createMasqueradeSnatRule(ports[i], containerId, containerAddress, ipVersion);
            natRules.emplace_back(snatRule);

            if (ipVersion == AF_INET)
            {
                const std::string dnatRule = createMasqueradeDnatRule(ports[i], containerId, containerAddress, ipVersion);
                natRules.emplace_back(dnatRule);
            }
            else if (ipVersion == AF_INET6)
            {
                const std::string filterRule = createNoIpv6LocalRule(ports[i], containerId, containerAddress, ipVersion);
                filterRules.emplace_back(filterRule);

                const std::string snatRule2 = createLocalLinkSnatRule(ports[i], containerId, containerAddress, ipVersion);
                natRules.emplace_back(snatRule2);
            }

        }

        // No need to bother with merge logic here as this is the only set of
        // rules added, just add them to the set
        Netfilter::RuleSet rules = {
            { Netfilter::TableType::Nat, natRules },
            { Netfilter::TableType::Filter, filterRules }
        };
        ruleSets.emplace(ruleSets.begin(), rules);
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
 *  @param[in]  ports               ports to forward.
 *  @param[in]  ipVersion           IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns true on success, otherwise false.
 */
bool constructHostToContainerRules(std::vector<Netfilter::RuleSet> &ruleSets,
                                   const std::string &containerId,
                                   const std::string &containerAddress,
                                   const std::vector<struct PortForward> &ports,
                                   const int ipVersion)
{
    std::list<std::string> appendRules;
    std::list<std::string> insertRules;

    // construct rules for each port
    for (size_t i = 0; i < ports.size(); i++)
    {
        // construct forwarding rule to insert to iptables
        std::string forwardingRule =
            createForwardingRule(ports[i], containerId, containerAddress, ipVersion);
        insertRules.emplace_back(forwardingRule);

        // construct prerouting rule to append to iptables
        const std::string preroutingRule =
            createPreroutingRule(ports[i], containerId, containerAddress, ipVersion);
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
 *  @param[in]  portForward The protocol and port to forward.
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 */
std::string createPreroutingRule(const PortForward &portForward,
                                 const std::string &id,
                                 const std::string &ipAddress,
                                 const int ipVersion)
{
    char buf[256] = {0};

    // We need to add -m <PROTOCOL> because it's automatically added by
    // iptables. If omitted, we won't be able to match the rule for deletion.
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
        destination = ipAddress + ":" + portForward.port;
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                 portForward.protocol.c_str(),
                 portForward.protocol.c_str(),
                 portForward.port.c_str(),
                 id.c_str(),
                 destination.c_str());
    }
    else if (ipVersion == AF_INET6)
    {
        destination = "[" + ipAddress + "]:" + portForward.port;
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                 portForward.protocol.c_str(),
                 portForward.protocol.c_str(),
                 portForward.port.c_str(),
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
 *  @param[in]  portForward The protocol and port to forward.
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 */
std::string createForwardingRule(const PortForward &portForward,
                                 const std::string &id,
                                 const std::string &ipAddress,
                                 const int ipVersion)
{
    char buf[256] = {0};

    // We need to add -m <PROTOCOL> because it's automatically added by
    // iptables. If omitted, we won't be able to match the rule for deletion.
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
                 portForward.protocol.c_str(),
                 portForward.protocol.c_str(),
                 portForward.port.c_str(),
                 id.c_str());
    }
    else if (ipVersion == AF_INET6)
    {
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                 ipAddress.c_str(), "128",
                 portForward.protocol.c_str(),
                 portForward.protocol.c_str(),
                 portForward.port.c_str(),
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
 *  @param[in]  ipVersion           IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns true on success, otherwise false.
 */
bool constructContainerToHostRules(std::vector<Netfilter::RuleSet> &ruleSets,
                                   const std::string &containerId,
                                   const std::string &containerAddress,
                                   const std::string &vethName,
                                   const std::vector<struct PortForward> &ports,
                                   const int ipVersion)
{
    std::list<std::string> natRules;
    std::list<std::string> filterRules;

    // construct rules for each port to enable forwarding on
    for (size_t i = 0; i < ports.size(); i++)
    {
        // construct dnat rule to insert to iptables
        const std::string dnatRule =
            createDnatRule(ports[i], containerId, containerAddress, ipVersion);
        natRules.emplace_back(dnatRule);

        // construct accept rule to insert to iptables
        const std::string acceptRule =
            createAcceptRule(ports[i], containerId, containerAddress, vethName,
                             ipVersion);
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
        ruleSets.emplace(ruleSets.begin(), rules);
    }
    else
    {
        // ruleSets vector isn't empty, so we need to merge our rulesets into
        // the ones already in there.
        auto natRuleset = ruleSets[0].find(Netfilter::TableType::Nat);
        if (natRuleset != ruleSets[0].end())
        {
            natRuleset->second.merge(natRules);
        }
        else
        {
            ruleSets[0].insert({ Netfilter::TableType::Nat, natRules });
        }

        auto filterRuleset = ruleSets[0].find(Netfilter::TableType::Filter);
        if (filterRuleset != ruleSets[0].end())
        {
            filterRuleset->second.merge(filterRules);
        }
        else
        {
            ruleSets[0].insert({ Netfilter::TableType::Filter, filterRules });
        }

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
 *  @param[in]  portForward The protocol and port to forward.
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 */
std::string createDnatRule(const PortForward &portForward,
                           const std::string &id,
                           const std::string &ipAddress,
                           const int ipVersion)
{
    char buf[256] = {0};

    std::string sourceAddr;
    std::string bridgeAddr;
    std::string destination;

    // We need to add -m <PROTOCOL> because it's automatically added by
    // iptables. If omitted, we won't be able to match the rule for deletion.
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
        destination = "127.0.0.1:" + portForward.port;
    }
    else
    {
        sourceAddr = std::string() + ipAddress + "/128";
        bridgeAddr = std::string() + BRIDGE_ADDRESS_IPV6 + "/128";
        destination = "[::1]:" + portForward.port;
    }

    // populate '%s' fields in base rule
    snprintf(buf, sizeof(buf), baseRule.c_str(),
             sourceAddr.c_str(),
             bridgeAddr.c_str(),
             portForward.protocol.c_str(),
             portForward.protocol.c_str(),
             portForward.port.c_str(),
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
 *  @param[in]  portForward The protocol and port to forward.
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  vethName    The name of the veth device that belongs to the
 *                          container.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 */
std::string createAcceptRule(const PortForward &portForward,
                             const std::string &id,
                             const std::string &ipAddress,
                             const std::string &vethName,
                             const int ipVersion)
{
    char buf[256] = {0};

    std::string sourceAddr;
    std::string loAddr;

    // We need to add -m <PROTOCOL> because it's automatically added by
    // iptables. If omitted, we won't be able to match the rule for deletion.
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
             portForward.protocol.c_str(),
             portForward.protocol.c_str(),
             portForward.port.c_str(),
             vethName.c_str(),
             id.c_str());

    return std::string(buf);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs an OUTPUT DNAT rule to forward packets from 127.0.0.1 inside
 *  the container to the bridge device (100.64.11.1) on the given port. We cannot
 *  do the same thing for IPv6. Check createNoIpv6LocalRule for details.
 *
 *  @param[in]  portForward The protocol and port to forward.
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 */
std::string createMasqueradeDnatRule(const PortForward &portForward,
                                    const std::string &id,
                                    const std::string &ipAddress,
                                    const int ipVersion)
{
    char buf[256] = {0};

    std::string destination;

    // create addresses based on IP version
    if (ipVersion == AF_INET)
    {
        std::string baseRule("OUTPUT "
                            "-o lo "
                            "-p %s "                   // protocol
                            "-m %s "                   // protocol
                            "--dport %s "              // port number
                            "-j DNAT "
                            "-m comment --comment %s " // Container id
                            "--to-destination %s"      // Bridge address:port
        );

        destination = std::string() + BRIDGE_ADDRESS + ":" + portForward.port;

        // populate '%s' fields in base rule
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                portForward.protocol.c_str(),
                portForward.protocol.c_str(),
                portForward.port.c_str(),
                id.c_str(),
                destination.c_str(),
                portForward.port.c_str());
    }

    return std::string(buf);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs an OUTPUT DNAT rule to reject IPv6 based local traffic.
 *  We would like this port to be accessible outside of the container
 *  but there is no IPv6 equivalent of:
 *  /proc/sys/net/ipv4/conf/interface/route_localnet
 *  so there is no possibility to route localnet trafic outside. For the clarity
 *  we just REJECT packets and hope that they will try to use IPv4 instead.
 *
 *  @param[in]  portForward The protocol and port to forward.
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 */
std::string createNoIpv6LocalRule(const PortForward &portForward,
                                    const std::string &id,
                                    const std::string &ipAddress,
                                    const int ipVersion)
{
    char buf[256] = {0};

    std::string destination;


    // create addresses based on IP version
    if (ipVersion == AF_INET6)
    {
        std::string baseRule("OUTPUT "
                            "-o lo "
                            "-p %s "                   // protocol
                            "-m %s "                   // protocol
                            "--dport %s "              // port number
                            "-m comment --comment %s " // Container id
                            "-j REJECT"                // Bridge address:port
        );

        destination = std::string() + BRIDGE_ADDRESS + ":" + portForward.port;

        // populate '%s' fields in base rule
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                portForward.protocol.c_str(),
                portForward.protocol.c_str(),
                portForward.port.c_str(),
                id.c_str());
    }

    return std::string(buf);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs an POSTROUTING SNAT rule so that the source address is changed
 *  to the veth0 inside the container so we get the replies.
 *
 *  @param[in]  portForward The protocol and port to forward.
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 *
 */
std::string createMasqueradeSnatRule(const PortForward &portForward,
                                    const std::string &id,
                                    const std::string &ipAddress,
                                    const int ipVersion)
{
    char buf[256] = {0};

    std::string bridgeAddr;
    std::string sourceAddr;
    std::string destination;

    std::string baseRule("POSTROUTING "
                        "-p %s "                    // protocol
                        "-s %s "                    // container localhost
                        "-d %s "                    // bridge address
                        "-j SNAT "
                        "-m comment --comment %s "  // container id
                        "--to %s");                 // container address

    // create addresses based on IP version
    if (ipVersion == AF_INET)
    {
        sourceAddr = "127.0.0.1";
        destination = std::string() + ipAddress;
        bridgeAddr = std::string() + BRIDGE_ADDRESS;
    }
    else
    {
        sourceAddr = "::1/128";
        destination = std::string() + ipAddress;
        bridgeAddr = std::string() + BRIDGE_ADDRESS_IPV6;
    }

    // populate '%s' fields in base rule
    snprintf(buf, sizeof(buf), baseRule.c_str(),
             portForward.protocol.c_str(),
             sourceAddr.c_str(),
             bridgeAddr.c_str(),
             id.c_str(),
             destination.c_str());

    return std::string(buf);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs an POSTROUTING SNAT rule so that the source address is changed
 *  to the veth0 inside the container so we get the replies. For Local Link
 *  addresses ("fe80::"" based)
 *
 *  @param[in]  portForward The protocol and port to forward.
 *  @param[in]  id          The id of the container making the request.
 *  @param[in]  ipAddress   The ip address of the container.
 *  @param[in]  ipVersion   IPv family version (AF_INET/AF_INET6).
 *
 *  @return returns the created rule.
 *
 */
std::string createLocalLinkSnatRule(const PortForward &portForward,
                                    const std::string &id,
                                    const std::string &ipAddress,
                                    const int ipVersion)
{
    char buf[256] = {0};

    std::string bridgeAddr;
    std::string sourceAddr;
    std::string destination;

    std::string baseRule("POSTROUTING "
                        "-p %s "                    // protocol
                        "-s %s "                    // container local link
                        "-d %s "                    // bridge address
                        "-j SNAT "
                        "-m comment --comment %s "  // container id
                        "--to %s");                 // container address

    // create addresses based on IP version
    if (ipVersion == AF_INET6)
    {
        sourceAddr = "fe80::/10";
        destination = std::string() + ipAddress;
        bridgeAddr = std::string() + BRIDGE_ADDRESS_IPV6;
    }
    else
    {
        //do nothing for IPv4 as there is only one IP per interface
        return std::string();
    }

    // populate '%s' fields in base rule
    snprintf(buf, sizeof(buf), baseRule.c_str(),
             portForward.protocol.c_str(),
             sourceAddr.c_str(),
             bridgeAddr.c_str(),
             id.c_str(),
             destination.c_str());

    return std::string(buf);
}
