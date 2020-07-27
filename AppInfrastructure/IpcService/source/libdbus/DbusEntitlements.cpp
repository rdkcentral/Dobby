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
 * DbusEntitlements.cpp
 *
 *  Created on: 19 Sep 2018
 */

#include <sstream>

#include "DbusEntitlements.h"
#include "DbusInterfaceWhiteList.h"

namespace AI_IPC
{

DbusEntitlements::DbusEntitlements(const std::shared_ptr<packagemanager::IPackageManager>& packageManager)
: mPackageManager(packageManager)
{
}

// Not very efficient, but this is the price for the flexibility...
bool DbusEntitlements::isInterfaceWhiteListed(const std::string& interface) const
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> guard(mMutex);

    bool whiteListed = false;

    for( const auto& whiteListedDbusInterface : WhiteListedDbusInterfaces )
    {
        if(interface.find(whiteListedDbusInterface) != std::string::npos)
        {
            AI_LOG_INFO("%s interface is in the white list", interface.c_str());
            whiteListed = true;
            break;
        }
    }

    AI_LOG_FN_EXIT();

    return whiteListed;
}

bool DbusEntitlements::isAllowed( uid_t userId, const std::string& service, const std::string& interface )
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> guard(mMutex);

    bool allowed = false;

    auto userIdIt = mDbusEntitlements.find(userId); //hashmap (userId)

    if(userIdIt == mDbusEntitlements.end())
    {
        // it is possible that the given userId has not been registered yet into the entitlements
        // cache - this could be, when an already installed app is started, after a reboot
        // because in this case there is no notification from the package manager, that a new package
        // has been installed. So in this case let's check dynamically and add the userId if needed
        std::vector<AICommon::AppId> loadedAppIds = mPackageManager->getLoadedAppIds();

        for(const auto& loadedAppId : loadedAppIds)
        {
            boost::optional<PackageMetadata> metaData = mPackageManager->getMetadata(loadedAppId);
            if(metaData && metaData->userId == userId)
            {
                addEntitlementNoLock(userId, metaData->appId, metaData->dbusCapability);
                break;
            }
        }

        // try to find the userId again
        userIdIt = mDbusEntitlements.find(userId);
    }

    if(userIdIt != mDbusEntitlements.end())
    {
        AI_LOG_INFO("userId %d is found in the cache", userId);

        // ok userId found
        if( userIdIt->second.find("*") == userIdIt->second.end() )
        {
            // no "*" as the service, this means that there should be specific service names defined for the app
            auto serviceId = userIdIt->second.find(service); //hashmap (service)
            if(serviceId != userIdIt->second.end())
            {
                // ok service found for the given userId
                if( !serviceId->second.empty() )
                {
                    auto interfaceIt = serviceId->second.find(interface); // hashset (interface)
                    if(interfaceIt != serviceId->second.end())
                    {
                        AI_LOG_INFO("Dbus service %s and interface %s is allowed for userId %d", service.c_str(), interface.c_str(), userId);
                        allowed = true;
                    }
                    else
                    {
                        AI_LOG_ERROR("Dbus interface %s is not enabled for userId %d", interface.c_str(), userId);
                    }
                }
                else
                {
                    // interface set is empty for the given service, meaning all interfaces are allowed
                    AI_LOG_INFO("All Dbus interfaces are enabled for the Dbus service %s for userId %d", service.c_str(), userId);
                    allowed = true;
                }
            }
            else
            {
                AI_LOG_ERROR("Dbus service %s is not enabled for userId %d", service.c_str(), userId);
            }
        }
        else
        {
            // service set is marked as "*" for the given userId, meaning all services (and interfaces) are allowed
            AI_LOG_INFO("All Dbus services/interfaces are enabled for userId %d", userId);
            allowed = true;
        }
    }
    else
    {
        AI_LOG_ERROR("UserId %d is not registered in the dbus capability cache", userId);
    }

    AI_LOG_FN_EXIT();

    return allowed;
}

void DbusEntitlements::addEntitlementLock( uid_t userId,
                                           const AICommon::AppId& appId,
                                           const PackageMetadata::DbusCapabilityType& packageDbusEntitlements )
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> guard(mMutex);
    addEntitlementNoLock(userId, appId, packageDbusEntitlements);

    AI_LOG_FN_EXIT();
}

void DbusEntitlements::addEntitlementNoLock( uid_t userId,
                                             const AICommon::AppId& appId,
                                             const PackageMetadata::DbusCapabilityType& packageDbusEntitlements )
{
    AI_LOG_FN_ENTRY();

    if( !packageDbusEntitlements.empty() )
    {
        // packageDbusEntitlements needs to contain at least a "*" as a key, meaning all services are allowed
        // otherwise specify the services the app is allowed to use

        mDbusEntitlements[userId] = packageDbusEntitlements;
    }
    else
    {
        AI_LOG_INFO("[%s] tried to register an empty dbusEntitlement data structure. This is not allowed and so [%s] will not be able to use DBus services!", appId.c_str(), appId.c_str());
    }

    AI_LOG_FN_EXIT();
}

void DbusEntitlements::removeEntitlementLock( uid_t userId )
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> guard(mMutex);

    removeEntitlementNoLock(userId);

    AI_LOG_FN_EXIT();
}

void DbusEntitlements::removeEntitlementNoLock( uid_t userId )
{
    AI_LOG_FN_ENTRY();

    AI_LOG_INFO("Removing the userId %d from the entitlements cache", userId);

    mDbusEntitlements.erase(userId);

    AI_LOG_FN_EXIT();
}

void DbusEntitlements::applicationStopped(uid_t userId)
{
    AI_LOG_FN_ENTRY();

    removeEntitlementLock(userId);

    AI_LOG_FN_EXIT();
}

} // namespace AI_IPC
