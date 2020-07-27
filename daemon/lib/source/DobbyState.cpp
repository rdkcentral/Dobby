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

#include "DobbyState.h"

#include <Logging.h>

#include <fcntl.h>
#include <unistd.h>


#define TOTAL_ADDRESS_POOL_SIZE 250

DobbyState::DobbyState(const std::shared_ptr<const IDobbySettings> &settings)
    : mSettings(settings)
{
    AI_LOG_FN_ENTRY();

    // start from xxx.xxx.xxx.2 to leave xxx.xxx.xxx.1 open for bridge device
    in_addr_t addrBegin = mSettings->addressRange() + 2;
    in_addr_t addrEnd = addrBegin + TOTAL_ADDRESS_POOL_SIZE;

    // populate the pool of available addresses
    for (in_addr_t addr = addrBegin; addr < addrEnd; addr++)
    {
        mAddressPool.push(addr);
    }

    AI_LOG_FN_EXIT();
}

DobbyState::~DobbyState()
{
}


// -----------------------------------------------------------------------------
/**
 *  @brief Gets the number of veth interfaces connected through bridge
 *
 *  @return number of interfaces connected
 */
uint32_t DobbyState::getBridgeConnections()
{
    return TOTAL_ADDRESS_POOL_SIZE - mAddressPool.size();
}


// -----------------------------------------------------------------------------
/**
 *  @brief Picks the next available ip address from the pool of addresses
 *
 *  @return returns free ip address from the pool, 0 if none available
 */
const in_addr_t DobbyState::getIpAddress(const std::string &vethName)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    if (mAddressPool.empty())
    {
        AI_LOG_ERROR_EXIT("no available ip addresses");
        return 0;
    }

    // get a free ip address from the pool
    const in_addr_t address = mAddressPool.front();

    // register ip address to veth
    mRegisteredAddresses.emplace(address, vethName);

    // remove the address from the pool
    mAddressPool.pop();

    AI_LOG_FN_EXIT();
    return address;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Adds the address back to the pool of available addresses, freeing it
 *  for use by other containers.
 *
 *  @param[in]  address     address to return to the pool of available addresses
 *
 *  @return true on success, false on failure.
 */
bool DobbyState::freeIpAddress(in_addr_t address)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // remove registered address<->veth pair
    mRegisteredAddresses.erase(address);

    // free ip address back into the address pool
    mAddressPool.push(address);

    AI_LOG_FN_EXIT();
    return true;
}
