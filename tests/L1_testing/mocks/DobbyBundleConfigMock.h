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
#include "DobbyBundleConfig.h"

class DobbyBundleConfigMock : public DobbyBundleConfigImpl {

public:
    virtual ~DobbyBundleConfigMock() = default;

    MOCK_METHOD(std::shared_ptr<rt_dobby_schema>, config, (), (const,override));
    MOCK_METHOD(bool, restartOnCrash, (), (const,override));
    MOCK_METHOD(bool, isValid, (), (const,override));
#ifdef LEGACY_COMPONENTS
    MOCK_METHOD((const std::map<std::string, Json::Value>&), legacyPlugins, (), (const));
#endif /* LEGACY_COMPONENTS */
    MOCK_METHOD((const std::map<std::string, Json::Value>&), rdkPlugins, (), (const,override));
};

