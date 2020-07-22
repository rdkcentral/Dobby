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
 * File:   Upstart.h
 *
 */
#ifndef UPSTART_H
#define UPSTART_H

#include <IIpcService.h>

#include <memory>
#include <string>
#include <vector>

// -----------------------------------------------------------------------------
/**
 *  @class Upstart
 *  @brief Wrapper for the upstart-dbus-bridge interface that allows starting
 *  and stopping system services.
 *
 *  Upstart is the init process of the system, it is responsible for starting
 *  things like mesh, the system dbus and others.  It is an open source project
 *  run by ubuntu; http://upstart.ubuntu.com/.
 *
 *  This class is just a wrapper around it's dbus interface, the best
 *  documentation on the interface I found is here:
 *  https://github.com/bernd/ruby-upstart/blob/master/UPSTART-DBUS.md
 *
 *  @warning At the time of writing we are only allowed to start a single
 *  service ('skyDobbyDaemon'), this is due to the deliberately restrictive
 *  dbus policy as specified at the following location on the STB
 *
 *      /DBUS/etc/dbus-1/system.d/Upstart.conf
 *
 */
class Upstart
{
public:
    explicit Upstart(const std::shared_ptr<AI_IPC::IIpcService>& ipcService);
    ~Upstart() = default;

public:
    bool start(const std::string& name,
               const std::vector<std::string>& env = std::vector<std::string>(),
               bool wait = true) const;

    bool restart(const std::string& name,
                 const std::vector<std::string>& env = std::vector<std::string>(),
                 bool wait = true) const;

    bool stop(const std::string& name,
              bool wait = true) const;

private:
    bool invokeMethod(const std::string& method,
                      const std::string& name,
                      const std::vector<std::string>& env,
                      bool wait) const;

private:
    const std::shared_ptr<AI_IPC::IIpcService>& mIpcService;
    const std::string mService;
    const std::string mInterface;
};

#endif // !defined(UPSTART_H)
