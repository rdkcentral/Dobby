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
 * File:   HolePuncherPlugin.h
 *
 * Copyright (C) Sky UK 2018+
 */
#ifndef HOLEPUNCHERPLUGIN_H
#define HOLEPUNCHERPLUGIN_H

#include <IDobbyPlugin.h>
#include <PluginBase.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <map>
#include <list>
#include <mutex>
#include <string>
#include <memory>



// -----------------------------------------------------------------------------
/**
 *  @class HolePuncherPlugin
 *  @brief Plugin just used to add iptables firewall rules to allow containered
 *  processes to run servers.
 *
 *  This plugin adds the necessary rules to iptables when the container is
 *  started and deletes them again when the container is stopped.  All the rules
 *  are tagged (via an iptables comment) with the name of the container, this
 *  should ensure rules are correctly added and removed.
 *
 */
class HolePuncherPlugin : public PluginBase
{
public:
    HolePuncherPlugin(const std::shared_ptr<IDobbyEnv>& env,
                      const std::shared_ptr<IDobbyUtils>& utils);
    virtual ~HolePuncherPlugin();

public:
    virtual std::string name() const final;
    virtual unsigned hookHints() const final;

public:
    virtual bool preStart(const ContainerId& id,
                          pid_t pid,
                          const std::string& rootfsPath,
                          const Json::Value& jsonData) final;

    virtual bool postStop(const ContainerId& id,
                          const std::string& rootfsPath,
                          const Json::Value& jsonData) final;

private:
    void getContainerIpAddress(in_addr_t *ipAddress) const;

    bool execIpTables(const std::list<std::string>& args,
                      int stdinFd = -1, int stdoutFd = -1, int stderrFd = -1) const;

    std::list<std::string> constructPreRoutingRuleArgs(bool add,
                                                       const ContainerId& id,
                                                       const std::string& protocol,
                                                       const std::string& ipAddress,
                                                       const std::string& portNumber) const;
    std::list<std::string> constructForwardingRuleArgs(bool add,
                                                       const ContainerId& id,
                                                       const std::string& protocol,
                                                       const std::string& ipAddress,
                                                       const std::string& portNumber) const;

    bool addHolePunch(const ContainerId& id, int protocol,
                      in_addr_t ipAddress, in_port_t portNumber);
    void removeHolePunch(const ContainerId& id, int protocol,
                         in_addr_t ipAddress, in_port_t portNumber);

private:
    const std::string mName;
    const std::shared_ptr<IDobbyUtils> mUtilities;

private:
    struct HolePunch
    {
        int protocol;
        in_addr_t ipAddress;
        in_port_t portNumber;
    };

private:
    std::mutex mLock;
    std::multimap<ContainerId, HolePunch> mHolePunchMap;
};


#endif // !defined(HOLEPUNCHERPLUGIN_H)
