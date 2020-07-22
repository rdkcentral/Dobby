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
#include <AIBase64.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>


using namespace std;
using namespace testing;
using namespace AICommon;

struct Base64Tests 
{
    const char * input, * output;
};

static std::vector<Base64Tests> base64TestStrings {
    {"", ""}, 
    {"M", "TQ=="}, 
    {"Ma", "TWE="}, 
    {"Man", "TWFu"}, 
    {"pleasure.", "cGxlYXN1cmUu"},
    {"leasure.", "bGVhc3VyZS4="},
    {"easure.", "ZWFzdXJlLg=="},
    {"asure.", "YXN1cmUu"},
    {"sure.", "c3VyZS4="}
};

TEST(TestBase64, basicEncodingTest)
{
    vector<Base64Tests>::iterator it;
    for ( it=base64TestStrings.begin() ; it != base64TestStrings.end(); ++it )
    {
        EXPECT_EQ((*it).output, encodeBase64((*it).input));
    }
}

TEST(TestBase64, basicDecodingTest)
{
    vector<Base64Tests>::iterator it;
    for ( it=base64TestStrings.begin() ; it != base64TestStrings.end(); ++it )
    {
        EXPECT_EQ((*it).input, decodeBase64((*it).output));
    }
}

//FIXME: This needs a base64 implementation that handles errors.
TEST(TestBase64, basicDecodingTestBroken)
{
    EXPECT_THROW(decodeBase64("SGVsbG8gV29ybGQHAHAHAHA23098745*())()()([]\\`~"), std::invalid_argument);
}

