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
#include <FileWatcher.h>
#include <Common/Observer.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <fstream>

using namespace testing;
using namespace AICommon;


class TestFileObserver : public Observer<FileWatcherEvent>
{
public:
    ~TestFileObserver() override { }

public:
    MOCK_METHOD1(fileChanged, void (const FixedPath&));
    MOCK_METHOD1(fileDeleted, void (const FixedPath&));
};


TEST(TestFileWatcher, testFileChangedEvent)
{
    std::shared_ptr<ScratchSpace> scratch = std::make_shared<ScratchSpace>("/tmp");
    std::shared_ptr<TestFileObserver> observer = std::make_shared<TestFileObserver>();

    FileWatcher watcher;
    watcher.addObserver(observer);


    // Create the file
    FixedPath file(scratch->path() + "/test1");

    std::ofstream ofs(file.path, std::ofstream::out);
    ofs << "line #1\n";
    ofs.close();

    // Install a watch on it
    ASSERT_TRUE(watcher.addPath(file));

    // Expect a file changed event
    EXPECT_CALL(*observer, fileChanged(file)).Times(1);

    // Write something to it
    ofs.open(file.path, std::ofstream::out | std::ofstream::app);
    ofs << "line #2\n";
    ofs.close();

    // Remove the watcher
    usleep(500000);
    ASSERT_TRUE(watcher.removePath(file));

    // Write something more
    ofs.open(file.path, std::ofstream::out | std::ofstream::app);
    ofs << "line #2\n";
    ofs.close();

    // Wait for something to happen
    usleep(500000);
    watcher.removeObserver(observer);
}


TEST(TestFileWatcher, testFileDeletedEvent)
{
    std::shared_ptr<ScratchSpace> scratch = std::make_shared<ScratchSpace>("/tmp");
    std::shared_ptr<TestFileObserver> observer = std::make_shared<TestFileObserver>();

    FileWatcher watcher;
    watcher.addObserver(observer);


    // Create the file
    FixedPath file(scratch->path() + "/test1");

    std::ofstream ofs(file.path, std::ofstream::out);
    ofs << "line #1\n";
    ofs.close();

    // Install a watch on it
    watcher.addPath(file);

    // Expect a file deleted event
    EXPECT_CALL(*observer, fileDeleted(file)).Times(1);

    // Delete the file
    scratch.reset();

    // Wait for something to happen
    usleep(500000);
    watcher.removeObserver(observer);
}


TEST(TestFileWatcher, testInstallWatcherOnNonExistingFile)
{
    FileWatcher watcher;
    ASSERT_FALSE(watcher.addPath(FixedPath("/tmp/monkeybrains")));
}

