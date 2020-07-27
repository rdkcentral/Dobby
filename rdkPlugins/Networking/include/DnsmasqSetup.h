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

#ifndef DNSMASQSETUP_H
#define DNSMASQSETUP_H

#include "NetworkingHelper.h"
#include "DobbyRdkPluginUtils.h"
#include "Netfilter.h"

#include <arpa/inet.h>

#include <map>
#include <list>
#include <string>
#include <memory>
#include <mutex>
#include <queue>

// -----------------------------------------------------------------------------
/**
 *  @namespace DnsmasqSetup
 *
 *  @brief Sets up iptables routing to allow apps within a network namespace
 *  to talk to the dnsmasq server running outside the container.
 *
 *  This works by routing traffic sent to the dobby bridge on port 53 to the
 *  localhost interface outside the container.
 */
namespace DnsmasqSetup
{
    bool set(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
             const std::shared_ptr<Netfilter> &netfilter,
             const std::shared_ptr<NetworkingHelper> &helper,
             const std::string &rootfsPath,
             const std::string &containerId,
             const NetworkType networkType);

    bool removeRules(const std::shared_ptr<Netfilter> &netfilter,
                     const std::shared_ptr<NetworkingHelper> &helper,
                     const std::string &containerId);
};

#endif // !defined(DNSMASQSETUP_H)
