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

#ifndef DOBBYSPECCONFIG_H
#define DOBBYSPECCONFIG_H

#include <set>
#include <bitset>
#include <memory>
#include "IDobbySettings.h"
#include "DobbyConfig.h"
#include "DobbyBundle.h"

class DobbySpecConfigImpl {

public:

    virtual ~DobbySpecConfigImpl() =default;

#if defined(LEGACY_COMPONENTS)
    virtual const std::map<std::string, Json::Value>& legacyPlugins() const =0;
    virtual const std::string spec() const =0;
#endif //defined(LEGACY_COMPONENTS)

    virtual const std::map<std::string, Json::Value>& rdkPlugins() const =0;

    virtual bool isValid() const =0;
    virtual std::shared_ptr<rt_dobby_schema> config() const =0;
    virtual bool restartOnCrash() const =0;

};

class DobbySpecConfig :public DobbyConfig {

protected:
    static DobbySpecConfigImpl* impl;

public:

    DobbySpecConfig();
    DobbySpecConfig(const std::shared_ptr<IDobbyUtils>& utils,const std::shared_ptr<const IDobbySettings>& settings,const ContainerId& id,const std::shared_ptr<const DobbyBundle>& bundle,const std::string& specJson);
    DobbySpecConfig(const std::shared_ptr<IDobbyUtils>& utils,const std::shared_ptr<const IDobbySettings>& settings,const std::shared_ptr<const DobbyBundle>& bundle,const std::string& specJson);
    ~DobbySpecConfig();

    static void setImpl(DobbySpecConfigImpl* newImpl);
    bool isValid() const;

#if defined(LEGACY_COMPONENTS)
    const std::map<std::string, Json::Value>& legacyPlugins() const;
    const std::string spec() const;
#endif //defined(LEGACY_COMPONENTS)

    const std::map<std::string, Json::Value>& rdkPlugins() const;

    std::shared_ptr<rt_dobby_schema> config() const;
    bool restartOnCrash() const;
};

#endif // !defined(DOBBYSPECCONFIG_H)
