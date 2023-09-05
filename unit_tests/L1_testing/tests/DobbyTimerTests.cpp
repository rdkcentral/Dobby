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
#include <memory>
#include <chrono>
#include <vector>
#include <set>

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include <Logging.h>

#include <gtest/gtest.h>

#include "DobbyTimer.h"


using namespace ::testing;

class DobbyTimerTests : public Test
{
public:
    void SetUp()
    {
        AICommon::initLogging();

        mTimer = std::make_shared<DobbyTimer>();

        mTimerEvents.reserve(1024);
    }

    void TearDown()
    {
        mTimer.reset();

        mTimerEvents.clear();
    }


protected:
    std::shared_ptr<DobbyTimer> mTimer;

public:
    typedef std::chrono::time_point<std::chrono::steady_clock> TimePoint;

    std::vector< std::pair<int, TimePoint> > mTimerEvents;

    bool onTimerEvent(int eventId)
    {
/*
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);

        fprintf(stderr, "%.010lu.%.06lu : eventId = %d\n",
                now.tv_sec, now.tv_nsec / 1000, eventId);
*/
        mTimerEvents.emplace_back(eventId, std::chrono::steady_clock::now());
        return true;
    }

    bool onTimerEventCancel(int eventId)
    {
        mTimerEvents.emplace_back(eventId, std::chrono::steady_clock::now());
        return false;
    }

    bool onTimerEventRemove(int eventId)
    {
        mTimerEvents.emplace_back(eventId, std::chrono::steady_clock::now());
        EXPECT_FALSE(mTimer->remove(eventId));
        return true;
    }


    long long timeDiff(const TimePoint& a, const TimePoint& b)
    {
        std::chrono::milliseconds diff = std::chrono::duration_cast<std::chrono::milliseconds>(a - b);
        return diff.count();
    }
};



