/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Synamedia
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

#pragma once

#include <gmock/gmock.h>
#include "IDobbyRdkLoggingPlugin.h"

class IDobbyRdkLoggingPluginMock : public IDobbyRdkLoggingPlugin {
public:

    virtual ~IDobbyRdkLoggingPluginMock() = default;

    MOCK_METHOD(void, RegisterPollSources, (int fd, std::shared_ptr<AICommon::IPollLoop> pollLoop), (override));
    MOCK_METHOD(void, DumpToLog, (const int bufferFd), (override));
    MOCK_METHOD(std::string , name, (), (const,override));
    MOCK_METHOD(unsigned, hookHints, (), (const,override));
    MOCK_METHOD(bool, postInstallation, (), (override));
    MOCK_METHOD(bool, preCreation, (), (override));
    MOCK_METHOD(bool, createRuntime, (), (override));
    MOCK_METHOD(bool, createContainer, (), (override));
#ifdef USE_STARTCONTAINER_HOOK
    MOCK_METHOD(bool, startContainer, (), (override));
#endif

    MOCK_METHOD(bool, postStart, (), (override));
    MOCK_METHOD(bool, postHalt, (), (override));
    MOCK_METHOD(bool, postStop, (), (override));
    MOCK_METHOD(std::vector<std::string>, getDependencies, (), (const,override));

};

