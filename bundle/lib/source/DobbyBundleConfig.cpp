/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
 * Copyright (C) Sky UK 2016+
 */

#include "DobbyBundleConfig.h"
#include "IDobbyUtils.h"

#include <array>

#include <grp.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/sysinfo.h>


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
    , mGpuEnabled(false)
    , mGpuMemLimit(GPU_MEMLIMIT_DEFAULT)
    , mSystemDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mSessionDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mDebugDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mConsoleDisabled(true)
    , mConsoleLimit(-1)
    , mRootfsPath("rootfs")
{
    // because jsoncpp can throw exceptions if we fail to check the json types
    // before performing conversions we wrap the whole parse operation in a
    // try / catch
    try
    {
        // go and parse the OCI config file for plugins to use
        mValid = parseOCIConfig(bundlePath);

        // deserialise config.json
        parser_error err;
        std::string configPath = bundlePath + "/config.json";
        mConf = std::shared_ptr<rt_dobby_schema>(
                    rt_dobby_schema_parse_file(configPath.c_str(), nullptr, &err),
                    free_rt_dobby_schema);

        if (mConf.get() == nullptr || err)
        {
            AI_LOG_ERROR_EXIT("Failed to parse bundle config, err '%s'", err);
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
}

DobbyBundleConfig::~DobbyBundleConfig()
{
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

bool DobbyBundleConfig::gpuEnabled() const
{
    return mGpuEnabled;
}

size_t DobbyBundleConfig::gpuMemLimit() const
{
    return mGpuMemLimit;
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

const std::map<std::string, Json::Value>& DobbyBundleConfig::legacyPlugins() const
{
    return mLegacyPlugins;
}

const std::map<std::string, Json::Value>& DobbyBundleConfig::rdkPlugins() const
{
    return mRdkPlugins;
}

const std::list<std::string> DobbyBundleConfig::sysHooks() const
{
    return mEnabledSysHooks;
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
    ssize_t length = bundleConfigFs.tellg();
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
        if (!processLegacyPlugins(mConfig["legacyPlugins"]))
        {
            return false;
        }
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

        // Parse Logging plugin
        if (rdkPlugins.isMember(RDK_LOGGING_PLUGIN_NAME))
        {
            if (!processLogging(rdkPlugins[RDK_LOGGING_PLUGIN_NAME]))
            {
                return false;
            }
        }

        // Parse GPU plugin
        if (rdkPlugins.isMember(RDK_GPU_PLUGIN_NAME))
        {
            if (!processGpu(rdkPlugins[RDK_GPU_PLUGIN_NAME]))
            {
                return false;
            }
        }

        // Parse IPC plugin
        if (rdkPlugins.isMember(RDK_IPC_PLUGIN_NAME))
        {
            if (!processIpc(rdkPlugins[RDK_IPC_PLUGIN_NAME]))
            {
                return false;
            }
        }

        // Parse DRM plugin
        if (rdkPlugins.isMember(RDK_DRM_PLUGIN_NAME))
        {
            if (!processDrm(rdkPlugins[RDK_DRM_PLUGIN_NAME]))
            {
                return false;
            }
        }

        // Parse RDK Services plugin
        if (rdkPlugins.isMember(RDK_RDKSERVICES_PLUGIN_NAME))
        {
            if (!processRdkServices(rdkPlugins[RDK_RDKSERVICES_PLUGIN_NAME]))
            {
                return false;
            }
        }
    }

    // enable syshooks for use whilst RDK plugins are developed
    setSysHooksAndRdkPlugins();

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the logging plugin field
 *
 *  Example json in rdkPlugins.logging:
 *
 *  "data":{
 *      "console": {
 *          "enabled":true,
 *          "path":"/tmp/data/console.log",
 *          "limit":1048576
 *      }
 *  }
 *
 *  The console settings aren't given to the crun, instead it determines the
 *  type of stream that we attach to read the console output.
 *
 *  Typically on debug builds the console will be redirected to a file on the
 *  flash.  On release builds it is redirected to /dev/null.
 *
 *  If console is null, then the terminal stdin / stdout & stderr are all
 *  redirected to /dev/null.  If not null then stdout & stderr are redirected
 *  into the supplied file, with an optional size limit on it.
 *
 *  @param[in]  value       The rdkPlugins.logging field from extended bundle
 *                          config.
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbyBundleConfig::processLogging(const Json::Value& value)
{
    if (!value.isObject() || !value.isMember("data") || !value["data"].isObject())
    {
        AI_LOG_ERROR("invalid rdkPlugin.logging.data field");
        return false;
    }
    Json::Value data = value["data"];

    if (data.isMember("console"))
    {
        const Json::Value& consoleData = data["console"];
        if (!consoleData.isObject())
        {
            AI_LOG_ERROR("invalid logging.data.console field");
            return false;
        }

        if (consoleData["enabled"].asBool() == false)
        {
            mConsoleDisabled = true;
            return true;
        }

        const Json::Value& path = consoleData["path"];
        if (path.isNull())
        {
            mConsoleDisabled = true;
        }
        else if (path.isString())
        {
            mConsolePath = path.asString();
        }
        else
        {
            AI_LOG_ERROR("invalid logging.data.console.path field");
            return false;
        }

        const Json::Value& limit = consoleData["limit"];
        if (limit.isNull())
        {
            mConsoleLimit = -1;
        }
        else if (limit.isIntegral())
        {
            mConsoleLimit = limit.asInt();
            mConsoleLimit = std::max<ssize_t>(mConsoleLimit, -1);
        }
        else
        {
            AI_LOG_ERROR("invalid logging.data.console.limit field");
            return false;
        }

        mConsoleDisabled = false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the ipc plugin field
 *
 *  Example json in rdkPlugins.ipc:
 *
 *  "data":{
 *      "session": "ai-public",
 *      "system": "system",
 *      "debug": "ai-private"
 *  }
 *
 *  This config options allow you to specify what bus to map into the container
 *  and what to call it inside the container.
 *
 *  @param[in]  value       The rdkPlugins.ipc field from extended bundle config.
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbyBundleConfig::processIpc(const Json::Value& value)
{
    if (!value.isObject() || !value.isMember("data") || !value["data"].isObject())
    {
        AI_LOG_ERROR("invalid rdkPlugin.ipc.data field");
        return false;
    }
    Json::Value data = value["data"];

    static const std::map<std::string, IDobbyIPCUtils::BusType> busTypes =
    {
        {   "system",       IDobbyIPCUtils::BusType::SystemBus     },
        {   "ai-public",    IDobbyIPCUtils::BusType::AIPublicBus   },
        {   "ai-private",   IDobbyIPCUtils::BusType::AIPrivateBus  },
    };

    // process the system dbus field
    {
        const Json::Value& system = data["system"];
        if (system.isString())
        {
            std::map<std::string, IDobbyIPCUtils::BusType>::const_iterator it = busTypes.find(system.asString());
            if (it == busTypes.end())
            {
                AI_LOG_ERROR("invalid 'ipc.data.system' field");
                return false;
            }

            mSystemDbus = it->second;
        }
        else if (!system.isNull())
        {
            AI_LOG_ERROR("invalid 'ipc.data.system' field");
            return false;
        }
    }

    // process the session dbus field
    {
        const Json::Value& session = data["session"];
        if (session.isString())
        {
            std::map<std::string, IDobbyIPCUtils::BusType>::const_iterator it = busTypes.find(session.asString());
            if (it == busTypes.end())
            {
                AI_LOG_ERROR("invalid 'ipc.data.session' field");
                return false;
            }

            mSessionDbus = it->second;
        }
        else if (!session.isNull())
        {
            AI_LOG_ERROR("invalid 'ipc.data.session' field");
            return false;
        }
    }

#if (AI_BUILD_TYPE == AI_DEBUG)
    // process the debug dbus field
    {
        const Json::Value& debug = data["debug"];
        if (debug.isString())
        {
            std::map<std::string, IDobbyIPCUtils::BusType>::const_iterator it = busTypes.find(debug.asString());
            if (it == busTypes.end())
            {
                AI_LOG_ERROR("invalid 'ipc.data.debug' field");
                return false;
            }

            mDebugDbus = it->second;
        }
        else if (!debug.isNull())
        {
            AI_LOG_ERROR("invalid 'ipc.data.debug' field");
            return false;
        }
    }
#endif

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the gpu plugin field
 *
 *  Example json in rdkPlugins.gpu:
 *
 *  "data":{
 *      "memory": 67108864
 *  }
 *
 *  @param[in]  value       The rdkPlugins.gpu field from extended bundle config.
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbyBundleConfig::processGpu(const Json::Value& value)
{
    if (!value.isObject() || !value.isMember("data") || !value["data"].isObject())
    {
        AI_LOG_ERROR("invalid rdkPlugin.gpu.data field");
        return false;
    }
    Json::Value data = value["data"];

    const Json::Value& memLimit = data["memory"];

    mGpuEnabled = true;

    if (memLimit.isIntegral())
    {
        mGpuMemLimit = memLimit.asUInt();
    }
    else if (memLimit.isNull())
    {
        mGpuMemLimit = GPU_MEMLIMIT_DEFAULT;
    }
    else
    {
        AI_LOG_ERROR("invalid 'gpu.data.memory' field");
        return false;
    }

    // lazily init the GPU dev nodes mapping - we used to do this at start-up
    // but hit an issue on broadcom platforms where the dev nodes aren't
    // created until the gpu library is used
    if (!mInitialisedGpuDevNodes)
    {
        DobbyConfig::initGpuDevNodes(mSettings->gpuDeviceNodes());
    }

    // check if a special 'GPU' group id is needed
    const int gpuGroupId = mSettings->gpuGroupId();
    if (gpuGroupId > 0)
    {
        // If gidMappings doesn't exist in bundle config, add it
        if (!mConfig["linux"]["gidMappings"].isArray())
        {
            mConfig["linux"]["gidMappings"] = Json::arrayValue;
        }

        Json::Value newGid;

        newGid["hostID"] = gpuGroupId;
        newGid["containerID"] = gpuGroupId;
        newGid["size"] = 1;

        mConfig["linux"]["gidMappings"].append(newGid);
    }

    // add any extra mounts (i.e. ipc sockets, shared memory files, etc)
    Json::Value newMount;
    if (mSettings->gpuHasExtraMounts())
    {
        const std::list<IDobbySettings::GpuExtraMount> extraMounts =
            mSettings->gpuExtraMounts();

        for (const IDobbySettings::GpuExtraMount& extraMount : extraMounts)
        {
            newMount["destination"] = extraMount.source;
            newMount["type"] = extraMount.target;
            newMount["source"] = extraMount.type;

            for (const std::string& flag : extraMount.flags)
            {
                newMount["options"].append(flag);
            }

            mConfig["mounts"].append(newMount);
        }
    }

    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Processes the legacy plugins field.
 *
 *  This parses the legacy Dobby plugins to mPlugins.
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

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the rdkServices plugin field
 *
 *  Example json in rdkPlugins.rdkServices:
 *
 *  "data":{
 *  }
 *
 *  @param[in]  value       The rdkPlugins.rdkServices field from extended bundle
 *                          config.
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbyBundleConfig::processRdkServices(const Json::Value& value)
{
    AI_LOG_ERROR("rdkServices plugin is not supported yet.");
    return false;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the drm plugin field
 *
 *  Example json in rdkPlugins.drm:
 *
 *  "data":{
 *  }
 *
 *  @param[in]  value       The rdkPlugins.drm field from extended bundle config.
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbyBundleConfig::processDrm(const Json::Value& value)
{
    AI_LOG_ERROR("drm plugin is not supported yet.");
    return false;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Sets the placeholder Dobby syshooks and removes RDK plugins in
 *  development.
 *
 *  NOTE: This should only be used until the RDK plugin is fully developed.
 *
 *  With this in place, we can have syshooks turned on or off selectively.
 */
void DobbyBundleConfig::setSysHooksAndRdkPlugins(void)
{
    // iterate through all rdk plugins in development to decide which syshooks
    // should still be used. The RDK plugins are listed in the static variable
    // DobbyConfig::mRdkPluginsInDevelopment
    std::map<std::string, std::list<std::string>>::const_iterator it = mRdkPluginsInDevelopment.begin();
    for (; it != mRdkPluginsInDevelopment.end(); ++it)
    {
        std::string rdkPluginName = it->first;
        std::list<std::string> pluginSysHooks = it->second;
        // if rdk plugin is turned on for this container, use syshooks instead
        if (mRdkPlugins.find(rdkPluginName) != mRdkPlugins.end())
        {
            mRdkPlugins.erase(rdkPluginName);
            std::list<std::string>::const_iterator jt = pluginSysHooks.begin();
            for (; jt != pluginSysHooks.end(); ++jt)
            {
                mEnabledSysHooks.emplace_back(*jt);
            }
        }
    }
}
