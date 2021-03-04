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
 * File:   ThreadedDispatcher.h
 *
 * Created on 26 June 2014
 *
 */

#ifndef THREADEDDISPATCHER_H
#define	THREADEDDISPATCHER_H

#include <IDispatcher.h>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <string>
#include <memory>

namespace AICommon
{

/**
 * @brief A dispatcher that does all the work on a single, separate thread
 *        started in constructor.
 */
class ThreadedDispatcher : public IDispatcher
{
public: //IDispatcher

    /**
     * Post an item of work to be executed on the thread owned by this dispatcher.
     */
    virtual void post(std::function<void ()> work) final;

    /**
     * @brief Ensures that anything that was in the queue before the call has been
     * executed before returning.
     */
    virtual void sync() final;

    /**
     * @brief Get dispatcher thread Id.
     */
    virtual bool invokedFromDispatcherThread() final;

public: //this class

    /**
     * @brief Perform any work remaining in the queue, then stop accepting new work.
     */
    void flush();

    /**
     * @brief Cancels any work that is not already in progress, stop accepting new work
     */
    void stop();

    ThreadedDispatcher(const std::string& name = std::string());

    /**
     * Creates a dispatcher with supplied SCHED_RR priority value and a thread name
     */
    ThreadedDispatcher(int priority, const std::string& name = std::string());

    ~ThreadedDispatcher();

private:

    /**
     * @brief Predicate for condition variable used communication with the worker thread.
     */
    bool hasMoreWorkOrWasStopRequested();

    /**
     * @brief The dispatcher thread entry point.
     */
    void doWork(const std::string& name, int priority);

    /**
    * @brief Returns next work item.
    * @note This function assumes that there is work to be done and that the mutex
    *       is acquired.
    */
    inline std::function<void ()> next();

    std::mutex m;
    std::condition_variable cv;
    bool running;
    std::deque<std::function<void ()>> q;
    std::thread t;

    typedef ThreadedDispatcher This;
};

} //AICommon

#endif	/* THREADEDDISPATCHER_H */

