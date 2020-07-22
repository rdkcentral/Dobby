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
//  SDBusAsyncReplySender.h
//  IpcService
//
//

#ifndef SDBUSASYNCREPLYSENDER_H
#define SDBUSASYNCREPLYSENDER_H

#include <IpcCommon.h>

#include <memory>
#include <atomic>

class SDBusIpcService;


class SDBusAsyncReplySender : public AI_IPC::IAsyncReplySender
{
public:
    SDBusAsyncReplySender(const std::shared_ptr<SDBusIpcService> &ipcService,
                          uint32_t replyId,
                          const char *senderName,
                          uid_t senderUserId,
                          AI_IPC::VariantList &&args);
    ~SDBusAsyncReplySender() final;

    AI_IPC::VariantList getMethodCallArguments() const override;

    bool sendReply(const AI_IPC::VariantList& replyArgs) override;

    uid_t getSenderUid() const override;
    std::string getSenderName() const override;

private:
    std::weak_ptr<SDBusIpcService> mIpcService;

    bool mReplySent;
    const uint32_t mReplyId;
    const std::string mSenderName;
    const AI_IPC::VariantList mArgs;
    mutable std::atomic<uid_t> mSenderUid;
};


#endif // SDBUSASYNCREPLYSENDER_H
