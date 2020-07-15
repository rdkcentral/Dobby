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
 * File:   DobbyConfig.h
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef DOBBYCONFIG_H
#define DOBBYCONFIG_H

#include "IDobbyUtils.h"
#include "ContainerId.h"
#include <IDobbyIPCUtils.h>
#include <IDobbySettings.h>
#include <Logging.h>

#if defined(RDK)
#  include <json/json.h>
#else
#  include <jsoncpp/json.h>
#endif

#ifdef DEV_VM
    #define PLUGINLAUNCHER_PATH "/usr/local/bin/DobbyPluginLauncher"
#else
    #define PLUGINLAUNCHER_PATH "/usr/bin/DobbyPluginLauncher"
#endif


#include "rt_dobby_schema.h"

#include <sstream>
#include <map>
#include <list>
#include <mutex>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/mount.h>

// Names of the RDK Plugins in the extended bundle
#define RDK_RDKSERVICES_PLUGIN_NAME     "rdkServices"
#define RDK_NETWORK_PLUGIN_NAME         "networking"
#define RDK_LOGGING_PLUGIN_NAME         "logging"
#define RDK_IPC_PLUGIN_NAME             "ipc"
#define RDK_STORAGE_PLUGIN_NAME         "storage"
#define RDK_DRM_PLUGIN_NAME             "drm"
#define RDK_GPU_PLUGIN_NAME             "gpu"

#define GPU_MEMLIMIT_DEFAULT            64 * 1024 * 1024


// -----------------------------------------------------------------------------
/**
 *  @class DobbyConfig
 *  @brief Interface that configuration file parser classes have to implement.
 */
class DobbyConfig
{
public:
    virtual ~DobbyConfig() = default;

    /**
     *  @brief Network type used for Network plugin
     */
    enum class NetworkType { None, Nat, Open };

    /**
     *  @brief Loopmount struct used for Storage plugin
     */
    typedef struct _LoopMount
    {
        std::string fsImagePath;
        std::string fsImageType;
        std::string destination;
        std::list<std::string> mountOptions;
        unsigned long mountFlags;
    } LoopMount;


// virtual methods to be overridden in derived classes
public:
    /**
     *  @brief Getters used for plugins
     */
    virtual bool isValid() const = 0;
    virtual uid_t userId() const = 0;
    virtual gid_t groupId() const = 0;
    virtual bool gpuEnabled() const = 0;
    virtual size_t gpuMemLimit() const = 0;
    virtual IDobbyIPCUtils::BusType systemDbus() const = 0;
    virtual IDobbyIPCUtils::BusType sessionDbus() const = 0;
    virtual IDobbyIPCUtils::BusType debugDbus() const = 0;
    virtual bool consoleDisabled() const = 0;
    virtual ssize_t consoleLimit() const = 0;
    virtual const std::string& consolePath() const = 0;
    virtual bool restartOnCrash() const = 0;
    virtual const std::string& rootfsPath() const = 0;
    virtual std::shared_ptr<rt_dobby_schema> config() const = 0;
    virtual const std::map<std::string, Json::Value>& legacyPlugins() const = 0;
    virtual const std::map<std::string, Json::Value>& rdkPlugins() const = 0;
    virtual const std::list<std::string> sysHooks() const = 0;

    /**
     *  @brief Get Dobby spec, defaults to empty
     */
    virtual const std::string spec() const
    { return std::string(); }

    /**
     *  @brief DEPRECATED - Rt priority used in Dobby specs
     */
    virtual int rtPriorityDefault() const
    { return 0; }


// non-virtual methods for default use
public:
    bool addMount(const std::string& source,
                  const std::string& target,
                  const std::string& fsType,
                  unsigned long mountFlags,
                  const std::list<std::string>& mountOptions);
    bool addEnvironmentVar(const std::string& envVar);
    bool writeConfigJson(const std::string& filePath) const;

    const std::string configJson() const;


// protected methods for derived classes to use
protected:
    bool writeConfigJsonImpl(const std::string& filePath) const;
    bool updateBundleConfig(const ContainerId& id,
                            std::shared_ptr<rt_dobby_schema> cfg,
                            const std::string& bundlePath);
    bool convertToCompliant(const ContainerId& id,
                            std::shared_ptr<rt_dobby_schema> cfg,
                            const std::string& bundlePath);

    static void initGpuDevNodes(const std::list<std::string>& devNodes);

    static std::mutex mGpuDevNodesLock;
    static bool mInitialisedGpuDevNodes;
    static std::string mGpuDevNodes;
    static std::string mGpuDevNodesPerms;

    static const std::map<std::string, std::list<std::string>> mRdkPluginsInDevelopment;

    mutable std::mutex mLock;

private:
    void addPluginLauncherHooks(std::shared_ptr<rt_dobby_schema> cfg, const std::string& bundlePath);
    void setPluginHookEntry(rt_defs_hook* entry, const std::string& name, const std::string& configPath);
    bool findPluginLauncherHookEntry(rt_defs_hook** hook, int len);
    bool findRdkPlugins(rt_defs_plugins_rdk_plugins *rdkPlugins);
};


#endif // !defined(DOBBYCONFIG_H)
