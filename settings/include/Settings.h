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

class Settings : public IDobbySettings
{
private:
    Settings();
    explicit Settings(const Json::Value& settings);

public:
    ~Settings() final = default;

    static std::shared_ptr<Settings> fromJsonFile(const std::string& filePath);
    static std::shared_ptr<Settings> defaultSettings();

public:
    void setDBusServiceName(const std::string& name);
    std::string dbusServiceName() const override;

    void setDBusObjectPath(const std::string& path);
    std::string dbusObjectPath() const override;


    std::string workspaceDir() const override;
    std::string persistentDir() const override;

    std::string consoleSocketPath() const override;

    std::map<std::string, std::string> extraEnvVariables() const override;

    std::list<std::string> gpuDeviceNodes() const override;
    int gpuGroupId() const override;
    bool gpuHasExtraMounts() const override;
    std::list<GpuExtraMount> gpuExtraMounts() const override;

    std::set<std::string> externalInterfaces() const override;

    void dump(int aiLogLevel = -1) const;

private:
    void setDefaults();

    bool isDir(const std::string& path, int accessFlags = 0) const;

    int getGroupId(const std::string& name) const;

    std::string getPathFromEnv(const char* env,
                               const char* fallbackPath) const;
    std::list<std::string> getPathsFromJson(const Json::Value& value) const;

    std::map<std::string, std::string> getEnvVarsFromJson(const Json::Value& root,
                                                          const Json::Path& path) const;

    std::list<std::string> getGpuDevNodes(const Json::Value& root,
                                          const Json::Path& path) const;

    std::list<GpuExtraMount> getGpuExtraMounts(const Json::Value& root,
                                               const Json::Path& path) const;
    bool processMountObject(const Json::Value& value,
                            GpuExtraMount* mount) const;

private:
    std::string mDBusServiceName;
    std::string mDBusObjectPath;
    std::string mWorkspaceDir;
    std::string mPersistentDir;
    std::string mConsoleSocketPath;

    std::map<std::string, std::string> mExtraEnvVars;

    int mGpuGroupId;
    std::list<std::string> mGpuDevNodes;
    std::list<GpuExtraMount> mGpuExtraMounts;

    std::set<std::string> mExternalInterfaces;
};

#endif // !defined(SETTINGS_H)
