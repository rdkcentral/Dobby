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
 * File:   DobbyAsync.cpp
 *
 * Copyright (C) BSKYB 2017+
 */

#include "DobbyAsync.h"

#include <Logging.h>

#include <thread>
#include <cstring>

#include <pthread.h>


class IDobbyAsyncResultPrivate
{
public:
    virtual ~IDobbyAsyncResultPrivate() = default;

public:
    virtual bool getResult() = 0;

};


class DobbyThreadedAsyncResult : public IDobbyAsyncResultPrivate
{
public:
    DobbyThreadedAsyncResult(const std::string &name, const std::function<bool()>& func)
        : mFunction(func)
        , mResult(false)
    {
        // copy the name for the thread
        if (!name.empty())
        {
            mThreadName.assign(name);
            if (mThreadName.length() > 15)
                mThreadName.resize(15);
        }

        // start the thread, we wrap the thread as want to set it's name and
        // capture the result of the function
        mThread = std::thread(&DobbyThreadedAsyncResult::threadWrapper, this);
        if (!mThread.joinable())
            AI_LOG_FATAL("failed to start async thread");
    }

    ~DobbyThreadedAsyncResult() final
    {
        if (mThread.joinable())
        {
            AI_LOG_FATAL("destroying an async result without waiting on it");

            // this may be a bad idea, but if we don't join it will probably
            // cause a crash as the thread has the this pointer
            mThread.join();
        }
    }

public:
    bool getResult() override
    {
        if (mThread.joinable())
        {
            mThread.join();
        }
        else
        {
            AI_LOG_WARN("calling getResult more than once");
        }

        return mResult;
    }

private:
    void threadWrapper()
    {
        // set the thread name for minidumps
        if (!mThreadName.empty())
            pthread_setname_np(pthread_self(), mThreadName.c_str());

        // execute the function and store the result
        mResult = mFunction();
    }

private:
    const std::function<bool()> mFunction;
    std::string mThreadName;
    std::thread mThread;
    bool mResult;
};


class DobbyDeferredAsyncResult : public IDobbyAsyncResultPrivate
{
public:
    explicit DobbyDeferredAsyncResult(const std::function<bool()>& func)
        : mFunction(func)
        , mFinished(false)
        , mResult(false)
    {
    }

    ~DobbyDeferredAsyncResult() final
    {
        if (!mFinished)
            AI_LOG_FATAL("destroying an async result without waiting on it");
    }

public:
    bool getResult() override
    {
        if (!mFinished)
        {
            mResult = mFunction();
            mFinished = true;
            return mResult;
        }
        else
        {
            AI_LOG_WARN("calling getResult more than once");
            return mResult;
        }
    }

private:
    const std::function<bool()> mFunction;
    bool mFinished;
    bool mResult;
};






DobbyAsyncResult::DobbyAsyncResult(IDobbyAsyncResultPrivate* priv)
    : mPrivate(priv)
{
}

DobbyAsyncResult::~DobbyAsyncResult()
{
    delete mPrivate;
}

bool DobbyAsyncResult::getResult()
{
    if (!mPrivate)
    {
        AI_LOG_FATAL("trying to get the results of an invalid object");
        return false;
    }

    return mPrivate->getResult();
}



std::shared_ptr<DobbyAsyncResult> DobbyAsyncImpl(const std::string &name,
                                                 const std::function<bool()>& func)
{
    // create the private results object which spawns the thread and starts
    // running the actual function
    DobbyThreadedAsyncResult *resultObj = new DobbyThreadedAsyncResult(name, func);

    // wrap the result object in a generic result that can be waited on
    return std::shared_ptr<DobbyAsyncResult>(new DobbyAsyncResult(resultObj));
}

std::shared_ptr<DobbyAsyncResult> DobbyDeferredImpl(const std::function<bool()>& func)
{
    // create the private results object which will execute the function when
    // the results are queried
    DobbyDeferredAsyncResult *resultObj = new DobbyDeferredAsyncResult(func);

    // wrap the result object in a generic result
    return std::shared_ptr<DobbyAsyncResult>(new DobbyAsyncResult(resultObj));
}

