/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2014 Sky UK
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
 * File:   ThreadedDispatcher.cpp
 *
 * Created on 26 June 2014
 *
 */
#include "ThreadedDispatcher.h"
#include <memory>
#include <thread>
#include <Logging.h>
#include <sstream>
namespace AICommon
{
ThreadedDispatcher::ThreadedDispatcher(const std::string& name /*= std::string()*/)
: ThreadedDispatcher(-1, name)
{
}
ThreadedDispatcher::ThreadedDispatcher(int priority, const std::string& name /*= std::string()*/)
: running(true)
, t(std::thread(&ThreadedDispatcher::doWork, this, name, priority))
{
}
void ThreadedDispatcher::post(std::function<void ()> work)
{
    std::unique_lock<std::mutex> lock(m);
    if(running)
    {
        q.push_back(work);
        lock.unlock();
        cv.notify_one();
    }
    else
    {
        AI_LOG_WARN("Ignoring work because the dispatcher is not running anymore");
        //can't throw an exception here because if this is executed from destructor,
        //which occurs when work adds more work things go horribly wrong.
        //Instead, ignore work.
    }
}
namespace
{
// -----------------------------------------------------------------------------
/**
 *  @brief Work item callback for the sync method.
 *
 *  This function is put on the queue by the sync method, it simply sets a
 *  boolean flag and notifies the conditional variable.
 *
 *  @param[in]  lock    The mutex lock to hold when setting the fired param
 *  @param[in]  cond    The conditional variable to use to wake up the caller
 *  @param[in]  fired   Reference to a boolean variable to set to true
 *
 */
void syncCallback(std::mutex* lock, std::condition_variable* cond, bool* fired)
{
    std::unique_lock<std::mutex> locker(*lock);
    *fired = true;
    cond->notify_all();
    locker.unlock();
}
} // namespace
/**
 * @brief Get dispatcher thread Id.
 */
bool ThreadedDispatcher::invokedFromDispatcherThread()
{
    bool res = (std::this_thread::get_id() == t.get_id());
    if (res)
    {
		std::stringstream ss;
        ss << "Caller thread Id [" << std::this_thread::get_id() << "] == [dispatcher thread Id " << t.get_id() << "]";
        AI_LOG_ERROR("%s", ss.str().c_str());
    }
    return res;
}
// -----------------------------------------------------------------------------
/**
 *  @brief Ensures that any items in the dispatch queue before this call are
 *  processed before the function returns.
 *
 *  The function blocks until everything in the queue prior to the call is
 *  processed.
 *
 *  It works by putting a dummy work item on the queue which takes a reference
 *  to a local conditional variable, we then wait on the conditional triggering.
 *
 */
void ThreadedDispatcher::sync()
{
    std::mutex lock;
    std::condition_variable cond;
    bool fired = false;
    // Take the queue lock and ensure we're still running
    std::unique_lock<std::mutex> qlocker(m);
    if (!running)
    {
        AI_LOG_DEBUG("Ignoring sync because dispatcher is not running");
        return;
    }
    // Add the work object to the queue which takes the lock and sets 'fired' to true
    q.push_back(std::bind(syncCallback, &lock, &cond, &fired));
    qlocker.unlock();
    cv.notify_one();
    // Wait for 'fired' to become true
    std::unique_lock<std::mutex> locker(lock);
    while (!fired)
    {
        cond.wait(locker);
    }
}
namespace
{
void unlockAndSetFlagToFalse(std::mutex& m, bool& flag)
{
    using namespace std;
    m.unlock();
    flag = false;
}
}
/**
 * @brief Perform any work remaining in the queue, then stop accepting new work.
 */
void ThreadedDispatcher::flush()
{
    //To ensure all the work that is in the queue is done, we lock a mutex.
    //post a job to the queue that unlocks it and stops running further jobs.
    //Then block here until that's done.
    if(running)
    {
        std::mutex m2;
        m2.lock();
        post(bind(unlockAndSetFlagToFalse, std::ref(m2), std::ref(this->running)));
        m2.lock();
        m2.unlock();
        stop();
    }
    else
    {
        AI_LOG_WARN("This dispatcher is no longer running. Ignoring flush request.");
    }
}
/**
 * @brief Cancels any work that is not already in progress, stop accepting new work
 */
void ThreadedDispatcher::stop()
{
    std::unique_lock<std::mutex> lock(m);
    running = false;
    lock.unlock();
    cv.notify_one();
    t.join();
}
ThreadedDispatcher::~ThreadedDispatcher()
{
    if(running)
    {
        stop();
    }
}
bool ThreadedDispatcher::hasMoreWorkOrWasStopRequested()
{
    return !q.empty() || !running;
}
void ThreadedDispatcher::doWork(const std::string& name, int priority)
{
    const char *threadName = name.empty() ? "AI_THR_DISPATCH" : name.c_str();
    pthread_setname_np(pthread_self(), threadName);
    if (priority > 0)
    {
        struct sched_param param;
        param.sched_priority = priority;
        int err = pthread_setschedparam(pthread_self(), SCHED_RR, &param);
        if (err != 0)
        {
            AI_LOG_SYS_ERROR(err, "Failed to set thread priority to %d", priority);
        }
    }
    std::unique_lock<std::mutex> lock(m);
    while(running)
    {
        using namespace std;
        cv.wait(lock, bind(&This::hasMoreWorkOrWasStopRequested, this));
        if(!q.empty())
        {
            std::function<void ()> work = next();
            //don't block adding things to work queue while dispatcher does the work.
            lock.unlock();
            work();
            lock.lock();
        }
    }
}
std::function<void ()> ThreadedDispatcher::next()
{
    auto work = std::move(q.front());
    q.pop_front();
    return work;
}
} //AICommon