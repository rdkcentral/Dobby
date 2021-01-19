//
// Created by Ben Gray on 25/11/2020.
//
#include "TapInterface.h"
#include "Netlink.h"
#include <Logging.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>

#define TAP_NAME "dobby_tap0"

// -----------------------------------------------------------------------------
/**
 *  @brief Creates the Dobby bridge device.
 *
 *  @return true on success, false on failure.
 */
bool TapInterface::createTapInterface()
{
    if (mFd >= 0)
    {
        // Tap device already open
        return true;
    }
    mFd = open("/dev/net/tun", O_CLOEXEC | O_RDWR);
    if (mFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open '/dev/net/tun'");
        return false;
    }
    struct ifreq ifr;
    bzero(&ifr, sizeof(ifr));
    // set the flags
    ifr.ifr_flags = IFF_TAP | IFF_NO_PI | IFF_ONE_QUEUE;
    strncpy(ifr.ifr_name, TAP_NAME, IFNAMSIZ);
    // ask for kernel for new device
    int ret = ioctl(mFd, TUNSETIFF, (void *) &ifr);
    if (ret != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create tap device '%s'", TAP_NAME);
        close(mFd);
        mFd = -1;
        return false;
    }
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Destroys the Dobby bridge device.
 *
 *  @return true on success, false on failure.
 */
bool TapInterface::destroyTapInterface()
{
    if ((mFd >= 0) && (close(mFd) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close the tap fd");
        return false;
    }
    mFd = -1;
    return true;
}

bool TapInterface::isValid()
{
    return (mFd >= 0);
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
    if  (mFd < 0)
        return false;
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
    if  (mFd < 0)
        return false;
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
bool TapInterface::setMACAddress(const std::shared_ptr<Netlink> &netlink, const std::array<uint8_t, 6>& address)
{
    return netlink->setIfaceMAC(TAP_NAME, address);
}