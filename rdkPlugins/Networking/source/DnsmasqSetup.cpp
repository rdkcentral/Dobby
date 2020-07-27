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

#include "DnsmasqSetup.h"

#include <Logging.h>
#include <fcntl.h>

std::mutex mLock;

// -----------------------------------------------------------------------------
/**
 *  @brief Sets up the iptables rules to route DNS ports outside the container
 *
 *  Adds container id as comments so that the correct rule will be deleted at
 *  postStop hook. This will lead to duplicate iptables entries if multiple
 *  containers set up dnsmasq, which shouldn't cause any issues other than an
 *  eyesore. The rules will be removed upon container deletion with the postStop
 *  hook.
 *
 *  @param[in]  containerId         Container identifier
 *  @param[in]  ipVersion           IP address family to use (AF_INET/AF_INET6).
 *
 *  @return netfilter ruleset
 */
Netfilter::RuleSet constructRules(const std::string &containerId, int ipVersion)
{
    AI_LOG_FN_ENTRY();

#if defined(DEV_VM)
    const std::string id(containerId);
#else
    const std::string id("\"" + containerId + "\"");
#endif

    // the following rule set was obtained by looking at what libvirt had setup
    // for the NAT connection, we're just replicating
    char buf[256];

    // setup NAT'ing on the Dobby network ip range through external interface
    std::list<std::string> natRules;
    std::string natRule("PREROUTING "
                        "-s %s "
                        "-d %s "
                        "-i " BRIDGE_NAME " "
                        "-p %s "
                        "-m %s --dport 53 "
                        "-m comment --comment " + id + " "
                        "-j DNAT --to-destination %s:53");

    // allow DNS packets from containers
    std::list<std::string> filterRules;
    std::string filterRule("DobbyInputChain -s %s "
                           "-d %s "
                           "-i " BRIDGE_NAME " "
                           "-p %s "
                           "-m %s --dport 53 "
                           "-m comment --comment " + id + " -j ACCEPT");

    if (ipVersion == AF_INET)
    {
        // compose IPv4 rules for 'nat' table
        snprintf(buf, sizeof(buf), natRule.c_str(),
                 BRIDGE_ADDRESS_RANGE "/24",
                 BRIDGE_ADDRESS "/32",
                 "udp",
                 "udp",
                 LOCALHOST);
        natRules.emplace_back(buf);
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), natRule.c_str(),
                 BRIDGE_ADDRESS_RANGE "/24",
                 BRIDGE_ADDRESS "/32",
                 "tcp",
                 "tcp",
                 LOCALHOST);
        natRules.emplace_back(buf);
        memset(buf, 0, sizeof(buf));

        // compose IPv4 rules for 'filter' table
        snprintf(buf, sizeof(buf), filterRule.c_str(),
                 BRIDGE_ADDRESS_RANGE "/24",
                 LOCALHOST "/32",
                 "udp",
                 "udp");
        filterRules.emplace_back(buf);
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), filterRule.c_str(),
                 BRIDGE_ADDRESS_RANGE "/24",
                 LOCALHOST "/32",
                 "tcp",
                 "tcp");
        filterRules.emplace_back(buf);
    }
    else if (ipVersion == AF_INET6)
    {
        // compose IPv6 rules for 'nat' table
        snprintf(buf, sizeof(buf), natRule.c_str(),
                 BRIDGE_ADDRESS_RANGE_IPV6 "/120",
                 BRIDGE_ADDRESS_IPV6 "/128",
                 "udp",
                 "udp",
                 "[" LOCALHOST_IPV6 "]");
        natRules.emplace_back(buf);
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), natRule.c_str(),
                 BRIDGE_ADDRESS_RANGE_IPV6 "/120",
                 BRIDGE_ADDRESS_IPV6 "/128",
                 "tcp",
                 "tcp",
                 "[" LOCALHOST_IPV6 "]");
        natRules.emplace_back(buf);
        memset(buf, 0, sizeof(buf));

        // compose IPv6 rules for 'filter' table
        snprintf(buf, sizeof(buf), filterRule.c_str(),
                 BRIDGE_ADDRESS_RANGE_IPV6 "/120",
                 LOCALHOST_IPV6 "/128",
                 "udp",
                 "udp");
        filterRules.emplace_back(buf);
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), filterRule.c_str(),
                 BRIDGE_ADDRESS_RANGE_IPV6 "/120",
                 LOCALHOST_IPV6 "/128",
                 "tcp",
                 "tcp");
        filterRules.emplace_back(buf);
        memset(buf, 0, sizeof(buf));
    }

    // add nat and filter rules to create a ruleset
    Netfilter::RuleSet appendRuleSet =
    {
        { Netfilter::TableType::Nat, natRules },
        { Netfilter::TableType::Filter, filterRules }
    };

    AI_LOG_FN_EXIT();
    return appendRuleSet;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Add iptables rules and create the /etc/resolv.conf file.
 *
 *  Run in createRuntime hook.
 *
 *  Create a new /etc/resolv.conf file specifying the name server as our bridge
 *  interface. Add a PREROUTING rule to the iptable NAT table, which will
 *  redirect the traffic to localhost outside the container for port 53 only.
 *
 *  @param[in]  utils               Instance of DobbyRdkPluginUtils class
 *  @param[in]  netfilter           Instance of Netfilter class
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  rootfsPath          Path to container rootfs on the host
 *  @param[in]  containerId         Container identifier
 *  @param[in]  networkType         Network type
 *
 *  @return true if successful, otherwise false
 */
