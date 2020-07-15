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
 * File:   Mutex.h
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef MUTEX_H
#define MUTEX_H

#include <pthread.h>
#include <errno.h>

#include <stdio.h>

#include <system_error>
#include <Logging.h>


namespace AICommon
{


#if (AI_BUILD_TYPE == AI_RELEASE)
    #define __MutexThrowOnError(err) \
        (void)err

#elif (AI_BUILD_TYPE == AI_DEBUG)
    #define __MutexThrowOnError(err) \
        do { \
            if (__builtin_expect((err != 0), 0)) \
                throw std::system_error(std::error_code(err, std::system_category())); \
        } while(0)

#else
    #error Unknown AI build type

#endif



/**
 *  @class AICommon::Mutex
 *
 *  Basic mutex, it has the same basic API as std::mutex so can be swapped out,
 *  in addition it implements the C++ 'BasicLockable' and 'Lockable'
 *  requirements meaning it can be used as a template for std::unique_lock and
 *  std::lock_guard.
 *
 */
class Mutex
{
public:
    Mutex()
    {
#if (AI_BUILD_TYPE == AI_DEBUG)
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
        pthread_mutex_init(&mLock, &attr);
        pthread_mutexattr_destroy(&attr);
#elif (AI_BUILD_TYPE == AI_RELEASE)
        pthread_mutex_init(&mLock, nullptr);
#else
        #error Unknown AI build type
#endif
    }

    Mutex(const Mutex&) = delete;
    Mutex(const Mutex&&) = delete;

    ~Mutex()
    {
        int err = pthread_mutex_destroy(&mLock);
        try
        {
            __MutexThrowOnError(err);
        }
        catch(const std::system_error& exception)
        {
            AI_LOG_FATAL("Mutex failed to be destroyed %s", exception.what());
        }
    }

public:
    void lock()
    {
        int err = pthread_mutex_lock(&mLock);
        __MutexThrowOnError(err);
    }

    void unlock()
    {
        int err = pthread_mutex_unlock(&mLock);
        __MutexThrowOnError(err);
    }

    bool try_lock()
    {
        int err = pthread_mutex_trylock(&mLock);
        if (err == 0)
        {
            return true;
        }
        else if (err == EBUSY)
        {
            return false;
        }
        else
        {
            __MutexThrowOnError(err);
            return false;
        }
    }

public:
    typedef pthread_mutex_t* native_handle_type;
    native_handle_type native_handle()
    {
        return &mLock;
    }

private:
    pthread_mutex_t mLock;
};

} // namespace AICommon


#endif // !defined(MUTEX_H)

