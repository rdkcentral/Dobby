/* If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#include "DobbyUtilsMock.h"

DobbyUtils::DobbyUtils()
{
}

DobbyUtils::~DobbyUtils()
{
}

void DobbyUtils::setImpl(DobbyUtilsImpl* newImpl)
{
    // Handles both resetting 'impl' to nullptr and assigning a new value to 'impl'
    EXPECT_TRUE ((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

bool DobbyUtils::cancelTimer(int timerId) const
{
   EXPECT_NE(impl, nullptr);

    return impl->cancelTimer(timerId);
}

int DobbyUtils::loopDeviceAssociate(int fileFd, std::string* loopDevPath) const
{
   EXPECT_NE(impl, nullptr);

    return impl->loopDeviceAssociate(fileFd,loopDevPath);
}

bool DobbyUtils::checkExtImageFile(int dirFd, const std::string& imageFileName,bool repair) const
{
   EXPECT_NE(impl, nullptr);

    return impl->checkExtImageFile(dirFd,imageFileName,repair);
}

bool DobbyUtils::formatExtImageFile(int dirFd, const std::string& imageFileName,const std::string& fsType) const
{
   EXPECT_NE(impl, nullptr);

    return impl->formatExtImageFile(dirFd,imageFileName,fsType);
}

bool DobbyUtils::mkdirRecursive(const std::string& path, mode_t mode) const
{
   EXPECT_NE(impl, nullptr);

    return impl->mkdirRecursive(path,mode);
}

bool DobbyUtils::mkdirRecursive(int dirFd, const std::string& path, mode_t mode) const
{
   EXPECT_NE(impl, nullptr);

    return impl->mkdirRecursive(dirFd,path,mode);
}

bool DobbyUtils::rmdirRecursive(const std::string& path) const
{
   EXPECT_NE(impl, nullptr);

    return impl->rmdirRecursive(path);
}

bool DobbyUtils::rmdirRecursive(int dirFd, const std::string& path) const
{
   EXPECT_NE(impl, nullptr);

    return impl->rmdirRecursive(dirFd,path);
}

bool DobbyUtils::rmdirContents(const std::string& path) const
{
   EXPECT_NE(impl, nullptr);

    return impl->rmdirContents(path);
}

bool DobbyUtils::rmdirContents(int dirFd, const std::string& path) const
{
   EXPECT_NE(impl, nullptr);

    return impl->rmdirContents(dirFd,path);
}

bool DobbyUtils::rmdirContents(int dirFd) const
{
   EXPECT_NE(impl, nullptr);

    return impl->rmdirContents(dirFd);
}

void DobbyUtils::cleanMountLostAndFound(const std::string& mountPoint,const std::string& logTag) const
{
   EXPECT_NE(impl, nullptr);

    return impl->cleanMountLostAndFound(mountPoint,logTag);
}

int DobbyUtils::getNamespaceFd(pid_t pid, int nsType) const
{
   EXPECT_NE(impl, nullptr);

    return impl->getNamespaceFd(pid,nsType);
}

bool DobbyUtils::writeTextFileAt(int dirFd, const std::string& path,const std::string& str,int flags, mode_t mode) const
{
   EXPECT_NE(impl, nullptr);

    return impl->writeTextFileAt(dirFd,path,str,flags,mode);
}

bool DobbyUtils::writeTextFile(const std::string& path,const std::string& str,int flags, mode_t mode) const
{
   EXPECT_NE(impl, nullptr);

    return impl->writeTextFile(path,str,flags,mode);
}

std::string DobbyUtils::readTextFile(const std::string& path,size_t maxLen) const
{
   EXPECT_NE(impl, nullptr);

    return impl->readTextFile(path,maxLen);
}

std::string DobbyUtils::readTextFileAt(int dirFd, const std::string& path,size_t maxLen) const
{
   EXPECT_NE(impl, nullptr);

    return impl->readTextFileAt(dirFd,path,maxLen);
}

unsigned int DobbyUtils::getDriverMajorNumber(const std::string &driverName) const
{
   EXPECT_NE(impl, nullptr);

    return impl->getDriverMajorNumber(driverName);
}

bool DobbyUtils::deviceAllowed(dev_t device) const
{
   EXPECT_NE(impl, nullptr);

    return impl->deviceAllowed(device);
}

void DobbyUtils::setIntegerMetaData(const ContainerId &id, const std::string &key,int value)
{
   EXPECT_NE(impl, nullptr);

    return impl->setIntegerMetaData(id,key,value);
}

int DobbyUtils::getIntegerMetaData(const ContainerId &id, const std::string &key,int defaultValue) const
{
   EXPECT_NE(impl, nullptr);

    return impl->getIntegerMetaData(id,key,defaultValue);
}

void DobbyUtils::setStringMetaData(const ContainerId &id, const std::string &key,const std::string &value)
{
   EXPECT_NE(impl, nullptr);

    return impl->setStringMetaData(id,key,value);
}

std::string DobbyUtils::getStringMetaData(const ContainerId &id, const std::string &key,const std::string &defaultValue) const
{
   EXPECT_NE(impl, nullptr);

    return impl->getStringMetaData(id,key,defaultValue);
}

void DobbyUtils::clearContainerMetaData(const ContainerId &id)
{
   EXPECT_NE(impl, nullptr);

    return impl->clearContainerMetaData(id);
}

bool DobbyUtils::insertEbtablesRule(const std::string &args) const
{
   EXPECT_NE(impl, nullptr);

    return impl->insertEbtablesRule(args);
}

bool DobbyUtils::deleteEbtablesRule(const std::string &args) const
{
   EXPECT_NE(impl, nullptr);

    return impl->deleteEbtablesRule(args);
}

bool DobbyUtils::callInNamespaceImpl(pid_t pid, int nsType, const std::function<void()>& func) const
{
   EXPECT_NE(impl, nullptr);

    return impl->callInNamespaceImpl(pid,nsType,func);
}

bool DobbyUtils::callInNamespaceImpl(int namespaceFd, const std::function<void()>& func) const
{
   EXPECT_NE(impl, nullptr);

    return impl->callInNamespaceImpl(namespaceFd,func);
}

int DobbyUtils::startTimerImpl(const std::chrono::milliseconds& timeout,bool oneShot,const std::function<bool()>& handler) const
{
   EXPECT_NE(impl, nullptr);

    return impl->startTimerImpl(timeout,oneShot,handler);
}


