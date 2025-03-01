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

#include "DobbyRdkPluginUtilsMock.h"

DobbyRdkPluginUtils::DobbyRdkPluginUtils()
{
}

DobbyRdkPluginUtils::DobbyRdkPluginUtils(const std::shared_ptr<rt_dobby_schema> &cfg,
                    const std::string &containerId)
{
}

DobbyRdkPluginUtils::DobbyRdkPluginUtils(const std::shared_ptr<rt_dobby_schema> &cfg,
                    const std::shared_ptr<IDobbyStartState> &startState,
                    const std::string &containerId)
{
}

DobbyRdkPluginUtils::DobbyRdkPluginUtils(const std::shared_ptr<rt_dobby_schema> &cfg,
                    const std::shared_ptr<const rt_state_schema> &state,
                    const std::string &containerId)
{
}

DobbyRdkPluginUtils::DobbyRdkPluginUtils(const std::shared_ptr<rt_dobby_schema> &cfg,
                    const std::shared_ptr<const rt_state_schema> &state,
                    const std::shared_ptr<IDobbyStartState> &startState,
                    const std::string &containerId)
{
}

DobbyRdkPluginUtils::~DobbyRdkPluginUtils()
{
}

void DobbyRdkPluginUtils::setImpl(DobbyRdkPluginUtilsImpl* newImpl)
{
    // Handles both resetting 'impl' to nullptr and assigning a new value to 'impl'
    EXPECT_TRUE ((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

bool DobbyRdkPluginUtils::callInNamespaceImpl(pid_t pid, int nsType,const std::function<bool()>& func) const
{
   EXPECT_NE(impl, nullptr);

    return impl->callInNamespaceImpl(pid,nsType,func);
}

void DobbyRdkPluginUtils::nsThread(int newNsFd, int nsType, bool* success,std::function<bool()>& func) const
{
   EXPECT_NE(impl, nullptr);

    return impl->nsThread(newNsFd,nsType,success,func);
}

pid_t DobbyRdkPluginUtils::getContainerPid() const
{
   EXPECT_NE(impl, nullptr);

    return impl->getContainerPid();
}

std::string DobbyRdkPluginUtils::getContainerId() const
{
   EXPECT_NE(impl, nullptr);

    return impl->getContainerId();
}

bool DobbyRdkPluginUtils::getContainerNetworkInfo(ContainerNetworkInfo &networkInfo)
{
   EXPECT_NE(impl, nullptr);

    return impl->getContainerNetworkInfo(networkInfo);
}

bool DobbyRdkPluginUtils::getTakenVeths(std::vector<std::string> &takenVeths)
{
   EXPECT_NE(impl, nullptr);

    return impl->getTakenVeths(takenVeths);
}

bool DobbyRdkPluginUtils::writeTextFile(const std::string &path,const std::string &str,int flags,mode_t mode) const
{
   EXPECT_NE(impl, nullptr);

    return impl->writeTextFile(path,str,flags,mode);
}

std::string DobbyRdkPluginUtils::readTextFile(const std::string &path) const
{
   EXPECT_NE(impl, nullptr);

    return impl->readTextFile(path);
}

bool DobbyRdkPluginUtils::addMount(const std::string &source,const std::string &target,const std::string &fsType,const std::list<std::string> &mountOptions) const
{
   EXPECT_NE(impl, nullptr);

    return impl->addMount(source,target,fsType,mountOptions);
}

bool DobbyRdkPluginUtils::mkdirRecursive(const std::string& path, mode_t mode)
{
   EXPECT_NE(impl, nullptr);

    return impl->mkdirRecursive(path,mode);
}

bool DobbyRdkPluginUtils::addEnvironmentVar(const std::string& envVar) const
{
   EXPECT_NE(impl, nullptr);

    return impl->addEnvironmentVar(envVar);
}

int DobbyRdkPluginUtils::addFileDescriptor(const std::string& pluginName, int fd)
{
   EXPECT_NE(impl, nullptr);

    return impl->addFileDescriptor(pluginName,fd);
}

std::list<int> DobbyRdkPluginUtils::files()
{
   EXPECT_NE(impl, nullptr);

    return impl->files();
}

std::list<int> DobbyRdkPluginUtils::files(const std::string& pluginName)
{
   EXPECT_NE(impl, nullptr);

    return impl->files(pluginName);
}

bool DobbyRdkPluginUtils::addAnnotation(const std::string& key, const std::string& value)
{
   EXPECT_NE(impl, nullptr);

    return impl->addAnnotation(key, value);
}

bool DobbyRdkPluginUtils::removeAnnotation(const std::string& key)
{
   EXPECT_NE(impl, nullptr);

    return impl->removeAnnotation(key);
}

std::map<std::string, std::string> DobbyRdkPluginUtils::getAnnotations() const
{
   EXPECT_NE(impl, nullptr);

    return impl->getAnnotations();
}