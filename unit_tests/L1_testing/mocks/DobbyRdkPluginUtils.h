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

#ifndef DOBBYRDKPLUGINUTILS_H
#define DOBBYRDKPLUGINUTILS_H

#include <sys/types.h>
#include <string>
#include <fstream>
#include <functional>
#include <memory>
#include <list>
#include <mutex>
#include <arpa/inet.h>
#include <vector>

#include "rt_dobby_schema.h"
#include "rt_state_schema.h"


#if defined (DOBBY_BUILD)
    #include "IDobbyStartState.h"
#else
    #include <Dobby/rdkPlugins/IDobbyStartState.h>
#endif

#define ADDRESS_FILE_DIR          "/tmp/dobby/plugin/networking/"

typedef struct ContainerNetworkInfo
{
    std::string vethName;
    std::string ipAddress;
    in_addr_t ipAddressRaw;
    std::string containerId;

    bool operator==(const ContainerNetworkInfo &rhs) const
    {
        if (containerId.empty() || rhs.containerId.empty())
        {
            return ipAddressRaw == rhs.ipAddressRaw;
        }
        return containerId == rhs.containerId;
    }
} ContainerNetworkInfo;

class DobbyRdkPluginUtilsImpl {
public:

    virtual bool callInNamespaceImpl(pid_t pid, int nsType,const std::function<bool()>& func) const =0;
    virtual void nsThread(int newNsFd, int nsType, bool* success,std::function<bool()>& func) const = 0;
    virtual pid_t getContainerPid() const = 0;
    virtual std::string getContainerId() const = 0;
    virtual bool getContainerNetworkInfo(ContainerNetworkInfo &networkInfo) = 0;
    virtual bool getTakenVeths(std::vector<std::string> &takenVeths) = 0;
    virtual bool writeTextFile(const std::string &path,const std::string &str,int flags,mode_t mode) const = 0;
    virtual std::string readTextFile(const std::string &path) const = 0;
    virtual bool addMount(const std::string &source,const std::string &target,const std::string &fsType,const std::list<std::string> &mountOptions) const = 0;
    virtual bool mkdirRecursive(const std::string& path, mode_t mode) = 0;
    virtual bool addEnvironmentVar(const std::string& envVar) const = 0 ;
    virtual int addFileDescriptor(const std::string& pluginName, int fd) = 0;
    virtual std::list<int> files() const = 0;
    virtual std::list<int> files(const std::string& pluginName) const = 0;
};

class DobbyRdkPluginUtils {

protected:
    static DobbyRdkPluginUtilsImpl* impl;

public:

    DobbyRdkPluginUtils();

    DobbyRdkPluginUtils(const std::shared_ptr<rt_dobby_schema> &cfg,
                        const std::string &containerId);
    DobbyRdkPluginUtils(const std::shared_ptr<rt_dobby_schema> &cfg,
                        const std::shared_ptr<IDobbyStartState> &startState,
                        const std::string &containerId);
    DobbyRdkPluginUtils(const std::shared_ptr<rt_dobby_schema> &cfg,
                        const std::shared_ptr<const rt_state_schema> &state,
                        const std::string &containerId);
    DobbyRdkPluginUtils(const std::shared_ptr<rt_dobby_schema> &cfg,
                        const std::shared_ptr<const rt_state_schema> &state,
                        const std::shared_ptr<IDobbyStartState> &startState,
                        const std::string &containerId);

    ~DobbyRdkPluginUtils();

    static void setImpl(DobbyRdkPluginUtilsImpl* newImpl);
    static DobbyRdkPluginUtils* getInstance();
    static bool callInNamespaceImpl(pid_t pid, int nsType,const std::function<bool()>& func);
    static void nsThread(int newNsFd, int nsType, bool* success,std::function<bool()>& func);
    static pid_t getContainerPid();
    static std::string getContainerId();
    static bool getContainerNetworkInfo(ContainerNetworkInfo &networkInfo);
    static bool getTakenVeths(std::vector<std::string> &takenVeths);
    static bool writeTextFile(const std::string &path,const std::string &str,int flags,mode_t mode);
    static std::string readTextFile(const std::string &path);
    static bool addMount(const std::string &source,const std::string &target,const std::string &fsType,const std::list<std::string> &mountOptions);
    static bool mkdirRecursive(const std::string& path, mode_t mode);
    static bool addEnvironmentVar(const std::string& envVar);
    static int addFileDescriptor(const std::string& pluginName, int fd);
    static std::list<int> files();
    static std::list<int> files(const std::string& pluginName);
};


#endif // !defined(DOBBYRDKPLUGINUTILS_H)











