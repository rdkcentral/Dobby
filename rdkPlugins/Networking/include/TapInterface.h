/*
 * File:   TapInterface.h
 *
 * Copyright (C) Sky.uk 2020
 */
#ifndef TAPINTERFACE_H
#define TAPINTERFACE_H
#include <string>
#include <memory>
#include <array>
#include <arpa/inet.h>
class Netlink;
// -----------------------------------------------------------------------------
/**
 * A set of function to create and destruct Tap device.
 */
namespace TapInterface
{
    bool createTapInterface();
    bool destroyTapInterface();
    bool isValid();
    const std::string name();
    bool up(const std::shared_ptr<Netlink> &netlink);
    bool down(const std::shared_ptr<Netlink> &netlink);
    std::array<uint8_t, 6> macAddress(const std::shared_ptr<Netlink> &netlink);
    bool setMACAddress(const std::shared_ptr<Netlink> &netlink,
                       const std::array<uint8_t, 6>& address);
    static int mFd=-1;
};
#endif // !defined(BRIDGEINTERFACE_H)