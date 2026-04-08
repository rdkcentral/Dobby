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
 * File:   DobbyBundleConfig.h
 *
 */
#ifndef DOBBYBUNDLECONFIG_H
#define DOBBYBUNDLECONFIG_H

#include "DobbyConfig.h"

#include <fstream>

// -----------------------------------------------------------------------------
/**
 *  @class DobbyBundleConfig
 *  @brief Takes a JSON formatted OCI bundle configuration file in the constructor,
 *  parses it and extracts the relevant fields.
 *
 *  It's main purpose is to read an extended OCI bundle config with plugins so
 *  it can be converted into an OCI compliant bundle.
 *
 *
 */
class DobbyBundleConfig : public DobbyConfig
{
// ----------------------------------------------------------------------------
/**
 * Public methods
 */
public:
    DobbyBundleConfig(const std::shared_ptr<IDobbyUtils>& utils,
                      const std::shared_ptr<const IDobbySettings>& settings,
                      const ContainerId& id,
                      const std::string& bundlePath);
    ~DobbyBundleConfig();

public:
    bool isValid() const override;

public:
    uid_t userId() const override;
    gid_t groupId() const override;
    void setUidGidMappings(uid_t userId, gid_t groupId);

public:
    IDobbyIPCUtils::BusType systemDbus() const override;
    IDobbyIPCUtils::BusType sessionDbus() const override;
    IDobbyIPCUtils::BusType debugDbus() const override;

public:
    bool consoleDisabled() const override;
    ssize_t consoleLimit() const override;
    const std::string& consolePath() const override;

public:
    bool restartOnCrash() const override;

public:
    const std::string& rootfsPath() const override;

public:
    std::shared_ptr<rt_dobby_schema> config() const override;

public:
   const std::map<std::string, Json::Value>& rdkPlugins() const override;

#if defined(LEGACY_COMPONENTS)
public:
    const std::map<std::string, Json::Value>& legacyPlugins() const override;
#endif // defined(LEGACY_COMPONENTS)

// ----------------------------------------------------------------------------
/**
 * Private methods
 */
private:
    bool parseOCIConfig(const std::string& bundlePath);
    bool constructConfig(const ContainerId& id,
                         const std::string& bundlePath);

private:
#if defined(LEGACY_COMPONENTS)
    bool processLegacyPlugins(const Json::Value& value);
#endif // defined(LEGACY_COMPONENTS)

// ----------------------------------------------------------------------------
/**
 * Member variables
 */
private:
    const std::shared_ptr<IDobbyUtils> mUtilities;
    const std::shared_ptr<const IDobbySettings> mSettings;

private:
    bool mValid;

private:
    Json::Value mConfig;
    std::shared_ptr<rt_dobby_schema> mConf;

private:
    uid_t mUserId;
    gid_t mGroupId;

private:
    //TODO: how to handle restart on crash option in extended bundles?
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
    std::map<std::string, Json::Value> mRdkPlugins;

#if defined(LEGACY_COMPONENTS)
private:
    std::map<std::string, Json::Value> mLegacyPlugins;
#endif // defined(LEGACY_COMPONENTS)

private:
    std::string mRootfsPath;
};

#endif // !defined(DOBBYBUNDLECONFIG_H)
