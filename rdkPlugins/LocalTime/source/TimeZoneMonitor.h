/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 Sky UK
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
#ifndef TIMEZONEMONITOR_H
#define TIMEZONEMONITOR_H

#include <set>
#include <mutex>
#include <thread>
#include <vector>
#include <filesystem>
#include <cstdint>

class TimeZoneMonitor
{
public:
    TimeZoneMonitor(std::filesystem::path timeZoneFilePath = "/etc/localtime");
    ~TimeZoneMonitor();

    void addPathToUpdate(const std::filesystem::path &path);
    void removePathToUpdate(const std::filesystem::path &path);

private:
    void monitorLoop();
    void processInotifyEvents(int inotifyFd);
    void recheckTimeZoneFile();
    void updateTimeZoneFile(const std::filesystem::path &linkPath,
                            const std::vector<uint8_t> &tzData);

private:
    const std::filesystem::path mTimeZoneFilePath;
    const std::string mTimeZoneFileName;

    std::thread mMonitorThread;
    int mStopEventFd = -1;
    std::mutex mLock;
    std::vector<uint8_t> mTimeZoneFileContent;
    std::set<std::filesystem::path> mPathsToUpdate;
};

#endif // TIMEZONEMONITOR_H
