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

#pragma once

#include <gmock/gmock.h>
#include "DobbyRdkPluginUtils.h"

class DobbyRdkPluginUtilsMock : public DobbyRdkPluginUtilsImpl {

public:

    virtual ~DobbyRdkPluginUtilsMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(bool, callInNamespaceImpl, (pid_t pid, int nsType,const std::function<bool()>& func), (const,override));
    MOCK_METHOD(void, nsThread, (int newNsFd, int nsType, bool* success,std::function<bool()>& func), (const,override));
    MOCK_METHOD(pid_t, getContainerPid, (), (const,override));
    MOCK_METHOD(std::string, getContainerId, (), (const,override));
    MOCK_METHOD(bool, getContainerNetworkInfo, (ContainerNetworkInfo &networkInfo), (override));
    MOCK_METHOD(bool, getTakenVeths, (std::vector<std::string> &takenVeths), (override));
    MOCK_METHOD(bool, writeTextFile, (const std::string &path,const std::string &str,int flags,mode_t mode), (const,override));
    MOCK_METHOD(std::string, readTextFile, (const std::string &path), (const,override));
    MOCK_METHOD(bool, addMount, (const std::string &source,const std::string &target,const std::string &fsType,const std::list<std::string> &mountOptions), (const,override));
    MOCK_METHOD(bool, mkdirRecursive, (const std::string& path, mode_t mode), (override));
    MOCK_METHOD(bool, addEnvironmentVar, (const std::string& envVar), (const,override));
    MOCK_METHOD(int, addFileDescriptor, (const std::string& pluginName, int fd), (override));
    MOCK_METHOD(std::list<int>, files, (), (const,override));
    MOCK_METHOD(std::list<int>, files, (const std::string& pluginName), (const,override));


};

