/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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
 * File:   DobbyRdkPluginUtils.h
 *
 */
#ifndef DOBBYRDKPLUGINUTILS_H
#define DOBBYRDKPLUGINUTILS_H

#include "rt_dobby_schema.h"

#include <sys/types.h>
#include <string>
#include <fstream>
#include <functional>
#include <memory>
#include <list>
#include <mutex>

// -----------------------------------------------------------------------------
/**
 *  @class DobbyRdkPluginUtils
 *
 *  @brief Class for useful utility methods for plugins such as adding mounts
 *  and environment variables.
 */
class DobbyRdkPluginUtils
{
public:
    DobbyRdkPluginUtils();
    ~DobbyRdkPluginUtils();

    // -------------------------------------------------------------------------
    /**
     *  @brief Slightly nicer version of callInNamespace, handles the function
     *  bind for you automatically
     *
     *  You'd typically use this to perform options in the namespace of a
     *  container.  The @a pid argument would be the pid of the containered
     *  process.
     *
     *  The @a nsType argument should be one of the following values:
     *      CLONE_NEWIPC  - run in a IPC namespace
     *      CLONE_NEWNET  - run in a network namespace
     *      CLONE_NEWNS   - run in a mount namespace
     *      CLONE_NEWPID  - run in a PID namespace
     *      CLONE_NEWUSER - run in a user namespace
     *      CLONE_NEWUTS  - run in a UTS namespace
     *
     *  @param[in]  pid         The pid owner of the namespace to enter,
     *                          typically the pid of the process in the
     *                          container.
     *  @param[in]  nsType      The type of the namespace to enter, see above.
     *  @param[in]  func        The actual function to execute.
     *
     *  @return true on success, false on failure.
     */
    template< class Function, class... Args >
    inline bool callInNamespace(pid_t pid, int nsType, Function&& f, Args&&... args) const
    {
        return this->callInNamespaceImpl(pid, nsType, std::bind(std::forward<Function>(f),
                                                                std::forward<Args>(args)...));
    }

    bool callInNamespaceImpl(pid_t pid, int nsType,
                             const std::function<bool()>& func) const;

    void nsThread(int newNsFd, int nsType, bool* success,
                  std::function<bool()>& func) const;

    pid_t getContainerPid(const std::string &stdin) const;

    bool writeTextFile(const std::string &path,
                       const std::string &str,
                       int flags,
                       mode_t mode) const;

    std::string readTextFile(const std::string &path) const;

    bool addMount(const std::shared_ptr<rt_dobby_schema> &cfg,
                  const std::string &source,
                  const std::string &target,
                  const std::string &fsType,
                  const std::list<std::string> &mountOptions) const;

    static bool mkdirRecursive(const std::string& path, mode_t mode);

    bool addEnvironmentVar(const std::shared_ptr<rt_dobby_schema> &cfg,
                           const std::string& envVar) const;

    mutable std::mutex mLock;
};

#endif // !defined(DOBBYRDKPLUGINUTILS_H)
