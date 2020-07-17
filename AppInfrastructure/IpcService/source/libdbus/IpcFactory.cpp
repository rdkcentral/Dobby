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
 * IpcFactory.cpp
 *
 *  Created on: 5 Jun 2015
 *      Author: riyadh
 */

#include "IpcCommon.h"
#include "IpcFactory.h"
#include "IpcService.h"

#include <Logging.h>
#include <IPackageManager.h>

#include <set>
#include <string>

namespace AI_IPC
{

std::shared_ptr<IIpcService> createIpcService(const std::shared_ptr<const AI_DBUS::IDbusServer>& dbusServer, const std::string& serviceName, int defaultTimeoutMs /*= -1*/)
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();

    return std::make_shared<IpcService>(dbusServer, serviceName, defaultTimeoutMs);
}

std::shared_ptr<IIpcService> createIpcService( const std::shared_ptr<const AI_DBUS::IDbusServer>& dbusServer,
                                               const std::string& serviceName,
                                               const std::shared_ptr<packagemanager::IPackageManager> &packageManager,
                                               bool dbusEntitlementCheckNeeded /* = false*/,
                                               int defaultTimeoutMs /*= -1*/)
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();

    return std::make_shared<IpcService>(dbusServer, serviceName, packageManager, dbusEntitlementCheckNeeded, defaultTimeoutMs);
}

std::shared_ptr<IIpcService> createSystemBusIpcService(const std::string& serviceName, int defaultTimeoutMs /*= -1*/)
{
    return std::make_shared<IpcService>(IpcService::BusType::SystemBus, serviceName, defaultTimeoutMs);
}

std::shared_ptr<IIpcService> createSessionBusIpcService(const std::string& serviceName, int defaultTimeoutMs /*= -1*/)
{
    return std::make_shared<IpcService>(IpcService::BusType::SessionBus, serviceName, defaultTimeoutMs);
}

std::shared_ptr<IIpcService> createIpcService(const std::string& address, const std::string& serviceName, int defaultTimeoutMs)
{
    return std::make_shared<IpcService>(address, serviceName, defaultTimeoutMs);
}

}
