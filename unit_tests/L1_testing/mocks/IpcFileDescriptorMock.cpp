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

#include "IpcFileDescriptorMock.h"

using::AI_IPC::IpcFileDescriptor;

IpcFileDescriptor::IpcFileDescriptor()
{
}

IpcFileDescriptor::IpcFileDescriptor(int fd)
{
}

IpcFileDescriptor::IpcFileDescriptor(const IpcFileDescriptor &other)
{
}

IpcFileDescriptor &IpcFileDescriptor::operator=(IpcFileDescriptor &&other)
{
    return *this;
}

IpcFileDescriptor &IpcFileDescriptor::operator=(const IpcFileDescriptor &other)
{
    return *this;
}

IpcFileDescriptor::~IpcFileDescriptor()
{
}

void IpcFileDescriptor::setImpl(IpcFileDescriptorApiImpl* newImpl)
{
    impl = newImpl;
}

IpcFileDescriptor* IpcFileDescriptor::getInstance()
{
    static IpcFileDescriptor* instance = nullptr;
    if(nullptr == instance)
    {
        instance = new IpcFileDescriptor();
    }
    return instance;
}

bool IpcFileDescriptor::isValid() const
{
   EXPECT_NE(impl, nullptr);

    return impl->isValid();
}

int IpcFileDescriptor::fd() const
{
   EXPECT_NE(impl, nullptr);

    return impl->fd();
}
