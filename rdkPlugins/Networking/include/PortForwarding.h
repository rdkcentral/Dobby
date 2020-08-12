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
 *  @namespace PortForwarding TODO: add loports
 *
 *  @brief Used to add iptables firewall rules to allow containered processes
 *  to run servers.
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
                     rt_defs_plugins_networking_data_port_forwarding *portForwards);

bool removePortForwards(const std::shared_ptr<Netfilter> &netfilter,
                        const std::shared_ptr<NetworkingHelper> &helper,
                        const std::string &containerId,
                        rt_defs_plugins_networking_data_port_forwarding *portForwards);
};

std::vector<Netfilter::RuleSet> constructRules(const std::shared_ptr<NetworkingHelper> &helper,
                                               const std::string &containerId,
                                               rt_defs_plugins_networking_data_port_forwarding *portForwards,
                                               const int ipVersion);

bool constructHostToContainerRules(std::vector<Netfilter::RuleSet> &ruleSets,
                                   const std::string &containerId,
                                   const std::string &containerAddress,
                                   rt_defs_plugins_networking_data_port_forwarding_host_to_container_element **ports,
                                   size_t len, const int ipVersion);

std::string createPreroutingRule(const std::string &id,
                                 const std::string &protocol,
                                 const std::string &ipAddress,
                                 const std::string &portNumber,
                                 const int ipVersion);

std::string createForwardingRule(const std::string &id,
                                 const std::string &protocol,
                                 const std::string &ipAddress,
                                 const std::string &portNumber,
                                 const int ipVersion);

bool constructContainerToHostRules(std::vector<Netfilter::RuleSet> &ruleSets,
                                   const std::string &containerId,
                                   const std::string &containerAddress,
                                   const std::string &vethName,
                                   rt_defs_plugins_networking_data_port_forwarding_container_to_host_element **ports,
                                   size_t len, const int ipVersion);

std::string createDnatRule(const std::string &id,
                           const std::string &protocol,
                           const std::string &ipAddress,
                           const std::string &portNumber,
                           const int ipVersion);

std::string createAcceptRule(const std::string &id,
                             const std::string &protocol,
                             const std::string &ipAddress,
                             const std::string &vethName,
                             const std::string &portNumber,
                             const int ipVersion);

#endif // !defined(PORTFORWARDING_H)