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

#include <Logging.h>

DobbyLogRelay::DobbyLogRelay(const std::string &sourceSocketPath,
                             const std::string &destinationSocketPath)
    : mSourceSocketPath(sourceSocketPath),
      mDestinationSocketPath(destinationSocketPath),
      mBuf{}
{
    AI_LOG_FN_ENTRY();

    mSourceSocketFd = createDgramSocket(mSourceSocketPath);
    if (mSourceSocketFd < 0)
    {
        AI_LOG_ERROR("Failed to create socket at %s", sourceSocketPath.c_str());
    }

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

int DobbyLogRelay::getSourceFd()
{
    return mSourceSocketFd;
}

void DobbyLogRelay::process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events)
{
    if (events & EPOLLIN)
    {
        ssize_t ret;
        memset(mBuf, 0, sizeof(mBuf));

        ret = TEMP_FAILURE_RETRY(read(mSourceSocketFd, mBuf, sizeof(mBuf)));
        if (ret < 0)
        {
            AI_LOG_SYS_ERROR(errno, "Errror during read");
        }

        if (sendto(mDestinationSocketFd, mBuf, sizeof(mBuf), 0, (struct sockaddr *)&mDestinationSocketAddress, sizeof(mDestinationSocketAddress)) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "sento failed");
        }
    }

    return;
}

int DobbyLogRelay::createDgramSocket(const std::string &path)
{
    AI_LOG_FN_ENTRY();

    // Remove the socket if it exists already...
    unlink(path.c_str());

    // Create a socket
    int sockFd;
    sockFd = socket(AF_UNIX, SOCK_DGRAM, 0);

    struct sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path.c_str());

    if (bind(sockFd, (const struct sockaddr *)&address, sizeof(address)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to bind socket @ '%s'", address.sun_path);
        return -1;
    }

    chmod(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);

    AI_LOG_FN_EXIT();
    return sockFd;
}