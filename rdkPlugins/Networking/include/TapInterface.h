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

/*
 * File:   TapInterface.h
 *
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