TEST_F(DobbyTimerTests, testSimpleTimeout)
{
    TimePoint start = std::chrono::steady_clock::now();
    int id;

    // set 4 timers 100ms apart and ensure they all fire within sensible ranges
    id = mTimer->add(std::chrono::milliseconds(300), true, std::bind(&DobbyTimerTests::onTimerEvent, this, 300));
    ASSERT_GT(id, 0);

    id = mTimer->add(std::chrono::milliseconds(200), true, std::bind(&DobbyTimerTests::onTimerEvent, this, 200));
    ASSERT_GT(id, 0);

    id = mTimer->add(std::chrono::milliseconds(100), true, std::bind(&DobbyTimerTests::onTimerEvent, this, 100));
    ASSERT_GT(id, 0);

    id = mTimer->add(std::chrono::milliseconds(0), true, std::bind(&DobbyTimerTests::onTimerEvent, this, 0));
    ASSERT_GT(id, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    ASSERT_EQ(mTimerEvents.size(), 4U);

    ASSERT_EQ(mTimerEvents[0].first, 0);
    ASSERT_NEAR(timeDiff(mTimerEvents[0].second, start), 0, 50);

    ASSERT_EQ(mTimerEvents[1].first, 100);
    ASSERT_NEAR(timeDiff(mTimerEvents[1].second, start), 100, 50);

    ASSERT_EQ(mTimerEvents[2].first, 200);
    ASSERT_NEAR(timeDiff(mTimerEvents[2].second, start), 200, 50);

    ASSERT_EQ(mTimerEvents[3].first, 300);
    ASSERT_NEAR(timeDiff(mTimerEvents[3].second, start), 300, 50);
}

TEST_F(DobbyTimerTests, testSimplePeriodicTimer)
{
    const std::chrono::milliseconds period(20);
    const std::chrono::milliseconds testPeriod(300);

    int id = mTimer->add(period, false, std::bind(&DobbyTimerTests::onTimerEvent, this, 123));
    ASSERT_GT(id, 0);

    std::this_thread::sleep_for(testPeriod);


    ASSERT_NEAR(static_cast<long long>(mTimerEvents.size()),
                (testPeriod.count() / period.count()), 10LL);

    std::vector< std::pair<int, TimePoint> >::const_iterator event = mTimerEvents.begin() + 1;
    for (; event != mTimerEvents.end(); ++event)
    {
        ASSERT_EQ(event->first, 123);

        TimePoint prevTime = (event - 1)->second;
        long long diff = timeDiff(event->second, prevTime);

        ASSERT_NEAR(diff, period.count(), 50);
    }
}

TEST_F(DobbyTimerTests, testCancelPeriodicTimer)
{
    TimePoint start = std::chrono::steady_clock::now();

    int id = mTimer->add(std::chrono::milliseconds(0), false, std::bind(&DobbyTimerTests::onTimerEventCancel, this, 246));
    ASSERT_GT(id, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ASSERT_EQ(mTimerEvents.size(), 1U);

    ASSERT_EQ(mTimerEvents[0].first, 246);
    ASSERT_NEAR(timeDiff(mTimerEvents[0].second, start), 0, 50);

    ASSERT_FALSE(mTimer->remove(id));
}

TEST_F(DobbyTimerTests, testRemoveTimer)
{
    std::set<int> timerIds;

    // add 63 timers
    for (unsigned i = 0; i < 63; i++)
    {
        int id = mTimer->add(std::chrono::milliseconds(1000), false,
                         std::bind(&DobbyTimerTests::onTimerEvent, this, 246));
        ASSERT_GT(id, 0);

        timerIds.insert(id);
    }

    // check that 63 unique timer ids were added
    ASSERT_EQ(timerIds.size(), 63U);

    // adding a 64th should fail
    ASSERT_EQ(-1, mTimer->add(std::chrono::milliseconds(1000), false,
                              std::bind(&DobbyTimerTests::onTimerEvent, this, 246)));

    // remove all 63 timers
    for (int id : timerIds)
        ASSERT_TRUE(mTimer->remove(id));
    timerIds.clear();

    // check that none triggered
    ASSERT_EQ(mTimerEvents.size(), 0U);


    TimePoint start = std::chrono::steady_clock::now();
    int realTimerId = mTimer->add(std::chrono::milliseconds(100), true,
                                  std::bind(&DobbyTimerTests::onTimerEvent, this, 0xbeef));
    ASSERT_GT(realTimerId, 0);


    // add 62 timers
    for (unsigned i = 0; i < 62; i++)
    {
        int id = mTimer->add(std::chrono::milliseconds(1000 + (rand() % 1000)), false,
                             std::bind(&DobbyTimerTests::onTimerEvent, this, 246));
        ASSERT_GT(id, 0);

        timerIds.insert(id);
    }

    // check that 62 unique timer ids were added
    ASSERT_EQ(timerIds.size(), 62U);

    for (unsigned n = 0; n < 8; n++)
    {
        // remove half of the timers at random
        for (unsigned i = 0; i < 32; i++)
        {
            std::set<int>::iterator it = timerIds.begin();
            std::advance(it, (rand() % timerIds.size()));

            ASSERT_TRUE(mTimer->remove(*it));
            timerIds.erase(it);
        }

        // add another 32
        for (unsigned i = 0; i < 32; i++)
        {
            int id = mTimer->add(std::chrono::milliseconds(1000 + (rand() % 1000)), false,
                                 std::bind(&DobbyTimerTests::onTimerEvent, this, 246));
            ASSERT_GT(id, 0);

            timerIds.insert(id);
        }
    }

    // check that 62 unique timer ids were added
    ASSERT_EQ(timerIds.size(), 62U);

    // remove all 62 timers
    for (int id : timerIds)
        ASSERT_TRUE(mTimer->remove(id));
    timerIds.clear();


    // finally check that the real timer triggered
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    ASSERT_EQ(mTimerEvents.size(), 1U);
    ASSERT_EQ(mTimerEvents[0].first, 0xbeef);
    ASSERT_NEAR(timeDiff(mTimerEvents[0].second, start), 100, 50);

}

TEST_F(DobbyTimerTests, testRemoveInsideHandlerFails)
{
    volatile int id = -1;
    volatile bool handlerCalled = false;

    auto handler = [&](void)
    {
        handlerCalled = true;
        EXPECT_FALSE(mTimer->remove(id));
        return true;
    };

    // create a timer that tries to remove itself when called
    id = mTimer->add(std::chrono::milliseconds(0), true, handler);
    ASSERT_GT(id, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    ASSERT_TRUE(handlerCalled);
}
