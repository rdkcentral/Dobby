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
 * AsyncReplySender.h
 *
 *  Created on: 5 Jun 2015
 *      Author: riyadh
 */

#ifndef AI_IPC_ASYNCREEPLYSENDER_H
#define AI_IPC_ASYNCREEPLYSENDER_H

#include "IpcCommon.h"

#include <memory>

#include <dbus/dbus.h>

namespace AI_IPC
{

class DbusConnection;


class AsyncReplySender : public IAsyncReplySender
{
public:

    AsyncReplySender(const std::weak_ptr<DbusConnection>& dbusConnection, DBusMessage *dbusRequestMsg, DBusMessage *dbusReplyMsg, const VariantList& argList);

    virtual ~AsyncReplySender();

    virtual VariantList getMethodCallArguments() const override;

    virtual bool sendReply(const VariantList& replyArgs) override;

    virtual uid_t getSenderUid() const override;

    virtual std::string getSenderName() const override;

private:

    const std::weak_ptr<DbusConnection> mDbusConnection;

    DBusMessage *mDbusReplyMsg;

    const VariantList mArgList;

    std::string mSenderName;
};

}

#endif /* AI_IPC_ASYNCREEPLYSENDER_H */
