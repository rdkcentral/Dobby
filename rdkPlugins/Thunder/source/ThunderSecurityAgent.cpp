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

#include "ThunderSecurityAgent.h"

#include <Logging.h>

#include <poll.h>
#include <unistd.h>
#include <sys/un.h>
#include <sys/socket.h>

ThunderSecurityAgent::ThunderSecurityAgent(const std::string &socketAddr,
                                           const std::chrono::milliseconds &defaultTimeout)
    : mSocketPath(socketAddr),
      mTimeout(defaultTimeout),
      mSocket(-1)
{
}

ThunderSecurityAgent::~ThunderSecurityAgent()
{
    if ((mSocket >= 0) && (::close(mSocket) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close socket");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns true if we have an open connection to the security agent.
 *
 */
bool ThunderSecurityAgent::isOpen() const
{
    std::lock_guard<std::mutex> locker(mLock);
    return (mSocket >= 0);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Opens a connection to the security agent.  This must be called, and
 *  return true before calling getToken()
 *
 */
bool ThunderSecurityAgent::open()
{
    std::lock_guard<std::mutex> locker(mLock);
    return openNoLock();
}

bool ThunderSecurityAgent::openNoLock()
{
    AI_LOG_FN_ENTRY();

    if (mSocket >= 0)
    {
        AI_LOG_WARN("socket is already opened");
        AI_LOG_FN_EXIT();
        return true;
    }

    // create the socket
    mSocket = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (mSocket < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create socket");
    }
    else
    {
        struct sockaddr_un addr;
        bzero(&addr, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, mSocketPath.c_str(), sizeof(addr.sun_path) - 1);
        if (TEMP_FAILURE_RETRY(connect(mSocket,
                                       reinterpret_cast<struct sockaddr *>(&addr),
                                       sizeof(struct sockaddr_un))) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to connect to socket @ '%s'",
                             mSocketPath.c_str());
            ::close(mSocket);
            mSocket = -1;
        }
        else
        {
            AI_LOG_INFO("open IPC connection to socket @ '%s'", mSocketPath.c_str());
        }
    }

    AI_LOG_FN_EXIT();
    return (mSocket >= 0);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Closes the connection to the security agent.
 *
 */
void ThunderSecurityAgent::close()
{
    std::lock_guard<std::mutex> locker(mLock);
    closeNoLock();
}

void ThunderSecurityAgent::closeNoLock()
{
    if ((mSocket >= 0) && (::close(mSocket) != 0))
        AI_LOG_SYS_ERROR(errno, "failed to close socket");
    mSocket = -1;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to get a token from the security agent.
 *
 */
std::string ThunderSecurityAgent::getToken(const std::string &bearerUrl)
{
    // ensure this is serialised
    std::lock_guard<std::mutex> locker(mLock);
    // sanity check
    if (mSocket < 0)
    {
        AI_LOG_ERROR("not connect to the security agent");
        return std::string();
    }
    // the id for token data is 10, see IPCSecurityToken.h
    if (send(10, bearerUrl))
    {
        // get the reply
        uint16_t replyId;
        std::string replyData;
        if (recv(&replyId, &replyData))
        {
            if ((replyId == 10) || (replyData.length() >= 64))
            {
                return replyData;
            }
            else
            {
                AI_LOG_ERROR("invalid reply received from security agent "
                             "(id:%hu length:%zu)",
                             replyId, replyData.length());
            }
        }
    }
    // if we've dropped out here it means something failed, to avoid a situation
    // where the reply may just have been delayed - and if we keep the socket
    // open then the next read may read the wrong security token - then we
    // close and re-open the socket on any error
    closeNoLock();
    openNoLock();
    // return empty token
    return std::string();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sends an IPC message to the security agent.
 *
 *  @param[in]  id          The message id, for getting the token this is 10.
 *  @param[in]  data        The data to add to the message.
 *
 *  @return true on success, false on failure.
 */
bool ThunderSecurityAgent::send(uint16_t id, const std::string &data) const
{
    // sanity check
    if (mSocket < 0)
    {
        AI_LOG_ERROR("ipc socket is not connected");
        return false;
    }

    // construct the message
    const std::vector<uint8_t> message = constructMessage(id, data);

    // and send it
    ssize_t wr = TEMP_FAILURE_RETRY(::send(mSocket, message.data(), message.size(), 0));
    if (wr < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to send %zu bytes on ipc socket",
                         message.size());
        return false;
    }
    else if (wr != static_cast<ssize_t>(message.size()))
    {
        AI_LOG_ERROR("failed to send entire message, only %zd bytes of %zu sent",
                     wr, message.size());
        return false;
    }

    AI_LOG_DEBUG("sent IPC message with id %u and data length %zu bytes",
                id, data.size());
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates a basic WPEFramework IPC::Core message for standard buffer
 *  arguments.
 *
 *  @param[in]  id          The message id.
 *  @param[in]  data        The data to add to the message.
 *
 *  @return a vector containing the serialised message buffer to send.
 */
std::vector<uint8_t> ThunderSecurityAgent::constructMessage(uint16_t id,
                                                            const std::string &data)
{
    // construct the request, the IPC format is, you can work it out from the
    // IPCConnector.h header, the Serialize and Deserialize methods
    //   - length of data
    //   - data identifier
    //   - data
    std::vector<uint8_t> message;
    message.reserve(6 + data.size());
    uint32_t idLength = (id > 0x3fff) ? 3 : (id > 0x007f) ? 2
                                                          : 1;
    uint32_t length = data.size() + idLength;
    // the length and id fields are in little endian format and variable length,
    // bit 7 is used to determine if it is the last byte that makes up the field
    do
    {
        uint8_t value = length & 0x7f;
        length >>= 7;
        if (length != 0)
            value |= 0x80;
        message.push_back(value);
    } while (length != 0);
    // the id's are bit shift by one, presumably because the reply is the id
    // with the lsb bit set
    id <<= 1;
    do
    {
        uint8_t value = id & 0x7f;
        id >>= 7;
        if (id != 0)
            value |= 0x80;
        message.push_back(value);
    } while (id != 0);
    // then just append the data and return
    message.insert(message.end(), data.begin(), data.end());
    return message;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to read a message from the ipc socket.  It will wait for a
 *  max @a mTimeout for a message before giving up and returning @c false.
 *
 *  @param[out] id          Pointer to a value to store the id of the received
 *                          message in.
 *  @param[out] data        Pointer to a string to populate with the messsage
 *                          data.
 *
 *  @return true on success, false on failure.
 */
bool ThunderSecurityAgent::recv(uint16_t *id, std::string *data) const
{
    // sanity check
    if (mSocket < 0)
    {
        AI_LOG_ERROR("ipc socket is not connected");
        return false;
    }

    // wait for data
    struct pollfd fd;
    fd.fd = mSocket;
    fd.events = POLLIN;
    int ret = TEMP_FAILURE_RETRY(poll(&fd, 1, mTimeout.count()));
    if (ret < 0)
    {
        AI_LOG_SYS_ERROR(errno, "error occurred polling on socket");
    }
    else if (ret == 0)
    {
        AI_LOG_WARN("timed-out waiting for IPC reply");
        return false;
    }

    if (fd.events & (POLLERR | POLLHUP))
    {
        AI_LOG_WARN("ipc socket closed unexpectedly");
    }

    // now try and read the socket
    uint8_t buffer[2048];
    ssize_t rd = TEMP_FAILURE_RETRY(::recv(mSocket, buffer, sizeof(buffer), 0));
    if (rd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to read messge from ipc socket");
        return false;
    }
    else if (rd == 0)
    {
        AI_LOG_WARN("ipc socket closed unexpectedly");
        return false;
    }

    AI_LOG_DEBUG("received IPC message of size %zd", rd);
    // process the reply
    return deconstructMessage(buffer, rd, id, data);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Given a buffer containing a serialised IPC message it attempts to
 *  validate and extract the id and data.
 *
 *  @param[in]  buf         Pointer to the message data received.
 *  @param[in]  bufLength   The length of the message in buf.
 *  @param[out] id          Pointer to a value to store the id of the received
 *                          message in.
 *  @param[out] data        Pointer to a string to populate with the message
 *                          data.
 *
 *  @return true if the message was valid, otherwise false.
 */
bool ThunderSecurityAgent::deconstructMessage(const uint8_t *buf, size_t bufLength,
                                              uint16_t *id, std::string *data)
{
    if (bufLength < 2)
    {
        AI_LOG_ERROR("ipc message received to small (%zu bytes)", bufLength);
        return false;
    }
    size_t n, index = 0;
    // the length and id fields are in little endian format and variable length,
    // bit 7 is used to determine if it is the last byte that makes up the field
    uint32_t length = 0;
    n = 0;
    while (true)
    {
        const uint32_t value = buf[index++];
        length |= ((value & 0x7f) << (7 * n++));
        if ((value & 0x80) == 0)
        {
            break;
        }
        if (index >= bufLength)
        {
            AI_LOG_ERROR("invalid or truncated ipc message - length field");
            return false;
        }
    }
    // the length value is the length of the message minus the size of the
    // length field itself
    if ((length == 0) || ((length + index) != bufLength))
    {
        AI_LOG_ERROR("invalid or truncated ipc message - length mismatch");
        return false;
    }
    // the ident field is formatted the same as the length field
    uint32_t ident = 0;
    n = 0;
    while (true)
    {
        const uint32_t value = buf[index++];
        ident |= ((value & 0x7f) << (7 * n++));
        if ((value & 0x80) == 0)
        {
            break;
        }
        if (index >= bufLength)
        {
            AI_LOG_ERROR("invalid or truncated ipc message - id field");
            return false;
        }
    }
    // copy the id, the id's are bit shifted by 1 on the wire
    if (id)
    {
        *id = static_cast<uint16_t>((ident >> 1) & 0xffff);
    }
    // the rest of the message is the data
    if (data && (index < bufLength))
    {
        data->assign(buf + index, buf + bufLength);
    }
    AI_LOG_INFO("received IPC reply with id %u and data size %zu",
                (ident >> 1), bufLength - index);
    return true;
}
