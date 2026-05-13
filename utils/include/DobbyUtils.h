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
 * File:   DobbyUtils.h
 *
 */
#ifndef DOBBYUTILS_H
#define DOBBYUTILS_H

#include "IDobbyUtils.h"

#include <map>
#include <mutex>
#include <functional>
#include <set>

class DobbyTimer;

// -----------------------------------------------------------------------------
/**
 *  @class DobbyUtils
 *  @brief Utility methods for hooks and the general containiser daemon
 *  @see IDobbyUtils
 *
 */
class DobbyUtils : public virtual IDobbyUtils
{
public:
    DobbyUtils();
    ~DobbyUtils() final;

public:
    int loopDeviceAssociate(int fileFd, std::string* loopDevPath) const override;

private:
    int openLoopDevice(std::string* loopDevice) const;
    bool attachFileToLoopDevice(int loopFd, int fileFd) const;

public:
    bool checkExtImageFile(int dirFd, const std::string& imageFileName,
                           bool repair) const override;
    bool formatExtImageFile(int dirFd, const std::string& imageFileName,
                            const std::string& fsType) const override;

private:
    int runE2fsTool(int dirFd, std::list<std::string>* consoleOutput,
                    const char* e2fsTool, ...) const;

public:
    bool mkdirRecursive(const std::string& path, mode_t mode) const override;
    bool mkdirRecursive(int dirFd, const std::string& path, mode_t mode) const override;

    bool rmdirRecursive(const std::string& path) const override;
    bool rmdirRecursive(int dirFd, const std::string& path) const override;

    bool rmdirContents(const std::string& path) const override;
    bool rmdirContents(int dirFd, const std::string& path) const override;
    bool rmdirContents(int dirFd) const override;

    void cleanMountLostAndFound(const std::string& mountPoint,
                                const std::string& logTag) const override;

private:
    static bool deleteRecursive(int dirfd, int depth);

public:
    int getNamespaceFd(pid_t pid, int nsType) const override;

private:
    bool callInNamespaceImpl(pid_t pid, int nsType,
                             const std::function<bool()>& func) const override;

    bool callInNamespaceImpl(int namespaceFd,
                             const std::function<bool()>& func) const override;

    void nsThread(int newNsFd, int nsType, bool* success,
                  std::function<bool()>& func) const;

public:
    bool writeTextFileAt(int dirFd, const std::string& path,
                         const std::string& str,
                         int flags, mode_t mode) const override;
    bool writeTextFile(const std::string& path,
                       const std::string& str,
                       int flags, mode_t mode) const override;

    std::string readTextFile(const std::string& path,
                             size_t maxLen) const override;
    std::string readTextFileAt(int dirFd, const std::string& path,
                               size_t maxLen) const override;


public:
    bool cancelTimer(int timerId) const override;

private:
    int startTimerImpl(const std::chrono::milliseconds& timeout,
                       bool oneShot,
                       const std::function<bool()>& handler) const override;

    std::shared_ptr<DobbyTimer> mTimerQueue;

public:
    unsigned int getDriverMajorNumber(const std::string &driverName) const override;

    bool deviceAllowed(dev_t device) const override;

private:
    void buildDeviceWhitelist();

    std::set<dev_t> mDeviceWhitelist;

    mutable std::mutex mMajorNumberLock;
    mutable std::map<std::string, unsigned int> mMajorNumberCache;

public:
    void setIntegerMetaData(const ContainerId &id, const std::string &key,
                            int value) override;
    int getIntegerMetaData(const ContainerId &id, const std::string &key,
                           int defaultValue) const override;

    void setStringMetaData(const ContainerId &id, const std::string &key,
                           const std::string &value) override;
    std::string getStringMetaData(const ContainerId &id, const std::string &key,
                                  const std::string &defaultValue) const override;

    void clearContainerMetaData(const ContainerId &id) override;

public:
    bool insertEbtablesRule(const std::string &args) const override;
    bool deleteEbtablesRule(const std::string &args) const override;

private:
    bool executeCommand(const std::string &command) const;
    int  getGIDorUID(pid_t pid, const std::string& idType) const;

public:
    uid_t getUID(pid_t pid) const override;
    gid_t getGID(pid_t pid) const override;

private:
    mutable std::mutex mMetaDataLock;
    std::map<std::pair<ContainerId, std::string>, int> mIntegerMetaData;
    std::map<std::pair<ContainerId, std::string>, std::string> mStringMetaData;

};


#endif // !defined(DOBBYUTILS_H)
