/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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
 * File:   Settings.h
 *
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include <IDobbySettings.h>

#if defined(RDK)
#  include <json/json.h>
#else
#  include <jsoncpp/json.h>
#endif

#include <memory>


// -----------------------------------------------------------------------------
/**
 *  @class Settings
 *  @brief Object containing the settings to pass to the Dobby.
 *
 *  Usually this is the parsed content of a JSON file and contains the platform
 *  specific details that Dobby needs to setup some stuff in the container.
 *
 */

class Settings final : public IDobbySettings
{
private:
    Settings();
    explicit Settings(const Json::Value& settings);

public:
    ~Settings() final = default;

    static std::shared_ptr<Settings> fromJsonFile(const std::string& filePath);
    static std::shared_ptr<Settings> defaultSettings();

public:
    std::string workspaceDir() const override;
    std::string persistentDir() const override;

    std::string consoleSocketPath() const override;

    std::map<std::string, std::string> extraEnvVariables() const override;

public:
    std::shared_ptr<HardwareAccessSettings> gpuAccessSettings() const override;
    std::shared_ptr<HardwareAccessSettings> vpuAccessSettings() const override;

    std::vector<std::string> externalInterfaces() const override;
    std::string addressRangeStr() const override;
    in_addr_t addressRange() const override;
    std::vector<std::string> defaultPlugins() const override;

    Json::Value rdkPluginsData() const override;

    LogRelaySettings logRelaySettings() const override;
    StraceSettings straceSettings() const override;
    ApparmorSettings apparmorSettings() const override;

    void dump(int aiLogLevel = -1) const;

private:
    void setDefaults();

    bool isDir(const std::string& path, int accessFlags = 0) const;

    int getGroupId(const std::string& name) const;
    std::set<int> getGroupIds(const Json::Value& field) const;

    std::string getPathFromEnv(const char* env,
                               const char* fallbackPath) const;
    std::list<std::string> getPathsFromJson(const Json::Value& value) const;

    std::map<std::string, std::string> getEnvVarsFromJson(const Json::Value& root,
                                                          const Json::Path& path) const;

    std::list<std::string> getDevNodes(const Json::Value& root,
                                       const Json::Path& path) const;

    std::list<ExtraMount> getExtraMounts(const Json::Value& root,
                                         const Json::Path& path) const;

    bool processMountObject(const Json::Value& value,
                            ExtraMount* mount) const;

    std::shared_ptr<HardwareAccessSettings> getHardwareAccess(const Json::Value& root,
                                                              const Json::Path& path) const;

    void dumpHardwareAccess(int aiLogLevel, const std::string& name,
                            const std::shared_ptr<const HardwareAccessSettings>& hwAccess) const;

private:
    std::string mWorkspaceDir;
    std::string mPersistentDir;
    std::string mConsoleSocketPath;

    std::map<std::string, std::string> mExtraEnvVars;

    std::shared_ptr<HardwareAccessSettings> mGpuHardwareAccess;
    std::shared_ptr<HardwareAccessSettings> mVpuHardwareAccess;

    std::vector<std::string> mExternalInterfaces;
    std::pair<std::string, in_addr_t> mAddressRange;
    std::vector<std::string>  mDefaultPlugins;

    Json::Value mRdkPluginsData;

    LogRelaySettings mLogRelaySettings;
    StraceSettings mStraceSettings;
    ApparmorSettings mApparmorSettings;
};

#endif // !defined(SETTINGS_H)
