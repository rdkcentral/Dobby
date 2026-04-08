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
/*
 * File:   DobbyWorkQueue.h
 *
 */

#ifndef DOBBYWORKQUEUE_H
#define DOBBYWORKQUEUE_H

#include <string>
#include <chrono>
#include <queue>
#include <atomic>
#include <thread>
#include <mutex>
#include "ConditionVariable.h"
#include <functional>

class DobbyWorkQueue
{
public:
    DobbyWorkQueue();
    ~DobbyWorkQueue();

    void run();
    bool runFor(const std::chrono::milliseconds &msecs);
    bool runUntil(const std::chrono::steady_clock::time_point &deadline);

    void exit();

public:
    using WorkFunc = std::function<void()>;

    bool doWork(WorkFunc &&work);
    bool postWork(WorkFunc &&work);

private:
    struct WorkItem
    {
        uint64_t tag;
        WorkFunc func;

        WorkItem(uint64_t t, WorkFunc &&f)
            : tag(t), func(std::move(f))
        { }
    };

    std::atomic<uint64_t> mWorkCounter;

    std::atomic<bool> mExitRequested;
    std::atomic<std::thread::id> mRunningThreadId;

    AICommon::Mutex mWorkQueueLock;
    AICommon::ConditionVariable mWorkQueueCond;
    std::queue< WorkItem > mWorkQueue;

    AICommon::Mutex mWorkCompleteLock;
    AICommon::ConditionVariable mWorkCompleteCond;
    std::atomic<uint64_t> mWorkCompleteCounter;
};


#endif // DOBBYWORKQUEUE_H
