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

#pragma once

#include <gmock/gmock.h>
#include "DobbyUtils.h"

class DobbyUtilsMock : public DobbyUtilsImpl{
public:
    virtual ~DobbyUtilsMock() = default;
    MOCK_METHOD(bool, cancelTimer, (int timerId), (const, override));
    MOCK_METHOD(int, loopDeviceAssociate, (int fileFd, std::string* loopDevPath), (const, override));
    MOCK_METHOD(bool, checkExtImageFile, (int dirFd, const std::string& imageFileName,bool repair), (const, override));
    MOCK_METHOD(bool, formatExtImageFile, (int dirFd, const std::string& imageFileName,const std::string& fsType), (const, override));
    MOCK_METHOD(bool, mkdirRecursive, (const std::string& path, mode_t mode), (const, override));
    MOCK_METHOD(bool, mkdirRecursive, (int dirFd, const std::string& path, mode_t mode), (const, override));
    MOCK_METHOD(bool, rmdirRecursive, (const std::string& path), (const, override));
    MOCK_METHOD(bool, rmdirRecursive, (int dirFd, const std::string& path), (const, override));
    MOCK_METHOD(bool, rmdirContents, (const std::string& path), (const, override));
    MOCK_METHOD(bool, rmdirContents, (int dirFd, const std::string& path), (const, override));
    MOCK_METHOD(bool, rmdirContents, (int dirFd), (const, override));
    MOCK_METHOD(void, cleanMountLostAndFound, (const std::string& mountPoint,const std::string& logTag), (const, override));
    MOCK_METHOD(int, getNamespaceFd, (pid_t pid, int nsType), (const, override));
    MOCK_METHOD(bool, writeTextFileAt, (int dirFd, const std::string& path,const std::string& str,int flags, mode_t mode), (const, override));
    MOCK_METHOD(bool, writeTextFile, (const std::string& path,const std::string& str,int flags, mode_t mode), (const, override));
    MOCK_METHOD(std::string, readTextFile, (const std::string& path,size_t maxLen), (const, override));
    MOCK_METHOD(std::string, readTextFileAt, (int dirFd, const std::string& path,size_t maxLen), (const, override));
    MOCK_METHOD(unsigned int, getDriverMajorNumber, (const std::string &driverName), (const, override));
    MOCK_METHOD(bool, deviceAllowed, (dev_t device), (const, override));
    MOCK_METHOD(void, setIntegerMetaData, (const ContainerId &id, const std::string &key,int value), (override));
    MOCK_METHOD(int, getIntegerMetaData, (const ContainerId &id, const std::string &key,int defaultValue), (const, override));
    MOCK_METHOD(void, setStringMetaData, (const ContainerId &id, const std::string &key,const std::string &value), (override));
    MOCK_METHOD(std::string, getStringMetaData, (const ContainerId &id, const std::string &key,const std::string &defaultValue), (const, override));
    MOCK_METHOD(void, clearContainerMetaData, (const ContainerId &id), (override));
    MOCK_METHOD(bool, insertEbtablesRule, (const std::string &args), (const, override));
    MOCK_METHOD(bool, deleteEbtablesRule, (const std::string &args), (const, override));
    MOCK_METHOD(bool, callInNamespaceImpl, (pid_t pid, int nsType, const std::function<void()>& func), (const, override));
    MOCK_METHOD(bool, callInNamespaceImpl, (int namespaceFd, const std::function<void()>& func), (const, override));
    MOCK_METHOD(int, startTimerImpl, (const std::chrono::milliseconds& timeout,bool oneShot,const std::function<bool()>& handler), (const, override));

};
