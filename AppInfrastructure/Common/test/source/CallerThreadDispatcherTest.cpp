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
 * File:   CallerThreadDispatcherTest.cpp
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 26 June 2014
 *
 * Copyright (C) Sky UK 2014
 */


#include <CallerThreadDispatcher.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
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

TEST(CallerTreadDispatcherTest, testPostedWorkIsDone)
{
    bool workDone = false;
    shared_ptr<IDispatcher> d = make_shared<CallerThreadDispatcher>();

    d->post(bind(assign, ref(workDone), true));

    EXPECT_EQ(workDone, true);
}

namespace
{
void getThreadId(thread::id& thread)
{
    thread = this_thread::get_id();
}
}

TEST(CallerTreadDispatcherTest, testWorkIsDoneOnSameThread)
{
    thread::id thread;
    shared_ptr<IDispatcher> d = make_shared<CallerThreadDispatcher>();

    d->post(bind(getThreadId, ref(thread)));

    EXPECT_EQ(this_thread::get_id(), thread);
    EXPECT_TRUE(!d->invokedFromDispatcherThread());
}
