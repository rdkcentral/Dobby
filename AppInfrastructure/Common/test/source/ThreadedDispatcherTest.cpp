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
 * File:   ThreadedDispatcherTest.cpp
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 26 June 2014
 *
 * Copyright (C) BSKYB 2014
 */


#include <ThreadedDispatcher.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <unistd.h>
#include <memory>
#include <thread>

using namespace std;
using namespace std::placeholders;
using namespace testing;
using namespace AICommon;

namespace
{
void assign(bool& what, bool value)
{
    what = value;
}
}

TEST(ThreadedDispatcherTest, testPostedWorkIsDone)
{
    bool workDone = false;
    shared_ptr<ThreadedDispatcher> d = make_shared<ThreadedDispatcher>();

    d->post(bind(assign, ref(workDone), true));
    d->flush();

    EXPECT_EQ(true, workDone);
}

namespace
{
void getThreadId(thread::id testThread)
{
    EXPECT_NE(this_thread::get_id(), testThread);
}
}

TEST(ThreadedDispatcherTest, testWorkIsDoneOnDifferentThread)
{
    shared_ptr<ThreadedDispatcher> d = make_shared<ThreadedDispatcher>();
    d->post(bind(getThreadId, this_thread::get_id()));
    d->flush();
}

namespace
{
void getId(int& id)
{
    static int counter = 0;
    id = counter++;
}
}

TEST(ThreadedDispatcherTest, testWorkIsDoneInTheOrderItWasPosted)
{
    shared_ptr<ThreadedDispatcher> d = make_shared<ThreadedDispatcher>();
    int first = 0;
    int second = 0;
    d->post(bind(getId, ref(first)));
    d->post(bind(getId, ref(second)));
    d->flush();
    EXPECT_LT( first, second );
}

TEST(ThreadedDispatcherTest, testDispatcherStopDoesntDeadlock)
{
    shared_ptr<ThreadedDispatcher> d = make_shared<ThreadedDispatcher>();
    int first = 0;
    d->post(bind(getId, ref(first)));
    d->post(bind(getId, ref(first)));
    d->stop();
}

namespace
{
void increment(uint32_t& id)
{
    ++id;
}
void sleepyIncrement(uint32_t& id)
{
    ++id;
    usleep(10000);
}
}

TEST(ThreadedDispatcherTest, testDispatcherDoesALotOfWork)
{
    shared_ptr<ThreadedDispatcher> d = make_shared<ThreadedDispatcher>();
    uint32_t counter = 0;
    const uint32_t iterationCount = 100000;
    for(uint32_t i = 0; i < iterationCount; ++i)
    {
        d->post(bind(increment, ref(counter)));
    }
    d->flush();
    EXPECT_EQ(iterationCount, counter);
}

void notifyCv(mutex& m, condition_variable& cv)
{
    lock_guard<mutex> lock(m);
    cv.notify_one();
}

TEST(ThreadedDispatcherTest, testDispatcherWorkAddsMoreWork)
{
    auto d = make_shared<ThreadedDispatcher>();
    mutex m;
    condition_variable cv;
    unique_lock<mutex> lock(m);

    function<void ()> f = bind(notifyCv, ref(m), ref(cv));
    d->post(bind(&IDispatcher::post, d, ref(f)));

    EXPECT_EQ(cv_status::no_timeout, cv.wait_for(lock, std::chrono::seconds(5)));

    d->flush();
}

TEST(ThreadedDispatcherTest, testDispatcherSync)
{
    shared_ptr<ThreadedDispatcher> d = make_shared<ThreadedDispatcher>();
    uint32_t counter = 0;
    const uint32_t iterationCount = 100;
    for(uint32_t i = 0; i < iterationCount; ++i)
    {
        d->post(bind(sleepyIncrement, ref(counter)));
    }
    d->sync();
    EXPECT_EQ(iterationCount, counter);
    ASSERT_TRUE(!d->invokedFromDispatcherThread());
}

