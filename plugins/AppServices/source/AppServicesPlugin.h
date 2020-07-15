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
/*
 * File:   AppServicesPlugin.h
 *
 * Copyright (C) BSKYB 2019+
 */
#ifndef APPSERVICESPLUGIN_H
#define APPSERVICESPLUGIN_H

#include <IDobbyPlugin.h>
#include <PluginBase.h>
#include <Netfilter.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <map>
#include <set>
#include <list>
#include <mutex>
#include <string>
#include <memory>



// -----------------------------------------------------------------------------
/**
 *  @class AppServicesPlugin
 *  @brief Plugin just used to map in access for AS services.
 *
 *  This plugin currently just uses iptables to setup routing to AS.  However
 *  the end goal is to have this plugin talk to the asproxy daemon and create
 *  a bespoke listening socket for AS services with fine grained access control.
 *
 */
class AppServicesPlugin : public PluginBase
{
public:
    AppServicesPlugin(const std::shared_ptr<IDobbyEnv>& env,
                      const std::shared_ptr<IDobbyUtils>& utils);
    ~AppServicesPlugin() final;

public:
    std::string name() const final;
    unsigned hookHints() const final;

public:
    bool postConstruction(const ContainerId& id,
                          const std::shared_ptr<IDobbyStartState>& startupState,
                          const std::string& rootfsPath,
                          const Json::Value& jsonData) final;

    bool preStart(const ContainerId& id,
                  pid_t pid,
                  const std::string& rootfsPath,
                  const Json::Value& jsonData) final;

    bool postStop(const ContainerId& id,
                  const std::string& rootfsPath,
                  const Json::Value& jsonData) final;

private:
    std::string constructDNATRule(const ContainerId& id,
                                  const std::string &containerIp,
                                  in_port_t port) const;
    std::string constructACCEPTRule(const ContainerId& id,
                                    const std::string &containerIp,
                                    const std::string &vethName,
                                    in_port_t port) const;

private:
    const std::string mName;
    const std::shared_ptr<IDobbyUtils> mUtilities;

private:
    enum LocalServicesPort : in_port_t
    {
        LocalServices1Port = 9001,
        LocalServices2Port = 9002,
        LocalServices3Port = 9003,
        LocalServices4Port = 9004,
        LocalServices5Port = 9009,
    };

private:
    std::mutex mLock;

    struct ServicesConfig
    {
        LocalServicesPort asPort;
        std::set<in_port_t> additionalPorts;
        Netfilter::RuleSet nfRuleSet;
    };

    std::map<ContainerId, ServicesConfig> mContainerServices;
    std::shared_ptr<Netfilter> mNetfilter;

};


#endif // !defined(APPSERVICESPLUGIN_H)
