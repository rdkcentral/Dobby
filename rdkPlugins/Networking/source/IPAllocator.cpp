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

#include "IPAllocator.h"

#include <Logging.h>

#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>

IPAllocator::IPAllocator(const std::shared_ptr<DobbyRdkPluginUtils> &utils)
    : mUtils(utils),
      mBeginAddress(INADDR_BRIDGE + 1),
      mEndAddress(mBeginAddress + TOTAL_ADDRESS_POOL_SIZE)
{
    AI_LOG_FN_ENTRY();

    // Update internal state based on the disk store
    if (!getContainerIpsFromDisk())
    {
        AI_LOG_ERROR("Failed to initialise IP backing store");
    }

    AI_LOG_FN_EXIT();
}

IPAllocator::~IPAllocator()
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

/**
 * @brief Allocated an IP address for the currently running container with the
 * specified veth
 *
 * @param[in]  vethName     Name of the veth interface used by the container
 *
 * @return Allocated IP address. -1 on error
 */
in_addr_t IPAllocator::allocateIpAddress(const std::string &vethName)
{
    return allocateIpAddress(mUtils->getContainerId(), vethName);
}

/**
 * @brief Allocated an IP address with the specified veth
 *
 * @param[in]  containerId  Name of the container to associate the IP with
 * @param[in]  vethName     Name of the veth interface used by the container
 *
 * @return Allocated IP address. -1 on error
 */
in_addr_t IPAllocator::allocateIpAddress(const std::string &containerId, const std::string &vethName)
{
    AI_LOG_FN_ENTRY();

    // Attempt to find a free IP address
    in_addr_t ipAddress = 0;
    for (in_addr_t addr = mBeginAddress; addr < mEndAddress; addr++)
    {
        // If the IP address isn't allocated, we can use it
        if (std::find_if(mAllocatedIps.begin(), mAllocatedIps.end(), [addr](const ContainerNetworkInfo &info)
                         { return info.ipAddressRaw == addr; }) == mAllocatedIps.end())
        {
            ipAddress = addr;
            break;
        }
    }

    if (ipAddress == 0)
    {
        AI_LOG_ERROR_EXIT("IP Address pool exhausted - cannot allocate IP address for %s", containerId.c_str());
        return 0;
    }

    AI_LOG_DEBUG("Allocating %s IP address %s (%u)", containerId.c_str(), ipAddressToString(htonl(ipAddress)).c_str(), ipAddress);

    std::string addressFilePath = ADDRESS_FILE_DIR + containerId;
    const std::string fileContent(std::to_string(ipAddress) + "/" + vethName);

    // write address and veth name to a file
    if (!mUtils->writeTextFile(addressFilePath, fileContent, O_CREAT | O_TRUNC, 0644))
    {
        AI_LOG_ERROR_EXIT("failed to write ip address file - could not alloate IP for %s", containerId.c_str());
        return 0;
    }

    AI_LOG_FN_EXIT();
    return ipAddress;
}

/**
 * @brief Releases a previously allocated IP address back to the pool so it can
 * be re-used by other containers
 *
 * @param[in]   containerId     Name of the container to deallocate the IP for
 *
 * @return True on success, false on error
 */
