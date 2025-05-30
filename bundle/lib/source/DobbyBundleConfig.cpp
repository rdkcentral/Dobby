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
 * File:   DobbyBundleConfig.cpp
 *
 */

#include "DobbyBundleConfig.h"
#include "IDobbyUtils.h"

#include <array>

#include <grp.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <fstream>


// -----------------------------------------------------------------------------
/**
 *  @brief Constructor that parses an OCI bundle's config file to be used by Dobby.
 *  Plugins under 'rdkPlugins' and 'legacyPlugins' are parsed if found (OCI bundle*).
 *
 *  @param[in]  utils               The daemon utils object.
 *  @param[in]  settings            Dobby settings object.
 *  @param[in]  id                  Container ID.
 *  @param[in]  bundlePath          Path to OCI bundle.
 */
DobbyBundleConfig::DobbyBundleConfig(const std::shared_ptr<IDobbyUtils>& utils,
                                     const std::shared_ptr<const IDobbySettings>& settings,
                                     const ContainerId& id,
                                     const std::string& bundlePath)
    : mUtilities(utils)
    , mSettings(settings)
    , mConf(nullptr)
    , mUserId(-1)
    , mGroupId(-1)
    , mRestartOnCrash(false)
    , mSystemDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mSessionDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mDebugDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mConsoleDisabled(true)
    , mConsoleLimit(-1)
    , mRootfsPath("rootfs")
{
    if(!constructConfig(id, bundlePath))
    {
        AI_LOG_WARN("Failed to create dobby config, retrying with backup");
        // We failed to create config, probably source file was corrupted
        // try to recover original config and create it again from that
        std::string backupConfig = bundlePath + "/config-dobby.json";
        if( access( backupConfig.c_str(), F_OK ) == 0 ) 
        {
            // return config file to original state
            std::ifstream src(backupConfig, std::ios::binary);
            std::ofstream dst(bundlePath + "/config.json", std::ios::binary);
            dst << src.rdbuf();

            //close the file else on further operations on the file, the size of the file equals 32764 even if it is greater than 32764.
            dst.close();

            // we need to re-run post installation hook, so remove success flag
            std::string postInstallPath = bundlePath + "/postinstallhooksuccess";
            remove(postInstallPath.c_str());

            // Retry creation of config
            constructConfig(id, bundlePath);
        }
    }

    if(!mValid)
    {
        AI_LOG_ERROR("Failed to create dobby config");
    }
}

