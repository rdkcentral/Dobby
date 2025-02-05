/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2015 Sky UK
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
 * File:   Main.cpp
 * Author:
 *
 */
#include <DobbyProtocol.h>
#include <DobbyProxy.h>

#include <Dobby/IDobbyProxy.h>

#include <IReadLine.h>
#include <Logging.h>
#include <IpcFactory.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sched.h>
#include <errno.h>
#include <glob.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/limits.h>
#include <dirent.h>
#include <future>

#include <list>
#include <string>
#include <memory>
#include <cctype>
#include <vector>
#include <fstream>


#define ARRAY_LENGTH(x)   (sizeof(x) / sizeof((x)[0]))

#if defined(LEGACY_COMPONENTS)
    #define ACCEPTED_START_PATHS "specfile/bundlepath"
#else
    #define ACCEPTED_START_PATHS "bundlepath"
#endif // defined(LEGACY_COMPONENTS)

typedef struct waitParms {
    std::string containerId;
    IDobbyProxyEvents::ContainerState state;
} waitParams;


//
static std::string gDBusService(DOBBY_SERVICE ".test");

//
static char** gCmdlineArgv = NULL;
static int gCmdlineArgc = 0;

std::mutex gLock;
std::promise<void> promise;


// -----------------------------------------------------------------------------
/**
 * @brief Called when a container stop event occurs. Used to ensure we wait
 * until the container has actually stopped before exiting.
 *
 */
