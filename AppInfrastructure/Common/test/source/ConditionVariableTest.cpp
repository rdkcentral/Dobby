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
#include <ConditionVariable.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <thread>
#include <mutex>

// All of these tests are 'borrowed' from the gcc libstdc++-v3 testsuite

typedef AICommon::Mutex mutex_type;
typedef AICommon::ConditionVariable condition_variable_type;

// Uncomment the following to sanity tests the tests
// typedef std::mutex mutex_type;
// typedef std::condition_variable condition_variable_type;

class ConditionVariablePR54185Test : public ::testing::Test
{
protected:
    condition_variable_type* cond = nullptr;
    mutex_type mx;
    int started;
    int NUM_THREADS;

protected:
    void SetUp()
    {
        started = 0;
        NUM_THREADS = 10;
    }

public:
    void do_thread_a()
    {
        std::unique_lock<mutex_type> lock(mx);
        if(++started >= NUM_THREADS)
        {
            cond->notify_all();
            delete cond;
            cond = nullptr;
        }
        else
            cond->wait(lock);
    }
};

TEST_F(ConditionVariablePR54185Test, pr54185)
{
    std::vector<std::thread> vec;
    for(int j = 0; j < 1000; ++j)
    {
        started = 0;
        cond = new condition_variable_type;
        for (int i = 0; i < NUM_THREADS; ++i)
            vec.emplace_back(&ConditionVariablePR54185Test::do_thread_a, this);
        for (int i = 0; i < NUM_THREADS; ++i)
            vec[i].join();
        vec.clear();
    }
}

