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
 * DbusEntitlementsTest.cpp
 *
 *  Created on: 19 Sep 2018
 *      Author: janosolah
 */
#include <memory>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock/MockPackageManager.h"
#include <AppId.h>
#include <IDbusPackageEntitlements.h>
#include "DbusEntitlements.h"


// The following are required as we don't link to the PackageManager
// or IdentityManager libraries (and we shouldn't)
inline std::ostream& operator<<(std::ostream& s, const IconDefinition & v)
{
    s << "IconDefinition";
    return s;
}

inline std::ostream& operator<<(std::ostream& s, const AppIdentityData& v)
{
    s << "AppIdentityData";
    return s;
}


using namespace testing;
using namespace std::placeholders;

namespace
{
    boost::optional<PackageMetadata> DbusEntMetadata(const AICommon::AppId& testAppId, uid_t userId, const PackageMetadata::DbusCapabilityType& dbusCap)
    {
        boost::optional<PackageMetadata> result;

        PackageMetadata meta;
        meta.appId = testAppId;
        meta.userId = userId;
        meta.dbusCapability = dbusCap;
        result = meta;

        return result;
    }
}

class DbusEntitlementTest : public ::testing::Test
{
public:
    void SetUp()
    {
        AICommon::initLogging();

        mockPm = std::make_shared<packagemanager::MockPackageManager>();
        mDbusEntitlements = std::make_shared<AI_IPC::DbusEntitlements>(mockPm);

        mAppId = AICommon::AppId("some-app");
        mUserId = 1001;
        mDbusCapability = {{"com.sky.ai.service1", {"com.sky.ai.service1.intf1", "com.sky.ai.service1.intf2"}},
                           {"com.sky.ai.service2", {}},
                          };
        mApps = {"some-app"};


    }

    void TearDown()
    {
        mockPm.reset();
        mDbusEntitlements.reset();
    }

protected: //accessible in the test cases
    std::shared_ptr<packagemanager::MockPackageManager> mockPm;
    std::shared_ptr<AI_IPC::IDbusPackageEntitlements> mDbusEntitlements;
    AICommon::AppId mAppId;
    uid_t mUserId;
    PackageMetadata::DbusCapabilityType mDbusCapability;
    std::vector<AICommon::AppId> mApps;

};

TEST_F(DbusEntitlementTest, Add)
{
    EXPECT_CALL(*mockPm, getLoadedAppIds()).WillOnce(Return(std::vector<AICommon::AppId>{mAppId}));

    EXPECT_CALL(*mockPm, getMetadata(mAppId)).WillOnce(Return(DbusEntMetadata(mAppId, mUserId, mDbusCapability)));
    EXPECT_CALL(*mockPm, getAppIds()).WillRepeatedly(Return(mApps));

    ASSERT_TRUE( mDbusEntitlements->isAllowed( 1001, "com.sky.ai.service1", "com.sky.ai.service1.intf1" ));
}

TEST_F(DbusEntitlementTest, AddAllInterface)
{
    EXPECT_CALL(*mockPm, getLoadedAppIds()).WillOnce(Return(std::vector<AICommon::AppId>{mAppId}));
    EXPECT_CALL(*mockPm, getMetadata(mAppId)).WillOnce(Return(DbusEntMetadata(mAppId, mUserId, mDbusCapability)));
    EXPECT_CALL(*mockPm, getAppIds()).WillRepeatedly(Return(mApps));

    ASSERT_TRUE( mDbusEntitlements->isAllowed( 1001, "com.sky.ai.service2", "com.sky.ai.service1.intf2" ));
}

TEST_F(DbusEntitlementTest, AddAllService)
{
    AICommon::AppId appId("someGod-app");
    uid_t userId(1002);
    PackageMetadata::DbusCapabilityType dbusCapability = {{"*",{}}};

    EXPECT_CALL(*mockPm, getLoadedAppIds()).WillOnce(Return(std::vector<AICommon::AppId>{appId}));

    // overloading the default with an empty capability
    EXPECT_CALL(*mockPm, getMetadata(appId)).WillRepeatedly(Return(DbusEntMetadata(appId, userId, dbusCapability)));

    EXPECT_CALL(*mockPm, getAppIds()).WillRepeatedly(Return(std::vector<AICommon::AppId>{"someGod-app"}));

    ASSERT_TRUE( mDbusEntitlements->isAllowed( userId, "com.sky.ai.service3", "com.sky.ai.service1.intf3" ));
}

TEST_F(DbusEntitlementTest, Remove)
{
    EXPECT_CALL(*mockPm, getLoadedAppIds()).WillOnce(Return(std::vector<AICommon::AppId>{mAppId}));
    EXPECT_CALL(*mockPm, getMetadata(mAppId)).WillOnce(Return(DbusEntMetadata(mAppId, mUserId, mDbusCapability)));

    ASSERT_TRUE( mDbusEntitlements->isAllowed( 1001, "com.sky.ai.service1", "com.sky.ai.service1.intf1" ));

    mDbusEntitlements->applicationStopped(1001);

    EXPECT_CALL(*mockPm, getLoadedAppIds()).WillOnce(Return(std::vector<AICommon::AppId>{}));

    ASSERT_FALSE( mDbusEntitlements->isAllowed( 1001, "com.sky.ai.service1", "com.sky.ai.service1.intf1" ));
}
