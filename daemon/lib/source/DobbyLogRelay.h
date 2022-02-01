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

#pragma once

#include "IPollLoop.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <mutex>

// Need a large buffer to store the entire datagram
#define BUFFER_SIZE (32 * 1024)

class DobbyLogRelay : public AICommon::IPollSource,
                      public std::enable_shared_from_this<DobbyLogRelay>
{
public:
    DobbyLogRelay(const std::string &sourceSocketPath,
                  const std::string &destinationSocketPath);
    ~DobbyLogRelay();

public:
    void process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events) override;
    void addToPollLoop(const std::shared_ptr<AICommon::IPollLoop> &pollLoop);

private:
    int createDgramSocket(const std::string& path);

private:
    const std::string mSourceSocketPath;
    const std::string mDestinationSocketPath;

    int mSourceSocketFd;
    int mDestinationSocketFd;

    sockaddr_un mDestinationSocketAddress;

    char mBuf[BUFFER_SIZE];
    std::mutex mLock;
};