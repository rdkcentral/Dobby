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

#include "MulticastSocketsPlugin.h"

#include <Logging.h>

#include <unistd.h>
#include <arpa/inet.h>

// -----------------------------------------------------------------------------
/**
  *  @brief Registers the MulticastSocketPlugin plugin object.
  */
REGISTER_DOBBY_PLUGIN(MulticastSocketPlugin);

MulticastSocketPlugin::MulticastSocketPlugin(const std::shared_ptr<IDobbyEnv> &env,
                                             const std::shared_ptr<IDobbyUtils> &utils)
    : mName("MulticastSockets"), mUtilities(utils)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

MulticastSocketPlugin::~MulticastSocketPlugin()
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Boilerplate that just returns the name of the hook
 *
 *  This string needs to match the name specified in the container spec json.
 *
 */
std::string MulticastSocketPlugin::name() const
{
    return mName;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Indiciates which hook points we want and whether to run the
 *  asynchronously or synchronously with the other hooks
 *
 *  For MulticastSocketPlugin everything is done in the PostConstruction phase.
 */
unsigned MulticastSocketPlugin::hookHints() const
{
    return IDobbyPlugin::PostConstructionSync;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates multicast server and client sockets out of the container
 *         and passes their file descriptors to the container in env variables
 *
 *  The json data is expected to be formatted like the following:
 *
 *      {
            "name": "MulticastSockets",
            "data": {
                "serverSockets": [
                    {
                        "name": "NAME"
                        "ip": "239.255.255.250",
                        "port": 1900
                    }
                ],
                "clientSockets": [
                    {
                        "name": "NAME1"
                    }
                ]
            }
        }
 *
 *  "serverSockets" and "clientSockets" are optional
 */
bool MulticastSocketPlugin::postConstruction(const ContainerId &id,
                                             const std::shared_ptr<IDobbyStartState> &startupState,
                                             const std::string &rootfsPath,
                                             const Json::Value &jsonData)
{
    std::vector<MulticastSocket> serverSockets = parseServerSocketsArray(jsonData);
    std::vector<std::string> clientSockets = parseClientSocketsArray(jsonData);

    for (const MulticastSocket &serverSocket : serverSockets)
    {
        int socket = createServerSocket(serverSocket.ipAddress, serverSocket.portNumber);
        int duppedSocket = startupState->addFileDescriptor(mName, socket);
        close(socket); //close original fd, it's already dupped and stored in startupState

        if (duppedSocket == -1)
        {
            AI_LOG_ERROR("Failed to duplicate server socket for container %s", id.c_str());
            return false;
        }

        char envVar[256];
        snprintf(envVar, sizeof(envVar), "MCAST_SERVER_SOCKET_%s_FD=%u", serverSocket.name.c_str(), duppedSocket);
        if (!startupState->addEnvironmentVariable(envVar))
        {
            AI_LOG_ERROR("Failed to set env variable for container %s", id.c_str());
            return false;
        }
    }

    for (const std::string &clientSocket : clientSockets)
    {
        int socket = createClientSocket();
        int duppedSocket = startupState->addFileDescriptor(mName, socket);
        close(socket); //close original fd, it's already dupped and stored in startupState

        if (duppedSocket == -1)
        {
            AI_LOG_ERROR("Failed to duplicate server socket for container %s", id.c_str());
            return false;
        }

        char envVar[256];
        snprintf(envVar, sizeof(envVar), "MCAST_CLIENT_SOCKET_%s_FD=%u", clientSocket.c_str(), duppedSocket);
        if (!startupState->addEnvironmentVariable(envVar))
        {
            AI_LOG_ERROR("Failed to set env variable for container %s", id.c_str());
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Parses and verifies server socket data from json array
 *
 *  The json data is expected (required) to be formatted like the following
 *
 *
    "serverSockets": [
        {
            "name": "NAME"
            "ip": "239.255.255.250",
            "port": 1900
        }
    ]
*/
std::vector<MulticastSocketPlugin::MulticastSocket> MulticastSocketPlugin::parseServerSocketsArray(const Json::Value &jsonData) const
{
    std::vector<MulticastSocket> socketsVec;

    const Json::Value &sockets = jsonData["serverSockets"];
    if (!sockets.isArray() || sockets.empty())
    {
        AI_LOG_INFO("'serverSockets' field is not an array or it's empty");
        return std::vector<MulticastSocket>();
    }

    for (Json::Value::const_iterator it = sockets.begin(); it != sockets.end(); ++it)
    {
        const Json::Value &socket = *it;
        if (!socket.isObject())
        {
            AI_LOG_ERROR("invalid 'socket' entry at index %u in 'serverSockets' array", it.index());
            continue;
        }

        MulticastSocket multicastSocket;

        const Json::Value name = socket["name"];
        const Json::Value ip = socket["ip"];
        const Json::Value port = socket["port"];

        if (!name.isString())
        {
            AI_LOG_ERROR("invalid name entry at index %u in 'serverSockets' array", it.index());
            continue;
        }

        in_addr_t ipAddr = 0;
        if (ip.isString() && inet_pton(AF_INET, ip.asString().c_str(), &ipAddr) != 1)
        {
            AI_LOG_WARN("invalid IP entry at index %u in 'serverSockets' array", it.index());
            continue;
        }

        if (!port.isIntegral())
        {
            AI_LOG_ERROR("invalid port entry at index %u in 'serverSockets' array", it.index());
            continue;
        }

        multicastSocket.ipAddress = ipAddr;
        multicastSocket.portNumber = static_cast<in_port_t>(port.asInt());
        multicastSocket.name = name.asString();

        socketsVec.push_back(multicastSocket);
    }

    return socketsVec;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Parses and verifies client socket data from json array
 *
 *  The json data is expected (required) to be formatted like the following
 *
 *
    "clientSockets": [
        {
            "name": "NAME1"
        }
    ]
*/
std::vector<std::string> MulticastSocketPlugin::parseClientSocketsArray(const Json::Value &jsonData) const
{
    std::vector<std::string> socketsVec;

    const Json::Value &sockets = jsonData["clientSockets"];
    if (!sockets.isArray() || sockets.empty())
    {
        AI_LOG_INFO("'clientSockets' field is not an array or it's empty");
        return std::vector<std::string>();
    }

    for (Json::Value::const_iterator it = sockets.begin(); it != sockets.end(); ++it)
    {
        const Json::Value &socket = *it;
        if (!socket.isObject())
        {
            AI_LOG_ERROR("invalid 'socket' entry at index %u in 'clientSockets' array", it.index());
            continue;
        }

        const Json::Value name = socket["name"];

        if (!name.isString())
        {
            AI_LOG_ERROR("invalid name entry at index %u in 'clientSockets' array", it.index());
            continue;
        }

        socketsVec.push_back(name.asString());
    }

    return socketsVec;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates socket and binds it to multicast \a ip and \a port
 *
 */
int MulticastSocketPlugin::createServerSocket(in_addr_t ip, in_port_t port)
{
    int multicastSocket = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (multicastSocket == -1)
    {
        AI_LOG_SYS_ERROR(errno, "Unable to create socket");
        return -1;
    }

    int onOff = 1;
    if (setsockopt(multicastSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char *>(&onOff), sizeof(onOff)) == -1)
    {
        AI_LOG_SYS_ERROR(errno, "Unable to set SO_REUSEADDR option");

        close(multicastSocket);
        return -1;
    }

    sockaddr_in multicastAddr;
    multicastAddr.sin_family = AF_INET;
    multicastAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    multicastAddr.sin_port = htons(port);
    if (bind(multicastSocket, reinterpret_cast<sockaddr *>(&multicastAddr), sizeof(multicastAddr)) == -1)
    {
        AI_LOG_SYS_ERROR(errno, "Unable to bind server socket");

        close(multicastSocket);
        return -1;
    }

    ip_mreqn group;
    memset(reinterpret_cast<void *>(&group), 0, sizeof(ip_mreqn));
    group.imr_address.s_addr = htonl(INADDR_ANY);
    group.imr_multiaddr.s_addr = ip;

    if (setsockopt(multicastSocket, IPPROTO_IP, IP_ADD_MEMBERSHIP, reinterpret_cast<char *>(&group), sizeof(group)) == -1)
    {
        AI_LOG_SYS_ERROR(errno, "Unable to set IP_ADD_MEMBERSHIP option");

        close(multicastSocket);
        return -1;
    }

    unsigned char ttl = 1;
    if (setsockopt(multicastSocket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl) == -1))
    {
        //this is not critical
        AI_LOG_SYS_ERROR(errno, "Failed to set TTL of server socket");
    }

    return multicastSocket;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates client udp socket
 *
 */
int MulticastSocketPlugin::createClientSocket()
{
    int ssdpSocket = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (ssdpSocket == -1)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to create client socket");
        return -1;
    }

    unsigned char ttl = 1;
    if (setsockopt(ssdpSocket, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl) == -1))
    {
        //this is not critical
        AI_LOG_SYS_ERROR(errno, "Failed to set TTL of client socket");
    }

    return ssdpSocket;
}
