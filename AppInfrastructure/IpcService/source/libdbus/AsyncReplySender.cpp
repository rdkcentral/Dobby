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
/*
 * AsyncReplySender.cpp
 *
 *  Created on: 9 Jun 2015
 *      Author: riyadh
 */

#include "IpcCommon.h"
#include "DbusConnection.h"
#include "DbusMessageParser.h"
#include "AsyncReplySender.h"
#include "IpcUtilities.h"

#include <Common/Interface.h>
#include <Logging.h>

#include <dbus/dbus.h>

#include <string>
#include <exception>
#include <mutex>

using namespace AI_IPC;

AsyncReplySender::AsyncReplySender(const std::weak_ptr<DbusConnection>& dbusConnection, DBusMessage *dbusRequestMsg, DBusMessage *dbusReplyMsg, const VariantList& argList)
    : mDbusConnection(dbusConnection)
    , mDbusReplyMsg(dbusReplyMsg)
    , mArgList(argList)
{
    AI_LOG_FN_ENTRY();

    // get and store the sender of the message
    const char *sender = dbus_message_get_sender(dbusRequestMsg);
    if (sender)
    {
        mSenderName = sender;
    }

    AI_LOG_FN_EXIT();
}

AsyncReplySender::~AsyncReplySender()
{
    AI_LOG_FN_ENTRY();

    if( mDbusReplyMsg )
    {
        dbus_message_unref(mDbusReplyMsg);
    }

    AI_LOG_FN_EXIT();
}

VariantList AsyncReplySender::getMethodCallArguments() const
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();

    return mArgList;
}

bool AsyncReplySender::sendReply(const VariantList& replyArgs)
{
    AI_LOG_FN_ENTRY();

    bool res = false;

    std::shared_ptr<DbusConnection> dbusConnection = mDbusConnection.lock();
    if (!dbusConnection)
    {
        AI_LOG_ERROR("failed to lock the dbus connection");
    }
    else if (!appendArgsToDbusMsg(mDbusReplyMsg, replyArgs))
    {
        AI_LOG_ERROR("Unable to append arguments to dbus message");
    }
    else
    {
        res = dbusConnection->sendMessageNoReply(mDbusReplyMsg);
    }

    AI_LOG_FN_EXIT();

    return res;
}

std::string AsyncReplySender::getSenderName() const
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();

    return mSenderName;
}

uid_t AsyncReplySender::getSenderUid() const
{
    AI_LOG_FN_ENTRY();

    uid_t uid = -1;

    if (mSenderName.empty())
    {
        AI_LOG_ERROR("no sender name stored");
    }
    else
    {
        std::shared_ptr<DbusConnection> dbusConnection = mDbusConnection.lock();
        if (!dbusConnection)
        {
            AI_LOG_ERROR("failed to lock the dbus connection");
        }
        else
        {
            uid = dbusConnection->getUnixUser(mSenderName);
        }
    }

    AI_LOG_FN_EXIT();

    return uid;
}
