#include "IPAllocator.h"

#include <Logging.h>

#include <sys/stat.h>
#include <dirent.h>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>

IPAllocator::IPAllocator(const std::shared_ptr<DobbyRdkPluginUtils> &utils)
    : mUtils(utils)
{
    AI_LOG_FN_ENTRY();

    // Update internal state based on the disk store
    if (!updateFromStore())
    {
        AI_LOG_ERROR("Failed to initialise IP backing store");
    }

    // Start from xxx.xxx.xxx.2 to leave xxx.xxx.xxx.1 open for bridge device
    in_addr_t addrBegin = INADDR_BRIDGE + 1;
    in_addr_t addrEnd = addrBegin + TOTAL_ADDRESS_POOL_SIZE;

    // Populate the pool of available addresses

    // Nothing allocated already, create full pool
    if (mAllocatedIps.size() == 0)
    {
        for (in_addr_t addr = addrBegin; addr < addrEnd; addr++)
        {
            mUnallocatedIps.push(addr);
        }
    }
    else
    {
        for (in_addr_t addr = addrBegin; addr < addrEnd; addr++)
        {
            if (std::find_if(mAllocatedIps.begin(), mAllocatedIps.end(), [addr](const ContainerNetworkInfo &info)
                             { return info.ipAddressRaw == addr; }) == mAllocatedIps.end())
            {
                mUnallocatedIps.push(addr);
            }
        }
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

    in_addr_t ipAddress = mUnallocatedIps.front();
    mUnallocatedIps.pop();

    AI_LOG_INFO("Allocating %s IP address %s (%u)", containerId.c_str(), ipAddressToString(htonl(ipAddress)).c_str(), ipAddress);

    std::string addressFilePath = ADDRESS_FILE_DIR + containerId;
    const std::string fileContent(std::to_string(ipAddress) + "/" + vethName);

    // write address and veth name to a file
    if (!mUtils->writeTextFile(addressFilePath, fileContent, O_CREAT | O_TRUNC, 0644))
    {
        AI_LOG_ERROR_EXIT("failed to write ip address file");
        return -1;
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
        AI_LOG_INFO("Deallocating IP address %s for %s", itr->ipAddress.c_str(), containerId.c_str());

        // Remove allocation and add back to the unallocated queue for re-use
        mAllocatedIps.erase(itr);
        mUnallocatedIps.push(itr->ipAddressRaw);
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
    const std::string filePath = ADDRESS_FILE_DIR + mUtils->getContainerId();
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

    const std::string ipStr = addressFileStr.substr(0, addressFileStr.find("/"));
    const in_addr_t ip = std::stoi(ipStr);

    // check if string contains a veth name after the ip address
    if (addressFileStr.length() <= ipStr.length() + 1)
    {
        AI_LOG_ERROR_EXIT("failed to get veth name from %s", filePath.c_str());
        return false;
    }

    networkInfo.containerId = basename(filePath.c_str());
    networkInfo.ipAddressRaw = ip;
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
bool IPAllocator::updateFromStore()
{
    AI_LOG_FN_ENTRY();

    // Dir doesn't exist, no containers have run yet
    struct stat buf;
    if (stat(ADDRESS_FILE_DIR, &buf) != 0)
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

    // Work out what IPs are currently allocated to what containers
    DIR *dir = opendir(ADDRESS_FILE_DIR);
    if (!dir)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to open directory @ '%s", ADDRESS_FILE_DIR);
        closedir(dir);
        return -1;
    }

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