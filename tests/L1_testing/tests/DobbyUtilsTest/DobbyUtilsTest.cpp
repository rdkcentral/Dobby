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

TEST_F(DobbyUtilsTest, TestCleanMountLostAndFoundRejectsSymlink)
{
        char mountTemplate[] = "/tmp/dobby-mount-XXXXXX";
        char targetTemplate[] = "/tmp/dobby-target-XXXXXX";
        char* mountPoint = mkdtemp(mountTemplate);
        char* target = mkdtemp(targetTemplate);
        ASSERT_NE(mountPoint, nullptr);
        ASSERT_NE(target, nullptr);

        std::string targetFile = std::string(target) + "/sentinel";
        int targetFd = open(targetFile.c_str(), O_CREAT | O_WRONLY | O_CLOEXEC, 0600);
        ASSERT_GE(targetFd, 0);
        close(targetFd);

        std::string lostFound = std::string(mountPoint) + "/lost+found";
        ASSERT_EQ(symlink(target, lostFound.c_str()), 0);

        test.cleanMountLostAndFound(mountPoint, std::string("0"));

        EXPECT_EQ(access(targetFile.c_str(), F_OK), 0);
        unlink(lostFound.c_str());
        unlink(targetFile.c_str());
        rmdir(target);
        rmdir(mountPoint);
}

TEST_F(DobbyUtilsTest, TestCleanMountLostAndFoundCleansDirectory)
{
        char mountTemplate[] = "/tmp/dobby-mount-XXXXXX";
        char* mountPoint = mkdtemp(mountTemplate);
        ASSERT_NE(mountPoint, nullptr);

        std::string lostFound = std::string(mountPoint) + "/lost+found";
        ASSERT_EQ(mkdir(lostFound.c_str(), 0700), 0);
        std::string staleFile = lostFound + "/stale";
        int staleFd = open(staleFile.c_str(), O_CREAT | O_WRONLY | O_CLOEXEC, 0600);
        ASSERT_GE(staleFd, 0);
        close(staleFd);

        test.cleanMountLostAndFound(mountPoint, std::string("0"));

        EXPECT_EQ(access(staleFile.c_str(), F_OK), -1);
        rmdir(lostFound.c_str());
        rmdir(mountPoint);
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

TEST_F(DobbyUtilsTest, TestgetUID)
{
        pid_t pid = getpid();
        EXPECT_EQ(test.getUID(pid),getuid());
}

TEST_F(DobbyUtilsTest, TestgetGID)
{
        pid_t pid = getpid();
        EXPECT_EQ(test.getGID(pid),getgid());
}