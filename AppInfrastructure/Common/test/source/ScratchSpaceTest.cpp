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
#include <ScratchSpace.h>
#include <FileUtilities.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <fstream>


using namespace std;
using namespace testing;
using namespace AICommon;


namespace
{
void touch(const std::string& path)
{
    ofstream f(path);
}
}

TEST(TestScratchSpace, TestFixedPath)
{
    const string scratchy = "/tmp/scratchy";
    EXPECT_FALSE(exists(scratchy)) << "Hey, clean up your filesystem!";

    {
        ScratchSpace s = ScratchSpace(FixedPath(scratchy));
        EXPECT_EQ(scratchy, s.path());
        EXPECT_TRUE(exists(s.path()));
        touch(s.path() + "/a_file");
        EXPECT_TRUE(exists(s.path() + "/a_file"));
    }

    EXPECT_FALSE(exists(scratchy));
}

TEST(TestScratchSpace, TestRandomPath)
{
    const string scratchy = "/tmp";
    string tmpPath;
    {
        ScratchSpace s = ScratchSpace(scratchy);
        EXPECT_NE(scratchy, s.path());
        EXPECT_TRUE(exists(s.path()));
        touch(s.path() + "/a_file");
        EXPECT_TRUE(exists(s.path() + "/a_file"));
        tmpPath = s.path();
    }
    EXPECT_FALSE(exists(tmpPath));
}

TEST(TestScratchSpace, TestThatScratchSpaceThrowsIfDirectoryCantBeCreated)
{
    EXPECT_THROW(ScratchSpace("/proc/self/canttouchthis"), std::runtime_error);
    EXPECT_THROW(ScratchSpace(FixedPath("/proc/self/hammertime")), std::runtime_error);

    ScratchSpace s1,s2;
    EXPECT_THROW(s1.initialise("/proc/self/canttouchthis"), std::runtime_error);
    EXPECT_THROW(s2.initialise(FixedPath("/proc/self/hammertime")), std::runtime_error);
}

TEST(TestScratchSpace, TestSize)
{
    const string scratchy = "/tmp";
    string tmpPath;
    {
        ScratchSpace s = ScratchSpace(scratchy);
        EXPECT_TRUE(exists(s.path()));
        ofstream f1(s.path() + "/a_file");
        f1<<"1234";
        f1.close();
        string dir = s.path() + "/abcd";
        string emptyDir = s.path() + "/dontlookhere";
        EXPECT_EQ(0, mkdir(dir.c_str(), S_IRWXU));
        EXPECT_EQ(0, mkdir(emptyDir.c_str(), S_IRWXU));
        touch(dir + "/another_file");
        ofstream f2(s.path() + "/another_file");
        f2<<"abcd";
        f2.close();
        EXPECT_EQ(8U, s.size());
        tmpPath = s.path();
    }
    EXPECT_FALSE(exists(tmpPath));
}
