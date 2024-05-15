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

#include "DobbyStreamMock.h"

DobbyBufferStream::DobbyBufferStream(ssize_t limit)
{
}

DobbyBufferStream::~DobbyBufferStream()
{
}

int DobbyBufferStream::dupWriteFD(int newFd, bool closeExec) const
{
    return -1;
}

void DobbyBufferStream::setImpl(DobbyBufferStreamImpl* newImpl)
{
    // Handles both resetting 'impl' to nullptr and assigning a new value to 'impl'
    EXPECT_TRUE ((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

std::vector<char> DobbyBufferStream::getBuffer() const
{
   EXPECT_NE(impl, nullptr);

    return impl->getBuffer();
}

int DobbyBufferStream::getMemFd() const
{
   EXPECT_NE(impl, nullptr);

    return impl->getMemFd();
}
