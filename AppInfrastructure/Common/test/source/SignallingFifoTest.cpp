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
 * File:   SignalingFifoTest.cpp
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 11 Feb 2015
 *
 * Copyright (C) Sky UK 2015
 */

#include <SignallingFifo.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <future>
#include <futureHack.h>
#include <unistd.h>
#include <chrono>

#include "SignallingFifo.h"

using namespace std;
using namespace std::placeholders;
using namespace testing;
using namespace AICommon;

using namespace std::chrono;

TEST(SignallingFifoTest, testPopReturnsWhatsPushed)
{
    SignallingFifo<int> fifo;
    fifo.push(5);
    auto val = fifo.pop(0);
    ASSERT_TRUE((bool)val);
    EXPECT_EQ(5, *val);
}

TEST(SignallingFifoTest, testPopEmptyQueue)
{
    SignallingFifo<int> fifo;
    auto val = fifo.pop(0);
    ASSERT_FALSE((bool)val);
}

TEST(SignallingFifoTest, testPopEmptyQueueWithTimeout)
{
    SignallingFifo<int> fifo;
    auto start = system_clock::now();
    auto val = std::async(std::launch::async, bind(&SignallingFifo<int>::pop, ref(fifo), 100));
    //generous timeout in case build servers get slow.
    ASSERT_EQ(future_status::ready, (future_status)val.wait_for(std::chrono::seconds(5)));
    auto stop = system_clock::now();
    EXPECT_FALSE((bool)val.get());
    EXPECT_GE(duration_cast<milliseconds>(stop - start).count(), milliseconds(100).count())<<"Should have waited at least 100ms.";
}

TEST(SignallingFifoTest, testPushWhilePopIsWaiting)
{
    SignallingFifo<int> fifo;
    auto start = system_clock::now();
    auto fut = std::async(std::launch::async, bind(&SignallingFifo<int>::pop, ref(fifo), 5000));
    usleep(microseconds(100000).count());
    fifo.push(5);
    ASSERT_EQ(future_status::ready, (future_status)fut.wait_for(std::chrono::seconds(5)));
    auto stop = system_clock::now();
    auto val = fut.get();
    ASSERT_TRUE((bool)val);
    EXPECT_EQ(5, *val);
    EXPECT_GE(duration_cast<milliseconds>(stop - start).count(), milliseconds(100).count())<<"Should have waited at least 100ms.";
}

