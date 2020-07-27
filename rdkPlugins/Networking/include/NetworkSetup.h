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

#ifndef NETWORKSETUP_H
#define NETWORKSETUP_H

#include "Netfilter.h"
#include "NetworkingPluginCommon.h"
#include "rt_dobby_schema.h"
#include <DobbyRdkPluginProxy.h>
#include <DobbyRdkPluginUtils.h>

#include <arpa/inet.h>
#include <map>
#include <list>
#include <string>
#include <memory>
#include <mutex>
#include <vector>


// -----------------------------------------------------------------------------
/**
 *  @namespace NetworkSetup
 *
 *  @brief Functions to set up networking for containers
 *
 */
namespace NetworkSetup
{
    bool setupBridgeDevice(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                           const std::shared_ptr<Netfilter> &netfilter,
                           const std::vector<std::string> &extIfaces);

    bool setupVeth(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                   const std::shared_ptr<Netfilter> &netfilter,
                   const std::shared_ptr<DobbyRdkPluginProxy> &dobbyProxy,
                   const std::string &rootfsPath,
                   const std::string &containerId,
                   const NetworkType networkType);

    bool removeVethPair(const std::shared_ptr<Netfilter> &netfilter,
                        const std::string ipAddressStr,
                        const std::string vethName,
                        const NetworkType networkType);

    bool removeBridgeDevice(const std::shared_ptr<Netfilter> &netfilter,
                            const std::vector<std::string> &extIfaces);

    void addSysfsMount(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                       const std::shared_ptr<rt_dobby_schema> &cfg);

    void addNetworkNamespace(const std::shared_ptr<rt_dobby_schema> &cfg);
};

bool setupContainerNet(const in_addr_t address);

#endif // !defined(NETWORKSETUP_H)
