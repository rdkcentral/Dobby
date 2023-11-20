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

#ifndef DOBBYUTILS_H
#define DOBBYUTILS_H

#include "IDobbyUtils.h"
#include <cstdint>
#include <string>
#include <list>
#include <chrono>
#include <functional>
#include <memory>
#include <map>
#include <string.h>

#include <sys/types.h>
#include <sys/sysmacros.h>
class DobbyTimer;

class DobbyUtilsImpl
{
public:

    virtual ~DobbyUtilsImpl() = default;
    virtual bool cancelTimer(int timerId) const = 0;
    virtual int startTimer(const std::chrono::microseconds& timeout,bool oneShot,const std::function<bool()>& handler) const = 0;

};

class DobbyUtils {

protected:
    static DobbyUtilsImpl* impl;

public:

     DobbyUtils();
    ~DobbyUtils();

    static void setImpl(DobbyUtilsImpl* newImpl);
    static DobbyUtils* getInstance();
    static bool cancelTimer(int timerId);
    static int startTimer(const std::chrono::microseconds& timeout,
                          bool oneShot,
                          const std::function<bool()>& handler);

};

#endif // !defined(DOBBYUTILS_H)
