/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2025 Sky UK
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
#ifndef INTERCONTAINERROUTING_H
#define INTERCONTAINERROUTING_H

#include "Netfilter.h"
#include "NetworkingHelper.h"
#include "DobbyRdkPluginUtils.h"
#include <rt_defs_plugins.h>

#include <memory>


// -----------------------------------------------------------------------------
/**
 *  @namespace InterContainerRouting
 *
 *  @brief Used to add iptables firewall rules to allow a container to either
 *  expose a port to another container or to access a port on another container.
 *
 *  @see the plugin's README.md for more details on usage.
 *
 *  This adds the necessary rules to iptables when the container is started and
 *  deletes them again when the container is stopped.  All the rules are tagged
 *  (via an iptables comment) with the name of the container, this should ensure
 *  rules are correctly added and removed.
 *
 */
namespace InterContainerRouting
{
    bool addRules(const std::shared_ptr<Netfilter> &netfilter,
                  const std::shared_ptr<NetworkingHelper> &helper,
                  const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                  rt_defs_plugins_networking_data_inter_container_element * const *portConfigs,
                  size_t numPortConfigs);

    bool removeRules(const std::shared_ptr<Netfilter> &netfilter,
                     const std::shared_ptr<NetworkingHelper> &helper,
                     const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                     rt_defs_plugins_networking_data_inter_container_element * const *portConfigs,
                     size_t numPortConfigs);
};


#endif // !defined(INTERCONTAINERROUTING_H)