/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
 * File:   DobbyRunc.cpp
 *
 */
#include "DobbyRunC.h"
#include "DobbyBundle.h"
#include "DobbyStream.h"
#include <Logging.h>
#include <Tracing.h>
#include <FileUtilities.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sstream>
#include <vector>
#include <json/json.h>


DobbyRunC::DobbyRunC(const std::shared_ptr<IDobbyUtils>& utils,
                     const std::shared_ptr<const IDobbySettings> &settings)
    : mUtilities(utils),
    mConsoleSocket(settings->consoleSocketPath())
#if defined(RDK)
    , mRuncPath("/usr/bin/crun")
#else
    , mRuncPath("/usr/sbin/runc")
#endif
    , mWorkingDir("/var/run/sky/crun")
    , mLogDir("/var/log")
    , mLogFilePath(mLogDir + "/crun.log")
{
    // sanity check
    if (access(mRuncPath.c_str(), X_OK) != 0)
    {
        AI_LOG_FATAL("failed to find runc tool @ '%s'", mRuncPath.c_str());
    }

    // NGDEV-84453: we can't rely on the /var/log or /var/run/runc directories
    // being present in the rootfs, to ensure we don't get any surprises create
    // them now
    utils->mkdirRecursive(mWorkingDir, 0775);
    utils->mkdirRecursive(mLogDir, 0775);
}

DobbyRunC::~DobbyRunC()
{
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the runc command line tool with the 'run' command
 *
 *  This is equivalent to calling the following on the command line
 *
 *      /usr/sbin/runc run --bundle <dir> <id>
 *
 *  The pid of the runc tool is returned, this will survive until the container
 *  is destroyed.  A watch should be placed on the pid to determine when
 *  it quits and this is the point the container is destroyed.
 *
 *  @param[in]  id      The id / name of the container to create
 *  @param[in]  bundle  The bundle directory to pass to the runc tool
 *  @param[in]  console The stream to attach to the stdout / stderr of the
 *                      runc tool.  This in effect is the console out of the
 *                      container.
 *  @param[in]  files   A list of file descriptors to pass into the container.
 *
 *  @return the pid of the runc process if successful, otherwise -1.
 */
pid_t DobbyRunC::run(const ContainerId& id,
                     const std::shared_ptr<const DobbyBundle>& bundle,
                     const std::shared_ptr<const IDobbyStream>& console,
                     const std::list<int>& files /*= std::list<int>()*/) const
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "runc::run");

    // run the following command "runc run --bundle <dir> <id>"
    pid_t pid = forkExecRunC({ "run", "--bundle", bundle->path().c_str(), id.c_str() },
                             { },
                             files,
                             console, console);
    if (pid <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to execute runc tool");
    }

    AI_LOG_FN_EXIT();
    return pid;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates the container but doesn't start the init process
 *
 *  This is equivalent to calling the following on the command line
 *      /usr/sbin/runc create --bundle <dir> <id>
 *
 *  The pid of the runc tool is returned, this will survive until the container
 *  is destroyed.  A watch should be placed on the pid to determine when
 *  it quits and this is the point the container is destroyed.
 *
 *  @param[in]  id      The id / name of the container to create
 *  @param[in]  bundle  The bundle directory to pass to the runc tool
 *  @param[in]  console The stream to attach to the stdout / stderr of the
 *                      runc tool.
 *  @param[in]  files   A list of file descriptors to pass into the container.
 *  @param[in]  customConfigPath   Path to a different config.json file to use
 *                                 instead of the one in the bundle
 *
 *  @return Pair of PIDs. First PID is the PID of the crun process, used to
 *  match the launched process to the logging connection. Second PID is the
 *  contents of the container pidfile and is the actual PID of the container
 */
