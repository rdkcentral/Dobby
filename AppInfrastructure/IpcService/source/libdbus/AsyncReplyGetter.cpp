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
 * AsyncReplyGetter.cpp
 *
 *  Created on: 9 Jun 2015
 *      Author: riyadh
 */

#include "IpcCommon.h"
#include "AsyncReplyGetter.h"
#include "DbusMessageParser.h"
#include "DbusConnection.h"
#include "IpcUtilities.h"

#include <Logging.h>

#include <dbus/dbus.h>

#include <exception>
#include <cinttypes>

using namespace AI_IPC;



AsyncReplyGetter::AsyncReplyGetter(const std::weak_ptr<DbusConnection>& dbusConnection, uint64_t token)
    : mDbusConnection(dbusConnection)
    , mReplyToken(token)
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

AsyncReplyGetter::~AsyncReplyGetter()
{
    AI_LOG_FN_ENTRY();

    // cancel the reply if no one has called getReply(..)
    uint64_t token = mReplyToken.exchange(0);
    if (token != 0)
    {
        // try and lock the dbus connection and then cancel the reply
        std::shared_ptr<DbusConnection> dbusConnection = mDbusConnection.lock();
        if (dbusConnection && !dbusConnection->cancelReply(token))
        {
            AI_LOG_ERROR("failed to cancel reply for token %" PRIu64 "", token);
        }
    }

    AI_LOG_FN_EXIT();
}

bool AsyncReplyGetter::getReply(VariantList& argList)
{
    AI_LOG_FN_ENTRY();

    // atomically get and clear the token to avoid races
    uint64_t token = mReplyToken.exchange(0);
    if (token == 0)
    {
        AI_LOG_ERROR_EXIT("invalid reply token %" PRIu64 "", token);
        return false;
    }

    // try and lock the dbus connection
    std::shared_ptr<DbusConnection> dbusConnection = mDbusConnection.lock();
    if (!dbusConnection)
    {
        AI_LOG_ERROR_EXIT("dbus connection has been closed");
        return false;
    }

    // get the reply object then release the connection
    DBusMessage* reply = dbusConnection->getReply(token);
    dbusConnection.reset();

    // sanity check there is a reply (this should be non-null even if a timeout
    // occurs)
    if (!reply)
    {
        AI_LOG_ERROR_EXIT("no reply object");
        return false;
    }

    //
    bool result = false;

    //
    const int recvMsgType = dbus_message_get_type(reply);
    if ((recvMsgType == DBUS_MESSAGE_TYPE_METHOD_RETURN) ||
        (recvMsgType == DBUS_MESSAGE_TYPE_ERROR))
    {
        try
        {
            DbusMessageParser dbusMessageParser(reply);
            if (dbusMessageParser.parseMsg())
            {
                VariantList argList_ = dbusMessageParser.getArgList();
                if (recvMsgType == DBUS_MESSAGE_TYPE_METHOD_RETURN)
                {
                    result = true;
                    argList.swap(argList_);
                }
                else
                {
                    if (!argList_.empty())
                    {
                        std::string errorMsg = boost::get<std::string>(argList_[0]);
                        AI_LOG_ERROR("error while waiting for reply - %s", errorMsg.c_str());
                    }
                    else
                    {
                        AI_LOG_ERROR("error while waiting for reply");
                    }
                }
            }
            else
            {
                AI_LOG_ERROR( "Unable to parse reply message" );
            }
        }
        catch (const std::exception& e)
        {
            AI_LOG_ERROR( "Unable to parse dbus reply message: %s.", e.what() );
        }
    }
    else
    {
        AI_LOG_ERROR( "Invalid message type received: %d.", recvMsgType );
    }

    // can release the reply object
    dbus_message_unref(reply);

    AI_LOG_FN_EXIT();
    return result;
}
