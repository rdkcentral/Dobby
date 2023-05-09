/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
/*
 * File:   DobbySpecConfig.h
 *
 */
#ifndef DOBBYSPECCONFIG_H
#define DOBBYSPECCONFIG_H

#include "DobbyConfig.h"
#include "DobbyBundle.h"

#include <set>
#include <bitset>
#include <memory>

namespace ctemplate {
    class TemplateDictionary;
}


// -----------------------------------------------------------------------------
/**
 *  @class DobbySpecConfig
 *  @brief Takes a JSON formatted spec file in the constructor, parses it and
 *  extracts the relevant fields.
 *
 *  The JSON should be in our custom 'dobby' container format, this includes
 *  extra custom fields for things like /etc files, plugin configurations
 *  and so forth.
 *
 *  It's main purpose is to convert the dobby spec file to a OCI conformant
 *  JSON file.
 *
 *
 */
class DobbySpecConfig : public DobbyConfig
{
public:
    DobbySpecConfig(const std::shared_ptr<IDobbyUtils>& utils,
                    const std::shared_ptr<const IDobbySettings>& settings,
                    const ContainerId& id,
                    const std::shared_ptr<const DobbyBundle>& bundle,
                    const std::string& specJson);
    DobbySpecConfig(const std::shared_ptr<IDobbyUtils>& utils,
                    const std::shared_ptr<const IDobbySettings>& settings,
                    const std::shared_ptr<const DobbyBundle>& bundle,
                    const std::string& specJson);
    ~DobbySpecConfig();

public:
    bool isValid() const override;

public:
    const std::string spec() const override;

public:
    uid_t userId() const override;
    gid_t groupId() const override;

public:
    IDobbyIPCUtils::BusType systemDbus() const override;
    IDobbyIPCUtils::BusType sessionDbus() const override;
    IDobbyIPCUtils::BusType debugDbus() const override;

public:
    bool restartOnCrash() const override;

public:
    std::shared_ptr<rt_dobby_schema> config() const override;

public:
    const std::string& etcHosts() const;
    const std::string& etcServices() const;
    const std::string& etcPasswd() const;
    const std::string& etcGroup() const;
    const std::string& etcLdSoPreload() const;

public:
    bool consoleDisabled() const override;
    ssize_t consoleLimit() const override;
    const std::string& consolePath() const override;

public:
    const std::map<std::string, Json::Value>& legacyPlugins() const override;
    const std::map<std::string, Json::Value>& rdkPlugins() const override;

public:
    typedef struct _MountPoint
    {
        enum Type { Directory, File } type;
        std::string destination;
    } MountPoint;
    std::vector<MountPoint> mountPoints() const;

public:
    const std::string& rootfsPath() const override;

private:
    bool parseSpec(ctemplate::TemplateDictionary* dictionary,
                   const std::string& json,
                   int bundleFd);

private:
    #define JSON_FIELD_PROCESSOR(x) \
        bool x(const Json::Value&, ctemplate::TemplateDictionary*)

    JSON_FIELD_PROCESSOR(processAppId);
    JSON_FIELD_PROCESSOR(processEnv);
    JSON_FIELD_PROCESSOR(processArgs);
    JSON_FIELD_PROCESSOR(processCwd);
    JSON_FIELD_PROCESSOR(processConsole);
    JSON_FIELD_PROCESSOR(processUser);
    JSON_FIELD_PROCESSOR(processUserNs);
    JSON_FIELD_PROCESSOR(processEtc);
    JSON_FIELD_PROCESSOR(processNetwork);
    JSON_FIELD_PROCESSOR(processRtPriority);
    JSON_FIELD_PROCESSOR(processRestartOnCrash);
    JSON_FIELD_PROCESSOR(processMounts);
    JSON_FIELD_PROCESSOR(processLegacyPlugins);
    JSON_FIELD_PROCESSOR(processMemLimit);
    JSON_FIELD_PROCESSOR(processGpu);
    JSON_FIELD_PROCESSOR(processVpu);
    JSON_FIELD_PROCESSOR(processDbus);
    JSON_FIELD_PROCESSOR(processSyslog);
    JSON_FIELD_PROCESSOR(processCpu);
    JSON_FIELD_PROCESSOR(processDevices);
    JSON_FIELD_PROCESSOR(processCapabilities);

    #undef JSON_FIELD_PROCESSOR

    bool processLoopMount(const Json::Value& value,
                            ctemplate::TemplateDictionary* dictionary,
                            Json::Value& loopMntData);

private:
    void insertIntoRdkPluginJson(const std::string& pluginName,
                                 const Json::Value& pluginData);
    bool processRdkPlugins(const Json::Value& value,
                           ctemplate::TemplateDictionary* dictionary);

private:
    template <std::size_t N>
    std::bitset<N> parseBitset(const std::string& str) const;

private:
    void storeMountPoint(const std::string &type,
                         const std::string &source,
                         const std::string &destination);

private:
    std::string jsonToString(const Json::Value& jsonObject);

private:
    static void addGpuDevNodes(const std::shared_ptr<const IDobbySettings::HardwareAccessSettings> &settings,
                               ctemplate::TemplateDictionary *dict);

    static  void addVpuDevNodes(const std::shared_ptr<const IDobbySettings::HardwareAccessSettings> &settings,
                                ctemplate::TemplateDictionary *dict);

private:
    const std::shared_ptr<IDobbyUtils> mUtilities;
    const std::shared_ptr<const IDobbySettings::HardwareAccessSettings> mGpuSettings;
    const std::shared_ptr<const IDobbySettings::HardwareAccessSettings> mVpuSettings;
    const std::vector<std::string> mDefaultPlugins;
    const Json::Value mRdkPluginsData;

private:
    bool mValid;
    ctemplate::TemplateDictionary* mDictionary;

private:
    Json::Value mSpec;
    Json::Value mRdkPluginsJson;
    std::shared_ptr<rt_dobby_schema> mConf;

private:
    enum class SpecVersion {
        Unknown,
        Version1_0,
        Version1_1,
    } mSpecVersion;

private:
    uid_t mUserId;
    gid_t mGroupId;

private:
    bool mRestartOnCrash;

private:
    IDobbyIPCUtils::BusType mSystemDbus;
    IDobbyIPCUtils::BusType mSessionDbus;
    IDobbyIPCUtils::BusType mDebugDbus;

private:
    bool mConsoleDisabled;
    std::string mConsolePath;
    ssize_t mConsoleLimit;

private:
    std::map<std::string, Json::Value> mLegacyPlugins;
    std::map<std::string, Json::Value> mRdkPlugins;

private:
    std::vector<MountPoint> mMountPoints;

private:
    std::string mEtcHosts;
    std::string mEtcServices;
    std::string mEtcPasswd;
    std::string mEtcGroup;
    std::string mEtcLdSoPreload;

private:
    static int mNumCores;

private:
    static const std::map<std::string, int> mAllowedCaps;

private:
    std::string mRootfsPath;

};


#endif // !defined(DOBBYSPECCONFIG_H)