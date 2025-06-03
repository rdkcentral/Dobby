/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2022 Sky UK
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

#include "DobbyLogRelay.h"

#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/un.h>
#include <string.h>

#include <Logging.h>

/**
 * @brief Create relay between two UNIX datagram sockets
 *
 * All messages sent to the source socket are forwarded to the
 * destination socket. Used to relay messages to the host syslog/journald
 * and ensure the messages are tagged with the dobby daemon PID for the
 * RDK log collection scripts
 */
DobbyLogRelay::DobbyLogRelay(const std::string &sourceSocketPath,
                             const std::string &destinationSocketPath)
    : mSourceSocketPath(sourceSocketPath),
      mDestinationSocketPath(destinationSocketPath),
      mBuf{}
{
    AI_LOG_FN_ENTRY();

    // Create the socket we're listening on
    mSourceSocketFd = createDgramSocket(mSourceSocketPath);
    if (mSourceSocketFd < 0)
    {
        AI_LOG_ERROR("Failed to create socket at %s", sourceSocketPath.c_str());
    }

    // If the socket we're forwarding to exists
    if (access(mDestinationSocketPath.c_str(), F_OK) < 0)
    {
        AI_LOG_ERROR("Socket %s does not exist, cannot create relay", mDestinationSocketPath.c_str());
    }
    else
    {
        // Connect to the socket we will relay messages to
        mDestinationSocketFd = socket(AF_UNIX, SOCK_DGRAM, 0);

        mDestinationSocketAddress = {};
        mDestinationSocketAddress.sun_family = AF_UNIX;
        strcpy(mDestinationSocketAddress.sun_path, mDestinationSocketPath.c_str());

        AI_LOG_INFO("Created log relay from %s to %s", mSourceSocketPath.c_str(), mDestinationSocketPath.c_str());
    }

    AI_LOG_FN_EXIT();
}

DobbyLogRelay::~DobbyLogRelay()
{
    AI_LOG_FN_ENTRY();

    // Close and remove the source socket we created
    if (close(mSourceSocketFd) < 0)
    {
        AI_LOG_SYS_WARN(errno, "Failed to close socket %s", mSourceSocketPath.c_str());
    }

    if (unlink(mSourceSocketPath.c_str()))
    {
        AI_LOG_SYS_ERROR(errno, "Failed to remove socket at '%s'", mSourceSocketPath.c_str());
    }

    // Close the destination socket (but don't remove it)
    if (close(mDestinationSocketFd) < 0)
    {
        AI_LOG_SYS_WARN(errno, "Failed to close socket %s", mDestinationSocketPath.c_str());
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Adds the log relay to a given poll loop so that the process() method is called
 * when the source socket receives data
 *
 * @param[in]   pollLoop    The poll loop to add ourselves to
 */
void DobbyLogRelay::addToPollLoop(const std::shared_ptr<AICommon::IPollLoop> &pollLoop)
{
    pollLoop->addSource(shared_from_this(), mSourceSocketFd, EPOLLIN);
}


/**
 * @brief Removes the log relay to a given poll loop
 *
 * @param[in]   pollLoop    The poll loop to add ourselves to
 */
void DobbyLogRelay::removeFromPollLoop(const std::shared_ptr<AICommon::IPollLoop> &pollLoop)
{
    pollLoop->delSource(shared_from_this(), mSourceSocketFd);
}

/**
 * @brief Called on the poll loop. Forwards the data from the source to the destination
 * socket
 *
 * @param[in]   pollLoop    The pollLoop instance that the process method was called from
 * @param[in]   events      The epoll event that occured
 */
void DobbyLogRelay::process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, epoll_event event)
{
    // Got some data
    if (event.events & EPOLLIN)
    {
        ssize_t ret;
        memset(mBuf, 0, sizeof(mBuf));

        struct sockaddr_storage src_addr;

        struct iovec iov[1];
        iov[0].iov_base=mBuf;
        iov[0].iov_len=sizeof(mBuf);

        struct msghdr message{};
        message.msg_name=&src_addr;
        message.msg_namelen=sizeof(src_addr);
        message.msg_iov=iov;
        message.msg_iovlen=1;
        message.msg_control=0;
        message.msg_controllen=0;

        // This is effectively a UDP message, so we have to read the whole datagram in one chunk
        // The first byte returned by read will always be the start of the datagram. We've set
        // a relatively large buffer size (32K) to try and avoid truncation
        ret = TEMP_FAILURE_RETRY(recvmsg(mSourceSocketFd, &message, 0));
        if (ret < 0)
        {
            AI_LOG_SYS_ERROR(errno, "Errror reading from socket @ %s", mSourceSocketPath.c_str());
            return;
        }
        else if (message.msg_flags & MSG_TRUNC)
        {
            // Log a warning if we know message data has been truncated to avoid weird surprises
            // TODO:: We could use MSG_PEEK to work out the size of the datagram and realloc a big enough buffer
            // (with some kind of hard limit to prevent resource exhaustion), but that will also double the number
            // of syscalls we have to make.
            AI_LOG_WARN("Message received on %s has been truncated", mSourceSocketPath.c_str());
        }

        if (sendto(mDestinationSocketFd, mBuf, ret, 0, (struct sockaddr *)&mDestinationSocketAddress, sizeof(mDestinationSocketAddress)) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to send message to socket @ '%s'", mDestinationSocketAddress.sun_path);
        }
    }

    return;
}

/**
 * @brief Create a SOCK_DGRAM AF_UNIX socket at the given path. Removes the socket
 * at the given path if it exists.
 *
 * @param[in]   path    The path the socket should be created at
 */
int DobbyLogRelay::createDgramSocket(const std::string &path)
{
    AI_LOG_FN_ENTRY();

    // Remove the socket if it exists already...
    if (unlink(path.c_str()) > 0)
    {
        AI_LOG_DEBUG("Removed existing socket @ '%s'", path.c_str());
    }

    // Create a socket
    int sockFd;
    sockFd = socket(AF_UNIX, SOCK_DGRAM, 0);

    struct sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path.c_str());

    if (bind(sockFd, (const struct sockaddr *)&address, sizeof(address)) < 0)
    {
        close(sockFd);
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to bind socket @ '%s'", address.sun_path);
        return -1;
    }

    // Make sure socket can be accessed inside container
    if (chmod(path.c_str(), 0666) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to set permissions on socket @ '%s'", address.sun_path);
    }

    AI_LOG_FN_EXIT();
    return sockFd;
}
