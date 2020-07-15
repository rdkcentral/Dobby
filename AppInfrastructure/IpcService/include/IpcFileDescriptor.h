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
 * IpcFileDescriptor.h
 *
 *  Created on:
 *      Author:
 */

#ifndef AI_IPC_IPCFILEDESCRIPTOR_H
#define AI_IPC_IPCFILEDESCRIPTOR_H

namespace AI_IPC
{

class IpcFileDescriptor
{
public:
    IpcFileDescriptor();
    explicit IpcFileDescriptor(int fd);
    IpcFileDescriptor(const IpcFileDescriptor &other);
    IpcFileDescriptor &operator=(IpcFileDescriptor &&other);
    IpcFileDescriptor &operator=(const IpcFileDescriptor &other);
    ~IpcFileDescriptor();

public:
    bool operator==(const IpcFileDescriptor& rhs) const;

public:
    bool isValid() const;
    int fd() const;

    int dup() const;

    void reset(int fd = -1);
    void clear();

private:
    int mFd;
};

} // namespace AI_IPC

#endif // AI_IPC_IPCFILEDESCRIPTOR_H
