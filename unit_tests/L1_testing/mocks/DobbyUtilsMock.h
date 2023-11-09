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

#pragma once

#include <gmock/gmock.h>
#include "DobbyUtils.h"

class DobbyUtilsMock : public DobbyUtilsImpl{
public:
    virtual ~DobbyUtilsMock() = default;
    MOCK_METHOD(bool, cancelTimer, (int timerId), (const, override));
    MOCK_METHOD(int, startTimer, (const std::chrono::microseconds& timeout, bool oneShot, const std::function<bool()>& handler), (const, override));
};
