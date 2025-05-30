/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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

#include "TapInterface.h"
#include "Netlink.h"
#include <Logging.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <net/if.h>
#include <linux/if_tun.h>

#define TAP_NAME "dobby_tap0"
#define TUNDEV "/dev/net/tun"

ifreq createInterfaceStruct()
{
    struct ifreq ifr;
    bzero(&ifr, sizeof(ifr));

    // set the flags
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_ONE_QUEUE;
    strncpy(ifr.ifr_name, TAP_NAME, IFNAMSIZ);
    return ifr;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns true if the platform has the TUN/TAP device driver and therefore
 *  can create tap devices
 *
 *  @return true if supported.
 */
bool TapInterface::platformSupportsTapInterface()
{
    struct stat buf;
    return stat(TUNDEV, &buf) == 0;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates the Dobby tap device.
 *
 *  @remark Based on implementation in iproute2 source code (iptuntap.c)
 *
 *  @return true on success, false on failure.
 */
bool TapInterface::createTapInterface(const std::shared_ptr<Netlink> &netlink)
{
    AI_LOG_FN_ENTRY();

    // Does the interface already exist?
    if (netlink->ifaceExists(std::string(TAP_NAME)))
    {
        AI_LOG_INFO("Tap device already exists");
        return true;
    }

    int fd = open(TUNDEV, O_RDWR);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open '/dev/net/tun'");
        return false;
    }

    struct ifreq ifr = createInterfaceStruct();

    // ask for kernel for new device
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) != 0)
    {
        close(fd);
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create tap device '%s'", TAP_NAME);
        return false;
    }

    // Without TUNSETPERSIST, the tap device is destroyed when the fd is closed
    // (i.e. when the plugin finishes)
    if (ioctl(fd, TUNSETPERSIST, 1) != 0)
    {
        close(fd);
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to set TUNSETPERSIST");
        return false;
    }

    valid = true;
    close(fd);

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Destroys the Dobby tap device if it exists
 *
 *  @return true on success, false on failure.
 */
bool TapInterface::destroyTapInterface(const std::shared_ptr<Netlink> &netlink)
{
    AI_LOG_FN_ENTRY();

    // Does the interface already exist? If not, then don't do anything
    if (!netlink->ifaceExists(std::string(TAP_NAME)))
    {
        AI_LOG_WARN("Tap device %s doesn't exist - cannot destroy", TAP_NAME);
        return true;
    }

    int fd = open(TUNDEV, O_RDWR);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open '/dev/net/tun'");
        return false;
    }

    struct ifreq ifr = createInterfaceStruct();

    // Since the tap device exists, this will reference the existing device,
    // not create a new one
    if (ioctl(fd, TUNSETIFF, (void *)&ifr) != 0)
    {
        close(fd);
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open existing device '%s'", TAP_NAME);
        return false;
    }

    // Set persist to false. Now when we close the fd, the interface will
    // be deleted
    if (ioctl(fd, TUNSETPERSIST, 0) != 0)
    {
        close(fd);
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to reset TUNSETPERSIST");
        return false;
    }

    valid = false;
    close(fd);

    AI_LOG_FN_EXIT();
    return true;
}

bool TapInterface::isValid()
{
    return valid;
}

const std::string TapInterface::name()
{
    return std::string(TAP_NAME);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Brings an interface up
 *
 *  @param[in]  netlink     Instance of the Netlink class.
 *
 *  @return true on success, false on failure.
 */
bool TapInterface::up(const std::shared_ptr<Netlink> &netlink)
{
    AI_LOG_FN_ENTRY();

    if (!isValid())
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_FN_EXIT();
    return netlink->ifaceUp(TAP_NAME);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Takes an interface down
 *
 *  @param[in]  netlink     Instance of the Netlink class.
 *
 *  @return true on success, false on failure.
 */
bool TapInterface::down(const std::shared_ptr<Netlink> &netlink)
{
    AI_LOG_FN_ENTRY();

    if (!isValid())
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    AI_LOG_FN_EXIT();
    return netlink->ifaceDown(TAP_NAME);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets the MAC address of the tap device
 *
 *  @param[in]  netlink     Instance of the Netlink class.
 *
 *  @return true on success, false on failure.
 */
std::array<uint8_t, 6> TapInterface::macAddress(const std::shared_ptr<Netlink> &netlink)
{
    return netlink->getIfaceMAC(TAP_NAME);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the MAC address of the tap device
 *
 *  @param[in]  netlink     Instance of the Netlink class.
 *  @param[in]  address     MAC address to be set
 *
 *  @return true on success, false on failure.
 */
bool TapInterface::setMACAddress(const std::shared_ptr<Netlink> &netlink, const std::array<uint8_t, 6> &address)
{
    return netlink->setIfaceMAC(TAP_NAME, address);
}
