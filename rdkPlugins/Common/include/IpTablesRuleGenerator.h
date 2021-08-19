/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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
#ifndef IPTABLESRULEGENERATOR_H
#define IPTABLESRULEGENERATOR_H

#include "DobbyRdkPluginUtils.h"
#include "DobbyNetworkingConstants.h"
#include <Logging.h>

#include <string>
#include <memory>
#include <netinet/in.h>
#include <inttypes.h>

class IpTablesRuleGenerator
{
public:
    // Localhost masquerade rules
    inline static std::string createMasqueradeDnatRule(const std::string &pluginName,
                                                       const std::string &containerId,
                                                       const in_port_t &port,
                                                       const std::string &protocol,
                                                       const int ipVersion)

    {
        AI_LOG_FN_ENTRY();

#if defined(DEV_VM)
        const std::string comment(pluginName + ":" + containerId);
#else
        const std::string comment("\"" + pluginName + ":" + containerId + "\"");
#endif

        std::string baseRule("OUTPUT "
                             "-o lo "
                             "-p %s "       // protocol
                             "-m %s "       // protocol
                             "--dport %hu " // port number
                             "-j DNAT "
                             "-m comment --comment %s " // Container id
                             "--to-destination %s"      // Bridge address:port
        );

        // create addresses based on IP version
        char destination[128];
        if (ipVersion == AF_INET)
        {
            snprintf(destination, sizeof(destination), "%s:%" PRIu16, BRIDGE_ADDRESS, port);
        }
        else
        {
            snprintf(destination, sizeof(destination), "%s:%" PRIu16, BRIDGE_ADDRESS_IPV6, port);
        }

        // populate fields in base rule
        char buf[256];
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                 protocol.c_str(),
                 protocol.c_str(),
                 port,
                 comment.c_str(),
                 destination);

        AI_LOG_DEBUG("Constructed rule: %s", buf);
        AI_LOG_FN_EXIT();
        return std::string(buf);
    }

    inline static std::string createMasqueradeSnatRule(const std::string &pluginName,
                                                       const std::string &containerId,
                                                       const std::string &ipAddress,
                                                       const std::string &protocol,
                                                       const int ipVersion)
    {
        AI_LOG_FN_ENTRY();

#if defined(DEV_VM)
        const std::string comment(pluginName + ":" + containerId);
#else
        const std::string comment("\"" + pluginName + ":" + containerId + "\"");
#endif

        std::string bridgeAddr;
        std::string sourceAddr;
        std::string destination;

        std::string baseRule("POSTROUTING "
                             "-p %s " // protocol
                             "-s %s " // container localhost
                             "-d %s " // bridge address
                             "-j SNAT "
                             "-m comment --comment %s " // container id
                             "--to %s");

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
        char buf[256];
        snprintf(buf, sizeof(buf), baseRule.c_str(),
                 protocol.c_str(),
                 sourceAddr.c_str(),
                 bridgeAddr.c_str(),
                 comment.c_str(),
                 destination.c_str());

        AI_LOG_DEBUG("Constructed rule: %s", buf);
        AI_LOG_FN_EXIT();
        return std::string(buf);
    }
};

#endif