std::pair<pid_t, pid_t> DobbyRunC::create(const ContainerId &id,
                                          const std::shared_ptr<const DobbyBundle> &bundle,
                                          const std::shared_ptr<const IDobbyStream> &console,
                                          const std::list<int> &files, /*= std::list<int>()*/
                                          const std::string& customConfigPath /*= ""*/) const
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "runc::create");

    // create a path to the pid file to write to
    const std::string pidFilePath(bundle->path() + "/container.pid");

    // get the number of file descriptors to pass into the container
    char preserveFds[32];
    sprintf(preserveFds, "%zu", files.size());

    std::vector<const char*> runtimeArgs =
    {
        "create",
        "--bundle", bundle->path().c_str(),
        "--console-socket", mConsoleSocket.c_str(),
        "--pid-file", pidFilePath.c_str(),
        #if defined(RDK)
        "--preserve-fds", preserveFds,
        #endif
    };

    if (!customConfigPath.empty())
    {
        runtimeArgs.push_back("--config");
        runtimeArgs.push_back(customConfigPath.c_str());
    }

    runtimeArgs.push_back(id.c_str());

    // run the following command "runc create --bundle <dir> <id>"
    pid_t pid = forkExecRunC(runtimeArgs,
                             { },
                             files,
                             console, console);
    if (pid <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to execute runc tool");
    }

    // block waiting for the forked process to complete
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return {-1,-1};
    }
    else if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("runc didn't exit?  status=0x%08x", status);
        return {-1,-1};
    }
    else if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        AI_LOG_ERROR_EXIT("create failed with status %d", WEXITSTATUS(status));
        return {-1,-1};
    }

    // now need to read the pid file it created so we know were to find the
    // container
    const size_t maxLength = 64;
    std::string pidFileContents = mUtilities->readTextFile(pidFilePath, maxLength);
    if (pidFileContents.empty())
    {
        AI_LOG_ERROR_EXIT("failed to read pid file contents");
        return {-1,-1};
    }

    char *endptr;
    pid_t containerPid = static_cast<pid_t>(strtol(pidFileContents.c_str(), &endptr, 0));
    if (endptr == pidFileContents.c_str())
    {
        AI_LOG_ERROR_EXIT("failed to to convert '%s' to a pid", pidFileContents.c_str());
        return {-1,-1};
    }

    AI_LOG_FN_EXIT();
    return std::make_pair(pid, containerPid);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Starts a container created with @a create command
 *
 *  This is equivalent to calling the following on the command line
 *      /usr/sbin/runc start --bundle <dir> <id>
 *
 *  @param[in]  id      The id of the container created earlier
 *  @param[in]  console The stream to attach to the stdout / stderr of the
 *                      runc tool.
 *
 *  @return true on success, false on failure.
 */
bool DobbyRunC::start(const ContainerId& id, const std::shared_ptr<const IDobbyStream> &console) const
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "runc::start");

    // run the following command "runc start <id>"
    pid_t pid = forkExecRunC({"start", id.c_str()},
                             {},
                             {},
                             console, console);
    if (pid <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to execute runc tool");
        return false;
    }

    // block waiting for the forked process to complete
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return false;
    }
    if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("runc didn't exit?  status=0x%08x", status);
        return false;
    }

    AI_LOG_FN_EXIT();

    // get the return code, 0 for success, 1 for failure
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the runc command line tool with the 'kill' command
 *
 *  This is equivalent to calling the following on the command line
 *      /usr/sbin/runc kill <id> <signal>
 *
 *
 *  We only support the following signals; SIGTERM, SIGKILL, SIGUSR1, SIGUSR2 &
 *  SIGUSR2.
 *
 *  @param[in]  id      The id / name of the container to create.
 *  @param[in]  signal  The signal number to send.
 *
 *  @return true or false based on the return code of the runc tool.
 */