bool DnsmasqSetup::set(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                       const std::shared_ptr<Netfilter> &netfilter,
                       const std::shared_ptr<NetworkingHelper> &helper,
                       const std::string &rootfsPath,
                       const std::string &containerId,
                       const NetworkType networkType)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // install the iptables rules
    if (helper->ipv4())
    {
        Netfilter::RuleSet ipv4RuleSet = constructRules(containerId, AF_INET);
        if (!netfilter->appendRules(ipv4RuleSet, AF_INET))
        {
            AI_LOG_ERROR_EXIT("failed to setup netfilter rules for dns");
            return false;
        }
    }
    if (helper->ipv6())
    {
        Netfilter::RuleSet ipv6RuleSet = constructRules(containerId, AF_INET6);
        if (!netfilter->appendRules(ipv6RuleSet, AF_INET6))
        {
            AI_LOG_ERROR_EXIT("failed to setup netfilter rules for dns");
            return false;
        }
    }

    // set the nameserver, if using a NAT network setup then set the bridge
    // device as the nameserver (iptables will re-direct to localhost)
    std::string content;
    if (networkType == NetworkType::Nat)
    {
        content.append("nameserver " BRIDGE_ADDRESS "\n");

        if (helper->ipv6())
        {
            content.append("nameserver " BRIDGE_ADDRESS_IPV6 "\n");
        }
    }
    else
    {
        content.append("nameserver " LOCALHOST "\n");

        if (helper->ipv6())
        {
            content.append("nameserver " LOCALHOST_IPV6 "\n");
        }
    }

    // write the /etc/resolv.conf for the container
    std::string filePath = rootfsPath + "/etc/resolv.conf";
    if (!utils->writeTextFile(filePath, content, O_CREAT | O_TRUNC, 0644))
    {
        AI_LOG_ERROR_EXIT("failed to create file @ '/etc/resolv.conf' within rootfs");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Deletes dnsmasq rules for the container
 *
 *  Run in postHalt hook.
 *
 *  @param[in]  netfilter           Instance of Netfilter class
 *  @param[in]  helper              Instance of NetworkingHelper.
 *  @param[in]  containerId         Container identifier
 *
 *  @return true if successful, otherwise false
 */
bool DnsmasqSetup::removeRules(const std::shared_ptr<Netfilter> &netfilter,
                               const std::shared_ptr<NetworkingHelper> &helper,
                               const std::string &containerId)
{
    AI_LOG_FN_ENTRY();

    if (helper->ipv4())
    {
        Netfilter::RuleSet ipv4RuleSet = constructRules(containerId, AF_INET);
        if (!netfilter->deleteRules(ipv4RuleSet, AF_INET))
        {
            AI_LOG_ERROR_EXIT("failed to delete netfilter rules for dnsmasq");
            return false;
        }
    }
    if (helper->ipv6())
    {
        Netfilter::RuleSet ipv6RuleSet = constructRules(containerId, AF_INET6);
        if (!netfilter->deleteRules(ipv6RuleSet, AF_INET6))
        {
            AI_LOG_ERROR_EXIT("failed to delete netfilter rules for dnsmasq");
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}
