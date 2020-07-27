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
 * DbusEntitlements.h
 *
 *  Created on: 19 Sep 2018
 */

#ifndef IPCSERVICE_LIB_SOURCE_DBUSENTITLEMENTS_H_
#define IPCSERVICE_LIB_SOURCE_DBUSENTITLEMENTS_H_

#include <unordered_map>
#include <unordered_set>
#include <list>
#include <mutex>

#include <IPackageManager.h>
#include <IDbusPackageEntitlements.h>
#include <AppId.h>

namespace std
{
template<>
struct hash<AICommon::AppId>
{
    size_t operator()(const AICommon::AppId& appId) const noexcept
    {
        return std::hash<std::string>()(appId.str());
    }
};
}

namespace AI_IPC
{

class DbusEntitlements : public IDbusPackageEntitlements
{
public:
    DbusEntitlements(const std::shared_ptr<packagemanager::IPackageManager>& packageManager);
    ~DbusEntitlements() = default;

public: // IDbusPackageEntitlements
    virtual bool isAllowed( uid_t userId, const std::string& service, const std::string& interface ) override;

    virtual void applicationStopped(uid_t userId) override;

    virtual bool isInterfaceWhiteListed(const std::string& interface) const override;

protected:
    void addEntitlementLock( uid_t userId,
                             const AICommon::AppId& appId,
                             const PackageMetadata::DbusCapabilityType& packageDbusEntitlements );

    void addEntitlementNoLock( uid_t userId,
                               const AICommon::AppId& appId,
                               const PackageMetadata::DbusCapabilityType& packageDbusEntitlements );

    void removeEntitlementLock( uid_t userId );

    void removeEntitlementNoLock( uid_t userId );

private:
    std::unordered_map<uid_t, PackageMetadata::DbusCapabilityType> mDbusEntitlements;
    std::shared_ptr<packagemanager::IPackageManager> mPackageManager;
    mutable std::mutex mMutex;
};

} // namespace AI_IPC


#endif /* IPCSERVICE_LIB_SOURCE_DBUSENTITLEMENTS_H_ */
