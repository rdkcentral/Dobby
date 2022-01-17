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

#include "MulticastSocketsPlugin.h"

#include <arpa/inet.h>

/**
 * Need to do this at the start of every plugin to make sure the correct
 * C methods are visible to allow PluginLauncher to find the plugin
 */
REGISTER_RDK_PLUGIN(MulticastSocketsPlugin);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
MulticastSocketsPlugin::MulticastSocketsPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                                               const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                               const std::string &rootfsPath)
    : mName("MulticastSockets"),
      mContainerConfig(containerConfig),
      mRootfsPath(rootfsPath),
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();

    if (mContainerConfig->rdk_plugins->multicastsockets != nullptr && mContainerConfig->rdk_plugins->multicastsockets->data != nullptr)
    {
        mPluginData = mContainerConfig->rdk_plugins->multicastsockets->data;
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 *
 * This plugin uses all the hooks so set all the flags
 */
unsigned MulticastSocketsPlugin::hookHints() const
{
    return IDobbyRdkPlugin::HintFlags::PreCreationFlag;
}

// Begin Hook Methods

/**
 * @brief Dobby Hook - run in host namespace before container creation process
 */
bool MulticastSocketsPlugin::preCreation()
{
    // Parse the server sockets array
    std::vector<MulticastSocket> serverSockets;
    serverSockets.reserve(mPluginData->server_sockets_len);

    for (size_t i = 0; i < mPluginData->server_sockets_len; i++)
    {
        auto socketData = mPluginData->server_sockets[i];

        MulticastSocket socket = {};
        socket.name = socketData->name;

        // Convert the ip address string to proper IP
        in_addr_t ipAddr;
        if (inet_pton(AF_INET, socketData->ip, &ipAddr) != 1)
        {
            AI_LOG_WARN("invalid IP entry %s in multicast server sockets", socketData->ip);
            continue;
        }
        socket.ipAddress = ipAddr;
        socket.portNumber = static_cast<in_port_t>(socketData->port);
        serverSockets.emplace_back(socket);
    }

    // Set up the sockets

    // Server sockets
    for (const auto &serverSocket : serverSockets)
    {
        int socket = createServerSocket(serverSocket.ipAddress, serverSocket.portNumber);
        int duppedSocket = mUtils->addFileDescriptor(mName, socket);
        close(socket); //close original fd, it's already dupped and stored in startupState

        if (duppedSocket == -1)
        {
            AI_LOG_ERROR("Failed to duplicate server socket for container %s", mUtils->getContainerId().c_str());
            return false;
        }

        // Add an environment variable inside the container to retrieve the fd of the socket
        char envVar[256];
        snprintf(envVar, sizeof(envVar), "MCAST_SERVER_SOCKET_%s_FD=%u", serverSocket.name.c_str(), duppedSocket);
        if (!mUtils->addEnvironmentVar(envVar))
        {
            AI_LOG_ERROR("Failed to set env variable for container %s", mUtils->getContainerId().c_str());
            return false;
        }
    }

    // Client sockets
    for (size_t i = 0; i < mPluginData->client_sockets_len; i++)
    {
        char *clientSocketName = mPluginData->client_sockets[i]->name;

        int socket = createClientSocket();
        int duppedSocket = mUtils->addFileDescriptor(mName, socket);
        close(socket); //close original fd, it's already dupped and stored in startupState

        if (duppedSocket == -1)
        {
            AI_LOG_ERROR("Failed to duplicate server socket for container %s", mUtils->getContainerId().c_str());
            return false;
        }

        char envVar[256];
        snprintf(envVar, sizeof(envVar), "MCAST_CLIENT_SOCKET_%s_FD=%u", clientSocketName, duppedSocket);
        if (!mUtils->addEnvironmentVar(envVar))
        {
            AI_LOG_ERROR("Failed to set env variable for container %s", mUtils->getContainerId().c_str());
            return false;
        }
    }

    return true;
}

// End hook methods

// -----------------------------------------------------------------------------
/**
 *  @brief Creates socket and binds it to multicast \a ip and \a port
 *
 */
int MulticastSocketsPlugin::createServerSocket(in_addr_t ip, in_port_t port) const
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
        AI_LOG_SYS_WARN(errno, "Failed to set TTL of server socket - non critical");
    }

    return multicastSocket;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates client udp socket
 *
 */
int MulticastSocketsPlugin::createClientSocket() const
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

/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> MulticastSocketsPlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_multicast_sockets *pluginConfig = mContainerConfig->rdk_plugins->multicastsockets;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

// Begin private methods
