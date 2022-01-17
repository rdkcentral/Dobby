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
/*
 * File: MulticastSocketsPlugin.h
 *
 */
#ifndef MulticastSocketsPlugin_H
#define MulticastSocketsPlugin_H

#include <RdkPluginBase.h>

#include <unistd.h>
#include <string>
#include <memory>
#include <vector>
#include <netinet/in.h>

class MulticastSocketsPlugin : public RdkPluginBase
{
public:
    MulticastSocketsPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                           const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                           const std::string &rootfsPath);

public:
    inline std::string name() const override
    {
        return mName;
    };

    unsigned hookHints() const override;

public:
    bool preCreation() override;

public:
    std::vector<std::string> getDependencies() const override;

private:
    int createServerSocket(in_addr_t ip, in_port_t port) const;
    int createClientSocket() const;

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const rt_defs_plugins_multicast_sockets_data *mPluginData;
    const std::string mRootfsPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;

    typedef struct MulticastSocket
    {
        std::string name;
        in_addr_t ipAddress;
        in_port_t portNumber;
    } MulticastSocket;
};

#endif // !defined(MulticastSocketsPlugin_H)
