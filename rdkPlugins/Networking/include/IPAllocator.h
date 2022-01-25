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

#ifndef IPALLOCATOR_H
#define IPALLOCATOR_H

#include <DobbyRdkPluginUtils.h>
#include "NetworkingPluginCommon.h"

#include <memory>
#include <arpa/inet.h>
#include <queue>

#define TOTAL_ADDRESS_POOL_SIZE 250

class DobbyRdkPluginUtils;

class IPAllocator
{
public:
    IPAllocator(const std::shared_ptr<DobbyRdkPluginUtils> &utils);
    ~IPAllocator();

public:
    in_addr_t allocateIpAddress(const std::string &vethName);
    in_addr_t allocateIpAddress(const std::string &containerId, const std::string &vethName);
    bool deallocateIpAddress(const std::string &containerId);
    bool getContainerNetworkInfo(const std::string &containerId, ContainerNetworkInfo &networkInfo) const;

public:
    static in_addr_t stringToIpAddress(const std::string &ipAddressStr);
    static std::string ipAddressToString(const in_addr_t &ipAddress);

private:
    bool getContainerIpsFromDisk();
    bool getNetworkInfo(const std::string &filePath, ContainerNetworkInfo &networkInfo) const;

private:
    std::queue<in_addr_t> mUnallocatedIps;
    std::vector<ContainerNetworkInfo> mAllocatedIps;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
};

#endif