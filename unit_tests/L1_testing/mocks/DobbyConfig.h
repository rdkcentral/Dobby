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

#ifndef DOBBYCONFIG_H
#define DOBBYCONFIG_H

#include "IDobbyUtils.h"
#include "ContainerId.h"
#include <IDobbyIPCUtils.h>
#include <IDobbySettings.h>
#include <Logging.h>
#include <rt_dobby_schema.h>

#if defined(RDK)
#  include <json/json.h>
#else
#  include <jsoncpp/json.h>
#endif

class DobbyConfigImpl {

public:

    virtual ~DobbyConfigImpl() =default;

    virtual bool writeConfigJson(const std::string& filePath) const = 0;
    virtual std::shared_ptr<rt_dobby_schema> config() const = 0;
    virtual bool changeProcessArgs(const std::string& command) = 0;
    virtual bool addWesterosMount(const std::string& socketPath) = 0;
    virtual bool addEnvironmentVar(const std::string& envVar) = 0;
    virtual bool enableSTrace(const std::string& logsDir) = 0;
    virtual void setApparmorProfile(const std::string& profileName) = 0;
    virtual void setPidsLimit(int limit) = 0;
    virtual std::string configJson() const = 0;

};

class DobbyConfig {

protected:
    static DobbyConfigImpl* impl;

public:

    DobbyConfig(){}
    ~DobbyConfig(){}

    static void setImpl(DobbyConfigImpl* newImpl);
    bool writeConfigJson(const std::string& filePath) const;
    virtual const std::map<std::string, Json::Value>& rdkPlugins() const = 0;
    const std::shared_ptr<rt_dobby_schema> config();
    bool changeProcessArgs(const std::string& command);
    bool addWesterosMount(const std::string& socketPath);
    bool addEnvironmentVar(const std::string& envVar);
    bool enableSTrace(const std::string& logsDir);
    void setApparmorProfile(const std::string& profileName);
    void setPidsLimit(int limit);
    const std::string configJson() const;

#if defined(LEGACY_COMPONENTS)
    virtual const std::string spec() const
    { return std::string(); }
    virtual const std::map<std::string, Json::Value>& legacyPlugins() const = 0;
#endif //defined(LEGACY_COMPONENTS)

};

#endif // !defined(DOBBYCONFIG_H)


