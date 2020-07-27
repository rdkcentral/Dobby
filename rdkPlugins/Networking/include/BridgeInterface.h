/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
 * File:   BridgeInterface.h
 *
 */
#ifndef BRIDGEINTERFACE_H
#define BRIDGEINTERFACE_H

#include <DobbyRdkPluginUtils.h>
#include "NetworkingHelper.h"
#include "Netlink.h"

#include <string>
#include <memory>

#include <arpa/inet.h>

// -----------------------------------------------------------------------------
/**
 *  @namespace BridgeInterface
 *
 *  @brief A set of functions to setup and bring down a bridge device.
 *  The main reason to use these functions separately rather than direct calls
 *  to a netlink object from NetworkSetup.cpp is to provide workarounds for
 *  libnl versions 3.3.x - 3.4.x.
 *
 *  @see ENABLE_LIBNL_BRIDGE_WORKAROUND
 */
namespace BridgeInterface
{
    bool createBridge(const std::shared_ptr<Netlink> &netlink);
    bool destroyBridge(const std::shared_ptr<Netlink> &netlink);
    bool up(const std::shared_ptr<Netlink> &netlink);
    bool down(const std::shared_ptr<Netlink> &netlink);
    bool setAddresses(const std::shared_ptr<Netlink> &netlink);
    bool setIfaceForwarding(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                            const std::shared_ptr<Netlink> &netlink,
                            bool enable);
    bool setIfaceRouteLocalNet(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                               const std::shared_ptr<Netlink> &netlink,
                               bool enable);
    bool setIfaceProxyNdp(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                          const std::shared_ptr<Netlink> &netlink,
                          bool enable);
    bool setIfaceAcceptRa(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                          const std::shared_ptr<Netlink> &netlink,
                          int value);

    bool disableStp(const std::shared_ptr<DobbyRdkPluginUtils> &utils);
};

bool netlinkFlagsWorkaround(short int mask, short int flags);
bool netlinkForwardingWorkaround(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                 bool enable);
bool netlinkRouteLocalNetWorkaround(const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                    bool enable);

#endif // !defined(BRIDGEINTERFACE_H)
