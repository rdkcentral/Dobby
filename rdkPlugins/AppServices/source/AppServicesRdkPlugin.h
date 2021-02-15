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
/*
 * File: AppServicesRdkPlugin.h
 *
 */
#ifndef APPSERVICESRDKPLUGIN_H
#define APPSERVICESRDKPLUGIN_H

#include <Netfilter.h>
#include <RdkPluginBase.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <unistd.h>
#include <string>
#include <memory>

/**
 *  @class AppServicesRdkPlugin
 *  @brief Plugin just used to map in access for AS services.
 *
 *  This plugin currently just uses iptables to setup routing to AS.  However
 *  the end goal is to have this plugin talk to the asproxy daemon and create
 *  a bespoke listening socket for AS services with fine grained access control.
 *
 */
class AppServicesRdkPlugin : public RdkPluginBase
{
public:
    AppServicesRdkPlugin(std::shared_ptr<rt_dobby_schema>& containerConfig,
                         const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                         const std::string &rootfsPath,
                         const std::string &hookStdin);

public:
    inline std::string name() const override
    {
        return mName;
    };

    unsigned hookHints() const override;

public:
    bool preCreation() override;
    bool createRuntime() override;
    bool postHalt() override;

private:
    enum LocalServicesPort : in_port_t
    {
        LocalServicesNone  = 0,
        LocalServicesInvalid = 1,

        LocalServices1Port = 9001,
        LocalServices2Port = 9002,
        LocalServices3Port = 9003,
        LocalServices4Port = 9004,
        LocalServices5Port = 9009,
    };

    LocalServicesPort getAsPort() const;

    Netfilter::RuleSet constructRules() const;
    void addRulesForPort(const std::string &containerIp, const std::string &vethName,
                         in_port_t port,
                         std::list<std::string>& acceptRules, std::list<std::string>& natRules) const;
    std::string constructDNATRule(const std::string &containerIp,
                                  in_port_t port) const;
    std::string constructCONNLIMITRule(const std::string &containerIp,
                                       const std::string &vethName,
                                       in_port_t port,
                                       uint32_t connLimit) const;
    std::string constructACCEPTRule(const std::string &containerIp,
                                    const std::string &vethName,
                                    in_port_t port) const;

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
    const std::string mRootfsPath;

    bool mValid;
    const rt_defs_plugins_app_services_rdk_data* mPluginConfig;
    std::shared_ptr<Netfilter> mNetfilter;
    const bool mEnableConnLimit;
};

#endif // !defined(APPSERVICESRDKPLUGIN_H)
