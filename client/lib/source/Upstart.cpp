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
#include "Upstart.h"

#include <Logging.h>

#include <chrono>


Upstart::Upstart(const std::shared_ptr<AI_IPC::IIpcService>& ipcService)
    : mIpcService(ipcService)
    , mService("com.ubuntu.Upstart")
    , mInterface("com.ubuntu.Upstart0_6.Job")
{
}

// -----------------------------------------------------------------------------
/**
 *  @brief Requests a start or a restart of a service
 *
 *  Since the Start and Restart methods are near identical in the way they
 *  are called over dbus, this function performs either based on the method
 *  argument.
 *
 *
 *  @param[in]  method      The method, should be either 'Start', 'Restart' or
 *                          'Stop'.
 *  @param[in]  name        The name of the service to start.
 *  @param[in]  env         Any extra environment variables to pass to the
 *                          service being started
 *  @param[in]  wait        Waits till the service is running before returning.
 *
 *  @return true if the service was started, otherwise false.
 */
bool Upstart::invokeMethod(const std::string& method,
                           const std::string& name,
                           const std::vector<std::string>& env,
                           bool wait) const
{
    AI_LOG_FN_ENTRY();

    // sanity check we have an ipc service object and upstart is available
    if (!mIpcService || !mIpcService->isServiceAvailable(mService))
    {
        AI_LOG_ERROR_EXIT("either ipc service not available or no '%s' service",
                          mService.c_str());
        return false;
    }

    // construct the method to request the start
    const AI_IPC::Method ipcMethod(mService,
                                   "/com/ubuntu/Upstart/jobs/" + name,
                                   mInterface,
                                   method);

    // construct the args to send
    const AI_IPC::VariantList ipcArgs({ env, wait });

    // for debugging log the start time so can display a warning if the request
    // took more than a couple of seconds to be processed
    std::chrono::time_point<std::chrono::steady_clock> startTime =
                                            std::chrono::steady_clock::now();

    // fire off the request and wait for the reply, we set a heathy timeout
    // value of 60 seconds (for NGDEV-67175)
    AI_IPC::VariantList ipcReply;
    if (!mIpcService->invokeMethod(ipcMethod, ipcArgs, ipcReply, (60 * 1000)))
    {
        AI_LOG_ERROR_EXIT("failed to send ipc request");
        return false;
    }

    // calculate how many milliseconds that took
    std::chrono::milliseconds timeTaken =
        std::chrono::duration_cast<std::chrono::milliseconds>
            (std::chrono::steady_clock::now() - startTime);
    if (timeTaken > std::chrono::seconds(2))
    {
        AI_LOG_WARN("upstart request took a rather long time (%llums)",
                    timeTaken.count());
    }

    // if a 'Stop' request there is no response
    if (method != "Stop")
    {
        // the result is a dbus object path, our IPC code converts that to a
        // string, so just check we got a string and it's sensible
        std::string objectPath;
        if (!AI_IPC::parseVariantList<std::string>(ipcReply, &objectPath))
        {
            AI_LOG_ERROR_EXIT("invalid reply to ipc request");
            return false;
        }

        // the object path should be of the form '/com/ubuntu/Upstart/jobs/<name>/_'
        const std::string expectObjPath("/com/ubuntu/Upstart/jobs/" + name + "/_");
        if (objectPath != expectObjPath)
        {
            AI_LOG_ERROR_EXIT("invalid reply to ipc request, expected '%s', "
                              "actual '%s'", expectObjPath.c_str(),
                              objectPath.c_str());
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to start the service with the given name.
 *
 *  This issues the a Start command on the dbus interface for the service with
 *  the given name.  The equivalent on the command line would look like this:
 *
 *      dbus-send \
 *          --system \
 *          --print-reply \
 *          --dest=com.ubuntu.Upstart \
 *          /com/ubuntu/Upstart/jobs/<name> \
 *          com.ubuntu.Upstart0_6.Job.Start \
 *          array:string:<env> boolean:<wait>
 *
 *
 *  @param[in]  name        The name of the service to start.
 *  @param[in]  env         Any extra environment variables to pass to the
 *                          service being started
 *  @param[in]  wait        Waits till the service is running before returning.
 *
 *  @return true if the service was started, otherwise false.
 */
bool Upstart::start(const std::string& name,
                    const std::vector<std::string>& env /*= std::vector<std::string>()*/,
                    bool wait /*= true*/) const
{
    return invokeMethod("Start", name, env, wait);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to perform a stop and a start on the service with the given
 *  name.
 *
 *  This issues the a Restart command on the dbus interface for the service with
 *  the given name.  The equivalent on the command line would look like this:
 *
 *      dbus-send \
 *          --system \
 *          --print-reply \
 *          --dest=com.ubuntu.Upstart \
 *          /com/ubuntu/Upstart/jobs/<name> \
 *          com.ubuntu.Upstart0_6.Job.Restart \
 *          array:string:<env> boolean:<wait>
 *
 *  Note this function will return an error if the service wasn't running prior
 *  to the call.
 *
 *  @param[in]  name        The name of the service to restart.
 *  @param[in]  env         Any extra environment variables to pass to the
 *                          service being restarted
 *  @param[in]  wait        Waits till the service is running before returning.
 *
 *  @return true if the service was restarted, otherwise false.
 */
bool Upstart::restart(const std::string& name,
                      const std::vector<std::string>& env /*= std::vector<std::string>()*/,
                      bool wait /*= true*/) const
{
    return invokeMethod("Restart", name, env, wait);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to stop the service with the given name.
 *
 *  This issues the a Stop command on the dbus interface for the service with
 *  the given name.  The equivalent on the command line would look like this:
 *
 *      dbus-send \
 *          --system \
 *          --print-reply \
 *          --dest=com.ubuntu.Upstart \
 *          /com/ubuntu/Upstart/jobs/<name> \
 *          com.ubuntu.Upstart0_6.Job.Stop \
 *          array:string:'' boolean:<wait>
 *
 *
 *  @param[in]  name        The name of the service to stop.
 *  @param[in]  wait        Waits till the service is stopped before returning.
 *
 *  @return true if the service was stopped, otherwise false.
 */
bool Upstart::stop(const std::string& name,
                   bool wait /*= true*/) const
{
    return invokeMethod("Stop", name, std::vector<std::string>(), wait);
}

