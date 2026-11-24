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
#include <cstring>
#include "DobbyTimer.h"
#include "ContainerId.h"
#include "Logging.h"
#include "SafeDirectory.h"
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

TEST_F(DobbyUtilsTest, TestCleanMountLostAndFoundUnlinksNestedSymlink)
{
        char mountTemplate[] = "/tmp/dobby-mount-XXXXXX";
        char targetTemplate[] = "/tmp/dobby-target-XXXXXX";
        char* mountPoint = mkdtemp(mountTemplate);
        char* target = mkdtemp(targetTemplate);
        ASSERT_NE(mountPoint, nullptr);
        ASSERT_NE(target, nullptr);

        std::string lostFound = std::string(mountPoint) + "/lost+found";
        std::string nested = lostFound + "/nested";
        ASSERT_EQ(mkdir(lostFound.c_str(), 0700), 0);
        ASSERT_EQ(mkdir(nested.c_str(), 0700), 0);

        std::string targetFile = std::string(target) + "/sentinel";
        int targetFd = open(targetFile.c_str(), O_CREAT | O_WRONLY | O_CLOEXEC, 0600);
        ASSERT_GE(targetFd, 0);
        close(targetFd);
        std::string linkPath = nested + "/target";
        ASSERT_EQ(symlink(target, linkPath.c_str()), 0);

        test.cleanMountLostAndFound(mountPoint, std::string("0"));

        EXPECT_EQ(access(linkPath.c_str(), F_OK), -1);
        EXPECT_EQ(access(targetFile.c_str(), F_OK), 0);
        rmdir(nested.c_str());
        rmdir(lostFound.c_str());
        unlink(targetFile.c_str());
        rmdir(target);
        rmdir(mountPoint);
}

TEST_F(DobbyUtilsTest, TestDirectoryEntryUnknownUsesNoFollowStat)
{
        char directoryTemplate[] = "/tmp/dobby-entry-XXXXXX";
        char* directory = mkdtemp(directoryTemplate);
        ASSERT_NE(directory, nullptr);

        std::string realDirectory = std::string(directory) + "/directory";
        std::string regularFile = std::string(directory) + "/file";
        std::string symbolicLink = std::string(directory) + "/link";
        ASSERT_EQ(mkdir(realDirectory.c_str(), 0700), 0);
        int fileFd = open(regularFile.c_str(), O_CREAT | O_WRONLY | O_CLOEXEC, 0600);
        ASSERT_GE(fileFd, 0);
        close(fileFd);
        ASSERT_EQ(symlink(realDirectory.c_str(), symbolicLink.c_str()), 0);

        int directoryFd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        ASSERT_GE(directoryFd, 0);
        dirent entry = {};
        entry.d_type = DT_UNKNOWN;
        std::strcpy(entry.d_name, "directory");
        EXPECT_TRUE(AICommon::directoryEntryIsDirectory(directoryFd, &entry));
        std::strcpy(entry.d_name, "file");
        EXPECT_FALSE(AICommon::directoryEntryIsDirectory(directoryFd, &entry));
        std::strcpy(entry.d_name, "link");
        EXPECT_FALSE(AICommon::directoryEntryIsDirectory(directoryFd, &entry));
        close(directoryFd);

        rmdir(realDirectory.c_str());
        unlink(regularFile.c_str());
        unlink(symbolicLink.c_str());
        rmdir(directory);
}

TEST_F(DobbyUtilsTest, TestDeleteRecursivePreservesDepthForSiblings)
{
        char directoryTemplate[] = "/tmp/dobby-depth-XXXXXX";
        char* directory = mkdtemp(directoryTemplate);
        ASSERT_NE(directory, nullptr);
        std::string first = std::string(directory) + "/first";
        std::string second = std::string(directory) + "/second";
        ASSERT_EQ(mkdir(first.c_str(), 0700), 0);
        ASSERT_EQ(mkdir(second.c_str(), 0700), 0);

        int directoryFd = open(directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        ASSERT_GE(directoryFd, 0);
        EXPECT_TRUE(test.deleteRecursive(directoryFd, 2));
        EXPECT_EQ(access(first.c_str(), F_OK), -1);
        EXPECT_EQ(access(second.c_str(), F_OK), -1);
        rmdir(directory);
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