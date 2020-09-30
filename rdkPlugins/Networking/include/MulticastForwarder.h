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

#ifndef MULTICASTFORWARDER_H
#define MULTICASTFORWARDER_H

#include "Netfilter.h"
#include "rt_dobby_schema.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <map>
#include <list>
#include <unordered_map>
#include <mutex>
#include <string>
#include <memory>
#include <vector>


// -----------------------------------------------------------------------------
/**
 *  @namespace MulticastForwarderPlugin
 *
 *  @brief Used to add iptables firewall rules, ebtables and smcroute rules to
 *  allow containered processes to receive multicast traffic.
 *
 *  This plugin adds the necessary rules to iptables, ebtables and smcroute when
 *  the container is started and deletes them again when the container is stopped.
 *  All the rules are tagged (via an iptables comment) with the name of the
 *  container, this should ensure rules are correctly added and removed.
 *
 */
namespace MulticastForwarder
{
bool set(const std::shared_ptr<Netfilter> &netfilter,
         const rt_defs_plugins_networking_data *pluginData,
         const std::string &vethName,
         const std::string &containerId,
         const std::vector<std::string> &extIfaces);

bool removeRules(const std::shared_ptr<Netfilter> &netfilter,
                 const rt_defs_plugins_networking_data *pluginData,
                 const std::string &vethName,
                 const std::string &containerId,
                 const std::vector<std::string> &extIfaces);
};

bool checkCompatibility();
int checkAddressFamily(const std::string &address);
bool executeCommand(const std::string &command);

bool addSmcrouteRules(const std::vector<std::string> &extIfaces, const std::string &address);
bool removeSmcrouteRules(const std::vector<std::string> &extIfaces, const std::string &address);

std::string constructPreRoutingIptablesRule(const std::string &containerId,
                                            const std::string &address,
                                            const in_port_t port,
                                            const int addressFamily);
std::string constructForwardingIptablesRule(const std::string &containerId,
                                            const std::string &address,
                                            const in_port_t port,
                                            const int addressFamily);
std::string constructEbtablesRule(const std::string &address,
                                  const std::string &vethName,
                                  const int addressFamily);

#endif // MULTICASTFORWARDER_H