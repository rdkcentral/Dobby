/* If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#ifndef AI_IPC_IPCFILEDESCRIPTOR_H
#define AI_IPC_IPCFILEDESCRIPTOR_H

namespace AI_IPC
{

class IpcFileDescriptorApiImpl {

public:
    virtual ~IpcFileDescriptorApiImpl() = default;
    virtual bool isValid() const = 0;
    virtual int fd() const= 0;

};

class IpcFileDescriptor {

protected:
    static IpcFileDescriptorApiImpl* impl;

public:
    IpcFileDescriptor();
    IpcFileDescriptor(int fd);
    IpcFileDescriptor(const IpcFileDescriptor &other);
    IpcFileDescriptor &operator=(IpcFileDescriptor &&other);
    IpcFileDescriptor &operator=(const IpcFileDescriptor &other);
    ~IpcFileDescriptor();

    static void setImpl(IpcFileDescriptorApiImpl* newImpl);
    static IpcFileDescriptor* getInstance();
    bool isValid() const;
    int fd() const;

};
}

#endif /* AI_IPC_IPCFILEDESCRIPTOR_H */
