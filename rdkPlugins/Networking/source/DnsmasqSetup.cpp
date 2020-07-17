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
 *
 *  @return netfilter ruleset
 */
Netfilter::RuleSet constructRules(const std::string &containerId)
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
                        "-s " BRIDGE_ADDRESS_RANGE "/24 "
                        "-d " BRIDGE_ADDRESS "/32 "
                        "-i " BRIDGE_NAME " "
                        "-p %s "
                        "-m %s --dport 53 "
                        "-m comment --comment " + id + " "
                        "-j DNAT --to-destination " LOCALHOST ":53");
    snprintf(buf, sizeof(buf), natRule.c_str(), "udp", "udp");
    natRules.emplace_back(buf);
    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), natRule.c_str(), "tcp", "tcp");
    natRules.emplace_back(buf);
    memset(buf, 0, sizeof(buf));

    // allow DNS packets from containers
    std::list<std::string> filterRules;
    std::string filterRule("DobbyInputChain -s " BRIDGE_ADDRESS_RANGE "/24 "
                           "-d " LOCALHOST "/32 "
                           "-i " BRIDGE_NAME " "
                           "-p %s "
                           "-m %s --dport 53 "
                           "-m comment --comment " + id + " -j ACCEPT");
    snprintf(buf, sizeof(buf), filterRule.c_str(), "udp", "udp");
    filterRules.emplace_back(buf);
    memset(buf, 0, sizeof(buf));
    snprintf(buf, sizeof(buf), filterRule.c_str(), "tcp", "tcp");
    filterRules.emplace_back(buf);

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
 *  Run in postInstallation hook.
 *
 *  Create a new /etc/resolv.conf file specifying the name server as our bridge
 *  interface. A PREROUTING rule is already added to the iptable NAT table, which
 *  will redirect the traffic to localhost outside the container for port 53 only.
 *
 *  @param[in]  utils               Instance of DobbyRdkPluginUtils class
 *  @param[in]  netfilter           Instance of Netfilter class
 *  @param[in]  rootfsPath          Path to container rootfs on the host
 *  @param[in]  containerId         Container identifier
 *  @param[in]  networkType         Network type
 *
 *  @return true if successful, otherwise false
 */
bool DnsmasqSetup::set(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                       const std::shared_ptr<Netfilter> &netfilter,
                       const std::string &rootfsPath,
                       const std::string &containerId,
                       const NetworkType networkType)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    Netfilter::RuleSet ruleSet = constructRules(containerId);

    // install the iptables rules
    if (!netfilter->appendRules(ruleSet))
    {
        AI_LOG_ERROR_EXIT("failed to setup netfilter rules for dns");
        return false;
    }

    // set the nameserver, if using a NAT network setup then set the bridge
    // device as the nameserver (iptables will re-direct to localhost)
    std::string content;
    if (networkType == NetworkType::Nat)
    {
        content = "nameserver " BRIDGE_ADDRESS "\n";
    }
    else
    {
        content = "nameserver " LOCALHOST "\n";
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
 *  Run in postStop hook.
 *
 *  @param[in]  netfilter           Instance of Netfilter class
 *  @param[in]  containerId         Container identifier
 *
 *  @return true if successful, otherwise false
 *
 */
bool DnsmasqSetup::removeRules(const std::shared_ptr<Netfilter> &netfilter, const std::string &containerId)
{
    AI_LOG_FN_ENTRY();

    Netfilter::RuleSet ruleSet = constructRules(containerId);
    if (!netfilter->deleteRules(ruleSet))
    {
        AI_LOG_ERROR_EXIT("failed to delete netfilter rules for dnsmasq");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}
