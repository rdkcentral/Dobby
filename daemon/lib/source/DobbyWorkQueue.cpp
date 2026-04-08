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
 * File:   DobbyWorkQueue.cpp
 *
 */

#include "DobbyWorkQueue.h"

#include <Logging.h>




DobbyWorkQueue::DobbyWorkQueue()
    : mWorkCounter(0)
    , mExitRequested(false)
    , mWorkCompleteCounter(0)
{
}

DobbyWorkQueue::~DobbyWorkQueue()
{
    if (!mWorkQueue.empty())
    {
        AI_LOG_WARN("destroying work queue with work items still in the queue");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Unblocks the runXXX functions.
 *
 *
 */
void DobbyWorkQueue::exit()
{
    // if already on the event loop thread then just set the flag
    if (std::this_thread::get_id() == mRunningThreadId)
    {
        mExitRequested = true;
    }
    else
    {
        // take the lock and set the terminate flag
        std::unique_lock<AICommon::Mutex> locker(mWorkQueueLock);

        // set the exit request flag and drop the lock
        mExitRequested = true;
        locker.unlock();

        // wake the event loop thread
        mWorkQueueCond.notify_all();
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the event loop.
 *
 *  This will block - running the event loop until exit() is called.
 *
 */
void DobbyWorkQueue::run()
{
    runUntil(std::chrono::steady_clock::time_point::max());
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the event loop for msecs milliseconds.
 *
 *  This will block - running the event loop for a fixed amount of time.  This
 *  will return when either the timeout expires or exit() is called.
 *
 *  @param[in]  deadline    The deadline time.
 *
 *  @return true.
 */
bool DobbyWorkQueue::runFor(const std::chrono::milliseconds &msecs)
{
    return runUntil(std::chrono::steady_clock::now() + msecs);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the event loop until the deadline time passes.
 *
 *  This will block - running the event loop for a fixed amount of time.  This
 *  will return when either the timeout expires or exit() is called.
 *
 *  @param[in]  deadline    The deadline time.
 *
 *  @return true.
 */
bool DobbyWorkQueue::runUntil(const std::chrono::steady_clock::time_point &deadline)
{
    // wait for the next work item added or mTerminateWorkQueue is true
    const auto predicate = [&]()
    {
        return mExitRequested || !mWorkQueue.empty();
    };

    // run the event loop by processing all lambdas posted to the work queue
    std::unique_lock<AICommon::Mutex> locker(mWorkQueueLock);

    // store the id of the thread running the loop
    mRunningThreadId = std::this_thread::get_id();

    while (!mExitRequested)
    {
        while (!mWorkQueue.empty())
        {
            WorkItem work = std::move(mWorkQueue.front());
            mWorkQueue.pop();

            locker.unlock();

            if (work.func)
                work.func();

            // signal completion of work item
            mWorkCompleteLock.lock();
            mWorkCompleteCounter = work.tag;
            mWorkCompleteLock.unlock();
            mWorkCompleteCond.notify_all();

            // re-take the queue lock before checking again
            locker.lock();
        }

        // wait for the next work item added or mTerminateWorkQueue is true
        if (deadline == std::chrono::steady_clock::time_point::max())
        {
            mWorkQueueCond.wait(locker, predicate);
        }
        else if (!mWorkQueueCond.wait_until(locker, deadline, predicate))
        {
            break; // timed out
        }
    }

    // make a best effort to ensure we leave no work items in the queue
    while (!mWorkQueue.empty())
    {
        WorkItem work = std::move(mWorkQueue.front());
        mWorkQueue.pop();

        if (work.func)
            work.func();

        // signal completion of work item
        mWorkCompleteLock.lock();
        mWorkCompleteCounter = work.tag;
        mWorkCompleteLock.unlock();
        mWorkCompleteCond.notify_all();
    }

    // clear the running thread id
    mRunningThreadId = std::thread::id();

    bool result = mExitRequested;
    mExitRequested = false;

    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Posts a work job onto the queue and waits till it completes.
 *
 *  This queues the work item and then returns.  It is thread safe, it
 *  is safe to call from the thread running the event loop or from another
 *  thread.
 *
 *  @param[in]  work            The work function to execute.
 *
 *  @return true.
 */
bool DobbyWorkQueue::doWork(WorkFunc &&work)
{
    // if already on the event loop thread then just execute the function
    if (std::this_thread::get_id() == mRunningThreadId)
    {
        work();
        return true;
    }

    // otherwise add to the queue and return
    std::unique_lock<AICommon::Mutex> queueLocker(mWorkQueueLock);

    // add to the queue
    const uint64_t tag = ++mWorkCounter;
    mWorkQueue.emplace(tag, std::move(work));

    queueLocker.unlock();

    // wake the event loop
    mWorkQueueCond.notify_one();


    // then wait for the function to be executed
    std::unique_lock<AICommon::Mutex> completeLocker(mWorkCompleteLock);
    while (mWorkCompleteCounter < tag)
    {
        // wait with a timeout for debugging, we log an error if been waiting
        // for over a second, which would indicate a lock up somewhere
        if (mWorkCompleteCond.wait_for(completeLocker, std::chrono::seconds(1)) == std::cv_status::timeout)
        {
            AI_LOG_WARN("been waiting for over a second for function to "
                        "execute, soft lock-up occurred?");
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Posts a work job onto the queue.
 *
 *  This just queues the work item and then returns.  It is thread safe, it
 *  is safe to call from the thread running the event loop or from another
 *  thread.
 *
 *  @param[in]  work            The work function to execute.
 *
 *  @return true.
 */
bool DobbyWorkQueue::postWork(WorkFunc &&work)
{
    // if already on the event loop thread then we don't need to take the
    // lock and can just push a new work item into the queue
    if (std::this_thread::get_id() == mRunningThreadId)
    {
        // add to the queue
        std::unique_lock<AICommon::Mutex> locker(mWorkQueueLock);
        const uint64_t tag = ++mWorkCounter;
        mWorkQueue.emplace(tag, std::move(work));
    }
    else
    {
        // otherwise add to the queue and wait till processed
        std::unique_lock<AICommon::Mutex> locker(mWorkQueueLock);

        // add to the queue
        const uint64_t tag = ++mWorkCounter;
        mWorkQueue.emplace(tag, std::move(work));

        locker.unlock();

        // wake the event loop
        mWorkQueueCond.notify_one();
    }

    return true;
}


