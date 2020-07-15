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
 * File:   AppIdTest.cpp
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 18 November 2015
 *
 * Copyright (C) BSKYB 2015
 */

#include <AppId.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>


using namespace std;
using namespace testing;
using namespace AICommon;


TEST(TestAppId, testSomeValidAppIds)
{
    vector<string> appIds = {
        "com.bskyb.epgui",
        "com.bskyb.news",
        "blah",
        "pac-man",
        "com.1234.some-thing",
    };

    for(const auto& appId : appIds)
    {
        EXPECT_NO_THROW(AppId obj(appId));
        EXPECT_TRUE(AppId::isValid(appId));
    }
}

TEST(TestAppId, testInvalidAppIds)
{
    vector<string> appIds = {
        "this appid is invalid",
        "../haha/you/wish/",
        "../../../../",
        "\"../../\"",
        "^^What.is.that?",
        ""
    };

    for(const auto& appId : appIds)
    {
        EXPECT_THROW(AppId obj(appId), AICommon::InvalidAppId);
        EXPECT_FALSE(AppId::isValid(appId));
    }
}

TEST(TestAppId, testStreamReadAppIds)
{
    std::string input("com.bskyb.epgui 1 com.bskyb.news 2 pac-man 3");
    stringstream ss(input);

    AppId appId;
    size_t hits;

    ss>>appId;
    ss>>hits;
    EXPECT_EQ(appId, AppId("com.bskyb.epgui"));
    EXPECT_EQ(hits, 1UL);

    ss>>appId;
    ss>>hits;
    EXPECT_EQ(appId, AppId("com.bskyb.news"));
    EXPECT_EQ(hits, 2UL);

    ss>>appId;
    ss>>hits;
    EXPECT_EQ(appId, AppId("pac-man"));
    EXPECT_EQ(hits, 3UL);

    EXPECT_TRUE(ss.eof());
    EXPECT_FALSE(ss.bad());
}

