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
#include "DobbyRunC.h"

class DobbyRunCMock : public DobbyRunCImpl {

public:

    virtual ~DobbyRunCMock() = default;

    MOCK_METHOD((std::pair<pid_t, pid_t>), create, ((const ContainerId &id),
                                   (const std::shared_ptr<const DobbyBundle> &bundle),
                                   (const std::shared_ptr<const IDobbyStream> &console),
                                   (const std::list<int> &files),
                                   (const std::string& customConfigPath)), (const,override));
    MOCK_METHOD(bool, destroy, (const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console, bool force), (const,override));
    MOCK_METHOD(bool, start, (const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console), (const,override));
    MOCK_METHOD(bool, killCont, (const ContainerId &id, int signal, bool all), (const,override));
    MOCK_METHOD(bool, resume, (const ContainerId& id), (const,override));
    MOCK_METHOD(bool, pause, (const ContainerId& id), (const,override));
    MOCK_METHOD((std::pair<pid_t, pid_t>), exec, ((const ContainerId& id), (const std::string& options), (const std::string& command)), (const,override));
    MOCK_METHOD(DobbyRunC::ContainerStatus, state, (const ContainerId& id), (const,override));
    MOCK_METHOD(std::list<DobbyRunC::ContainerListItem>, list, (), (const,override));
    MOCK_METHOD(const std::string, getWorkingDir, (), (const,override));
};

