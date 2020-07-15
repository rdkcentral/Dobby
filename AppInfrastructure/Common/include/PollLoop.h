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
 * File:   PollLoop.h
 *
 * Copyright (C) BSKYB 2015+
 */
#ifndef POLLLOOP_H
#define POLLLOOP_H

#include "IPollLoop.h"
#include "SpinLock.h"

#include <strings.h>
#include <sys/timerfd.h>

#include <atomic>
#include <thread>
#include <string>
#include <mutex>
#include <list>
#include <map>

namespace AICommon
{


// -----------------------------------------------------------------------------
/**
 *  @class PollLoop
 *  @brief A wrapper around epoll that allows for adding, modifing & deleting
 *  of source events.
 *
 *  Poll loop sources are a tuple of a shared_ptr<IPollSource>, an fd and a
 *  bitmask of events to listen on.  PollSource objects are store as weak_ptrs
 *  and only locked when they have been triggered and their process() function
 *  is to be called.
 *
 *  This should make the race conditions with calling an object that has been
 *  destroy safe, however it does means that PollSource objects shouldn't
 *  assume that their process() methods won't be called after they've been
 *  removed from the poll loop.
 *
 */
class PollLoop : public IPollLoop, public std::enable_shared_from_this<PollLoop>
{
public:
    PollLoop(const std::string& name, int maxSources = 512, long deferredTimeInterval = 20);
    virtual ~PollLoop();

public:
    virtual bool start(int priority = -1) override;
    virtual void stop() override;

    virtual bool addSource(const std::shared_ptr<IPollSource>& source, int fd, uint32_t events) override;
    virtual bool modSource(const std::shared_ptr<IPollSource>& source, uint32_t events) override;
    virtual void delSource(const std::shared_ptr<IPollSource>& source) override;

    virtual std::thread::id threadId() const override;
    virtual pid_t gettid() const override;

private:
    void init();
    void run(const std::string& name, int priority);

    inline void enableDeferredTimer();
    inline void disableDeferredTimer();

private:
    // The name given to the poll thread
    const std::string mName;

    std::thread mEPollThread;

    // The TID of the poll thread
    std::atomic<pid_t> mEPollThreadId;

    // The actual epoll descriptor, on valid when the epoll loop is running
    int mEPollFd;

    // The eventfd used to kill the thread (on stop())
    int mDeathEventFd;

    // A timerfd that is used to wake up epoll sources that previous asked to
    // defer their processing
    int mDeferTimerFd;

    // The time period that the defer timer fires
    const struct itimerspec mDeferTimerSpec;

    // Spinlock protecting access to the source list
    Spinlock mLock;

    // The maximum number of sources that can be added to the poll loop
    const int mMaxSources;

    // The number of sources that currently have the EPOLLDEFERRED flag set
    int mDeferredSources;

private:
    typedef struct tagPollSourceWrapper
    {
        tagPollSourceWrapper(std::shared_ptr<IPollSource> _source, int _fd, uint32_t _events)
            : source(_source)
            , fd(_fd)
            , events(_events)
        { }

        std::weak_ptr<IPollSource> source;
        int fd;
        uint32_t events;

    } PollSourceWrapper;

    std::list<PollSourceWrapper> mSources;
};

} // namespace AICommon

#endif // !defined(POLLLOOP_H)