void containerStopCallback(int32_t cd, const std::string &containerId,
                           IDobbyProxyEvents::ContainerState state,
                           const void *params)
{
    const std::string *id = static_cast<const std::string *>(params);

    // Interested in stop events only
    if (state == IDobbyProxyEvents::ContainerState::Stopped && containerId == *id)
    {
        AI_LOG_INFO("Container %s has stopped", containerId.c_str());
        promise.set_value();
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief Called when a container stop event occurs. Used to ensure we wait
 * until the container has actually stopped before exiting.
 *
 */
void containerWaitCallback(int32_t cd, const std::string &containerId,
                           IDobbyProxyEvents::ContainerState state,
                           const void *params)
{
    const waitParams *wp = static_cast<const waitParams*>(params);

    if (state == wp->state && containerId == wp->containerId)
    {
        AI_LOG_INFO("Wait complete");
        promise.set_value();
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static int32_t getContainerDescriptor(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                                      const std::string& id)
{
    // get a list of the containers so can match id with descriptor
    const std::list<std::pair<int32_t, std::string>> containers = dobbyProxy->listContainers();
    for (const std::pair<int32_t, std::string>& container : containers)
    {
        char strDescriptor[32];
        sprintf(strDescriptor, "%d", container.first);

        if ((id == strDescriptor) || (id == container.second))
        {
            return container.first;
        }
    }

    return -1;
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void stopCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                        const std::shared_ptr<const IReadLineContext>& readLine,
                        const std::vector<std::string>& args)
{
    if (args.size() < 1)
    {
        readLine->printLnError("must provide at least one arg; <id>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }

    bool withPrejudice = false;
    for (size_t i = 1; i < args.size(); i++)
    {
        if (args[i] == "--force")
        {
            withPrejudice = true;
        }
    }

    // Register an event listener to monitor for the container stop
    std::lock_guard<std::mutex> locker(gLock);
    promise = std::promise<void>();
    const void *vp = static_cast<void*>(new std::string(id));
    int listenerId = dobbyProxy->registerListener(&containerStopCallback, vp);

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        std::future<void> future = promise.get_future();
        if (!dobbyProxy->stopContainer(cd, withPrejudice))
        {
            readLine->printLnError("failed to stop the container");
        }
        else
        {
            // Block here until container has stopped
            future.wait();
            readLine->printLn("stopped container '%s'", id.c_str());
        }
    }

    // Always make sure we unregister our callback
    dobbyProxy->unregisterListener(listenerId);
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void startCommand(const std::shared_ptr<IDobbyProxy> &dobbyProxy,
                         const std::shared_ptr<const IReadLineContext> &readLine,
                         const std::vector<std::string> &args)
{
    if (args.size() < 2 || args[0].empty() || args[1].empty())
    {
        readLine->printLnError("must provide at least two args; <id> <specfile/bundlepath>");
        return;
    }

    size_t i = 0;
    std::list<int> files;
    std::string displaySocketPath;
    std::vector<std::string> envVars;

    // Command will be in the form "start --<option1> --<optionN> <id> <specfile> <commands>"
    while (i < args.size() && args[i].c_str()[0] == '-')
    {
        if (args[i] == "--westeros-socket")
        {
            // TODO:: This won't work if the arg is in the form --westeros-socket=/path/to/socket
            // The next arg should be the path to socket
            i++;

            char *westerosPath = realpath(args[i].c_str(), NULL);
            if (westerosPath == nullptr)
            {
                readLine->printLnError("Path '%s' does not exist", args[i].c_str());
                return;
            }
            displaySocketPath = westerosPath;
        }
        else if (args[i] == "--envvar")
        {
            // TODO:: This won't work if the arg is in the form --envvar=foo:bar
            // The next arg should be the envvar
            i++;

            std::string var = args[i];
            envVars.emplace_back(var);
        }
        else
        {
            readLine->printLnError("unknown argument '%s'", args[i].c_str());
            return;
        }
        i++;
    }

    // If we parsed any options, check we've still got enough remaining args
    if (args.size() - i < 2)
    {
        readLine->printLnError("must provide at least two args; <id> <" ACCEPTED_START_PATHS ">");
        return;
    }

    // Get the container ID
    std::string id = args[i];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }
    i++;

    // Get the path to spec/bundle
    char buf[PATH_MAX];
    realpath(args[i].data(), buf);
    const std::string path = buf;
    if (path.empty())
    {
        readLine->printLnError("invalid path '%s'", id.c_str());
        return;
    }
    i++;

    // Any remaining options treated as commands to exec in container
    std::string command;
    while (i < args.size())
    {
        // Add space between command args
        if (!command.empty())
        {
            command.append(" ");
        }
        command.append(args[i]);
        i++;
    }

    int32_t cd;

    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) < 0)
    {
        readLine->printLnError("failed to stat '%s' (%d - %s)",
                               path.c_str(), errno, strerror(errno));
        return;
    }

    // check if path points to a directory
    if (S_ISDIR(statbuf.st_mode))
    {
        // path points to a directory check that the path contains a config file
        struct dirent *dir;
        DIR *d = opendir(path.c_str());
        if (d == nullptr)
        {
            readLine->printLnError("failed to opendir '%s' (%d - %s)",
                                   path.c_str(), errno, strerror(errno));
            return;
        }
        bool configFound = false;
        while ((dir = readdir(d)) != nullptr)
        {
            if (strcmp(dir->d_name, "config.json") == 0)
            {
                // config file found, we can continue
                configFound = true;
                break;
            }
        }
        closedir(d);

        if (!configFound)
        {
            readLine->printLnError("no config.json file found in '%s'", path.c_str());
            return;
        }

        cd = dobbyProxy->startContainerFromBundle(id, path, files, command, displaySocketPath, envVars);
    }
    else
    {
#if defined(LEGACY_COMPONENTS)
        // Path does not point to a directory, check that the file in path has
        // a '.json' filename extension.
        if (path.find(".json") == std::string::npos)
        {
            readLine->printLnError("please provide the path to a bundle or a "
                                   "valid .json file");
            return;
        }

        std::ifstream file(path, std::ifstream::binary);
        if (!file)
        {
            readLine->printLnError("failed to open '%s'", args[1].c_str());
            return;
        }

        file.seekg(0, std::ifstream::end);
        ssize_t length = file.tellg();
        file.seekg(0, std::ifstream::beg);

        char *buffer = new char[length];
        file.read(buffer, length);

        std::string jsonSpec(buffer, length);
        delete[] buffer;
        cd = dobbyProxy->startContainerFromSpec(id, jsonSpec, files, command, displaySocketPath, envVars);
#else
        readLine->printLnError("please provide the path to a bundle directory");
        return;
#endif // defined(LEGACY_COMPONENTS)
    }

    if (cd < 0)
    {
        readLine->printLnError("failed to start container '%s'", id.c_str());
    }
    else
    {
        readLine->printLn("started '%s' container, descriptor is %d", id.c_str(), cd);
    }

    std::list<int>::iterator it = files.begin();
    for (; it != files.end(); ++it)
    {
        int fd = *it;
        if ((fd >= 0) && (close(fd) != 0))
        {
            readLine->printLnError("failed to close fd (%d - %s)", errno,
                                   strerror(errno));
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void pauseCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                        const std::shared_ptr<const IReadLineContext>& readLine,
                        const std::vector<std::string>& args)
{
    if (args.size() < 1)
    {
        readLine->printLnError("must provide at least one arg; <id>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        if (!dobbyProxy->pauseContainer(cd))
        {
            readLine->printLnError("failed to pause the container");
        }
        else
        {
            readLine->printLn("paused container '%s'", id.c_str());
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void resumeCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                        const std::shared_ptr<const IReadLineContext>& readLine,
                        const std::vector<std::string>& args)
{
    if (args.size() < 1)
    {
        readLine->printLnError("must provide at least one arg; <id>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        if (!dobbyProxy->resumeContainer(cd))
        {
            readLine->printLnError("failed to resume container '%s'", id.c_str());
        }
        else
        {
            readLine->printLn("resumed container '%s'", id.c_str());
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void hibernateCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                              const std::shared_ptr<const IReadLineContext>& readLine,
                              const std::vector<std::string>& args)
{
    if (args.size() < 1)
    {
        readLine->printLnError("must provide at least one arg; <id>");
        return;
    }

    size_t i = 0;
    std::string options;

    // Find options from arguments (start with a '--')
    while (args[i].length() > 1 && args[i].c_str()[0] == '-' && args[i].c_str()[1] == '-')
    {
        // strip off the '--'
        std::string arg(args[i].c_str() + 2);
        // Add comma between options
        if (!options.empty())
        {
            options.append(",");
        }
        options.append(arg);
        i++;
    }

    std::string id = args[i];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        if (!dobbyProxy->hibernateContainer(cd, options))
        {
            readLine->printLnError("failed to hibernate the container");
        }
        else
        {
            readLine->printLn("hibernate successful for container '%s'", id.c_str());
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void wakeupCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                           const std::shared_ptr<const IReadLineContext>& readLine,
                           const std::vector<std::string>& args)
{
    if (args.size() < 1)
    {
        readLine->printLnError("must provide at least one arg; <id>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        if (!dobbyProxy->wakeupContainer(cd))
        {
            readLine->printLnError("failed to wakeup container '%s'", id.c_str());
        }
        else
        {
            readLine->printLn("wakeup container '%s' successful", id.c_str());
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void mountCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                         const std::shared_ptr<const IReadLineContext>& readLine,
                         const std::vector<std::string>& args)
{
    if (args.size() < 4 || args[0].empty() || args[1].empty() || args[2].empty() || args[3].empty())
    {
        readLine->printLnError("must provide at least 4 args; <id> <source> <destination> <mountFlags>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }
    std::string source(args[1]);
    std::string destination(args[2]);
    std::vector<std::string> mountFlags;
    std::string mountData;

    // parse args[3] which is a comma separated list of flags into a vector of strings
    std::string flags = args[3];
    size_t pos = 0;
    while ((pos = flags.find(",")) != std::string::npos)
    {
        std::string flag = flags.substr(0, pos);
        mountFlags.push_back(flag);
        flags.erase(0, pos + 1);
    }
    mountFlags.push_back(flags);

    // mountData is optional for now
    if(args.size() >= 5 && !args[4].empty())
    {
        mountData = args[4];
    }

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        if (!dobbyProxy->addContainerMount(cd, source, destination, mountFlags, mountData))
        {
            readLine->printLnError("failed to mount %s inside the container %s", source.c_str(), id.c_str());
        }
        else
        {
            readLine->printLn("mount successful for container '%s'", id.c_str());
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void unmountCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                         const std::shared_ptr<const IReadLineContext>& readLine,
                         const std::vector<std::string>& args)
{
    if (args.size() < 2 || args[0].empty() || args[1].empty())
    {
        readLine->printLnError("must provide at least two args; <id> <source>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }
    std::string source(args[1]);
    
    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        if (!dobbyProxy->removeContainerMount(cd, source))
        {
            readLine->printLnError("failed to unmount %s inside the container %s", source.c_str(), id.c_str());
        }
        else
        {
            readLine->printLn("unmount successful for container '%s'", id.c_str());
        }
    }
}
// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void annotateCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                         const std::shared_ptr<const IReadLineContext>& readLine,
                         const std::vector<std::string>& args)
{
    if (args.size() < 3 || args[0].empty() || args[1].empty() || args[2].empty())
    {
        readLine->printLnError("must provide at least 3 args; <id> <key> <value>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }
    std::string annotateKey(args[1]);
    std::string annotateValue(args[2]);

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        if (!dobbyProxy->addAnnotation(cd, annotateKey, annotateValue))
        {
            readLine->printLnError("failed to add %s %s pair inside the container %s", annotateKey.c_str(), annotateValue.c_str(), id.c_str());
        }
        else
        {
            readLine->printLn("annotate successful for container '%s'", id.c_str());
        }
    }
}
// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void removeAnnotationCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                         const std::shared_ptr<const IReadLineContext>& readLine,
                         const std::vector<std::string>& args)
{
    if (args.size() < 2 || args[0].empty() || args[1].empty())
    {
        readLine->printLnError("must provide at least 2 args; <id> <key>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }
    std::string annotateKey(args[1]);

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        if (!dobbyProxy->removeAnnotation(cd, annotateKey))
        {
            readLine->printLnError("failed to remove %s key from the container %s annotations", annotateKey.c_str(), id.c_str());
        }
        else
        {
            readLine->printLn("removed %s key from container '%s' annotations", annotateKey.c_str(), id.c_str());
        }
    }
}
// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void execCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                        const std::shared_ptr<const IReadLineContext>& readLine,
                        const std::vector<std::string>& args)
{
    if (args.size() < 2 || args[0].empty() || args[1].empty())
    {
        readLine->printLnError("must provide at least two args; <id> <command>");
        return;
    }

    size_t i = 0;
    std::string options;

    // Find options from arguments (start with a '-')
    while (args[i].c_str()[0] == '-')
    {
        // Add space between options
        if (!options.empty())
        {
            options.append(" ");
        }
        options.append(args[i]);
        i++;
    }

    std::string id = args[i];
    if (i >= args.size())
    {
        readLine->printLnError("No container id given");
        return;
    }
    i++;

    // Create a command from leftover args
    std::string command;
    if (i >= args.size())
    {
        readLine->printLnError("No command given for exec.");
        return;
    }
    while (i < args.size())
    {
        // Add space between command args
        if (!command.empty())
        {
            command.append(" ");
        }
        command.append(args[i]);
        i++;
    }

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        if (!dobbyProxy->execInContainer(cd, options, command))
        {
            readLine->printLnError("failed to execute command in container '%s'", id.c_str());
        }
        else
        {
            readLine->printLn("executed command in '%s' container, descriptor is %d", id.c_str(), cd);
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void listCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                        const std::shared_ptr<const IReadLineContext>& readLine,
                        const std::vector<std::string>& args)
{
    std::list<std::pair<int32_t, std::string>> containers = dobbyProxy->listContainers();
    if (containers.empty())
    {
        readLine->printLn("no containers");
    }
    else
    {
        readLine->printLn(" descriptor | id                               | state");
        readLine->printLn("------------|----------------------------------|-------------");

        for (const std::pair<int32_t, std::string>& details : containers)
        {
            std::string state;
            switch (dobbyProxy->getContainerState(details.first))
            {
                case int(IDobbyProxyEvents::ContainerState::Invalid):
                    state = "invalid";
                    break;
                case int(IDobbyProxyEvents::ContainerState::Starting):
                    state = "starting";
                    break;
                case int(IDobbyProxyEvents::ContainerState::Running):
                    state = "running";
                    break;
                case int(IDobbyProxyEvents::ContainerState::Stopping):
                    state = "stopping";
                    break;
                case int(IDobbyProxyEvents::ContainerState::Paused):
                    state = "paused";
                    break;
                case int(IDobbyProxyEvents::ContainerState::Hibernating):
                    state = "hibernating";
                    break;
                case int(IDobbyProxyEvents::ContainerState::Hibernated):
                    state = "hibernated";
                    break;
                case int(IDobbyProxyEvents::ContainerState::Awakening):
                    state = "awakening";
                    break;
                default:
                    state = "ERR!";
            }

            readLine->printLn(" %10d | %-32s | %s", details.first,
                              details.second.c_str(), state.c_str());
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void infoCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                        const std::shared_ptr<const IReadLineContext>& readLine,
                        const std::vector<std::string>& args)
{
    if (args.size() < 1)
    {
        readLine->printLnError("must provide at least one arg; <id>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        const std::string stats = dobbyProxy->getContainerInfo(cd);
        if (stats.empty())
        {
            readLine->printLnError("failed to get container info");
        }
        else
        {
            readLine->printLn("%s", stats.c_str());
        }
    }
}


// -----------------------------------------------------------------------------
/**
 * @brief Blocks until the specified container to start/stop then returns 0.
 *
 * This is useful for scripting purposes on devices that can't use the
 * Thunder plugin for container control. Designed to be similar to lxc-wait
 *
 */
static void waitCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                        const std::shared_ptr<const IReadLineContext>& readLine,
                        const std::vector<std::string>& args)
{
    if (args.size() != 2)
    {
        readLine->printLnError("must provide a 2 args; <id> <state>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }

    std::string state = args[1];
    if (state.empty())
    {
        readLine->printLnError("Must specify a container state to wait for");
        return;
    }

    IDobbyProxyEvents::ContainerState containerState;

    std::transform(state.begin(), state.end(), state.begin(), ::tolower);
    if (state == "started" || state == "running")
    {
        containerState = IDobbyProxyEvents::ContainerState::Running;
    }
    else if (state == "stopped")
    {
        containerState = IDobbyProxyEvents::ContainerState::Stopped;
    }
    else
    {
        readLine->printLnError("Invalid container state '%s'", state.c_str());
        return;
    }

    // Now wait until the specified container enters the desired state
    std::lock_guard<std::mutex> locker(gLock);
    promise = std::promise<void>();

    waitParams params {
        id,
        containerState
    };

    const void *vp = static_cast<void*>(&params);
    int listenerId = dobbyProxy->registerListener(&containerStopCallback, vp);

    // Block
    std::future<void> future = promise.get_future();
    future.wait();

    readLine->printLn("Container %s has changed state to %s", id.c_str(), state.c_str());
    dobbyProxy->unregisterListener(listenerId);
}

#if (AI_BUILD_TYPE == AI_DEBUG) && defined(LEGACY_COMPONENTS)
// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void dumpCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                        const std::shared_ptr<const IReadLineContext>& readLine,
                        const std::vector<std::string>& args)
{
    if (args.size() < 1)
    {
        readLine->printLnError("must provide at least one arg; <id>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        const std::string spec = dobbyProxy->getSpec(cd);
        if (spec.empty())
        {
            readLine->printLnError("failed to get container spec");
        }
        else
        {
            readLine->printLn("%s", spec.c_str());
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void bundleCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                          const std::shared_ptr<const IReadLineContext>& readLine,
                          const std::vector<std::string>& args)
{
    if (args.size() < 2)
    {
        readLine->printLnError("must provide at least two args; <id> <" ACCEPTED_START_PATHS ">");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }

    std::string path = args[1];
    struct stat statbuf;
    if (stat(path.c_str(), &statbuf) < 0)
    {
        readLine->printLnError("failed to stat '%s' (%d - %s)",
                               path.c_str(), errno, strerror(errno));
        return;
    }

    // Path must point to a Dobby spec file (.json), not a bundle dir
    if (S_ISDIR(statbuf.st_mode))
    {
        readLine->printLnError("Path is not a valid Dobby Spec JSON file");
        return;
    }

    std::ifstream file(path, std::ifstream::binary);
    if (!file)
    {
        readLine->printLnError("failed to open '%s'", args[1].c_str());
        return;
    }

    file.seekg(0, std::ifstream::end);
    ssize_t length = file.tellg();
    file.seekg(0, std::ifstream::beg);

    char* buffer = new char[length];
    file.read(buffer, length);

    std::string jsonSpec(buffer, length);
    delete [] buffer;


    if (dobbyProxy->createBundle(id, jsonSpec))
    {
        readLine->printLn("bundle created for container with id '%s'", id.c_str());
    }
    else
    {
        readLine->printLnError("failed to create bundle with container id '%s'", id.c_str());
    }
}
#endif // (AI_BUILD_TYPE == AI_DEBUG) && defined(LEGACY_COMPONENTS)

#if (AI_ENABLE_TRACING)
// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void traceStartCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                              const std::shared_ptr<const IReadLineContext>& readLine,
                              const std::vector<std::string>& args)
{
    if (args.size() < 1)
    {
        readLine->printLnError("must provide at least one arg; <file>");
        return;
    }

    std::string path = args[0];
    if (path.empty())
    {
        readLine->printLnError("invalid trace file path '%s'", path.c_str());
        return;
    }

    // open / create the trace file
    int fd = open(path.c_str(), O_CLOEXEC | O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd < 0)
    {
        readLine->printLnError("Failed to open / create trace file '%s' (%d - %s)",
                               path.c_str(), errno, strerror(errno));
        return;
    }

    if (dobbyProxy->startInProcessTracing(fd, ""))
    {
        readLine->printLn("started tracing to file '%s'", path.c_str());
    }
    else
    {
        readLine->printLnError("failed to start tracing, check Dobby log for details");
    }

    close(fd);
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void traceStopCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                             const std::shared_ptr<const IReadLineContext>& readLine,
                             const std::vector<std::string>& args)
{
    (void) readLine;
    (void) args;

    dobbyProxy->stopInProcessTracing();
}

#endif // (AI_ENABLE_TRACING)

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void setDbusCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                           const std::shared_ptr<const IReadLineContext>& readLine,
                           const std::vector<std::string>& args)
{
    if (args.size() < 2 || args[0].empty() || args[1].empty())
    {
        readLine->printLnError("must provide at least two args; <private>|<public> <address>");
        return;
    }

    bool privateBus;
    if (args[0] == "private")
        privateBus = true;
    else if (args[0] == "public")
        privateBus = false;
    else
    {
        readLine->printLnError("first argument must be either 'private' or 'public'");
        return;
    }

    const std::string address = args[1];

    if (!dobbyProxy->setAIDbusAddress(privateBus, address))
    {
        readLine->printLnError("failed to set the AI %s dbus address",
                               privateBus ? "private" : "public");
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void shutdownCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                            const std::shared_ptr<const IReadLineContext>& readLine,
                            const std::vector<std::string>& args)
{
    AI_LOG_FN_ENTRY();

    if (!dobbyProxy->shutdown())
    {
        readLine->printLnError("failed to shutdown daemon");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
static void setLogLevelCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                               const std::shared_ptr<const IReadLineContext>& readLine,
                               const std::vector<std::string>& args)
{
    AI_LOG_FN_ENTRY();

    if (args.size() < 1 || args[0].empty())
    {
        readLine->printLnError("must provide at least one arg; <level>");
        return;
    }

    const std::string level = args[0];

    int levelNo;
    if (strcasecmp(level.c_str(), "FATAL") == 0)
        levelNo = AI_DEBUG_LEVEL_FATAL;
    else if (strcasecmp(level.c_str(), "ERROR") == 0)
        levelNo = AI_DEBUG_LEVEL_ERROR;
    else if (strcasecmp(level.c_str(), "WARNING") == 0)
        levelNo = AI_DEBUG_LEVEL_WARNING;
    else if (strcasecmp(level.c_str(), "MILESTONE") == 0)
        levelNo = AI_DEBUG_LEVEL_MILESTONE;
    else if (strcasecmp(level.c_str(), "INFO") == 0)
        levelNo = AI_DEBUG_LEVEL_INFO;
    else if (strcasecmp(level.c_str(), "DEBUG") == 0)
        levelNo = AI_DEBUG_LEVEL_DEBUG;
    else
    {
        readLine->printLnError("Error: invalid LEVEL argument, possible values are "
                               "FATAL, ERROR, WARNING, MILESTONE, INFO or DEBUG\n");
        return;
    }

    if (!dobbyProxy->setLogLevel(levelNo))
    {
        readLine->printLnError("failed to set log level");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 * @brief Initialises the interactive commands
 *
 *
 *
 */
static void initCommands(const std::shared_ptr<IReadLine>& readLine,
                         const std::shared_ptr<IDobbyProxy>& dobbyProxy)
{
    AI_LOG_FN_ENTRY();

    readLine->addCommand("shutdown",
                         std::bind(shutdownCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "shutdown",
                         "Asks the daemon to shutdown\n",
                         "\n");

    readLine->addCommand("start",
                         std::bind(startCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "start [options...] <id> <" ACCEPTED_START_PATHS "> [command]",
                         "Starts a container using the given path. Can optionally specify the command "
                         "to run inside the container. Any arguments after command are treated as "
                         "arguments to the command.\n",
                         "  --westeros-socket    Mount the specified westeros socket into the container\n"
                         "  --envvar             Add an environment variable for this container\n");

    readLine->addCommand("stop",
                         std::bind(stopCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "stop <id> [options...]",
                         "Stops a container with the given id\n",
                         "  --force        Shuts down the container with prejudice (SIGKILL).\n");

    readLine->addCommand("pause",
                         std::bind(pauseCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "pause <id>",
                         "Pauses a container with the given id\n",
                         "\n");

    readLine->addCommand("resume",
                         std::bind(resumeCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "resume <id>",
                         "Resumes a container with the given id\n",
                         "\n");

    readLine->addCommand("hibernate",
                         std::bind(hibernateCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "hibernate [options...] <id>",
                         "Hibernate a container with the given id\n",
                         "\n");

    readLine->addCommand("wakeup",
                         std::bind(wakeupCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "wakeup <id>",
                         "wakeup a container with the given id\n",
                         "\n");
    
    readLine->addCommand("mount",
                         std::bind(mountCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "mount <id> <source> <destination> <mountFlags> <mountData>",
                         "mount a directory from the host inside the container with the given id\n",
                         "\n");
    
    readLine->addCommand("unmount",
                         std::bind(unmountCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "unmount <id> <source>",
                         "unmount a directory inside the container with the given id\n",
                         "\n");

    readLine->addCommand("annotate",
                         std::bind(annotateCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "annonate <id> <key> <value>",
                         "annotate the container with a key value pair\n",
                         "\n");

    readLine->addCommand("remove-annotation",
                         std::bind(removeAnnotationCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "remove-annotation <id> <key>",
                         "removes a key from the container's annotations\n",
                         "\n");

    readLine->addCommand("exec",
                         std::bind(execCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "exec [options...] <id> <command>",
                         "Executes a command in the container with the given id\n",
                         "\n");

    readLine->addCommand("list",
                         std::bind(listCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "list",
                         "Lists all the containers the daemon is managing\n",
                         "\n");

    readLine->addCommand("info",
                         std::bind(infoCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "info <id>",
                         "Gets the json stats for the given container\n",
                         "\n");

    readLine->addCommand("wait",
                         std::bind(waitCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "wait <id> <state>",
                         "Waits for a container with ID to enter a specified state (started, stopped)\n",
                         "\n");

    readLine->addCommand("set-log-level",
                         std::bind(setLogLevelCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "set-log-level <level>",
                         "Dynamically change the log level of the DobbyDaeon daemon. possible values:\n"
                         "FATAL, ERROR, WARNING, MILESTONE, INFO or DEBUG",
                         "\n");                         

#if (AI_BUILD_TYPE == AI_DEBUG) && defined(LEGACY_COMPONENTS)
    readLine->addCommand("dumpspec",
                         std::bind(dumpCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "dumpspec <id> [options...]",
                         "Dumps the json spec used to create the container\n",
                         "\n");

    readLine->addCommand("bundle",
                         std::bind(bundleCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "bundle <id> <specfile> [options...]",
                         "Creates a bundle containing rootfs and config.json for runc\n"
                         "but doesn't actually run it.  Useful for debugging runc issues\n",
                         "\n");
#endif //(AI_BUILD_TYPE == AI_DEBUG) && defined(LEGACY_COMPONENTS)

#if (AI_ENABLE_TRACING)
    readLine->addCommand("trace-start",
                         std::bind(traceStartCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "trace-start <file> [options...]",
                         "Starts the 'in process' tracing of DobbyDaemon, storing the trace\n"
                         "in <file>. The trace is in Perfetto format (https://perfetto.dev/) \n",
                         "  --filter=STR   A category filter string (not yet implemented)\n");

    readLine->addCommand("trace-stop",
                         std::bind(traceStopCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "trace-stop",
                         "Stops the 'in process' running on the DobbyDaemon.  This doesn't\n"
                         "stop any system level tracing enabled via the traced daemon\n",
                         "\n");
#endif // (AI_ENABLE_TRACING)


    readLine->addCommand("set-dbus",
                         std::bind(setDbusCommand, dobbyProxy, std::placeholders::_1, std::placeholders::_2),
                         "set-dbus <private>|<public> <address>",
                         "Sets the AI dbus address\n",
                         "\n");


    AI_LOG_FN_EXIT();
}



// -----------------------------------------------------------------------------
/**
 * @brief Simply prints the version string on stdout
 *
 *
 *
 */
static void displayVersion()
{
    printf("Version: " DOBBY_VERSION  "\n");
}

// -----------------------------------------------------------------------------
/**
 * @brief Simply prints the usage options to stdout
 *
 *
 *
 */
static void displayUsage()
{
    printf("Usage: DobbyTool <option(s)> <cmd>\n");
    printf("  Tool for investigating and debugging issues with the Dobby daemon\n");
    printf("\n");
    printf("  -h, --help                    Print this help and exit\n");
    printf("  -v, --verbose                 Increase the log level\n");
    printf("  -V, --version                 Display this program's version number\n");
    printf("\n");
    printf("  -a, --dbus-address=ADDRESS    The dbus address to talk to, if not set attempts\n");
    printf("                                to find the dbus socket in the usual places\n");
    printf("\n");
}



// -----------------------------------------------------------------------------
/**
 * @brief Parses the command line args
 *
 *
 *
 */
static void parseArgs(const int argc, char **argv)
{
    struct option longopts[] =
    {
        { "help",           no_argument,        nullptr,    (int)'h'    },
        { "verbose",        no_argument,        nullptr,    (int)'v'    },
        { "version",        no_argument,        nullptr,    (int)'V'    },
        { "service",        required_argument,  nullptr,    (int)'s'    },
        { nullptr,          0,                  nullptr,    0           }
    };

    opterr = 0;

    int c;
    int longindex;
    while ((c = getopt_long(argc, argv, "+hvVa:s:", longopts, &longindex)) != -1)
    {
        switch (c)
        {
            case 'h':
                displayUsage();
                exit(EXIT_SUCCESS);
                break;
            case 'v':
                __ai_debug_log_level++;
                break;

            case 'V':
                displayVersion();
                exit(EXIT_SUCCESS);
                break;

            case '?':
                if (optopt == 'c')
                    fprintf(stderr, "Warning: Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Warning: Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Warning: Unknown option character `\\x%x'.\n", optopt);

            default:
                exit(EXIT_FAILURE);
                break;
        }
    }

    if (optind < argc)
    {
        gCmdlineArgv = argv + optind;
        gCmdlineArgc = argc - optind;
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
int main(int argc, char * argv[])
{
    // Append the pid onto the end of the service name so we can run multiple
    // clients
    char strPid[32];
    sprintf(strPid, ".pid%d", getpid());
    gDBusService += strPid;

    // Parse all the command line args
    parseArgs(argc, argv);

    // Setup the AI logging stuff
    // AICommon::initLogging(nullptr);

    // Create the ReadLine object
    std::shared_ptr<IReadLine> readLine = IReadLine::create();
    if (!readLine || !readLine->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create ReadLine object");
        exit(EXIT_FAILURE);
    }

    // Create the IPC service and start it, this spawns a thread and runs the dbus
    // event loop inside it
    AI_LOG_INFO("starting dbus service");
    AI_LOG_INFO("  bus address '%s'", DBUS_SYSTEM_ADDRESS);
    AI_LOG_INFO("  service name '%s'", gDBusService.c_str());

    std::shared_ptr<AI_IPC::IIpcService> ipcService;

    // create an IPCService that attach to the dbus daemon, this throws an
    // exception if it can't connect
    try
    {
        ipcService = AI_IPC::createIpcService(DBUS_SYSTEM_ADDRESS, gDBusService);
    }
    catch (const std::exception& e)
    {
        AI_LOG_ERROR("failed to create IPC service: %s", e.what());
        exit(EXIT_FAILURE);
    }

    if (!ipcService)
    {
        AI_LOG_ERROR("failed to create IPC service");
        exit(EXIT_FAILURE);
    }
    else
    {

        // Start the IPCService which kicks off the dispatcher thread
        ipcService->start();

        // Create a DobbyProxy remote service that wraps up the dbus API
        // calls to the Dobby daemon
        std::shared_ptr<IDobbyProxy> dobbyProxy =
            std::make_shared<DobbyProxy>(ipcService, DOBBY_SERVICE, DOBBY_OBJECT);

        // Add the commands to the readline loop
        initCommands(readLine, dobbyProxy);

        // Check if the command line contained the commands to send, otherwise
        // start the interactive shell
        if (gCmdlineArgv && gCmdlineArgc)
        {
            readLine->runCommand(gCmdlineArgc, gCmdlineArgv);
        }
        else
        {
            // Run the readline loop
            readLine->run();
        }

        // Stop the service and fall out
        ipcService->stop();
    }

    // And we're done
    AICommon::termLogging();
    return EXIT_SUCCESS;
}
