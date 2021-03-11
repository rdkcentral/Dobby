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

#ifndef THUNDERPLUGIN_H
#define THUNDERPLUGIN_H

#include <Netfilter.h>
#include <RdkPluginBase.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <map>
#include <set>
#include <list>
#include <mutex>
#include <string>
#include <memory>

class ThunderSecurityAgent;

// -----------------------------------------------------------------------------
/**
 *  @class ThunderPlugin
 *  @brief Plugin used to map in the wpeframework (aka thunder) server.
 *
 *  @note This is NOT a "thunder plugin" for the WPEFramework, instead it is
 *  plugin to dobby to allow containers to access the WPEFramework services.
 *
 *  This plugin does two things; it uses iptables to setup routing to the
 *  wpeframework server, and it optionally creates a security token for the app
 *  and puts it in the containers env variables.
 *
 */
class ThunderPlugin : public RdkPluginBase
{
public:
    ThunderPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                  const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                  const std::string &rootfsPath);

public:
    inline std::string name() const override
    {
        return mName;
    };

    unsigned hookHints() const final;

public:
    bool postInstallation() final;

    bool preCreation() final;

    bool createRuntime() final;

    bool postHalt() final;

public:
    std::vector<std::string> getDependencies() const override;

private:
    Netfilter::RuleSet constructRules() const;

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

    in_port_t mThunderPort;
    std::shared_ptr<Netfilter> mNetfilter;

private:
    std::mutex mLock;
    const bool mEnableConnLimit;
};
#endif // !defined(THUNDERPLUGIN_H)