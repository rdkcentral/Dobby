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

#ifndef DOBBYSTATE_H
#define DOBBYSTATE_H

#include <mutex>
#include <queue>
#include <map>
#include <netinet/in.h>

// -----------------------------------------------------------------------------
/**
 *  @class DobbyState
 *
 *  @brief Manages and stores the daemon's states and configurations. Used by
 *  plugins to maintain state between hook points.
 *
 *  nb: this is currently only used by Networking plugin, but is intended to be
 *  open for use by other plugins in the future, thus the generic class name
 *  'DobbyState'.
 */
class DobbyState
{
public:
    DobbyState();
    ~DobbyState();

public:
    uint32_t getBridgeConnections();
    const in_addr_t getIpAddress(const std::string &vethName);
    bool freeIpAddress(in_addr_t address);

private:
    std::queue<in_addr_t> mAddressPool;
    std::map<in_addr_t, std::string> mRegisteredAddresses;
    mutable std::mutex mLock;
};


#endif // !defined(DOBBYSTATE_H)
