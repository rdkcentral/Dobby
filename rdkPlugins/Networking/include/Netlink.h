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
/*
 * File:   Netlink.h
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef NETLINK_H
#define NETLINK_H

#include <string>
#include <mutex>

#include <arpa/inet.h>

struct nl_sock;

class NlLink;


// -----------------------------------------------------------------------------
/**
 *  @class Netlink
 *  @brief Basic wrapper around the libnl netlink library
 *
 *  There is only expected to be one of these objects (i.e. a shared_ptr is
 *  passed around).  The object represents a single netlink socket.
 *
 *  At construction time a new netlink socket is opened, on destruction it is
 *  closed.
 *
 */
class Netlink
{
public:
    Netlink();
    ~Netlink();

public:
    bool isValid() const;

public:
    bool ifaceUp(const std::string& ifaceName);
    bool ifaceDown(const std::string& ifaceName);

    bool ifaceIsUp(const std::string& ifaceName) const;

    bool setIfaceAddress(const std::string& ifaceName,
                         in_addr_t address, in_addr_t netmask);
    bool setIfaceForwarding(const std::string& ifaceName,
                            bool enable);

    bool setIfaceRouteLocalNet(const std::string& ifaceName,
                               bool enable);

public:
    bool createBridge(const std::string& bridgeName);
    bool destroyBridge(const std::string& bridgeName);

    bool addIfaceToBridge(const std::string& bridgeName,
                          const std::string& ifaceName);
    bool delIfaceFromBridge(const std::string& bridgeName,
                            const std::string& ifaceName);

public:
    std::string createVeth(const std::string& peerVethName,
                           pid_t peerPid);
    bool checkVeth(const std::string& vethName);


public:
    bool addRoute(const std::string& iface, in_addr_t destination,
                  in_addr_t gateway, in_addr_t netmask);

private:
    bool applyChangesToLink(const std::string& ifaceName,
                            const NlLink& changes);

    bool setLinkAddress(const NlLink& link,
                        in_addr_t address, in_addr_t netmask);

    bool setIfaceConfig(const std::string& ifaceName, unsigned int configId,
                        uint32_t value);


    std::string getAvailableVethName(const int startIndex) const;

private:
    struct nl_sock* mSocket;
    int mSysClassNetDirFd;
    mutable std::mutex mLock;
};


#endif // !defined(NETLINK_H)
