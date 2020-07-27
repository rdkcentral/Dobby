/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2015 Sky UK
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
 * File:   SpinLock.h
 *
 */
#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <atomic>

namespace AICommon
{

/**
 * Basic spinlock used in the PollLoop and Jumper objects as locking is needed but
 * a mutex is overkill.  It has the same basic API as std::mutex so can be
 * swapped out, in addition it implements the C++ BasicLockable and Lockable
 * requirements meaning it can be used as a template for std::unique_lock and
 * std::lock_guard.
 *
 *
 *
 */
class Spinlock
{
public:
    Spinlock() : mLocked(ATOMIC_FLAG_INIT)
    { }

public:
    void lock()
    {
        while (mLocked.test_and_set(std::memory_order_acquire)) ;
    }
    void unlock()
    {
        mLocked.clear(std::memory_order_release);
    }
    bool try_lock()
    {
        return !mLocked.test_and_set(std::memory_order_acquire);
    }

private:
    std::atomic_flag mLocked;
};

} // namespace AICommon


#endif // !defined(SPINLOCK_H)

