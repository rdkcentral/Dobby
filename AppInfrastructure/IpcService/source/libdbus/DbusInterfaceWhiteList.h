/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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
 * DbusInterfaceWhiteList.h
 *
 *  Created on: 1 Oct 2018
 *      Author: janosolah
 */

#ifndef IPCSERVICE_LIB_SOURCE_DBUSINTERFACEWHITELIST_H_
#define IPCSERVICE_LIB_SOURCE_DBUSINTERFACEWHITELIST_H_

#include <string>
#include <unordered_set>

namespace AI_IPC
{

/**
 * @brief This set contains the substrings of the white listed interfaces
 *
 * These substrings are looked for in the interface name received from DBus.
 */
const std::unordered_set<std::string> WhiteListedDbusInterfaces =
{
    "org.freedesktop", // .*
};

}



#endif /* IPCSERVICE_LIB_SOURCE_DBUSINTERFACEWHITELIST_H_ */
