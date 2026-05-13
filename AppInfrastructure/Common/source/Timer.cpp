/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2019 Sky UK
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
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Function:
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Timer.h"

#include <pthread.h>

using namespace AICommon;



///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Timer::~Timer()
{
    try {
        cancel();
    } catch (const std::exception& e) {
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Timer::start(const std::chrono::milliseconds &timeout,
                  TimerType type, TimerThreadPriority prio,
                  const std::function<void()> &callback)
{
    mCallback = callback;
    mCancel = false;

    // start the timer thread
    if (type == TimerType::OneRun)
    {
        mTimerThread = std::thread(&Timer::singleShotTimer, this, prio,
                                   (std::chrono::steady_clock::now() + timeout));
    }
    else
    {
        mTimerThread = std::thread(&Timer::recurringTimer, this, prio, timeout);
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Timer::cancel()
{
    // take the lock, set the cancel flag and wait for the thread to terminate
    {
        std::lock_guard<Mutex> locker(mLock);
        mCancel = true;
    }

    // check if being called from within the callback itself
    if (std::this_thread::get_id() != mTimerThread.get_id())
    {
        if (mTimerThread.joinable())
        {
            // notify the thread of the change in state
            mCond.notify_all();

            // wait for the thread to exit
            mTimerThread.join();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Timer::singleShotTimer(TimerThreadPriority prio,
                            const std::chrono::steady_clock::time_point &deadline)
{
    pthread_setname_np(pthread_self(), "AI_SINGLE_TIMER");

#if defined(ANDROID)
    // on android ignore the timer priority
    (void)prio;
#else
    if (prio == TimerThreadPriority::Low)
    {
        struct sched_param param = { 3 };
        pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    }
#endif

    std::unique_lock<Mutex> locker(mLock);

    while (!mCancel && (std::chrono::steady_clock::now() < deadline))
    {
        mCond.wait_until(locker, deadline);
    }

    if (!mCancel && mCallback)
    {
        locker.unlock();
        mCallback();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Timer::recurringTimer(TimerThreadPriority prio,
                           const std::chrono::milliseconds &interval)
{
    pthread_setname_np(pthread_self(), "AI_REPEAT_TIMER");

#if defined(ANDROID)
    // on android ignore the timer priority
    (void)prio;
#else
    if (prio == TimerThreadPriority::Low)
    {
        struct sched_param param = { 3 };
        pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    }
#endif

    std::unique_lock<Mutex> locker(mLock);

    std::chrono::steady_clock::time_point nextTimeout =
            std::chrono::steady_clock::now() + interval;

    while (!mCancel)
    {
        while (!mCancel && (std::chrono::steady_clock::now() < nextTimeout))
        {
            mCond.wait_until(locker, nextTimeout);
        }

        if (!mCancel)
        {
            nextTimeout += interval;

            if (mCallback)
            {
                locker.unlock();
                mCallback();
                locker.lock();
            }
        }
    }
}

