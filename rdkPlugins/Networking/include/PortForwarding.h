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
#ifndef PORTFORWARDING_H
#define PORTFORWARDING_H

#include "Netfilter.h"
#include "NetworkingHelper.h"
#include "DobbyRdkPluginUtils.h"
#include <rt_defs_plugins.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <map>
#include <list>
#include <mutex>
#include <string>
#include <memory>
#include <vector>


// -----------------------------------------------------------------------------
/**
 *  @namespace PortForwarding
 *
 *  @brief Used to add iptables firewall rules to allow port forwarding between
 *  the container and the host.
 *
 *  Has the ability to both add rules to forward ports from container to host
 *  and from host to container.
 *
 *  @see the plugin's README.md for more details on usage.
 *
 *  This adds the necessary rules to iptables when the container is started and
 *  deletes them again when the container is stopped.  All the rules are tagged
 *  (via an iptables comment) with the name of the container, this should ensure
 *  rules are correctly added and removed.
 *
 */
namespace PortForwarding
{
bool addPortForwards(const std::shared_ptr<Netfilter> &netfilter,
                     const std::shared_ptr<NetworkingHelper> &helper,
                     const std::string &containerId,
                     rt_defs_plugins_networking_data_port_forwarding *portsConfig);

bool removePortForwards(const std::shared_ptr<Netfilter> &netfilter,
                        const std::shared_ptr<NetworkingHelper> &helper,
                        const std::string &containerId,
                        rt_defs_plugins_networking_data_port_forwarding *portsConfig);

bool addLocalhostMasquerading(const std::shared_ptr<NetworkingHelper> &helper,
                              const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                              rt_defs_plugins_networking_data_port_forwarding *portsConfig);
};

typedef struct PortForward
{
    std::string protocol;
    std::string port;
} PortForward;

typedef struct PortForwards
{
    std::vector<struct PortForward> hostToContainer;
    std::vector<struct PortForward> containerToHost;
    bool isValid;
} PortForwards;

std::string parseProtocol(const std::string &protocol);
PortForwards parsePortsConfig(rt_defs_plugins_networking_data_port_forwarding *portsConfig);

std::vector<Netfilter::RuleSet> constructPortForwardingRules(const std::shared_ptr<NetworkingHelper> &helper,
                                               const std::string &containerId,
                                               const PortForwards &portForwards,
                                               const int ipVersion);

std::vector<Netfilter::RuleSet> constructMasqueradeRules(const std::shared_ptr<NetworkingHelper> &helper,
                                                         const std::string &containerId,
                                                         const PortForwards &portForwards,
                                                         const int ipVersion);

bool constructHostToContainerRules(std::vector<Netfilter::RuleSet> &ruleSets,
                                   const std::string &containerId,
                                   const std::string &containerAddress,
                                   const std::vector<struct PortForward> &ports,
                                   const int ipVersion);

std::string createPreroutingRule(const PortForward &portForward,
                                 const std::string &id,
                                 const std::string &ipAddress,
                                 const int ipVersion);

std::string createForwardingRule(const PortForward &portForward,
                                 const std::string &id,
                                 const std::string &ipAddress,
                                 const int ipVersion);

bool constructContainerToHostRules(std::vector<Netfilter::RuleSet> &ruleSets,
                                   const std::string &containerId,
                                   const std::string &containerAddress,
                                   const std::string &vethName,
                                   const std::vector<struct PortForward> &ports,
                                   const int ipVersion);

std::string createDnatRule(const PortForward &portForward,
                           const std::string &id,
                           const std::string &ipAddress,
                           const int ipVersion);

std::string createAcceptRule(const PortForward &portForward,
                             const std::string &id,
                             const std::string &ipAddress,
                             const std::string &vethName,
                             const int ipVersion);

std::string createMasqueradeDnatRule(const PortForward &portForward,
                                     const std::string &id,
                                     const std::string &ipAddress,
                                     const int ipVersion);

std::string createMasqueradeSnatRule(const PortForward &portForward,
                                    const std::string &id,
                                    const std::string &ipAddress,
                                    const int ipVersion);

std::string createLocalLinkSnatRule(const PortForward &portForward,
                                    const std::string &id,
                                    const std::string &ipAddress,
                                    const int ipVersion);

std::string createNoIpv6LocalRule(const PortForward &portForward,
                                    const std::string &id,
                                    const std::string &ipAddress,
                                    const int ipVersion);

#endif // !defined(PORTFORWARDING_H)