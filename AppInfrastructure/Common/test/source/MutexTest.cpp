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
#include <Mutex.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <valgrind.h>

#include <chrono>
#include <mutex>


// All of these tests are 'borrowed' from the gcc libstdc++-v3 testsuite

typedef AICommon::Mutex mutex_type;

// Uncomment the following to sanity tests the tests
// typedef std::mutex mutex_type;

TEST(MutexTest, con1)
{
    try
    {
        mutex_type m1;
    }
    catch (const std::system_error& e)
    {
        FAIL();
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(MutexTest, destructor_locked)
{
    // Valgrind borks on this test as it's deliberately doing bad things
    // so don't run it if running under valgrind
    if (RUNNING_ON_VALGRIND)
    {
        printf("\033[0;32m[          ] \033[m");
        printf("\033[0;33mSkipping test because running under valgrind\n\033[m");
        return;
    }

    // Nb this test verifies that a system_error exception is thrown in the
    // destructor, however since C++11 all destructors are 'noexcept(true)'
    // meaning that an exception will cause a std::terminate.
    // Hence this test is a death test rather than one that just captures
    // the exception.
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    // Destroying locked mutex raises system error, or undefined.
    // POSIX == may fail with EBUSY.
    ASSERT_DEATH_IF_SUPPORTED(
    {
        mutex_type m;
        m.lock();
    }, "terminate called after throwing an instance of 'std::system_error'");
}

TEST(MutexTest, lock_1)
{
    try
    {
        mutex_type m;
        m.lock();
        m.unlock();
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

TEST(MutexTest, unlock_1)
{
    mutex_type m;
    ASSERT_THROW(m.unlock(), std::system_error);
}

TEST(MutexTest, try_lock_1)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m1, m2, m3;
        lock_type l1(m1, std::defer_lock),
        l2(m2, std::defer_lock),
        l3(m3, std::defer_lock);

        try
        {
            int result = std::try_lock(l1, l2, l3);
            ASSERT_EQ(result, -1);
        }
        catch (const std::system_error& e)
        {
            FAIL();
        }
    }
    catch (const std::system_error& e)
    {
        FAIL();
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(MutexTest, try_lock_2_01)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m1, m2, m3;
        lock_type l1(m1);
        int result = std::try_lock(m1, m2, m3);
        ASSERT_EQ( result, 0 );
        ASSERT_TRUE( l1.owns_lock() );
        lock_type l2(m2);
        lock_type l3(m3);
    }
    catch (const std::system_error& e)
    {
        FAIL();
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(MutexTest, try_lock_2_02)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m1, m2, m3;
        lock_type l2(m2);
        int result = std::try_lock(m1, m2, m3);
        ASSERT_EQ( result, 1 );
        ASSERT_TRUE( l2.owns_lock() );
        lock_type l1(m1);
        lock_type l3(m3);
    }
    catch (const std::system_error& e)
    {
        FAIL();
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(MutexTest, try_lock_2_03)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m1, m2, m3;
        lock_type l3(m3);
        int result = std::try_lock(m1, m2, m3);
        ASSERT_EQ( result, 2 );
        ASSERT_TRUE( l3.owns_lock() );
        lock_type l1(m1);
        lock_type l2(m2);
    }
    catch (const std::system_error& e)
    {
        FAIL();
    }
    catch (...)
    {
        FAIL();
    }
}

struct user_lock
{
    user_lock() : is_locked(false) { }
    ~user_lock() = default;
    user_lock(const user_lock&) = default;

    void lock()
    {
        ASSERT_FALSE( is_locked );
        is_locked = true;
    }

    bool try_lock()
    { return is_locked ? false : (is_locked = true); }

    void unlock()
    {
        ASSERT_TRUE( is_locked );
        is_locked = false;
    }
    
private:
    bool is_locked;
};

TEST(MutexTest, try_lock_3)
{
    try
    {
        mutex_type m1;
        std::recursive_mutex m2;
        user_lock m3;

        try
        {
            //heterogeneous types
            int result = std::try_lock(m1, m2, m3);
            EXPECT_EQ( result, -1 );
            m1.unlock();
            m2.unlock();
            m3.unlock();
        }
        catch (const std::system_error& e)
        {
            FAIL() << e.what();
        }
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

struct unreliable_lock
{
    mutex_type m;
    std::unique_lock<mutex_type> l;

    static int count;
    static int throw_on;
    static int lock_on;

    unreliable_lock() : l(m, std::defer_lock) { }

    ~unreliable_lock()
    {
        EXPECT_FALSE( l.owns_lock() );
    }

    void lock()
    {
        if (count == throw_on)
            throw throw_on;
        ++count;
        l.lock();
    }
    bool try_lock()
    {
        if (count == throw_on)
            throw throw_on;
        std::unique_lock<mutex_type> l2(m, std::defer_lock);
        if (count == lock_on)
            l2.lock();
        ++count;
        return l.try_lock();
    }

    void unlock()
    {
        ASSERT_TRUE( l.owns_lock() );
        l.unlock();
    }
    
};

int unreliable_lock::count = 0;
int unreliable_lock::throw_on = -1;
int unreliable_lock::lock_on = -1;


TEST(MutexTest, try_lock_4_01)
{
    unreliable_lock l1, l2, l3;

    try
    {
        unreliable_lock::count = 0;
        int result = std::try_lock(l1, l2, l3);
        ASSERT_EQ( result, -1 );
        ASSERT_EQ( unreliable_lock::count, 3 );
        l1.unlock();
        l2.unlock();
        l3.unlock();
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(MutexTest, try_lock_4_02)
{
    unreliable_lock l1, l2, l3;

    try
    {
        // test behaviour when a lock is already held
        unreliable_lock::lock_on = 0;
        while (unreliable_lock::lock_on < 3)
        {
            unreliable_lock::count = 0;
            int failed = std::try_lock(l1, l2, l3);
            ASSERT_EQ( failed, unreliable_lock::lock_on );
            ++unreliable_lock::lock_on;
        }
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(MutexTest, try_lock_4_03)
{
    unreliable_lock l1, l2, l3;

    try
    {
        // test behaviour when an exception is thrown
        unreliable_lock::throw_on = 0;
        while (unreliable_lock::throw_on < 3)
        {
            unreliable_lock::count = 0;
            int failed = std::try_lock(l1, l2, l3);
            ASSERT_EQ( failed, unreliable_lock::throw_on );
            ++unreliable_lock::throw_on;
        }
    }
    catch (...)
    {
        FAIL();
    }
}

TEST(MutexTest, unique_lock_cons_1)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        lock_type lock;

        ASSERT_FALSE( lock.owns_lock() );
        ASSERT_FALSE( (bool)lock );
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

TEST(MutexTest, unique_lock_cons_2)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m;
        lock_type lock(m);

        ASSERT_TRUE( lock.owns_lock() );
        ASSERT_TRUE( (bool)lock );
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

TEST(MutexTest, unique_lock_cons_3)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m;
        lock_type lock(m, std::defer_lock);

        ASSERT_FALSE( lock.owns_lock() );
        ASSERT_FALSE( (bool)lock );
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

TEST(MutexTest, unique_lock_cons_4)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m;
        lock_type lock(m, std::try_to_lock);

        ASSERT_TRUE( lock.owns_lock() );
        ASSERT_TRUE( (bool)lock );
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

TEST(MutexTest, unique_lock_locking_1)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m;
        lock_type l(m, std::defer_lock);

        l.lock();

        ASSERT_TRUE( (bool)l );
        ASSERT_TRUE( l.owns_lock() );

        l.unlock();

        ASSERT_FALSE( (bool)l );
        ASSERT_FALSE( l.owns_lock() );
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

TEST(MutexTest, unique_lock_locking_2_01)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        lock_type l;

        // Lock unique_lock w/o mutex
        try
        {
            l.lock();
        }
        catch (const std::system_error& ex)
        {
            ASSERT_EQ( ex.code(), std::make_error_code(std::errc::operation_not_permitted) );
        }
        catch (...)
        {
            FAIL();
        }
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

TEST(MutexTest, unique_lock_locking_2_02)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m;
        lock_type l(m);

        // Lock already locked unique_lock.
        try
        {
            l.lock();
        }
        catch (const std::system_error& ex)
        {
            ASSERT_EQ( ex.code(), std::make_error_code(std::errc::resource_deadlock_would_occur) );
        }
        catch (...)
        {
            FAIL();
        }
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

TEST(MutexTest, unique_lock_modifiers_1)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m;
        lock_type l1(m);
        lock_type l2;

        try
        {
            l1.swap(l2);
        }
        catch (const std::system_error& e)
        {
            FAIL() << e.what();
        }
        catch(...)
        {
            FAIL();
        }
        
        ASSERT_FALSE( (bool)l1 );
        ASSERT_TRUE( (bool)l2 );
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

TEST(MutexTest, unique_lock_modifiers_2)
{
    typedef std::unique_lock<mutex_type> lock_type;

    try
    {
        mutex_type m;
        lock_type l1(m);
        lock_type l2;

        try
        {
            l1.swap(l2);
        }
        catch (const std::system_error& e)
        {
            FAIL() << e.what();
        }
        catch(...)
        {
            FAIL();
        }
        
        ASSERT_FALSE( (bool)l1 );
        ASSERT_TRUE( (bool)l2 );
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

