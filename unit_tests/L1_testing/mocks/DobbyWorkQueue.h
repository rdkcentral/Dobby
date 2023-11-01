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

#ifndef DOBBYWORKQUEUE_H
#define DOBBYWORKQUEUE_H

#include <string>
#include <chrono>

using WorkFunc = std::function<void()>;
class DobbyWorkQueueImpl{

public:
   virtual ~DobbyWorkQueueImpl() = default;
   virtual bool runFor(const std::chrono::milliseconds &msecs) = 0;
   virtual void exit() = 0;
   virtual bool postWork(const WorkFunc &work) = 0;
};

class DobbyWorkQueue {

protected:
    static DobbyWorkQueueImpl* impl;

public:

    static void setImpl(DobbyWorkQueueImpl* newImpl)
    {
        impl = newImpl;
    }

    static DobbyWorkQueue* getInstance()
    {
        static DobbyWorkQueue* instance = nullptr;
        if(nullptr != instance)
        {
            instance =  new DobbyWorkQueue();
        }
        return instance;
    }


    static bool runFor(const std::chrono::milliseconds &msecs)
    {
        return impl->runFor(msecs);
    }

    static void exit()
    {
        impl->exit();
    }

    static bool postWork(WorkFunc &&work)
    {
        return impl->postWork(work);
    }

};

#endif // DOBBYWORKQUEUE_H