TEST(ConditionVariableTest, cons_1)
{
    try
    {
        condition_variable_type c1;
    }
    catch (const std::system_error& e)
    {
        FAIL() << e.what();
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(ConditionVariableTest, members_1)
{
    try
    {
        std::chrono::microseconds ms(500);
        condition_variable_type c1;
        mutex_type m;
        std::unique_lock<mutex_type> l(m);

        auto then = std::chrono::system_clock::now();
        std::cv_status result = c1.wait_for(l, ms);
        ASSERT_EQ( result, std::cv_status::timeout );
        ASSERT_GE( (std::chrono::system_clock::now() - then), ms );
        ASSERT_TRUE( l.owns_lock() );
    }
    catch (const std::system_error& e)
    {
        FAIL() << e.what();
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(ConditionVariableTest, members_2)
{
    try
    {
        std::chrono::microseconds ms(500);
        condition_variable_type c1;
        mutex_type m;
        std::unique_lock<mutex_type> l(m);

        auto then = std::chrono::steady_clock::now();
        std::cv_status result = c1.wait_until(l, then + ms);
        ASSERT_EQ( result, std::cv_status::timeout );
        ASSERT_GE( (std::chrono::steady_clock::now() - then), ms );
        ASSERT_TRUE( l.owns_lock() );
    }
    catch (const std::system_error& e)
    {
        FAIL() << e.what();
    }
    catch (...)
    {
        FAIL();
    }
}

static void triggerAll(const std::chrono::milliseconds& delay,
                       condition_variable_type& cv)
{
    std::this_thread::sleep_for(delay);
    cv.notify_all();
}

static void setValue(const std::chrono::milliseconds& delay,
                     bool& value)
{
    std::this_thread::sleep_for(delay);
    value = true;
}

static void triggerAllWithValue(const std::chrono::milliseconds& delay,
                                condition_variable_type& cv,
                                mutex_type& m,
                                bool& value)
{
    std::this_thread::sleep_for(delay);
    std::unique_lock<mutex_type> locker(m);
    value = true;
    cv.notify_all();
}


TEST(ConditionVariableTest, basic_1)
{
    try
    {
        condition_variable_type cv;
        mutex_type m;

        std::unique_lock<mutex_type> l(m);
        std::thread t = std::thread(&triggerAll, std::chrono::milliseconds(500),
                                    std::ref(cv));

        cv.wait(l);

        ASSERT_TRUE(t.joinable());
        t.join();
    }
    catch (const std::system_error& e)
    {
        FAIL() << e.what();
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(ConditionVariableTest, basic_2)
{
    try
    {
        bool value = false;
        condition_variable_type cv;
        mutex_type m;

        std::unique_lock<mutex_type> l(m);
        std::thread t = std::thread(triggerAllWithValue,
                                    std::chrono::milliseconds(500),
                                    std::ref(cv), std::ref(m), std::ref(value));

        cv.wait(l, [&]{ return value == true; });

        ASSERT_TRUE(t.joinable());
        t.join();
    }
    catch (const std::system_error& e)
    {
        FAIL() << e.what();
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(ConditionVariableTest, basic_3)
{
    try
    {
        condition_variable_type cv;
        mutex_type m;

        std::unique_lock<mutex_type> l(m);
        std::thread t = std::thread(triggerAll, std::chrono::milliseconds(500),
                                    std::ref(cv));

        std::cv_status result = cv.wait_for(l, std::chrono::milliseconds(1000));
        EXPECT_EQ(result, std::cv_status::no_timeout);

        ASSERT_TRUE(t.joinable());
        t.join();
    }
    catch (const std::system_error& e)
    {
        FAIL() << e.what();
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(ConditionVariableTest, basic_4)
{
    try
    {
        bool value = false;
        condition_variable_type cv;
        mutex_type m;

        std::unique_lock<mutex_type> l(m);

        std::thread t = std::thread(triggerAll, std::chrono::milliseconds(500),
                                    std::ref(cv));

        bool result = cv.wait_for(l, std::chrono::milliseconds(1000),
                                  [&]{ return value == true; });
        EXPECT_FALSE( result );

        ASSERT_TRUE( t.joinable() );
        t.join();


        t = std::thread(setValue, std::chrono::milliseconds(500),
                        std::ref(value));

        result = cv.wait_for(l, std::chrono::milliseconds(1000),
                             [&]{ return value == true; });
        EXPECT_TRUE( result );

        ASSERT_TRUE( t.joinable() );
        t.join();
    }
    catch (const std::system_error& e)
    {
        FAIL() << e.what();
    }
    catch (...)
    {
        FAIL();
    }
}

// A bit hacky - but copy'n'pasted the time-out calculator from the
// ConditionVariable implementation here so can perform some tests
// to ensure it calculates the correct time-out
static struct timespec calcTimeoutAbs(const std::chrono::nanoseconds& rel_time)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    ts.tv_sec += std::chrono::duration_cast<std::chrono::seconds>(rel_time).count();
    ts.tv_nsec += (rel_time % std::chrono::seconds(1)).count();

    if (ts.tv_nsec > 1000000000L)
    {
        ts.tv_nsec -= 1000000000L;
        ts.tv_sec += 1;
    }

    return ts;
}

static double timeval_subtract(struct timespec x, struct timespec y)
{
    struct timespec result;

    // Perform the carry for the later subtraction by updating y.
    if (x.tv_nsec < y.tv_nsec)
    {
        long secs = (y.tv_nsec - x.tv_nsec) / 1000000000 + 1;
        y.tv_nsec -= 1000000000 * secs;
        y.tv_sec += secs;
    }
    if ((x.tv_nsec - y.tv_nsec) > 1000000000)
    {
       long secs = (x.tv_nsec - y.tv_nsec) / 1000000000;
       y.tv_nsec += 1000000000 * secs;
       y.tv_sec -= secs;
    }

    // Compute the time remaining to wait. tv_nsec is certainly positive.
    result.tv_sec = x.tv_sec - y.tv_sec;
    result.tv_nsec = x.tv_nsec - y.tv_nsec;

    //
    return (double)result.tv_sec + (double)((double)result.tv_nsec / 1000000000.0f);
}


TEST(ConditionVariableTest, timeouts_1)
{
    struct timespec ts, now;

    ts = calcTimeoutAbs(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(1)));
    clock_gettime(CLOCK_MONOTONIC, &now);
    ASSERT_NEAR(timeval_subtract(ts, now), 3600.0f, 0.010f);

    ts = calcTimeoutAbs(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::hours(8760)));
    clock_gettime(CLOCK_MONOTONIC, &now);
    ASSERT_NEAR(timeval_subtract(ts, now), 31536000.0f, 0.010f);

    ts = calcTimeoutAbs(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(100)));
    clock_gettime(CLOCK_MONOTONIC, &now);
    ASSERT_NEAR(timeval_subtract(ts, now), 0.100f, 0.010f);


    ts = calcTimeoutAbs(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::milliseconds(-100)));
    clock_gettime(CLOCK_MONOTONIC, &now);
    ASSERT_NEAR(timeval_subtract(ts, now), -0.100f, 0.010f);
}



