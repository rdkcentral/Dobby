/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2019 Sky UK
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
//
//  SDBusAsyncReplyGetter.cpp
//  IpcService
//
//

#include "SDBusAsyncReplyGetter.h"


using namespace AI_IPC;

// -----------------------------------------------------------------------------
/*!
    \class SDBusAsyncReplyGetter
    \brief Implements the IAsyncReplyGetter getter interface to provide an API
    to get the results to a method call.


 */


SDBusAsyncReplyGetter::SDBusAsyncReplyGetter()
    : mFinished(false)
    , mSucceeded(false)
{
}

// -----------------------------------------------------------------------------
/*!
    \overload

    Blocking call that clients make when they want to get the reply to a
    method call.

 */
bool SDBusAsyncReplyGetter::getReply(VariantList& argList)
{
    std::unique_lock<std::mutex> locker(mLock);

    while (!mFinished)
    {
        mCond.wait(locker);
    }

    argList = mArgs;
    return mSucceeded;
}

// -----------------------------------------------------------------------------
/*!
    Called by the SDBusIpcService class when a reply is received for the
    method call.

 */
void SDBusAsyncReplyGetter::setReply(bool succeeded, VariantList &&argList)
{
    // store the args and set the finished flag
    {
        std::lock_guard<std::mutex> locker(mLock);

        mArgs.swap(argList);
        mSucceeded = succeeded;
        mFinished = true;
    }

    // and then wake any threads blocked in the getReply call
    mCond.notify_all();
}

