/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 Sky UK
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

#include "InterContainerRouting.h"

#include <algorithm>
#include <Logging.h>
#include <fcntl.h>


typedef struct InterContainerPort
{
    enum class Protocol
    {
        Invalid = 0,
        Tcp,
        Udp,
    } protocol;

    in_port_t port;
    bool localHostMasquerade;

} InterContainerPort;

typedef struct InterContainerPorts
{
    std::vector<struct InterContainerPort> inPorts;
    std::vector<struct InterContainerPort> outPorts;
    bool isValid;
} InterContainerPorts;



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
static InterContainerPort::Protocol parseProtocol(const char *protocol)
{
    // if no protocol was set, default to tcp
    if (protocol == nullptr || strlen(protocol) == 0)
    {
        return InterContainerPort::Protocol::Tcp;
    }

    if (strcasecmp(protocol, "udp") == 0)
    {
        return InterContainerPort::Protocol::Udp;
    }
    else if (strcasecmp(protocol, "tcp") == 0)
    {
        return InterContainerPort::Protocol::Tcp;
    }
    else
    {
        return InterContainerPort::Protocol::Invalid;
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Parse the libocispec struct formatted inter container data into a
 *  InterContainerPorts type struct.
 *
 *  @param[in]  portsConfig         Port forwarding configuration data array.
 *  @param[in]  numPortConfigs      The number of data structures in portsConfig.
 *
 *  @return parsed data structure.
 */
static InterContainerPorts parseConfig(rt_defs_plugins_networking_data_inter_container_element * const *portConfigs,
                                       size_t numPortConfigs)
{
    InterContainerPorts containerPorts = { {}, {}, false };

    for (size_t i = 0; i < numPortConfigs; i++)
    {
        const rt_defs_plugins_networking_data_inter_container_element *portConfig = portConfigs[i];
        if (!portConfig)
        {
            AI_LOG_WARN("null entry in the inter container port config");
            continue;
        }

        InterContainerPort port = { InterContainerPort::Protocol::Tcp, portConfig->port, false };

        port.protocol = parseProtocol(portConfig->protocol);
        if (port.protocol == InterContainerPort::Protocol::Invalid)
        {
            AI_LOG_ERROR("invalid protocol value '%s' for port %hu at index %zu",
                         portConfig->protocol, port.port, i);
            continue;
        }

        port.localHostMasquerade = portConfig->localhost_masquerade_present && portConfig->localhost_masquerade;

        if (strcasecmp(portConfig->direction, "in") == 0)
        {
            containerPorts.inPorts.emplace_back(std::move(port));
        }
        else if (strcasecmp(portConfig->direction, "out") == 0)
        {
            containerPorts.outPorts.emplace_back(std::move(port));
        }
        else
        {
            AI_LOG_ERROR("invalid direction value '%s' for port %hu at index %zu",
                         portConfig->direction, port.port, i);
            continue;
        }
    }

    // parsed all port configurations correctly, set valid object
    containerPorts.isValid = true;

    return containerPorts;
}

// -----------------------------------------------------------------------------
/**
 * @brief Constructs rules to allow requests to the container localhost on certain
 * ports to be automatically forwarded to the host's localhost.
 *
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  portsConfig         structs containing ports to configure.
 *
 * @return RuleSet to configure iptables
 */
static Netfilter::RuleSet constructLocalHostMasqueradeRules(const std::shared_ptr<NetworkingHelper> &helper,
                                                            const InterContainerPorts &containerPorts)
{
    char ruleBuf[512];
    std::list<std::string> natRules;

    const std::string containerAddress = helper->ipv4AddrStr();

    // For incoming (server) ports then we need to set up DNAT rules in the PREROUTING chain
    for (const InterContainerPort &inPort : containerPorts.inPorts)
    {
        if (inPort.localHostMasquerade)
        {
            snprintf(ruleBuf, sizeof(ruleBuf),
                     "PREROUTING "
                     "-s %s/24 "                                // any container address
                     "-d %s/32 "                                // container address
                     "-p %s "                                   // protocol
                     "-m %s --dport %hu "                       // protocol and port number
                     "-j DNAT --to-destination 127.0.0.1:%hu",  // port number
                     BRIDGE_ADDRESS_RANGE,
                     containerAddress.c_str(),
                     (inPort.protocol == InterContainerPort::Protocol::Udp) ? "udp" : "tcp",
                     (inPort.protocol == InterContainerPort::Protocol::Udp) ? "udp" : "tcp",
                     inPort.port,
                     inPort.port);

            natRules.emplace_back(ruleBuf);
        }
    }

    // For outgoing (client) ports then we need to set up DNAT rules in the OUTPUT chain
    for (const InterContainerPort &outPort : containerPorts.outPorts)
    {
        if (outPort.localHostMasquerade)
        {
            snprintf(ruleBuf, sizeof(ruleBuf),
                     "OUTPUT "
                     "-o lo "                                   // output interface
                     "-p %s "                                   // protocol
                     "-m %s --dport %hu "                       // protocol and port number
                     "-j DNAT --to-destination %s:%hu",         // bridge address and port number
                     (outPort.protocol == InterContainerPort::Protocol::Udp) ? "udp" : "tcp",
                     (outPort.protocol == InterContainerPort::Protocol::Udp) ? "udp" : "tcp",
                     outPort.port,
                     BRIDGE_ADDRESS, outPort.port);

            natRules.emplace_back(ruleBuf);
        }
    }

    // No need to bother with merge logic here as this is the only set of
    // rules added, just add them to the set
    return { { Netfilter::TableType::Nat, std::move(natRules) } };
}

// -----------------------------------------------------------------------------
/**
 * @brief Creates the iptables rules to run in the container for setting up
 * localhost masquerade rules.
 *
 *
 *
 * @param[in]  helper              Instance of NetworkingHelper.
 * @param[in]  containerId         Container identifier.
 * @param[in]  portsConfig         libocispec structs containing ports to
 *                                 forward.
 *
 *  @return true on success, otherwise false.
 */
static bool addLocalhostMasquerading(const std::shared_ptr<NetworkingHelper> &helper,
                                     const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                     const InterContainerPorts &portsConfig)
{
    AI_LOG_FN_ENTRY();

    // Version of netfilter for inside the container namespace
    std::shared_ptr<Netfilter> nsNetfilter = std::make_shared<Netfilter>();

    // Construct IPv4 rules to iptables
    Netfilter::RuleSet ruleSet = constructLocalHostMasqueradeRules(helper, portsConfig);
    if (!ruleSet.empty())
    {
        // insert vector index 0 of constructed rules
        if (!nsNetfilter->addRules(ruleSet, AF_INET, Netfilter::Operation::Insert))
        {
            AI_LOG_ERROR_EXIT("failed to insert localhost masquerade rules to iptables");
            return false;
        }
    }

    // Apply the iptables rules
    if (!nsNetfilter->applyRules(AF_INET))
    {
        AI_LOG_ERROR_EXIT("failed to apply iptables rules for inter-container localhost masquerade");
        return false;
    }

    // Enable route_localnet inside the container
    static const std::string routingFilename = "/proc/sys/net/ipv4/conf/eth0/route_localnet";
    utils->writeTextFile(routingFilename, "1", O_TRUNC | O_WRONLY, 0);

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Construct the rules based on input in the bundle config.
 *
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier.
 *  @param[in]  portConfigs         structs containing ports to forward.
 *  @param[in]  ipVersion           IPv family version (AF_INET/AF_INET6).
 *
 *  @return always returns true.
 */
Netfilter::RuleSet constructRules(const std::shared_ptr<NetworkingHelper> &helper,
                                  const std::string &containerId,
                                  const InterContainerPorts &containerPorts,
                                  const int ipVersion)
{
    std::string containerAddress;
    std::string containerAddressWithMask;
    std::string containersAddressRange;
    std::string bridgeAddressWithMask;
    if (ipVersion == AF_INET)
    {
        containerAddress = helper->ipv4AddrStr();
        containerAddressWithMask = containerAddress + "/32";
        containersAddressRange = BRIDGE_ADDRESS_RANGE "/24";
        bridgeAddressWithMask = BRIDGE_ADDRESS "/32";
    }
    else if (ipVersion == AF_INET6)
    {
        containerAddress = "[" + helper->ipv6AddrStr() + "]";
        containerAddressWithMask = helper->ipv6AddrStr() + "/128";
        containersAddressRange = BRIDGE_ADDRESS_RANGE_IPV6 "/120";
        bridgeAddressWithMask = BRIDGE_ADDRESS_IPV6 "/128";
    }
    else
    {
        AI_LOG_ERROR("supported ip address families are AF_INET or AF_INET6");
        return { };
    }

    const std::string vethName = helper->vethName();

    char ruleBuf[512];
    std::list<std::string> natRules;
    std::list<std::string> filterRules;

    // For incoming (server) ports then we need to set up filter rules to
    // allow packets from the bridge to the container, and also DNAT rule
    // to redirect traffic from the bridge to the container
    for (const InterContainerPort &inPort : containerPorts.inPorts)
    {
        // Add a forwarding rule to allow the traffic from the container to be
        // forwarded to other interfaces on the bridge
        snprintf(ruleBuf, sizeof(ruleBuf),
                 "FORWARD "
                 "-s %s "                                   // container address
                 "-d %s "                                   // any container address
                 "-i " BRIDGE_NAME " "
                 "-o " BRIDGE_NAME " "
                 "-p %s "                                   // protocol
                 "-m %s "                                   // protocol
                 "--sport %hu "                             // port number
                 "-m physdev "
                 "--physdev-in %s "                         // container veth number on bridge
                 "-m comment --comment \"inter-in:%s\" "    // container id
                 "-j ACCEPT",                               // accept the packet
                 containerAddressWithMask.c_str(),
                 containersAddressRange.c_str(),
                 (inPort.protocol == InterContainerPort::Protocol::Udp) ? "udp" : "tcp",
                 (inPort.protocol == InterContainerPort::Protocol::Udp) ? "udp" : "tcp",
                 inPort.port,
                 vethName.c_str(),
                 containerId.c_str());

        filterRules.emplace_back(ruleBuf);

        // Add a rule to the PREROUTING chain to DNAT the packets from any container
        // to the bridge address to the container address
        snprintf(ruleBuf, sizeof(ruleBuf),
                 "PREROUTING "
                 "-s %s "                                   // any container address
                 "-d %s "                                   // bridge address
                 "-i " BRIDGE_NAME " "
                 "-p %s "                                   // protocol
                 "-m %s "                                   // protocol
                 "--dport %hu "                             // port number
                 "-m comment --comment \"inter-in:%s\" "    // container id
                 "-j DNAT --to-destination %s:%hu",         // container address and port
                 containersAddressRange.c_str(),
                 bridgeAddressWithMask.c_str(),
                 (inPort.protocol == InterContainerPort::Protocol::Udp) ? "udp" : "tcp",
                 (inPort.protocol == InterContainerPort::Protocol::Udp) ? "udp" : "tcp",
                 inPort.port,
                 containerId.c_str(),
                 containerAddress.c_str(), inPort.port);

        natRules.emplace_back(ruleBuf);
    }

    // For outgoing (client) ports then we need to set up forwarding rule to
    // allow packets from the container to be forwarded to other interfaces on
    // the bridge
    for (const InterContainerPort &outPort : containerPorts.outPorts)
    {
        snprintf(ruleBuf, sizeof(ruleBuf),
                 "FORWARD "
                 "-s %s "                                   // container address
                 "-d %s "                                   // any container address
                 "-i " BRIDGE_NAME " "
                 "-o " BRIDGE_NAME " "
                 "-p %s "                                   // protocol
                 "-m %s "                                   // protocol
                 "--dport %hu "                             // port number
                 "-m physdev "
                 "--physdev-in %s "                         // container veth number on bridge
                 "-m comment --comment \"inter-out:%s\" "   // container id
                 "-j ACCEPT",                               // accept the packet
                 containerAddressWithMask.c_str(),
                 containersAddressRange.c_str(),
                 (outPort.protocol == InterContainerPort::Protocol::Udp) ? "udp" : "tcp",
                 (outPort.protocol == InterContainerPort::Protocol::Udp) ? "udp" : "tcp",
                 outPort.port,
                 vethName.c_str(),
                 containerId.c_str());

        filterRules.emplace_back(ruleBuf);
    }

    // No need to bother with merge logic here as this is the only set of
    // rules added, just add them to the set
    Netfilter::RuleSet ruleSet = {
        { Netfilter::TableType::Nat, natRules },
        { Netfilter::TableType::Filter, filterRules }
    };

    return ruleSet;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds the necessary iptables firewall rules to enable routing of
 *  packets to / from one container to another.
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  utils               Instance of DobbyRdkPluginUtils.
 *  @param[in]  portConfigs         libocispec structs containing ports to
 *                                  setup, both incoming and outgoing.
 *  @param[in]  numPortConfigs      The number of portConfigs.
 *
 *  @return true on success, otherwise false.
 */
bool InterContainerRouting::addRules(const std::shared_ptr<Netfilter> &netfilter,
                                     const std::shared_ptr<NetworkingHelper> &helper,
                                     const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                     rt_defs_plugins_networking_data_inter_container_element * const *portConfigs,
                                     size_t numPortConfigs)
{
    AI_LOG_FN_ENTRY();

    // parse the libocispec struct data
    const InterContainerPorts containerPorts = parseConfig(portConfigs, numPortConfigs);
    if (!containerPorts.isValid)
    {
        AI_LOG_ERROR_EXIT("failed to parse port configurations");
        return false;
    }

    // bail early if there are no ports to forward
    if (containerPorts.inPorts.empty() && containerPorts.outPorts.empty())
    {
        AI_LOG_DEBUG("no inter-container ports to forward");
        AI_LOG_FN_EXIT();
        return true;
    }

    const std::string containerId = utils->getContainerId();

    // add IPv4 rules to iptables if needed
    if (helper->ipv4())
    {
        Netfilter::RuleSet ipv4Rules = constructRules(helper,
                                                      containerId,
                                                      containerPorts,
                                                      AF_INET);
        if (!ipv4Rules.empty())
        {
            if (!netfilter->addRules(ipv4Rules, AF_INET, Netfilter::Operation::Insert))
            {
                AI_LOG_ERROR_EXIT("failed to insert port forward rules to iptables");
                return false;
            }
        }
    }

    // add IPv6 rules to iptables if needed
    if (helper->ipv6())
    {
        Netfilter::RuleSet ipv6Rules = constructRules(helper,
                                                      containerId,
                                                      containerPorts,
                                                      AF_INET6);
        if (!ipv6Rules.empty())
        {
            if (!netfilter->addRules(ipv6Rules, AF_INET6, Netfilter::Operation::Insert))
            {
                AI_LOG_ERROR_EXIT("failed to insert port forward rules to ip6tables");
                return false;
            }
        }
    }

    // check if any ports require localhost masquerading, we only support it for IPv4
    if (helper->ipv4())
    {
        bool requireLocalhostMasquerading = false;
        for (const InterContainerPort &containerPort : containerPorts.inPorts)
            requireLocalhostMasquerading = requireLocalhostMasquerading || containerPort.localHostMasquerade;
        for (const InterContainerPort &containerPort : containerPorts.outPorts)
            requireLocalhostMasquerading = requireLocalhostMasquerading || containerPort.localHostMasquerade;

        // apply any localhost masquerading rules if needed
        if (requireLocalhostMasquerading)
        {
            if (!utils->callInNamespace(utils->getContainerPid(), CLONE_NEWNET,
                                        &addLocalhostMasquerading,
                                        helper, utils, containerPorts))
            {
                AI_LOG_ERROR_EXIT("failed to add localhost masquerade iptables rules inside container");
                return false;
            }
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes the inter container port forwarding rules from iptables.
 *
 *  @param[in]  netfilter           Instance of Netfilter class.
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  utils               Instance of DobbyRdkPluginUtils.
 *  @param[in]  portConfigs         libocispec structs containing ports to
 *                                  setup, both incoming and outgoing.
 *  @param[in]  numPortConfigs      The number of portConfigs.
 *
 *  @return true on success, otherwise false.
 */
bool InterContainerRouting::removeRules(const std::shared_ptr<Netfilter> &netfilter,
                                        const std::shared_ptr<NetworkingHelper> &helper,
                                        const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                        rt_defs_plugins_networking_data_inter_container_element * const *portConfigs,
                                        size_t numPortConfigs)
{
    AI_LOG_FN_ENTRY();

    // parse the libocispec struct data
    const InterContainerPorts containerPorts = parseConfig(portConfigs, numPortConfigs);
    if (!containerPorts.isValid)
    {
        AI_LOG_ERROR_EXIT("failed to parse port configurations");
        return false;
    }

    const std::string containerId = utils->getContainerId();

    // delete IPv4 rules from ip6tables if needed
    if (helper->ipv4())
    {
        Netfilter::RuleSet ipv4Rules = constructRules(helper, containerId, containerPorts, AF_INET);
        if (!ipv4Rules.empty())
        {
            if (!netfilter->addRules(ipv4Rules, AF_INET, Netfilter::Operation::Delete))
            {
                AI_LOG_ERROR_EXIT("failed to delete inter-container iptables rule");
                return false;
            }
        }
    }

    // delete IPv6 rules from ip6tables if needed
    if (helper->ipv6())
    {
        Netfilter::RuleSet ipv6Rules = constructRules(helper, containerId, containerPorts, AF_INET6);
        if (!ipv6Rules.empty())
        {
            if (!netfilter->addRules(ipv6Rules, AF_INET6, Netfilter::Operation::Delete))
            {
                AI_LOG_ERROR_EXIT("failed to delete inter-container ip6tables rule");
                return false;
            }
        }
    }

    // no need to delete the masquerade rules as these were only applied inside
    // the container namespace

    AI_LOG_FN_EXIT();
    return true;
}