bool DobbyRunC::kill(const ContainerId& id, int signal, bool all) const
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "runc::kill");

    // convert the signal to string
    std::string strSignal;
    switch (signal)
    {
        case SIGTERM:
            strSignal = "TERM";
            break;
        case SIGKILL:
            strSignal = "KILL";
            break;
        case SIGUSR1:
            strSignal = "USR1";
            break;
        case SIGUSR2:
            strSignal = "USR2";
            break;
        case SIGHUP:
            strSignal = "HUP";
            break;
        default:
            AI_LOG_ERROR_EXIT("signal %d not supported", signal);
            return false;
    }

    // run the following command "runc kill <id> KILL"
    pid_t pid = -1;
    if (all)
    {
        pid = forkExecRunC({"kill", "--all", id.c_str(), strSignal.c_str()}, {});
    }
    else
    {
        pid = forkExecRunC({"kill", id.c_str(), strSignal.c_str()}, {});
    }

    if (pid <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to execute runc tool");
        return false;
    }

    // block waiting for the forked process to complete
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return false;
    }
    if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("runc didn't exit?  status=0x%08x", status);
        return false;
    }


    // get the return code, 0 for success, 1 for failure
    bool returnValue = (WEXITSTATUS(status) == EXIT_SUCCESS);

    // Fix problem where SIGTERM was masked and containers never exited
    if(signal == SIGTERM)
    {
        int retryCounter = 10;

        // get current container status
        ContainerStatus contStatus = state(id);

        // Unknown (container deleted), or Stopped (continer stopped)
        // are both valid options after successfull kill
        while (contStatus != ContainerStatus::Unknown &&
               contStatus != ContainerStatus::Stopped &&
               retryCounter > 0)
        {
            retryCounter--;
            usleep(500);
            contStatus = state(id);
        }

        // Container wasn't killed
        if(retryCounter <= 0)
        {
            AI_LOG_DEBUG("SIGTERM kill wasn't kill container (probably masked), "
                        "retrying kill with SIGKILL");
            // retry kill with SIGKILL now, its result will be proper result now
            returnValue = DobbyRunC::kill(id, SIGKILL, all);
        }
    }

    AI_LOG_FN_EXIT();
    return returnValue;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the runc command line tool with the 'pause' command
 *
 *  This is equivalent to calling the following on the command line:
 *
 *      /usr/sbin/runc pause <id>
 *
 *  @param[in]  id      The id / name of the container to pause.
 *
 *  @return true or false based on the return code of the runc tool.
 */
bool DobbyRunC::pause(const ContainerId& id) const
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "runc::pause");

    // run the following command "runc pause <id>"
    pid_t pid = forkExecRunC( { "pause", id.c_str() }, { } );
    if (pid <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to execute runc tool");
        return false;
    }

    // block waiting for the forked process to complete
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return false;
    }
    if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("runc didn't exit?  status=0x%08x", status);
        return false;
    }

    AI_LOG_FN_EXIT();

    // get the return code, 0 for success, 1 for failure
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the runc command line tool with the 'resume' command
 *
 *  This is equivalent to calling the following on the command line
 *
 *      /usr/sbin/runc resume <id>
 *
 *  @param[in]  id      The id / name of the container to resume.
 *
 *  @return true or false based on the return code of the runc tool.
 */
bool DobbyRunC::resume(const ContainerId& id) const
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "runc::resume");

    // run the following command "runc pause <id>"
    pid_t pid = forkExecRunC( { "resume", id.c_str() }, { } );
    if (pid <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to execute runc tool");
        return false;
    }

    // block waiting for the forked process to complete
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return false;
    }
    if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("runc didn't exit?  status=0x%08x", status);
        return false;
    }

    AI_LOG_FN_EXIT();

    // get the return code, 0 for success, 1 for failure
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the crun command line tool with the 'exec' command
 *
 *  This is equivalent to calling the following on the command line
 *
 *      /usr/sbin/crun exec [options...] --detach <id> <command>
 *
 *  The exec is run with the --detach option enabled so we're not stuck waiting for
 *  the command to finish execution.
 *
 *  TODO:: Fix bug where Dobby does not correctly detect when container exits
 *  after running exec due to zombie process issue
 *
 *  @param[in]  id          The id / name of the container to execute the command in.
 *  @param[in]  options     The options to execute the command with.
 *  @param[in]  command     The command to execute.
 *
 *  @return Pair of PIDs. First PID is the PID of the crun process, used to
 *  match the launched process to the logging connection. Second PID is the
 *  PID of the newly launched process
 */
