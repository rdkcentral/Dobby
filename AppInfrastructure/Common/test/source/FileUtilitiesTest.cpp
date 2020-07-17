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
 * File:   FileUtililitiesTest.cpp
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 11 July 2014
 *
 * Copyright (C) Sky UK 2014
 */

#include <FileUtilities.h>
#include <ScratchSpace.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <fstream>

using namespace std;
using namespace testing;
using namespace AICommon;


TEST(TestSplitPath, TestAbsolutePathSplitting)
{
    const string path = "/highway/to/hell";
    EXPECT_EQ(vector<string>({"highway", "to", "hell"}), splitPath(path));
}

TEST(TestSplitPath, TestShortAbsolutePathSplitting)
{
    const string path = "/tmp";
    EXPECT_EQ(vector<string>({"tmp"}), splitPath(path));
}

TEST(TestSplitPath, TestShortAbsolutePathMultiSlashSplitting)
{
    const string path = "//tmp";
    EXPECT_EQ(vector<string>({"tmp"}), splitPath(path));
}

TEST(TestSplitPath, TestShortRelativePathSplitting)
{
    const string path = "tmp";
    EXPECT_EQ(vector<string>({"tmp"}), splitPath(path));
}

TEST(TestSplitPath, TestRelativePathSplitting)
{
    const string path = "highway/to/hell";
    EXPECT_EQ(vector<string>({"highway", "to", "hell"}), splitPath(path));
}

TEST(TestSplitPath, TestSplitRoot)
{
    const string path = "/";
    EXPECT_EQ(vector<string>(), splitPath(path));
}

TEST(TestSplitPath, TestAbsolutePathSplittingWithTrailingSlash)
{
    const string path = "/highway/to/hell/";
    EXPECT_EQ(vector<string>({"highway", "to", "hell"}), splitPath(path));
}

TEST(TestSplitPath, TestSpuriousSlashes)
{
    const string path = "/highway/to///////////hell/";
    EXPECT_EQ(vector<string>({"highway", "to", "hell"}), splitPath(path));
}

TEST(TestExists, TestExists)
{
    ScratchSpace scratch("/tmp");
    const string path = scratch.path() + "/file";

    ofstream s(path);
    s<<"contents";
    s.close();

    EXPECT_TRUE(exists(path));
}

TEST(TestExists, TestDoesntExist)
{
    EXPECT_FALSE(exists("/not/there"));
}

TEST(TestMkdir, TestRecursiveMkdirAbsolutePath)
{
    ScratchSpace scratch("/tmp");

    const string path = scratch.path() + "/hello/there";

    EXPECT_TRUE(mkdirRecursive(path));
    EXPECT_TRUE(exists(path));
}

TEST(TestMkdir, TestRecursiveMkdirRelativePath)
{
    ScratchSpace scratch("/tmp");
    EXPECT_EQ(0, chdir(scratch.path().c_str()));

    const string path =  "hello/there";

    EXPECT_TRUE(mkdirRecursive(path));
    EXPECT_TRUE(exists(scratch.path() + "/" + path));
    EXPECT_EQ(0, chdir("/"));
}

TEST(TestMkdir, TestErrorReportingCantCreateDirectory)
{
    const string path =  "/proc/forbidden";

    EXPECT_FALSE(mkdirRecursive(path));
    EXPECT_FALSE(exists(path));
}

TEST(TestMkdir, TestErrorReportingObstructingFile)
{
    ScratchSpace scratch("/tmp");
    const string path = scratch.path() + "/hello/there";
    ofstream s(scratch.path() + "/hello");
    s.close();
    EXPECT_FALSE(mkdirRecursive(path));
    EXPECT_FALSE(exists(path));
}

TEST(TestResolvePath, TestResolvePath)
{
    ScratchSpace scratch("/tmp");
    string path = scratch.path() + "/hello/there";
    EXPECT_TRUE(mkdirRecursive(path));
    EXPECT_EQ(path, resolvePath(scratch.path() + "/hello/../hello/there"));
}

