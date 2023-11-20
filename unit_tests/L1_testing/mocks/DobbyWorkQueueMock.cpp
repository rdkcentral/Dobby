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

#include "DobbyWorkQueueMock.h"

void DobbyWorkQueue::setImpl(DobbyWorkQueueImpl* newImpl)
{
    impl = newImpl;
}

DobbyWorkQueue* DobbyWorkQueue::getInstance()
{
    static DobbyWorkQueue* instance = nullptr;
    if(nullptr == instance)
    {
        instance =  new DobbyWorkQueue();
    }
    return instance;
}


bool DobbyWorkQueue::runFor(const std::chrono::milliseconds &msecs)
{
   EXPECT_NE(impl, nullptr);

    return impl->runFor(msecs);
}

void DobbyWorkQueue::exit()
{
   EXPECT_NE(impl, nullptr);

    impl->exit();
}

bool DobbyWorkQueue::postWork(WorkFunc &&work)
{
   EXPECT_NE(impl, nullptr);

    return impl->postWork(work);
}
