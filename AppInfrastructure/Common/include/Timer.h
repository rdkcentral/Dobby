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
 * File:   Timer.h
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 29 May 2015
 */

#ifndef AICOMMON_TIMER_H
#define AICOMMON_TIMER_H

#include "Mutex.h"
#include "ConditionVariable.h"

#include <chrono>
#include <thread>
#include <functional>

namespace AICommon
{
    enum class TimerType
    {
        OneRun = 0,
        Recurring = 1
    };

    enum class TimerThreadPriority
    {
        Default,
        Low
    };


    class Timer
    {

    public:
        /**
         * @brief Starts a timer that will expire after timeout and execute action.
         * @param The second parameter is a function to be called on expiry, and third
         *        an later are (optional) arguments to it.
         *        eg.:
         *        auto t1 = Timer(minutes(30), orderBeer, 5, "Fursty Ferret");
         *        auto t2 = Timer(hours(3), leavePub);
         */

        template<typename F, typename... Args>
        Timer(const std::chrono::milliseconds &timeout, F f, Args&&... args)
        {
            start(timeout, TimerType::OneRun, TimerThreadPriority::Default, std::bind(f, args...));
        }

        template<typename F, typename... Args>
        Timer(const std::chrono::milliseconds &timeout, TimerType type, TimerThreadPriority prio, F f, Args&&... args)
        {
            start(timeout, type, prio, std::bind(f, args...));
        }

        Timer(Timer&& other) = delete;
        Timer(const Timer&) = delete;

        Timer& operator=(Timer&& other) = delete;
        Timer& operator=(const Timer& other) = delete;

        /**
         * @brief The destructor cancels the timer if it's still not expired.
         * @note If the action is executing it will block until it finishes.
         */
        ~Timer();

        /**
         * @brief Cancels the timer.
         *
         * @note if you call it more than once, subsequent calls will be ignored.
         */
        void cancel();


    private:
        void start(const std::chrono::milliseconds &timeout, TimerType type, TimerThreadPriority prio, const std::function<void()> &callback);

        void singleShotTimer(TimerThreadPriority prio, const std::chrono::steady_clock::time_point &deadline);
        void recurringTimer(TimerThreadPriority prio, const std::chrono::milliseconds &interval);

    private:
        std::function<void()> mCallback;

        std::thread mTimerThread;
        Mutex mLock;
        ConditionVariable mCond;
        bool mCancel;

    };

} // namespace AICommon

#endif   // !defined(AICOMMON_TIMER_H)

