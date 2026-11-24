/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#include <gtest/gtest.h>

#include <fcntl.h>
#include <string>
#include <unistd.h>

#include "StorageHelper.h"

TEST(StorageHelperTest, CleanMountLostAndFoundRejectsSymlinkAndCleansStaleEntries)
{
    char mountTemplate[] = "/tmp/storage-helper-mount-XXXXXX";
    char targetTemplate[] = "/tmp/storage-helper-target-XXXXXX";
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
    StorageHelper::cleanMountLostAndFound(mountPoint, "symlink");
    EXPECT_EQ(access(targetFile.c_str(), F_OK), 0);
    ASSERT_EQ(unlink(lostFound.c_str()), 0);

    ASSERT_EQ(mkdir(lostFound.c_str(), 0700), 0);
    std::string staleFile = lostFound + "/stale-file";
    std::string staleDirectory = lostFound + "/stale-directory";
    int staleFd = open(staleFile.c_str(), O_CREAT | O_WRONLY | O_CLOEXEC, 0600);
    ASSERT_GE(staleFd, 0);
    close(staleFd);
    ASSERT_EQ(mkdir(staleDirectory.c_str(), 0700), 0);

    StorageHelper::cleanMountLostAndFound(mountPoint, "stale");
    EXPECT_EQ(access(staleFile.c_str(), F_OK), -1);
    EXPECT_EQ(access(staleDirectory.c_str(), F_OK), -1);
    EXPECT_EQ(access(targetFile.c_str(), F_OK), 0);

    unlink(targetFile.c_str());
    rmdir(lostFound.c_str());
    rmdir(target);
    rmdir(mountPoint);
}
