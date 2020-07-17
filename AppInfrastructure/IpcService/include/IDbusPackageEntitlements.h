/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
 * IDbusPackageEntitlements.h
 *
 *  Created on: 19 Sep 2018
 */

#ifndef IPCSERVICE_LIB_INCLUDE_IDBUSPACKAGEENTITLEMENTS_H_
#define IPCSERVICE_LIB_INCLUDE_IDBUSPACKAGEENTITLEMENTS_H_

#include <sys/types.h>
#include <string>

namespace AI_IPC
{
class IDbusPackageEntitlements
{
public:
    virtual bool isAllowed( uid_t userId, const std::string& service, const std::string& interface ) = 0;
    virtual void applicationStopped(uid_t userId) = 0;
    virtual bool isInterfaceWhiteListed(const std::string& interface) const = 0;

    virtual ~IDbusPackageEntitlements() = default;
};

}



#endif /* IPCSERVICE_LIB_INCLUDE_IDBUSPACKAGEENTITLEMENTS_H_ */