DobbyBundleConfig::~DobbyBundleConfig()
{
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates config object
 *
 *  This method parses OCI config and creates dobby config based on that.
 *  This was an old constructor for DobbyBundleConfig, but we need to be able
 *  to recover in case config gets damaged.
 * 
 *  @param[in]  id            Container ID.
 *  @param[in]  bundlePath    Path to the container's OCI bundle
 * 
 *
 *  @return true if the config is valid, otherwise false.
 */
bool DobbyBundleConfig::constructConfig(const ContainerId& id, const std::string& bundlePath)
{
    // because jsoncpp can throw exceptions if we fail to check the json types
    // before performing conversions we wrap the whole parse operation in a
    // try / catch
    try
    {
        // go and parse the OCI config file for plugins to use
        mValid = parseOCIConfig(bundlePath);

        // de-serialise config.json
        parser_error err = nullptr;
        std::string configPath = bundlePath + "/config.json";
        mConf = std::shared_ptr<rt_dobby_schema>(
                    rt_dobby_schema_parse_file(configPath.c_str(), nullptr, &err),
                    free_rt_dobby_schema);

        if (mConf.get() == nullptr || err)
        {
            AI_LOG_ERROR_EXIT("Failed to parse bundle config, err '%s'", err);
            if (err)
            {
                free(err);
                err = nullptr;
            }
            mValid = false;
        }
        else
        {
            // convert OCI config to compliant using libocispec
            mValid &= DobbyConfig::convertToCompliant(id, mConf, bundlePath);
        }
    }
    catch (const Json::Exception& e)
    {
        AI_LOG_ERROR("exception thrown during config parsing - %s", e.what());
        mValid = false;
    }

    return mValid;
}

bool DobbyBundleConfig::isValid() const
{
    return mValid;
}

uid_t DobbyBundleConfig::userId() const
{
    return mUserId;
}

gid_t DobbyBundleConfig::groupId() const
{
    return mGroupId;
}

const std::string& DobbyBundleConfig::rootfsPath() const
{
    return mRootfsPath;
}

bool DobbyBundleConfig::restartOnCrash() const
{
    return mRestartOnCrash;
}

IDobbyIPCUtils::BusType DobbyBundleConfig::systemDbus() const
{
    return mSystemDbus;
}

IDobbyIPCUtils::BusType DobbyBundleConfig::sessionDbus() const
{
    return mSessionDbus;
}

IDobbyIPCUtils::BusType DobbyBundleConfig::debugDbus() const
{
    return mDebugDbus;
}

bool DobbyBundleConfig::consoleDisabled() const
{
    return mConsoleDisabled;
}

ssize_t DobbyBundleConfig::consoleLimit() const
{
    return mConsoleLimit;
}

const std::string& DobbyBundleConfig::consolePath() const
{
    return mConsolePath;
}

#if defined(LEGACY_COMPONENTS)
const std::map<std::string, Json::Value>& DobbyBundleConfig::legacyPlugins() const
{
    return mLegacyPlugins;
}
#endif //defined(LEGACY_COMPONENTS)

const std::map<std::string, Json::Value>& DobbyBundleConfig::rdkPlugins() const
{
    return mRdkPlugins;
}

std::shared_ptr<rt_dobby_schema> DobbyBundleConfig::config() const
{
    return mValid ? std::shared_ptr<rt_dobby_schema>(mConf) : nullptr;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Parses the bundle config's contents that are needed by plugins
 *
 *  The function is atomic, therefore if it returns true you can
 *  guarantee it stuck and will be set for the lifetime of the function.
 *
 *  @param[in]  bundlePath    path to the container's OCI bundle
 *
 *  @return true if the path was set, otherwise false.
 */
bool DobbyBundleConfig::parseOCIConfig(const std::string& bundlePath)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // Parse config.json to a Json::Value type
    std::ifstream bundleConfigFs(bundlePath + "/config.json", std::ifstream::binary);
    if (!bundleConfigFs)
    {
        AI_LOG_ERROR_EXIT("failed to open bundle config file at '%s'", bundlePath.c_str());
        return false;
    }
    bundleConfigFs.seekg(0, std::ifstream::end);
    uint32_t length = bundleConfigFs.tellg();
    bundleConfigFs.seekg(0, std::ifstream::beg);
    char* buffer = new char[length];
    bundleConfigFs.read(buffer, length);
    std::string jsonConfigString(buffer, length);
    delete [] buffer;
    std::istringstream sin(jsonConfigString);
    sin >> mConfig;

    // Populate the object with any needed values
    mUserId = mConfig["process"]["user"]["uid"].asInt();
    mGroupId = mConfig["process"]["user"]["gid"].asInt();
    mRootfsPath = mConfig["root"]["path"].asString();

    // Parse legacy plugins if present & not null
    if (mConfig.isMember("legacyPlugins") && mConfig["legacyPlugins"].isObject())
    {
#if defined(LEGACY_COMPONENTS)
        if (!processLegacyPlugins(mConfig["legacyPlugins"]))
        {
            return false;
        }
#else
        AI_LOG_ERROR_EXIT("legacyPlugins is unsupported, build with "
                          "LEGACY_COMPONENTS=ON to use legacy plugins");
        return false;
#endif //defined(LEGACY_COMPONENTS)
    }

    // Parse rdk plugins if present & not null
    if (mConfig.isMember("rdkPlugins") && mConfig["rdkPlugins"].isObject())
    {
        Json::Value rdkPlugins = mConfig["rdkPlugins"];

        if (!rdkPlugins.isObject())
        {
            AI_LOG_ERROR("invalid rdkPlugins field");
        }

        for (const auto &rdkPluginName : rdkPlugins.getMemberNames())
        {
            mRdkPlugins.emplace(rdkPluginName, rdkPlugins[rdkPluginName]);
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

#if defined(LEGACY_COMPONENTS)
// -----------------------------------------------------------------------------
/**
 *  @brief Processes the legacy plugins field.
 *
 *  This parses the legacy Dobby plugins to mLegacyPlugins.
 *
 *  @param[in]  value       The legacyPlugins field from extended bundle config.
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbyBundleConfig::processLegacyPlugins(const Json::Value& value)
{
    if (!value.isObject())
    {
        AI_LOG_ERROR("invalid legacyPlugins field");
        return false;
    }

    for (const std::string& id : value.getMemberNames())
    {
        const Json::Value& plugin = value.get(id, "");

        if (!plugin.isObject())
        {
            AI_LOG_ERROR("invalid legacyPlugin entry %s", id.c_str());
            return false;
        }

        // the name field must be a string
        const Json::Value& name = id;
        if (!name.isString())
        {
            AI_LOG_ERROR("invalid legacyPlugin.name entry %s", id.c_str());
            return false;
        }

        // the data can be anything, we don't place any restrictions on it since
        // it's just passed to the hook library for processing
        Json::Value data = plugin["data"];

        // add the hook to the list
        mLegacyPlugins.emplace(name.asString(), std::move(data));
    }

    return true;
}
#endif //defined(LEGACY_COMPONENTS)
