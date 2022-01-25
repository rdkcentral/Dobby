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
    bool updateFromStore();
    bool getNetworkInfo(const std::string &filePath, ContainerNetworkInfo &networkInfo) const;

private:
    std::queue<in_addr_t> mUnallocatedIps;
    std::vector<ContainerNetworkInfo> mAllocatedIps;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
};

#endif