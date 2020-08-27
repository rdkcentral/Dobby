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

#include <list>
#include <string>
#include <memory>
#include <cctype>
#include <vector>
#include <fstream>


#define ARRAY_LENGTH(x)   (sizeof(x) / sizeof((x)[0]))

//
static std::string gDBusService("com.sky.dobby.test");

//
static char** gCmdlineArgv = NULL;
static int gCmdlineArgc = 0;

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

    int32_t cd = getContainerDescriptor(dobbyProxy, id);
    if (cd < 0)
    {
        readLine->printLnError("failed to find container '%s'", id.c_str());
    }
    else
    {
        if (!dobbyProxy->stopContainer(cd, withPrejudice))
        {
            readLine->printLnError("failed to stop the container");
        }
        else
        {
            readLine->printLn("stopped container '%s'", id.c_str());
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
static void startCommand(const std::shared_ptr<IDobbyProxy> &dobbyProxy,
                         const std::shared_ptr<const IReadLineContext> &readLine,
                         const std::vector<std::string> &args)
{
    if (args.size() < 2 || args[0].empty() || args[1].empty())
    {
        readLine->printLnError("must provide at least two args; <id> <specfile/bundlepath>");
        return;
    }

    int i = 0;
    std::list<int> files;
    std::string displaySocketPath;

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
        readLine->printLnError("must provide at least two args; <id> <specfile/bundlepath>");
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
        readLine->printLnError("invalid path to spec file or bundle '%s'", id.c_str());
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

    // If path is to a file, expect it to be a dobby spec, otherwise expect
    // it to be the path to a bundle.
    if (S_ISDIR(statbuf.st_mode))
    {
        // check that the path contains a config file
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

        cd = dobbyProxy->startContainerFromBundle(id, path, files, command, displaySocketPath);
    }
    else
    {
        // check that the file in path has a '.json' filename extension
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
        cd = dobbyProxy->startContainerFromSpec(id, jsonSpec, files, command, displaySocketPath);
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
static void execCommand(const std::shared_ptr<IDobbyProxy>& dobbyProxy,
                        const std::shared_ptr<const IReadLineContext>& readLine,
                        const std::vector<std::string>& args)
{
    if (args.size() < 2 || args[0].empty() || args[1].empty())
    {
        readLine->printLnError("must provide at least two args; <id> <command>");
        return;
    }

    int i = 0;
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

#if (AI_BUILD_TYPE == AI_DEBUG)
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
#endif // (AI_BUILD_TYPE == AI_DEBUG)

#if (AI_BUILD_TYPE == AI_DEBUG)
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
        readLine->printLnError("must provide at least two args; <id> <specfile>");
        return;
    }

    std::string id = args[0];
    if (id.empty())
    {
        readLine->printLnError("invalid container id '%s'", id.c_str());
        return;
    }

    std::ifstream file(args[1], std::ifstream::binary);
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
#endif // (AI_BUILD_TYPE == AI_DEBUG)

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
 * @brief Initilises the interactive commands
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
                         "start [options...] <id> <specfile/bundlepath> [command]",
                         "Starts a container using the given spec file or bundle path. Can optionally "
                         "specify the command to run inside the contianer. Any arguments after command "
                         "are treated as arguments to the command.\n",
                         "  --westeros-socket    Mount the specified westeros socket into the container\n");

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

#if (AI_BUILD_TYPE == AI_DEBUG)
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
#endif // (AI_BUILD_TYPE == AI_DEBUG)

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
    printf("Version: \n");
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
    printf("  -s, --service=SERVICE         The dbus service the file mapper daemon is on [%s]\n", gDBusService.c_str());
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

            case 's':
                gDBusService = reinterpret_cast<const char*>(optarg);
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
    AICommon::initLogging(nullptr);

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