std::pair<pid_t, pid_t> DobbyRunC::exec(const ContainerId& id, const std::string& options, const std::string& command) const
{
    AI_LOG_FN_ENTRY();

    // Just save the PID somewhere temporary so we can read it
    std::string pidFilePath = "/tmp/exec" + id.str() + ".pid";

    std::vector<std::string> opts;
    std::vector<std::string> cmd;
    std::string tmp;

    std::vector<const char *> args =
    {
        "exec",
        "--detach",
        "--tty",
        "--console-socket", mConsoleSocket.c_str(),
        "--pid-file", pidFilePath.c_str()
    };


    std::stringstream ss_opts(options);
    // Insert space delimited options string into a vector
    while(getline(ss_opts, tmp, ' '))
    {
        opts.push_back(tmp);
    }

    // Insert strings from options vector into args for crun
    for (int i = 0; i < opts.size(); i++)
    {
        args.push_back(opts.at(i).c_str());
    }

    args.push_back(id.c_str());

    // Must launch processes with DobbyInit so signals are sent properly
    args.push_back("/usr/libexec/DobbyInit");

    // Insert space delimited command string into a vector
    std::stringstream ss_cmd(command);
    while(getline(ss_cmd, tmp, ' '))
    {
        cmd.push_back(tmp);
    }

    // Insert strings from command vector into args for crun
    for (int i = 0; i < cmd.size(); i++)
    {
        args.push_back(cmd.at(i).c_str());
    }

    // Run the following command "crun exec [options...] --detach <id> <command>"
    pid_t pid = forkExecRunC(args, {});

    if (pid <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to execute runc tool");
        return {-1,-1};
    }

    // block waiting for the forked process to complete
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return {-1,-1};
    }
    else if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("runc didn't exit?  status=0x%08x", status);
        return {-1,-1};
    }

    // Get the PID of the newly executed process
    const size_t maxLength = 64;
    std::string pidFileContents = mUtilities->readTextFile(pidFilePath, maxLength);
    if (pidFileContents.empty())
    {
        AI_LOG_ERROR_EXIT("failed to read pid file contents");
        return {-1,-1};
    }

    char *endptr;
    pid_t execPid = static_cast<pid_t>(strtol(pidFileContents.c_str(), &endptr, 0));
    if (endptr == pidFileContents.c_str())
    {
        AI_LOG_ERROR_EXIT("failed to to convert '%s' to a pid", pidFileContents.c_str());
        return {-1,-1};
    }

    // Now delete the pidfile as we don't need it again
    if (remove(pidFilePath.c_str()) < 0)
    {
        AI_LOG_SYS_WARN(errno,"Failed to delete exec pidfile");
    }

    AI_LOG_FN_EXIT();
    return std::make_pair(pid, execPid);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the runc command line tool with the 'delete' command
 *
 *  This is equivalent to calling the following on the command line
 *
 *      /usr/sbin/runc delete <id>
 *
 *  This command will delete any cruft that the runc tool has left behind,
 *  things like cgroups, log and or pid files, etc.
 *
 *  @param[in]  id      The id / name of the container to create.
 *
 *  @return true or false based on the return code of the runc tool.
 */
bool DobbyRunC::destroy(const ContainerId& id, const std::shared_ptr<const IDobbyStream>& console) const
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "runc::destroy");

    // run the following command "runc delete <id>"
    // Start by being nice and issuing a "normal" delete
    pid_t pid = forkExecRunC({ "delete", id.c_str() },
                             { }, {}, console, console);
    if (pid <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to execute runc tool");
        return false;
    }

    // block waiting for the forked process to complete
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return false;
    }
    if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("runc didn't exit?  status=0x%08x", status);
        return false;
    }

    // If we failed to delete the container, try again with --force
    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        AI_LOG_WARN("Container '%s' could not be deleted - force deleting", id.c_str());

        pid = forkExecRunC({ "delete", "-f", id.c_str() },
                            { }, {}, console, console);
        if (pid <= 0)
        {
            AI_LOG_ERROR_EXIT("failed to execute runc tool");
            return false;
        }

        // block waiting for the forked process to complete
        if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
            return false;
        }
        if (!WIFEXITED(status))
        {
            AI_LOG_ERROR_EXIT("runc didn't exit?  status=0x%08x", status);
            return false;
        }
    }

    std::string containerDir = mWorkingDir + "/" + id.str();

    // forcefully delete the container directory if the delete command is unable
    // to do it properly
    if (AICommon::exists(containerDir))
    {
        AI_LOG_ERROR("container directory not deleted - remove forcefully [%s]",
                     containerDir.c_str());

        AICommon::deleteDirectory(containerDir);
        if (AICommon::exists(containerDir))
        {
            AI_LOG_ERROR("container directory still exist - we may be unable to"
                         " launch app %s until next reboot", id.c_str());
        }
    }

    AI_LOG_FN_EXIT();

    // get the return code, 0 for success, 1 for failure
    return (WEXITSTATUS(status) == EXIT_SUCCESS);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets the container status from the json object.
 *
 *  @param[in]  state    Json object containing the container state.
 *
 *  @return the container state, or ContainerStatus::Unknown.
 */
