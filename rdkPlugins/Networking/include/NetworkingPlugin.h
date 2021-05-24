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

#ifndef NETWORKINGPLUGIN_H
#define NETWORKINGPLUGIN_H

#include <RdkPluginBase.h>
#include <DobbyProtocol.h>
#include <DobbyRdkPluginProxy.h>
#include "Netfilter.h"
#include "NetworkingHelper.h"
#include "IpcFactory.h"

#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>


/**
 * @class Dobby Networking Plugin
 */
class NetworkingPlugin : public RdkPluginBase
{
public:
    NetworkingPlugin(std::shared_ptr<rt_dobby_schema> &cfg,
                     const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                     const std::string &rootfsPath);
    ~NetworkingPlugin();

public:
    inline std::string name() const override
    {
        return mName;
    };

    unsigned hookHints() const override;

public:
    bool postInstallation() override;
    bool createRuntime() override;
    bool createContainer() override;
    bool postHalt() override;

public:
    std::vector<std::string> getDependencies() const override;

private:
    bool createRemoteService();

private:
    bool mValid;
    const std::string mName;
    NetworkType mNetworkType;

    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;

    const std::string mRootfsPath;
    const rt_defs_plugins_networking_data *mPluginData;

    std::shared_ptr<AI_IPC::IIpcService> mIpcService;
    std::shared_ptr<DobbyRdkPluginProxy> mDobbyProxy;
    std::shared_ptr<NetworkingHelper> mHelper;
    std::shared_ptr<Netfilter> mNetfilter;
};

#endif // !defined(NETWORKINGPLUGIN_H)
