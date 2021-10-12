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

#ifndef NETWORKINGHELPER_H
#define NETWORKINGHELPER_H

#include <DobbyNetworkingConstants.h>

#include <netinet/in.h>
#include <string>

enum class NetworkType { None, Nat, Open };
class NetworkingHelper
{
public:
    NetworkingHelper(bool ipv4Enabled, bool ipv6Enabled);
    ~NetworkingHelper();

public:
    bool storeContainerInterface(in_addr_t addr, const std::string &vethName);

    bool ipv4() const;
    in_addr_t ipv4Addr() const;
    std::string ipv4AddrStr() const;

    bool ipv6() const;
    struct in6_addr ipv6Addr() const;
    std::string ipv6AddrStr() const;

    std::string vethName() const;

public:
    static struct in6_addr in6addrCreate(const in_addr_t inaddr);

private:
    bool mIpv4Enabled;
    in_addr_t mIpv4Addr;
    std::string mIpv4AddrStr;

    bool mIpv6Enabled;
    struct in6_addr mIpv6Addr;
    std::string mIpv6AddrStr;

    std::string mVethName;
};

#endif // !defined(NETWORKINGHELPER_H)
