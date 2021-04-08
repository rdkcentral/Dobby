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

#include "NetworkingHelper.h"

#include <Logging.h>

#include <arpa/inet.h>


NetworkingHelper::NetworkingHelper(bool ipv4Enabled, bool ipv6Enabled)
    : mIpv4Enabled(ipv4Enabled),
    mIpv4Addr(INADDR_CREATE(0,0,0,0)),
    mIpv4AddrStr(std::string()),
    mIpv6Enabled(ipv6Enabled),
    mIpv6Addr(IN6ADDR_BASE),
    mIpv6AddrStr(std::string())
{
    if (!mIpv4Enabled && !mIpv6Enabled)
    {
        AI_LOG_WARN("both IPv4 and IPv6 mode are disabled in config, defaulting to IPv4 only");
        mIpv4Enabled = true;
    }
}

NetworkingHelper::~NetworkingHelper()
{
}

// -----------------------------------------------------------------------------
/* NetworkingHelper getters
 */

bool NetworkingHelper::ipv4() const
{
    return mIpv4Enabled;
}

in_addr_t NetworkingHelper::ipv4Addr() const
{
    return mIpv4Addr;
}

std::string NetworkingHelper::ipv4AddrStr() const
{
    return mIpv4AddrStr;
}

bool NetworkingHelper::ipv6() const
{
    return mIpv6Enabled;
}

struct in6_addr NetworkingHelper::ipv6Addr() const
{
    return mIpv6Addr;
}

std::string NetworkingHelper::ipv6AddrStr() const
{
    return mIpv6AddrStr;
}

std::string NetworkingHelper::vethName() const
{
    return mVethName;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs addresses for the container based on input address. Also
 *  stores the veth device used for the container.
 *
 *  @param[in]  addr        IPv4 address to construct addresses from.
 *  @param[in]  vethName    Name of the veth device reserved for the container.
 *
 *  @return true if successful, otherwise false
 */
bool NetworkingHelper::storeContainerInterface(in_addr_t addr, const std::string &vethName)
{
    // store IPv4 address in binary form
    mIpv4Addr = addr;

    // construct IPv4 address string from binary form
    char ipv4AddressStr[INET_ADDRSTRLEN];
    struct in_addr ipAddress_ = { htonl(mIpv4Addr) };
    if (inet_ntop(AF_INET, &ipAddress_, ipv4AddressStr, INET_ADDRSTRLEN) == nullptr)
    {
        AI_LOG_SYS_ERROR(errno, "failed to convert in_addr to string");
        return false;
    }
    mIpv4AddrStr = ipv4AddressStr;

    // construct IPv6 address from IPv4 address
    mIpv6Addr = in6addrCreate(addr);

    // construct IPv6 address string from binary form
    char ipv6AddressStr[INET6_ADDRSTRLEN];
    if (inet_ntop(AF_INET6, &mIpv6Addr, ipv6AddressStr, INET6_ADDRSTRLEN) == nullptr)
    {
        AI_LOG_SYS_ERROR(errno, "failed to convert in6_addr to string");
        return false;
    }
    mIpv6AddrStr = ipv6AddressStr;

    mVethName = vethName;

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs an IPv6 address to be used by Dobby
 *
 *  Takes an IPv4 address type and merges it into IN6ADDR_BASE to make a unique
 *  IPv6 address.
 *
 *  The final address will be 2080:d0bb:1e::nnnn:nnnn, where the "n"s will be
 *  replaced by the IPv4 address binary.
 *
 *  @param[in]  inaddr     IPv4 address to merge to the base address
 *
 *  @return IPv6 address
 */
struct in6_addr NetworkingHelper::in6addrCreate(const in_addr_t inaddr)
{
    const in_addr_t inaddrHost = inaddr;
    struct in6_addr address = IN6ADDR_BASE;

    address.s6_addr[12] = (inaddrHost >> 24) & 0x000000ff;
    address.s6_addr[13] = (inaddrHost >> 16) & 0x000000ff;
    address.s6_addr[14] = (inaddrHost >>  8) & 0x000000ff;
    address.s6_addr[15] = (inaddrHost >>  0) & 0x000000ff;

    return address;
}