DobbyRunC::ContainerStatus DobbyRunC::getContainerStatusFromJson(const Json::Value &state) const
{
    static const Json::StaticString statusLabel("status");

    const Json::Value &status = state[statusLabel];
    if (!status.isString())
    {
        AI_LOG_ERROR("runc state json doesn't contain a 'status' field");
        return ContainerStatus::Unknown;
    }

    const char *str = status.asCString();
    if (strcasecmp(str, "created") == 0)
        return ContainerStatus::Created;
    if (strcasecmp(str, "running") == 0)
        return ContainerStatus::Running;
    if (strcasecmp(str, "pausing") == 0)
        return ContainerStatus::Pausing;
    if (strcasecmp(str, "paused") == 0)
        return ContainerStatus::Paused;
    if (strcasecmp(str, "stopped") == 0)
        return ContainerStatus::Stopped;

    return ContainerStatus::Unknown;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the runc command line tool with the 'state' command
 *
 *  This is equivalent to calling the following on the command line
 *
 *      /usr/sbin/runc state <id>
 *
 *  The response is json formatted for easy parsing of the container data.
 *
 *  @param[in]  id      The id / name of the container to get the state for.
 *
 *  @return the container state, or ContainerStatus::Unknown if the container
 *  isn't running.
 */
DobbyRunC::ContainerStatus DobbyRunC::state(const ContainerId& id) const
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "runc::state");

    // buffer to store the output
    std::shared_ptr<DobbyBufferStream> bufferStream =
        std::make_shared<DobbyBufferStream>();

    // run the following command "runc delete <id>"
    pid_t pid = forkExecRunC({ "state", id.c_str() }, { }, { },
                             bufferStream);
    if (pid <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to execute runc tool");
        return ContainerStatus::Unknown;
    }

    // block waiting for the forked process to complete
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return ContainerStatus::Unknown;
    }
    if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("runc didn't exit?  status=0x%08x", status);
        return ContainerStatus::Unknown;
    }

    // check succeeded
    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        AI_LOG_WARN("runc state %s failed with status %u",
                    id.c_str(), WEXITSTATUS(status));
        AI_LOG_FN_EXIT();
        return ContainerStatus::Unknown;
    }


    // read the json string
    const std::vector<char> json = bufferStream->getBuffer();
    if (json.empty())
    {
        AI_LOG_WARN("failed to get any reply from 'runc state %s' call",
                    id.c_str());
        AI_LOG_FN_EXIT();
        return ContainerStatus::Unknown;
    }


    // parse it
    Json::CharReaderBuilder builder;
    builder["strictRoot"] = true;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    Json::Value root;
    std::string errors;
    if (!reader->parse(json.data(), json.data() + json.size(), &root, &errors))
    {
        AI_LOG_ERROR_EXIT("failed to parse json output from 'runc state %s' call - %s",
                          id.c_str(), errors.c_str());
        return ContainerStatus::Unknown;
    }

    // check we read an object
    if (!root.isObject())
    {
        AI_LOG_ERROR_EXIT("invalid json object type");
        return ContainerStatus::Unknown;
    }

    AI_LOG_FN_EXIT();

    return getContainerStatusFromJson(root);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Runs the runc command line tool with the 'list' command
 *
 *  This is equivalent to calling the following on the command line
 *
 *      /usr/sbin/runc list
 *
 *  We use the json formatted response for easy parsing of the container data.
 *
 *  @return A map of current container ids and their status.
 */
