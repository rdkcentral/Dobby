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

#ifndef NETWORKINGPLUGINCOMMON_H
#define NETWORKINGPLUGINCOMMON_H

#include <netinet/in.h>

// path to file in container rootfs where the container's ip address is stored
#define ADDRESS_FILE_PATH       "/dobbyaddress"

#define BRIDGE_NAME             "dobby0"

#if defined(DEV_VM)
#define PEER_NAME               "enp0s3"
#else
#define PEER_NAME               "eth0"
#endif

// creates an in_addr_t type from ip address
#define INADDR_CREATE(a, b, c, d) \
    ( ((((in_addr_t)(a)) << 24) & 0xff000000) | \
      ((((in_addr_t)(b)) << 16) & 0x00ff0000) | \
      ((((in_addr_t)(c)) <<  8) & 0x0000ff00) | \
      ((((in_addr_t)(d)) <<  0) & 0x000000ff) )

// commonly used ip addresses created as in_addr_t type
#define INADDR_BRIDGE                   INADDR_CREATE( 100,  64,  11,   1 )
#define INADDR_BRIDGE_NETMASK           INADDR_CREATE( 255, 255, 255,   0 )
#define INADDR_RANGE_START              INADDR_CREATE( 100,  64,  11,   2 )
#define INADDR_RANGE_END                INADDR_CREATE( 100,  64,  11, 250 )

// commonly used ip address string literals for iptables rules
// NB: the bridge addresses must work with the above INADDR_* addresses
#define BRIDGE_ADDRESS_RANGE    "100.64.11.0"
#define BRIDGE_ADDRESS          "100.64.11.1"
#define LOCALHOST               "127.0.0.1"

enum class NetworkType { None, Nat, Open };

#endif // !defined(NETWORKINGPLUGINCOMMON_H)
