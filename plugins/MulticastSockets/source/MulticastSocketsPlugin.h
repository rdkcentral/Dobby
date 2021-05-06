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

#ifndef MULTICASTSOCKETSPLUGIN_H
#define MULTICASTSOCKETSPLUGIN_H


#include <IDobbyPlugin.h>
#include <PluginBase.h>

#include <netinet/in.h>

#include <vector>
#include <string>
#include <memory>


// -----------------------------------------------------------------------------
/**
 *  @class MulticastSocketPlugin
 *  @brief Plugin used to setup multicast server and client sockets out of the container
 *  and passes their file descriptors to process inside the container
 *
 *  This plugin creates all necessary sockets when
 *  the container is started and closes them when the container is stopped.
 *
 */
class MulticastSocketPlugin final : public PluginBase
{
public:
    MulticastSocketPlugin(const std::shared_ptr<IDobbyEnv>& env,
                          const std::shared_ptr<IDobbyUtils>& utils);
    ~MulticastSocketPlugin() final;

public:
    std::string name() const final;
    unsigned hookHints() const final;

public:
    bool postConstruction(const ContainerId& id,
        const std::shared_ptr<IDobbyStartState>& startupState,
        const std::string& rootfsPath,
        const Json::Value& jsonData) final;

private:
    struct MulticastSocket
    {
        std::string name;
        in_addr_t ipAddress;
        in_port_t portNumber;
    };

    std::vector<MulticastSocket> parseServerSocketsArray(const Json::Value& jsonData) const;
    std::vector<std::string> parseClientSocketsArray(const Json::Value& jsonData) const;

    int createServerSocket(in_addr_t ip, in_port_t port);
    int createClientSocket();

private:
    const std::string mName;
    const std::shared_ptr<IDobbyUtils> mUtilities;
};

#endif // MULTICASTSOCKETSPLUGIN_H