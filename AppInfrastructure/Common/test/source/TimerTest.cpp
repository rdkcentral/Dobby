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
#include <Timer.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <chrono>
#include <atomic>


using namespace std;
using namespace testing;
using namespace AICommon;
using namespace std::chrono;

TEST(TimerTest, setTimerAndWaitForAction)
{
    bool complete = false;
    Timer t(milliseconds(100), [&complete](){ complete = true; });
    this_thread::sleep_for(milliseconds(150));
    EXPECT_TRUE(complete);
}

TEST(TimerTest, setRecurringTimerAndWaitForAction)
{
    atomic<int> counter(0);
    Timer t(milliseconds(100), TimerType::Recurring, TimerThreadPriority::Default, [&counter](){ counter++; });
    this_thread::sleep_for(milliseconds(250));
    t.cancel();
    EXPECT_EQ(2, counter);
}

TEST(TimerTest, setRecurringTimerAndCancel)
{
    atomic<int> counter(0);
    Timer t(milliseconds(100), TimerType::Recurring, TimerThreadPriority::Default, [&counter](){ counter++; });
    t.cancel();
    this_thread::sleep_for(milliseconds(250));
    EXPECT_EQ(0, counter);
}

TEST(TimerTest, setTimerAndCancel)
{
    bool complete = false;
    Timer t( milliseconds(100), [&complete](){ complete = true; });
    t.cancel();
    this_thread::sleep_for(milliseconds(150));
    EXPECT_FALSE(complete);
}

TEST(TimerTest, testActionParameterBinding)
{
    bool complete = false;
    auto func = [&complete](int magic)
        {
            if(magic == 42)
            {
                complete = true;
            }
        };

    Timer t(milliseconds(100), func, 5);
    this_thread::sleep_for(milliseconds(150));
    EXPECT_FALSE(complete);

    Timer t2(milliseconds(100), func, 42);
    this_thread::sleep_for(milliseconds(150));
    EXPECT_TRUE(complete);
}

TEST(TimerTest, testDoubleCancel)
{
    Timer t(milliseconds(1000), [](){});
    t.cancel();
    t.cancel();
}

class Foo
{
public:
    void square(int x)
    {
       cout<<x*x<<endl;
    }
};

TEST(TimerTest, testMemberFunctionBind)
{
    Foo f;
    Timer t(milliseconds(0), &Foo::square, &f, 7);
}
