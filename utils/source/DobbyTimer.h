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
 * File:   DobbyTimer.h
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef DOBBYTIMER_H
#define DOBBYTIMER_H

#include <IDGenerator.h>

#include <time.h>

#include <set>
#include <thread>
#include <mutex>
#include <chrono>


// -----------------------------------------------------------------------------
/**
 *  @class DobbyTimer
 *  @brief Utility object that can be used to register a callback function to
 *  execute in the future
 *
 *  Multiple callbacks can be registered via this object, internally it runs a
 *  thread with a single timerfd that wakes up at the correct time and then
 *  calls any handlers registered.
 *
 *  All callbacks are processed in the same thread, so obviously one timer
 *  handler can block all the others, clients should bear this in mind.
 *
 *  @warning Currently if you try and call DobbyTimer::remove from inside the
 *  handler callback function it will return with an error.  If you want to
 *  stop a repeating timer then return false from the handler.
 *
 *  @warning This object currently only supports a maximum of 63 timers.
 *
 */
class DobbyTimer
{
public:
    DobbyTimer();
    ~DobbyTimer();

public:
    void stop();

public:
    int add(const std::chrono::milliseconds& timeout, bool oneShot,
            const std::function<bool()>& func);
    bool remove(int timerId);

private:
    void timerThread();

    void updateTimerFd() const;
    struct timespec calcAbsTime(const struct timespec& now,
                                const std::chrono::milliseconds& timeout) const;


private:
    typedef struct tagTimerEntry
    {
        int id;
        bool oneshot;
        struct timespec expiry;
        std::function<bool()> func;
        std::chrono::milliseconds timeout;

        bool isLessThanOrEqualTo(const struct timespec& rhs) const
        {
            return (expiry.tv_sec < rhs.tv_sec) ||
                    ((expiry.tv_sec == rhs.tv_sec) &&
                     (expiry.tv_nsec <= rhs.tv_nsec));
        }

    } TimerEntry;

    class TimerEntryCompare
    {
    public:
        bool operator() (const TimerEntry& a, const TimerEntry& b)
        {
            return (a.expiry.tv_sec < b.expiry.tv_sec) ||
                   ((a.expiry.tv_sec == b.expiry.tv_sec) &&
                    (a.expiry.tv_nsec < b.expiry.tv_nsec));
        }
    };

    std::multiset<TimerEntry, TimerEntryCompare> mTimersQueue;

private:
    std::recursive_mutex mLock;
    std::thread mThread;
    int mTimerFd;
    int mEventFd;

    AICommon::IDGenerator<6> mIdGenerator;
};


#endif // !defined(DOBBYTIMER_H)
