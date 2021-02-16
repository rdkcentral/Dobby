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

#include <netinet/in.h>
#include <string>

#define BRIDGE_NAME             "dobby0"

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

// -----------------------------------------------------------------------------
// IPv4 address macros

// creates an in_addr_t type from ip address
#define INADDR_CREATE(a, b, c, d) \
    ( ((((in_addr_t)(a)) << 24) & 0xff000000) | \
      ((((in_addr_t)(b)) << 16) & 0x00ff0000) | \
      ((((in_addr_t)(c)) <<  8) & 0x0000ff00) | \
      ((((in_addr_t)(d)) <<  0) & 0x000000ff) )

// commonly used ip addresses created as in_addr_t type
#define INADDR_BRIDGE                   INADDR_CREATE( 100,  64,  11,   1 )
#define INADDR_BRIDGE_NETMASK           INADDR_CREATE( 255, 255, 255,   0 )
#define INADDR_LO                       INADDR_CREATE( 127,   0,   0,   1 )
#define INADDR_LO_NETMASK               INADDR_CREATE( 255,   0,   0,   0 )

// commonly used ip address string literals for iptables rules
// NB: the bridge addresses must work with the above INADDR_* addresses
#define BRIDGE_ADDRESS_RANGE          "100.64.11.0"
#define BRIDGE_ADDRESS                "100.64.11.1"
#define LOCALHOST                     "127.0.0.1"


// -----------------------------------------------------------------------------
// IPv6 address helpers

// 2080:d0bb:1e::
static const struct in6_addr IN6ADDR_BASE = {{
    0x20, 0x80, 0xd0, 0xbb,
    0x00, 0x1e, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
}};

// 0000:0000:0000:0000:0000:0000:0000:0000
static const struct in6_addr IN6ADDR_ANY = {{
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
}};

#define BRIDGE_ADDRESS_RANGE_IPV6     "2080:d0bb:1e::6440:b00"
#define BRIDGE_ADDRESS_IPV6           "2080:d0bb:1e::6440:b01"
#define LOCALHOST_IPV6                "::1"

#endif // !defined(NETWORKINGHELPER_H)
