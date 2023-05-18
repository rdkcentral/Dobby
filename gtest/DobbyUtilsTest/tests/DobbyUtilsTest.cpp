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
#include <fcntl.h>
#include "DobbyTimer.h"
#include "ContainerId.h"
#include "Logging.h"
#define private public
#include "DobbyUtils.h"

class DobbyUtilsTest : public ::testing::Test
{
         protected:
             DobbyUtils test;
             ContainerId t_id;
};

TEST_F(DobbyUtilsTest, TestRecursiveMkdirAbsolutePath)
{
        std::string path = "/tmp/hello/some/long/path";

        EXPECT_TRUE(test.mkdirRecursive(path, 0700));
}

TEST_F(DobbyUtilsTest, TestRmdirContentsAbsolutePath)
{
        EXPECT_TRUE(test.rmdirContents("/tmp/hello"));
}

TEST_F(DobbyUtilsTest, TestRmdirRecursiveAbsolutePath)
{
        EXPECT_TRUE(test.rmdirRecursive("/tmp/hello"));
}

TEST_F(DobbyUtilsTest, TestCleanMountLostAndFound)
{
        std::string tmp = "/lost+found/some/long/path/file.xyz";

        test.mkdirRecursive(tmp, 0700);

        test.cleanMountLostAndFound("/home", std::string("0"));
}

TEST_F(DobbyUtilsTest, TestAttachFileToLoopDevice)
{
        std::string loopDevPath;

        int loopDevFd = test.openLoopDevice(&loopDevPath);

        int fileFd = open("/tmp/test1", O_CREAT | O_RDWR, 0644);

        EXPECT_TRUE(test.attachFileToLoopDevice(loopDevFd,fileFd));

        test.rmdirRecursive("/tmp/test1");
}

TEST_F(DobbyUtilsTest, TestwriteTextFile)
{
        test.writeTextFile("/tmp/hi","Hello World",O_CREAT,0644);
}

TEST_F(DobbyUtilsTest, TestreadTextFile)
{
        EXPECT_EQ(test.readTextFile("/tmp/hi",4096),"Hello World");
        test.rmdirRecursive("/tmp/hi");
}

TEST_F(DobbyUtilsTest, TestContainerMetaData)
{
        t_id.create("a123");

        test.setStringMetaData(t_id,"ipaddr","127.0.0.1");
        EXPECT_EQ(test.getStringMetaData(t_id,"ipaddr",""),"127.0.0.1");

        test.setIntegerMetaData(t_id,"port",9998);
        EXPECT_EQ(test.getIntegerMetaData(t_id,"port",0),9998);

        test.clearContainerMetaData(t_id);
        EXPECT_EQ(test.getStringMetaData(t_id,"ipaddr",""),"");
        EXPECT_EQ(test.getIntegerMetaData(t_id,"port",0),0);
}
