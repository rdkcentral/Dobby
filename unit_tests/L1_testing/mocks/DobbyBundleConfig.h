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

#ifndef DOBBYBUNDLECONFIG_H
#define DOBBYBUNDLECONFIG_H

#include <fstream>
#include "DobbyConfig.h"

class DobbyBundleConfigImpl {

public:

    virtual std::shared_ptr<rt_dobby_schema> config() const =0;
    virtual bool restartOnCrash() const =0;
    virtual bool writeConfigJson(const std::string& filePath) const = 0;
    virtual const std::map<std::string, Json::Value>& rdkPlugins() const = 0;
    virtual bool isValid() const = 0;

};

class DobbyBundleConfig : public DobbyConfig {

protected:
    static DobbyBundleConfigImpl* impl;

public:

    DobbyBundleConfig();
    DobbyBundleConfig(const std::shared_ptr<IDobbyUtils>& utils,const std::shared_ptr<const IDobbySettings>& settings,const ContainerId& id,const std::string& bundlePath);
    ~DobbyBundleConfig();

    bool isValid() const ;
    const std::map<std::string, Json::Value>& rdkPlugins() const ;
    static void setImpl(DobbyBundleConfigImpl* newImpl);
    static DobbyBundleConfig* getInstance();
    static std::shared_ptr<rt_dobby_schema> config();
    static bool restartOnCrash();
    static bool writeConfigJson(const std::string& filePath);
};


#endif // !defined(DOBBYBUNDLECONFIG_H)


