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

#ifndef DOBBYUTILS_H
#define DOBBYUTILS_H

#include "IDobbyUtils.h"
#include <cstdint>
#include <string>
#include <list>
#include <chrono>
#include <functional>
#include <memory>
#include <map>
#include <string.h>

#include <sys/types.h>
#include <sys/sysmacros.h>
class DobbyTimer;

class DobbyUtilsImpl
{
public:

    virtual ~DobbyUtilsImpl() = default;
    virtual bool cancelTimer(int timerId) const = 0;
    virtual int loopDeviceAssociate(int fileFd, std::string* loopDevPath) const = 0;
    virtual bool checkExtImageFile(int dirFd, const std::string& imageFileName,bool repair) const =0;
    virtual bool formatExtImageFile(int dirFd, const std::string& imageFileName,const std::string& fsType) const =0;
    virtual bool mkdirRecursive(const std::string& path, mode_t mode) const =0;
    virtual bool mkdirRecursive(int dirFd, const std::string& path, mode_t mode) const =0;
    virtual bool rmdirRecursive(const std::string& path) const =0;
    virtual bool rmdirRecursive(int dirFd, const std::string& path) const =0;
    virtual bool rmdirContents(const std::string& path) const =0;
    virtual bool rmdirContents(int dirFd, const std::string& path) const =0;
    virtual bool rmdirContents(int dirFd) const =0;
    virtual void cleanMountLostAndFound(const std::string& mountPoint,const std::string& logTag) const =0;
    virtual int getNamespaceFd(pid_t pid, int nsType) const =0;
    virtual bool writeTextFileAt(int dirFd, const std::string& path,const std::string& str,int flags, mode_t mode) const =0;
    virtual bool writeTextFile(const std::string& path,const std::string& str,int flags, mode_t mode) const =0;
    virtual std::string readTextFile(const std::string& path,size_t maxLen) const =0;
    virtual std::string readTextFileAt(int dirFd, const std::string& path,size_t maxLen) const =0;
    virtual unsigned int getDriverMajorNumber(const std::string &driverName) const =0;
    virtual bool deviceAllowed(dev_t device) const =0;
    virtual void setIntegerMetaData(const ContainerId &id, const std::string &key,int value) =0;
    virtual int getIntegerMetaData(const ContainerId &id, const std::string &key,int defaultValue) const =0;
    virtual void setStringMetaData(const ContainerId &id, const std::string &key,const std::string &value) =0;
    virtual std::string getStringMetaData(const ContainerId &id, const std::string &key,const std::string &defaultValue) const =0;
    virtual void clearContainerMetaData(const ContainerId &id) =0;
    virtual bool insertEbtablesRule(const std::string &args) const =0;
    virtual bool deleteEbtablesRule(const std::string &args) const =0;
    virtual bool callInNamespaceImpl(pid_t pid, int nsType, const std::function<void()>& func) const = 0;
    virtual bool callInNamespaceImpl(int namespaceFd, const std::function<void()>& func) const = 0;
    virtual int startTimerImpl(const std::chrono::milliseconds& timeout,bool oneShot,const std::function<bool()>& handler) const = 0;
};

class DobbyUtils : public virtual IDobbyUtils_v3 {

protected:
    static DobbyUtilsImpl* impl;

public:

     DobbyUtils();
    ~DobbyUtils();

    static void setImpl(DobbyUtilsImpl* newImpl);
    bool cancelTimer(int timerId) const override;
    int loopDeviceAssociate(int fileFd, std::string* loopDevPath) const override;
    bool checkExtImageFile(int dirFd, const std::string& imageFileName,bool repair) const override;
    bool formatExtImageFile(int dirFd, const std::string& imageFileName,const std::string& fsType) const override;
    bool mkdirRecursive(const std::string& path, mode_t mode) const override;
    bool mkdirRecursive(int dirFd, const std::string& path, mode_t mode) const override;
    bool rmdirRecursive(const std::string& path) const override;
    bool rmdirRecursive(int dirFd, const std::string& path) const override;
    bool rmdirContents(const std::string& path) const override;
    bool rmdirContents(int dirFd, const std::string& path) const override;
    bool rmdirContents(int dirFd) const override;
    void cleanMountLostAndFound(const std::string& mountPoint,const std::string& logTag) const override;
    int getNamespaceFd(pid_t pid, int nsType) const override;
    bool writeTextFileAt(int dirFd, const std::string& path,const std::string& str,int flags, mode_t mode) const override;
    bool writeTextFile(const std::string& path,const std::string& str,int flags, mode_t mode) const override;
    std::string readTextFile(const std::string& path,size_t maxLen) const override;
    std::string readTextFileAt(int dirFd, const std::string& path,size_t maxLen) const override;
    unsigned int getDriverMajorNumber(const std::string &driverName) const override;
    bool deviceAllowed(dev_t device) const override;
    void setIntegerMetaData(const ContainerId &id, const std::string &key,int value) override;
    int getIntegerMetaData(const ContainerId &id, const std::string &key,int defaultValue) const override;
    void setStringMetaData(const ContainerId &id, const std::string &key,const std::string &value) override;
    std::string getStringMetaData(const ContainerId &id, const std::string &key,const std::string &defaultValue) const override;
    void clearContainerMetaData(const ContainerId &id) override;
    bool insertEbtablesRule(const std::string &args) const override;
    bool deleteEbtablesRule(const std::string &args) const override;
    bool callInNamespaceImpl(pid_t pid, int nsType, const std::function<void()>& func) const override;
    bool callInNamespaceImpl(int namespaceFd, const std::function<void()>& func) const override;
    int startTimerImpl(const std::chrono::milliseconds& timeout,bool oneShot,const std::function<bool()>& handler) const override;

};

#endif // !defined(DOBBYUTILS_H)
