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
 * DbusUserIdSenderIdCache.cpp
 *
 *  Created on: 21 Sep 2018
 *      Author: janosolah
 */

#include <Logging.h>
#include "DbusUserIdSenderIdCache.h"
#include <IpcCommon.h>
#include <IDbusPackageEntitlements.h>

namespace AI_IPC
{

DbusUserIdSenderIdCache::DbusUserIdSenderIdCache( AI_IPC::IIpcService& parentIpcService,
                                                  const std::shared_ptr<IDbusPackageEntitlements>& dbusPackageEntitlements)
: mParentIpcService(parentIpcService)
, mDbusPackageEntitlements(dbusPackageEntitlements)
{
    mNameChangedSignalHandler =
            mParentIpcService.registerSignalHandler(AI_IPC::Signal("/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged"),
                                                     std::bind(&DbusUserIdSenderIdCache::nameChanged, this, std::placeholders::_1));
    if (mNameChangedSignalHandler.empty())
    {
        AI_LOG_ERROR("failed to register for NameOwnerChanged signal, this means that DbusUserIdSenderIdCache won't remove the senderId-userId mapping from the cache!");
    }
}

DbusUserIdSenderIdCache::~DbusUserIdSenderIdCache()
{
    AI_LOG_FN_ENTRY();

    // unregister the NameOwnerChanged handler
    if( !mParentIpcService.unregisterHandler(mNameChangedSignalHandler) )
    {
        AI_LOG_ERROR( "failed to unregister the NameOwnerChanged signal");
    }

    mNameChangedSignalHandler = "";

    // flush the dbus event queue
    mParentIpcService.flush();

    AI_LOG_FN_EXIT();
}

void DbusUserIdSenderIdCache::addSenderIUserId(const std::string& senderId, uid_t userId)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> guard(mMutex);

    AI_LOG_INFO("Assigning %s to %d", senderId.c_str(), userId);

    mSenderIdUserIdCache[senderId] = userId;

    AI_LOG_FN_EXIT();
}

boost::optional<uid_t> DbusUserIdSenderIdCache::getUserId(const std::string& senderId) const
{
    AI_LOG_FN_ENTRY();

    boost::optional<uid_t> userId;
    std::lock_guard<std::mutex> guard(mMutex);

    auto userIdIt = mSenderIdUserIdCache.find(senderId);
    if(userIdIt != mSenderIdUserIdCache.end())
    {
        userId = userIdIt->second;
    }

    AI_LOG_FN_EXIT();

    return userId;
}

void DbusUserIdSenderIdCache::removeUserId(const std::string& senderId)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> guard(mMutex);

    AI_LOG_INFO("Removing the cached senderid %s", senderId.c_str());

    // notify the DbusEntitlement cache object that the app is stopped, so
    // the cache entries for the given appId can be removed from there as well.
    mDbusPackageEntitlements->applicationStopped(mSenderIdUserIdCache[senderId]);

    mSenderIdUserIdCache.erase(senderId);

    AI_LOG_FN_EXIT();
}

void DbusUserIdSenderIdCache::nameChanged(const AI_IPC::VariantList& args)
{
    AI_LOG_FN_ENTRY();

    // Expecting three arg:  (std::string, std::string, std::string)
    std::string name;
    std::string oldOwner;
    std::string newOwner;
    if (!AI_IPC::parseVariantList
            <std::string, std::string, std::string>
            (args, &name, &oldOwner, &newOwner))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO("NameOwnerChanged('%s', '%s', '%s')",
                    name.c_str(), oldOwner.c_str(), newOwner.c_str());

        // Detect if the client is disappearing
        if ((name == oldOwner) && newOwner.empty())
        {
            AI_LOG_INFO("dbus client '%s' has left the bus", name.c_str());

            // remove the senderId - userId mapping
            removeUserId(name);
        }
    }

    AI_LOG_FN_EXIT();
}

} // namespace AI_IPC


