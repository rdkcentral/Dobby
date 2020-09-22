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
 * IpcFactory.h
 *
 *  Created on: 5 Jun 2015
 *      Author: riyadh
 */

#ifndef AI_IPC_IPCFACTORY_H
#define AI_IPC_IPCFACTORY_H

#include "IIpcService.h"

#include <set>
#include <memory>
#include <string>

#define IPC_SERVICE_APP_PROCESS "com.sky.ai.app_process"

namespace packagemanager
{
    class IPackageManager;
}

namespace AI_DBUS
{

class IDbusServer;

}

namespace AI_IPC
{

/**
 * @brief Create a IPC service
 *
 * A remote process is identified by a name, i.e., the service name. It can have one to several associated objects distinguished from each other using
 * object paths. A object can be considered as a remote instance that can be accessed through its interfaces.
 *
 * @parameter[in]   dbusServer          The dbus daemon the server will be connecting to
 * @parameter[in]   serviceName         Service name
 * @parameter[in]   defaultTimeoutMs    The default timeout to use for method calls, if -1 then use the libdbus default (30 seconds)
 *
 * @returns On success: Shared pointer pointing to the IPC service instance.
 * @returns On failure: Empty shared pointer.
 */
std::shared_ptr<IIpcService> createIpcService(const std::shared_ptr<const AI_DBUS::IDbusServer>& dbusServer, const std::string& serviceName, int defaultTimeoutMs = -1);

/**
 * @brief Create a IPC service
 *
 * A remote process is identified by a name, i.e., the service name. It can have one to several associated objects distinguished from each other using
 * object paths. A object can be considered as a remote instance that can be accessed through its interfaces.
 *
 * @parameter[in]   dbusServer          The dbus daemon the server will be connecting to
 * @parameter[in]   serviceName         Service name
 * @parameter[in]   packageManager      The package manager the DbusHardening functionality will use
 * @parameter[in]   defaultTimeoutMs    The default timeout to use for method calls, if -1 then use the libdbus default (30 seconds)
 *
 * @returns On success: Shared pointer pointing to the IPC service instance.
 * @returns On failure: Empty shared pointer.
 */
std::shared_ptr<IIpcService> createIpcService( const std::shared_ptr<const AI_DBUS::IDbusServer>& dbusServer,
                                               const std::string& serviceName,
                                               const std::shared_ptr<packagemanager::IPackageManager> &packageManager,
                                               bool dbusEntitlementCheckNeeded = false,
                                               int defaultTimeoutMs = -1);

/**
 * @brief Create a IPC service attached to one of the known buses
 *
 * A remote process is identified by a name, i.e., the service name. It can have one to several associated objects distinguished from each other using
 * object paths. A object can be considered as a remote instance that can be accessed through its interfaces.
 *
 * @parameter[in]   serviceName         Service name
 * @parameter[in]   defaultTimeoutMs    The default timeout to use for method calls, if -1 then use the libdbus default (30 seconds)
 *
 * @returns On success: Shared pointer pointing to the IPC service instance.
 * @returns On failure: Empty shared pointer.
 */
std::shared_ptr<IIpcService> createSystemBusIpcService(const std::string& serviceName, int defaultTimeoutMs = -1);
std::shared_ptr<IIpcService> createSessionBusIpcService(const std::string& serviceName, int defaultTimeoutMs = -1);

/**
 * @brief Create a IPC service attached to the bus with the given address.
 *
 * A remote process is identified by a name, i.e., the service name. It can have one to several associated objects distinguished from each other using
 * object paths. A object can be considered as a remote instance that can be accessed through its interfaces.
 *
 * @parameter[in]   address             The dbus address to connect to.
 * @parameter[in]   serviceName         Service name
 * @parameter[in]   defaultTimeoutMs    The default timeout to use for method calls, if -1 then use the libdbus default (30 seconds)
 *
 * @returns On success: Shared pointer pointing to the IPC service instance.
 * @returns On failure: Empty shared pointer.
 */
std::shared_ptr<IIpcService> createIpcService(const std::string& address, const std::string& serviceName, int defaultTimeoutMs = -1);

}

#endif /* AI_IPC_IPCFACTORY_H */
