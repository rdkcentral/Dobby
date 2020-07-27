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
 * DbusUserIdSenderIdCache.h
 *
 *  Created on: 21 Sep 2018
 *      Author: janosolah
 */

#ifndef IPCSERVICE_LIB_SOURCE_DBUSUSERIDSENDERIDCACHE_H_
#define IPCSERVICE_LIB_SOURCE_DBUSUSERIDSENDERIDCACHE_H_

#include <unordered_map>
#include <mutex>

#include <IIpcService.h>
#include <IDbusUserIdSenderIdCache.h>
#include "IpcVariantList.h"

namespace AI_IPC
{

class IDbusPackageEntitlements;

class DbusUserIdSenderIdCache : public IDbusUserIdSenderIdCache
{
public:
    DbusUserIdSenderIdCache( AI_IPC::IIpcService& parentIpcService,
                             const std::shared_ptr<IDbusPackageEntitlements>& dbusPackageEntitlements );
    ~DbusUserIdSenderIdCache();

    void nameChanged(const AI_IPC::VariantList& args);

public: // IDbusUserIdSenderIdCache
    virtual void addSenderIUserId(const std::string& senderId, uid_t userId) override;
    virtual boost::optional<uid_t> getUserId(const std::string& senderId) const override;
    virtual void removeUserId(const std::string& senderId) override;

private:
    std::unordered_map<std::string, uid_t> mSenderIdUserIdCache;
    mutable std::mutex mMutex;
    std::string mNameChangedSignalHandler;

    // This object cannot exist without its parent
    AI_IPC::IIpcService& mParentIpcService;

    // This object is here to be able to notify it when the app is stopped, and also the app has been removed while it was running
    // In this case the DbusPackageEntitlements object doesn't remove the cache entries related to the given appId, but will "wait"
    // for a notification from the DbusUserIdSenderIdCache. Don't think it's necessary to implement the observer/notifier interface
    // for this, as these objects are strictly internal to the IpcService object.
    std::shared_ptr<IDbusPackageEntitlements> mDbusPackageEntitlements;
};

} // namespace AI_IPC



#endif /* IPCSERVICE_LIB_SOURCE_DBUSUSERIDSENDERIDCACHE_H_ */
