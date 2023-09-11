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

#include <gtest/gtest.h>
#include "ContainerId.h"
using namespace std;

class ContainerIdTest : public ::testing::Test {
         protected:
             ContainerId tid,rid;
};

TEST_F(ContainerIdTest,CheckNumeric)
{
        rid = tid.create("123");
        EXPECT_EQ(rid.str(),"");
}

TEST_F(ContainerIdTest,CheckDoubleDot)
{
        rid = tid.create("a..123");
        EXPECT_EQ(rid.str(),"");
}

TEST_F(ContainerIdTest,CheckAlphanumeric)
{
        rid = tid.create("a.123");
        EXPECT_EQ(rid.str(),"a.123");
}