std::map<ContainerId, DobbyRunC::ContainerStatus> DobbyRunC::list() const
{
    AI_LOG_FN_ENTRY();

    AI_TRACE_EVENT("Dobby", "runc::list");

    // buffer to store the output
    std::shared_ptr<DobbyBufferStream> bufferStream =
        std::make_shared<DobbyBufferStream>();

    // run the following command "runc delete <id>"
    pid_t pid = forkExecRunC({ "list", "--format", "json" }, { }, { },
                             bufferStream);
    if (pid <= 0)
    {
        AI_LOG_ERROR_EXIT("failed to execute runc tool");
        return std::map<ContainerId, DobbyRunC::ContainerStatus>();
    }

    // block waiting for the forked process to complete
    int status;
    if (TEMP_FAILURE_RETRY(waitpid(pid, &status, 0)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "waitpid failed");
        return std::map<ContainerId, DobbyRunC::ContainerStatus>();
    }
    if (!WIFEXITED(status))
    {
        AI_LOG_ERROR_EXIT("runc didn't exit?  status=0x%08x", status);
        return std::map<ContainerId, DobbyRunC::ContainerStatus>();
    }

    // check succeeded
    if (WEXITSTATUS(status) != EXIT_SUCCESS)
    {
        AI_LOG_WARN("\"runc list\" failed with status %u", WEXITSTATUS(status));
        AI_LOG_FN_EXIT();
        return std::map<ContainerId, DobbyRunC::ContainerStatus>();
    }


    // read the json string
    const std::vector<char> json = bufferStream->getBuffer();
    if (json.empty())
    {
        AI_LOG_WARN("failed to get any reply from \"runc list\" call");
        AI_LOG_FN_EXIT();
        return std::map<ContainerId, DobbyRunC::ContainerStatus>();
    }


    // parse it
    Json::CharReaderBuilder builder;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    Json::Value root;
    std::string errors;
    if (!reader->parse(json.data(), json.data() + json.size(), &root, &errors))
    {
        AI_LOG_WARN("failed to parse json output from \"runc list\" call - %s",
                    errors.c_str());
        AI_LOG_FN_EXIT();
        return std::map<ContainerId, DobbyRunC::ContainerStatus>();
    }

    // a null json type is returned if no containers are running, this is not
    // an error
    if (root.isNull())
    {
        AI_LOG_FN_EXIT();
        return std::map<ContainerId, DobbyRunC::ContainerStatus>();
    }

    // if not null then check we got an array type
    if (!root.isArray())
    {
        AI_LOG_ERROR_EXIT("invalid json array type");
        return std::map<ContainerId, DobbyRunC::ContainerStatus>();
    }

    std::map<ContainerId, DobbyRunC::ContainerStatus> containers;

    // iterate through the containers
    for (const Json::Value &entry : root)
    {
        if (!entry.isObject())
        {
            AI_LOG_WARN("container list contains non json object value");
            continue;
        }

        const Json::Value &id = entry["id"];
        if (!id.isString())
        {
            AI_LOG_WARN("container list contains invalid object value");
            continue;
        }

        ContainerId id_ = ContainerId::create(id.asCString());
        if (!id_.isValid())
        {
            AI_LOG_WARN("container list contains invalid id value");
            continue;
        }

        containers[id_] = getContainerStatusFromJson(entry);
    }

    AI_LOG_FN_EXIT();

    return containers;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Performs a fork then exec of the runC binary with the supplied args
 *
 *  This function blocks until the runC process has finished, then it returns
 *  the integer result.
 *
 *  stdin is redirected to /dev/null before the exec. If a stdoutStream or
 *  stderrStream arguments are supplied then the stdout/stderr output will be
 *  written into the supplied streams, otherwise they'll also be redirected
 *  to /dev/null.
 *
 *
 */
pid_t DobbyRunC::forkExecRunC(const std::vector<const char*>& args,
                               const std::initializer_list<const char*>& envs,
                               const std::list<int>& files /*= std::list<int>() */,
                               const std::shared_ptr<const IDobbyStream>& stdoutStream /*= nullptr*/,
                               const std::shared_ptr<const IDobbyStream>& stderrStream /*= nullptr*/) const
{
    AI_LOG_FN_ENTRY();

    // sanity check the number of fds
    const size_t maxFiles = 128;
    if (files.size() > maxFiles)
    {
        AI_LOG_ERROR("too many file descriptors passed, limit of 128");
        return -1;
    }


    // setup the args and environment variables now as we can't safely use
    // malloc after the fork (because we're multi-threaded)
    // Arguments (the first args is always the executable name)
    std::vector<char*> argv;
    argv.reserve(args.size() + 6);
    argv.push_back(strdup("crun"));

    // Set the path to the root data, by default this is '/run/runc', we
    // move it to '/var/run/runc'
    argv.push_back(strdup("--root"));
    argv.push_back(strdup(mWorkingDir.c_str()));

    // On non-production builds store the runc log
#if (AI_BUILD_TYPE == AI_DEBUG)
    argv.push_back(strdup("--log"));
    argv.push_back(strdup(mLogFilePath.c_str()));
#endif

    // Add the rest of the args
    for (const char *arg : args)
    {
        argv.push_back(strdup(arg));
    }

    // Always terminate the args with a nullptr
    argv.push_back(nullptr);


    // Environment
    std::vector<char*> envv;
    envv.reserve(envs.size() + 3);
    for (const char *env : envs)
    {
        envv.push_back(strdup(env));
    }

#if !defined(RDK)
    // Frustratingly runc doesn't have an option for passing in arbitrary
    // file descriptors, however it does support the systemd LISTEN_PID &
    // LISTEN_FDS environment vars, which basically do the equivalent
    if (!files.empty())
    {
        char buf[64];

        snprintf(buf, sizeof(buf), "LISTEN_FDS=%zu", files.size());
        envv.push_back(strdup(buf));

        snprintf(buf, sizeof(buf), "LISTEN_PID=%d", getpid());
        envv.push_back(strdup(buf));
    }
#endif

    // Always terminate the args with a nullptr
    envv.push_back(nullptr);


    // finally do the fork
    pid_t pid = vfork();
    if (pid < 0)
    {
        AI_LOG_SYS_ERROR(errno, "fork failed");
    }
    else if (pid == 0)
    {
        // In child process

        // Open /dev/null so can redirect stdin, stdout and stderr to that
        int devNull = open("/dev/null", O_RDWR);
        if (devNull < 0)
            _exit(EXIT_FAILURE);

        // Remap stdin to /dev/null
        dup2(devNull, STDIN_FILENO);

        // Remap stdout to either the supplied stream or /dev/null
        if (stdoutStream)
            stdoutStream->dupWriteFD(STDOUT_FILENO, false);
        else
            dup2(devNull, STDOUT_FILENO);

        // Remap stderr to either the supplied stream or /dev/null
        if (stderrStream)
            stderrStream->dupWriteFD(STDERR_FILENO, false);
        else
            dup2(devNull, STDERR_FILENO);

        // Don't need /dev/null anymore
        if (devNull > STDERR_FILENO)
        {
            close(devNull);
            devNull = -1;
        }

        // From now onwards we should not call any Dobby library code because
        // it may perform logging and therefore use file descriptors that will
        // be closed


        // All the descriptors in the list should have O_CLOEXEC flag set, so we
        // need to strip it off all of them, in addition we set the file
        // descriptors to be sequential starting from 3.
        if (!files.empty())
        {

            // We have to do a double dup pass as we can't guarantee the
            // supplied file descriptors won't be closed by the dup2 call.
            std::array<int, maxFiles> duppedFiles;
            duppedFiles.fill(-1);

            const int firstSafeFd = 3 + static_cast<int>(files.size());

            //for (int fd : files)
            std::list<int>::const_iterator it = files.begin();
            for (size_t n = 0; it != files.end(); ++it, n++)
            {
                int tmpFd = fcntl(*it, F_DUPFD_CLOEXEC, firstSafeFd);
                if (tmpFd < firstSafeFd)
                    _exit(EXIT_FAILURE);

                duppedFiles[n] = tmpFd;

                if (close(*it) != 0)
                    _exit(EXIT_FAILURE);
            }

            // Now can safely dup2 the files to the correct fd numbers, this
            // will also remove the O_CLOEXEC flag.
            for (size_t n = 0; n < duppedFiles.size(); n++)
            {
                int oldfd = duppedFiles[n];
                if (oldfd < 0)
                    break;

                int newfd = 3 + static_cast<int>(n);

                if (dup2(oldfd, newfd) != newfd)
                    _exit(EXIT_FAILURE);

                if (close(oldfd) != 0)
                    _exit(EXIT_FAILURE);
            }
        }


        // Reset the file mode mask to defaults
        umask(0);

        // Reset the signal mask, we need to do this because signal masks are
        // inherited and we've explicitly blocked SIGCHLD as we're monitoring
        // that using sigwaitinfo
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGCHLD);
        if (sigprocmask(SIG_UNBLOCK, &set, nullptr) != 0)
            _exit(EXIT_FAILURE);

        // Create a new SID for the child process
        //if (setsid() < 0)
        //    _exit(EXIT_FAILURE);

        // Change the current working directory
        if ((chdir("/")) < 0)
            _exit(EXIT_FAILURE);


        // And finally exec the binary
        execve(mRuncPath.c_str(), argv.data(), envv.data());
        _exit(EXIT_FAILURE);
    }


    // in the parent process so clean up the memory allocated for the args
    // and environment vars
    for (char *arg : argv)
    {
        free(arg);
    }
    for (char *env : envv)
    {
        free(env);
    }


    AI_LOG_FN_EXIT();
    return pid;
}


