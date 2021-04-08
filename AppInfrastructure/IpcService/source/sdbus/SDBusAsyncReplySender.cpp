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
//  SDBusAsyncReplySender.cpp
//  IpcService
//
//

#include "SDBusAsyncReplySender.h"
#include "SDBusIpcService.h"

#include <Logging.h>


SDBusAsyncReplySender::SDBusAsyncReplySender(const std::shared_ptr<SDBusIpcService> &ipcService,
                                             uint32_t replyId,
                                             const char *senderName,
                                             uid_t senderUserId,
                                             AI_IPC::VariantList &&args)
    : mIpcService(ipcService)
    , mReplySent(false)
    , mReplyId(replyId)
    , mSenderName(senderName ? senderName : "")
    , mArgs(std::move(args))
    , mSenderUid(senderUserId)
{
}

SDBusAsyncReplySender::~SDBusAsyncReplySender()
{
    if (!mReplySent)
    {
        AI_LOG_WARN("no reply sent for dbus method call");

        std::shared_ptr<SDBusIpcService> ipcService = mIpcService.lock();
        ipcService->freeMethodReply(mReplyId);
    }
}

bool SDBusAsyncReplySender::sendReply(const AI_IPC::VariantList& replyArgs)
{
    if (mReplySent)
    {
        AI_LOG_WARN("reply already sent");
        return false;
    }

    std::shared_ptr<SDBusIpcService> ipcService = mIpcService.lock();
    if (!ipcService)
    {
        AI_LOG_WARN("can't send reply as IpcService object has been destroyed");
        return false;
    }

    mReplySent = ipcService->sendMethodReply(mReplyId, replyArgs);
    return mReplySent;
}

AI_IPC::VariantList SDBusAsyncReplySender::getMethodCallArguments() const
{
    return mArgs;
}

uid_t SDBusAsyncReplySender::getSenderUid() const
{
    // if we don't have a valid uid (likely) then go a fetch one from the
    // the IpcService - this involves another call to the dbus daemon
    if (mSenderUid == uid_t(-1))
    {
        std::shared_ptr<SDBusIpcService> ipcService = mIpcService.lock();
        if (!ipcService)
        {
            AI_LOG_WARN("can't send reply as IpcService object has been destroyed");
        }
        else
        {
            mSenderUid = ipcService->getSenderUid(mSenderName);
        }
    }

    // return the possibly invalid user id
    return mSenderUid;
}

std::string SDBusAsyncReplySender::getSenderName() const
{
    return mSenderName;
}
