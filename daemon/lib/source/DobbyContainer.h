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
 * File:   DobbyContainer.h
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef DOBBYCONTAINER_H
#define DOBBYCONTAINER_H

#include <sys/types.h>

#include <cstdint>
#include <memory>
#include <chrono>
#include <bitset>
#include <mutex>
#include <list>


class DobbyBundle;
class DobbyConfig;
class DobbyRootfs;
class IDobbyStream;
class DobbyRdkPluginManager;

// -----------------------------------------------------------------------------
/**
 *  @class DobbyContainer
 *  @brief Wrapper object used to store container resources
 *
 *  This class stores the state of the container, along with it's config, bundle
 *  directory, rootfs and console resources.
 *
 *  In addition it stores the pid of the controller runc process managing the
 *  container, we need this to detect when the container has finially shutdown.
 *
 *  Lastly we also store a unique descriptor for the container, this is used
 *  alongside the container id string to identify events from a container.
 *
 *  @see DobbyManager
 */
class DobbyContainer
{
public:
    DobbyContainer() = delete;
    DobbyContainer(DobbyContainer&) = delete;
    DobbyContainer(DobbyContainer&&) = delete;

private:
    friend class DobbyManager;
    DobbyContainer(const std::shared_ptr<const DobbyBundle>& _bundle,
                   const std::shared_ptr<const DobbyConfig>& _config,
                   const std::shared_ptr<const DobbyRootfs>& _rootfs);

    DobbyContainer(const std::shared_ptr<const DobbyBundle>& _bundle,
                   const std::shared_ptr<const DobbyConfig>& _config,
                   const std::shared_ptr<const DobbyRootfs>& _rootfs,
                   const std::shared_ptr<const DobbyRdkPluginManager>& _rdkPluginManager);

public:
    ~DobbyContainer();

public:
    const int32_t descriptor;
    const std::shared_ptr<const DobbyBundle> bundle;
    const std::shared_ptr<const DobbyConfig> config;
    const std::shared_ptr<const DobbyRootfs> rootfs;
    const std::shared_ptr<const DobbyRdkPluginManager> rdkPluginManager;

public:
    pid_t containerPid;
    bool hasCurseOfDeath;
    enum class State { Starting, Running, Stopping, Paused } state;
    std::string customConfigFilePath;

public:
    void setRestartOnCrash(const std::list<int>& files);
    void clearRestartOnCrash();

    bool shouldRestart(int statusCode);
    const std::list<int>& files() const;

private:
    bool mRestartOnCrash;
    std::list<int> mFiles;

    unsigned mRestartCount;
    std::chrono::time_point<std::chrono::steady_clock> mLastRestartAttempt;

private:
    static std::mutex mIdsLock;
    static std::bitset<1024> mUsedIds;

    static int32_t allocDescriptor();
    static void freeDescriptor(int32_t cd);

};


#endif // DOBBYCONTAINER_H
