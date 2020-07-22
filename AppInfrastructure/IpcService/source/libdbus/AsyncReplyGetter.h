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
 * AsyncReplyGetter.h
 *
 *  Created on: 9 Jun 2015
 *      Author: riyadh
 */

#ifndef AI_IPC_ASYNCREPLYGETTER_H
#define AI_IPC_ASYNCREPLYGETTER_H

#include "IpcCommon.h"

#include <memory>
#include <atomic>

namespace AI_IPC
{

class DbusConnection;

class AsyncReplyGetter : public IAsyncReplyGetter
{
public:

    AsyncReplyGetter(const std::weak_ptr<DbusConnection>& dbusConnection, uint64_t token);

    ~AsyncReplyGetter();

    virtual bool getReply(VariantList& argList) override;

private:

    const std::weak_ptr<DbusConnection> mDbusConnection;

    std::atomic<uint64_t> mReplyToken;

};

}

#endif /* AI_IPC_ASYNCREPLYGETTER_H */