bool IPAllocator::deallocateIpAddress(const std::string &containerId)
{
    AI_LOG_FN_ENTRY();

    // Nothing to do, already deallocated
    if (mAllocatedIps.empty())
    {
        return true;
    }

    // Remove file from disk store
    const std::string addressFilePath = ADDRESS_FILE_DIR + containerId;
    if (unlink(addressFilePath.c_str()) == -1)
    {
        AI_LOG_WARN("failed to remove address file for container %s at %s", containerId.c_str(), addressFilePath.c_str());
        return false;
    }

    // Remove from in-memory store
    ContainerNetworkInfo tmp;
    tmp.containerId = containerId;

    auto itr = std::find(mAllocatedIps.begin(), mAllocatedIps.end(), tmp);
    if (itr != mAllocatedIps.end())
    {
        AI_LOG_DEBUG("Deallocating IP address %s for %s", itr->ipAddress.c_str(), containerId.c_str());

        // Remove allocation
        mAllocatedIps.erase(itr);
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief Retrieves the networking information (veth, ip) for a given container
 *
 * @param[in]   containerId     Name of the container to retrieve the information about
 * @param[out]  networkInfo     Struct to store the container network info in
 *
 * @return True on success, false on failure
 */
bool IPAllocator::getContainerNetworkInfo(const std::string &containerId, ContainerNetworkInfo &networkInfo) const
{
    const std::string filePath = ADDRESS_FILE_DIR + containerId;
    return getNetworkInfo(filePath, networkInfo);
}

/**
 * @brief Retrieves the networking information (veth, ip) from a file from the store
 *
 * @param[in]   filePath        Path to the file to parse for information about the container network
 * @param[out]  networkInfo     Struct to store the container network info in
 *
 * @return True on success, false on failure
 */
bool IPAllocator::getNetworkInfo(const std::string &filePath, ContainerNetworkInfo &networkInfo) const
{
    AI_LOG_FN_ENTRY();

    // Parse the file
    const std::string addressFileStr = mUtils->readTextFile(filePath);
    if (addressFileStr.empty())
    {
        AI_LOG_ERROR_EXIT("failed to get IP address and veth name assigned to container from %s",
                          filePath.c_str());
        return false;
    }

    // file contains IP address in in_addr_t form
    const std::string ipStr = addressFileStr.substr(0, addressFileStr.find("/"));

    // check if string contains a veth name after the ip address
    if (addressFileStr.length() <= ipStr.length() + 1)
    {
        AI_LOG_ERROR_EXIT("failed to get veth name from %s", filePath.c_str());
        return false;
    }

    const in_addr_t ip = std::stoi(ipStr);
    char* filePathCopy = strdup(filePath.c_str());
    networkInfo.containerId = basename(filePathCopy);
    free(filePathCopy);
    networkInfo.ipAddressRaw = ip;

    // Convert the in_addr_t value to a human readable value (e.g. 100.64.11.x)
    networkInfo.ipAddress = ipAddressToString(htonl(ip));
    networkInfo.vethName = addressFileStr.substr(ipStr.length() + 1, addressFileStr.length());

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief Synchronise the in-memory pool of allocated IPs with the disk store
 *
 * @return True on success
 */
bool IPAllocator::getContainerIpsFromDisk()
{
    AI_LOG_FN_ENTRY();

    mAllocatedIps.clear();

    // Dir doesn't exist, no containers have run yet
    DIR *dir = opendir(ADDRESS_FILE_DIR);
    if (!dir)
    {
        if (errno == ENOENT)
        {
            // Create directory we will store IPs in
            if (!mUtils->mkdirRecursive(ADDRESS_FILE_DIR, 0644))
            {
                AI_LOG_ERROR_EXIT("Failed to create dir @ '%s'", ADDRESS_FILE_DIR);
                return false;
            }

            AI_LOG_FN_EXIT();
            return true;
        }
        else
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "Failed to open directory @ '%s'", ADDRESS_FILE_DIR);
            return false;
        }
    }

    // Work out what IPs are currently allocated to what containers
    // Each container gets a file in the store directory
    // Filename = container ID
    // Contents = ipaddress/veth
    struct dirent *entry = nullptr;
    char pathBuf[PATH_MAX + 1];
    bzero(pathBuf, sizeof(pathBuf));

    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_type == DT_REG && entry->d_name[0] != '.')
        {
            std::string fullPath = std::string(ADDRESS_FILE_DIR) + entry->d_name;

            ContainerNetworkInfo networkInfo;
            if (!getNetworkInfo(fullPath, networkInfo))
            {
                AI_LOG_ERROR("Failed to parse network info from file %s", fullPath.c_str());
                continue;
            }
            mAllocatedIps.emplace_back(networkInfo);
        }
    }

    closedir(dir);

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief Convert an string to an IP address. Note - doesn't do any
 * byte-order modifications
 *
 * @param[in]   ipAddr      IP Address to convert
 *
 * @return IP address integer
 */
in_addr_t IPAllocator::stringToIpAddress(const std::string &ipAddr)
{
    in_addr_t tmp;

    inet_pton(AF_INET, ipAddr.c_str(), &tmp);

    AI_LOG_DEBUG("Converted IP %s -> %u", ipAddr.c_str(), tmp);
    return tmp;
}

/**
 * @brief Convert an IP address to string. Note - doesn't do any
 * byte-order modifications
 *
 * @param[in]   ipAddress      IP Address to convert
 *
 * @return IP address string
 */
std::string IPAllocator::ipAddressToString(const in_addr_t &ipAddress)
{
    char str[INET_ADDRSTRLEN];
    in_addr_t tmp = ipAddress;

    inet_ntop(AF_INET, &tmp, str, sizeof(str));

    AI_LOG_DEBUG("Converted IP %u -> %s", ipAddress, str);
    return std::string(str);
}
