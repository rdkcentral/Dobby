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
 * File:   Main.cpp
 * Author:
 *
 */
#include "Settings.h"

#include <Dobby.h>
#include <DobbyProtocol.h>

#include <Logging.h>
#include <Tracing.h>
#include <IpcFactory.h>

#if defined(AI_ENABLE_TRACING)
    #include <PerfettoTracing.h>
#endif

#ifdef USE_BREAKPAD
    #include "breakpad_wrapper.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <sched.h>
#include <errno.h>
#include <glob.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>
#include <memory>
#include <chrono>
#include <thread>
#include <cmath>



// The default realtime priority to run the daemon as, on RDK it defaults to off
#if defined(RDK)
static int gPriority = -1;
#else
static int gPriority = 12;
#endif

//
static bool gDaemonise = true;

//
static bool gNoConsole = false;

//
#if (AI_BUILD_TYPE == AI_DEBUG)
static int gLogLevel = AI_DEBUG_LEVEL_INFO;
#else
static int gLogLevel = AI_DEBUG_LEVEL_MILESTONE;
#endif

//
static bool gUseSyslog = false;

//
static bool gUseJournald = false;

//
static int gPrintPidFd = -1;

//
static std::string gDbusAddress;

//
static std::string gSettingsFilePath("/etc/dobby.json");




// -----------------------------------------------------------------------------
/**
 * @brief Simply prints the version string on stdout
 *
 *
 *
 */
static void displayVersion()
{
    printf("Version: " DOBBY_VERSION "\n");
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
    printf("Usage: DobbyDaemon <option(s)>\n");
    printf("  Daemon that starts / stops / manages containers.\n");
    printf("\n");
    printf("  -h, --help                    Print this help and exit\n");
    printf("  -v, --verbose                 Increase the log level\n");
    printf("  -V, --version                 Display this program's version number\n");
    printf("\n");
    printf("  -f, --settings-file=PATH      Path to a JSON dobby settings file [%s]\n", gSettingsFilePath.c_str());
    printf("  -a, --dbus-address=ADDRESS    The dbus address to put the admin service on [system bus]\n");
    printf("  -p, --priority=PRIORITY       Sets the SCHED_RR priority of the daemon [RR,12]\n");
    printf("  -n, --nofork                  Do not fork and daemonise the process\n");
    printf("  -k, --noconsole               Disable console output\n");
    printf("  -g, --syslog                  Send all initial logging to syslog rather than the console\n");
#if defined(RDK) && defined(USE_SYSTEMD)
    printf("  -j, --journald                Enables logging to journald\n");
#endif
    printf("\n");
    printf("  Besides the above options the daemon checks for the follow\n");
    printf("  environment variables\n");
    printf("\n");
    printf("  AI_WORKSPACE_PATH=<PATH>      The path to tmpfs dir to use as workspace\n");
    printf("  AI_PERSISTENT_PATH=<PATH>     The path to dir that is persistent across boots\n");
    printf("  AI_PLATFORM_IDENT=<IDENT>     The 4 characters than make up the STB platform id\n");
    printf("\n");
}

// -----------------------------------------------------------------------------
/**
 * @brief Parses the command line args
 *
 *
 *
 */
static void parseArgs(int argc, char **argv)
{
    struct option longopts[] = {
        { "help",           no_argument,        nullptr,    (int)'h' },
        { "verbose",        no_argument,        nullptr,    (int)'v' },
        { "version",        no_argument,        nullptr,    (int)'V' },
        { "settings-file",  required_argument,  nullptr,    (int)'f' },
        { "dbus-address",   required_argument,  nullptr,    (int)'a' },
        { "nofork",         no_argument,        nullptr,    (int)'n' },
        { "priority",       required_argument,  nullptr,    (int)'p' },
        { "noconsole",      no_argument,        nullptr,    (int)'k' },
        { "syslog",         no_argument,        nullptr,    (int)'g' },
        { "journald",       no_argument,        nullptr,    (int)'j' },
        { nullptr,          0,                  nullptr,    0        }
   };

    opterr = 0;

    int c;
    int longindex;
    while ((c = getopt_long(argc, argv, "hvVa:f:s:np:kgj", longopts, &longindex)) != -1)
    {
        switch (c)
        {
            case 'h':
                displayUsage();
                exit(EXIT_SUCCESS);
                break;

            case 'v':
                gLogLevel++;
                break;

            case 'V':
                displayVersion();
                exit(EXIT_SUCCESS);
                break;

            case 'p':
                gPriority = atoi(optarg);
                break;

            case 'a':
                gDbusAddress = reinterpret_cast<const char*>(optarg);
                break;

            case 'f':
                gSettingsFilePath = reinterpret_cast<const char*>(optarg);
                if (access(gSettingsFilePath.c_str(), R_OK) != 0)
                {
                    fprintf(stderr, "Error: cannot access settings file @ '%s'\n",
                            gSettingsFilePath.c_str());
                    exit(EXIT_FAILURE);
                }
                break;

            case 'n':
                gDaemonise = false;
                break;

            case 'k':
                gNoConsole = true;
                break;

            case 'g':
                gUseSyslog = true;
                break;

#if defined(RDK) && defined(USE_SYSTEMD)
            case 'j':
                gUseJournald = true;
                break;
#endif
            case '?':
                if (optopt == 'c')
                    fprintf(stderr, "Warning: Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Warning: Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Warning: Unknown option character `\\x%x'.\n", optopt);
                break;

            default:
                exit(EXIT_FAILURE);
                break;
        }
    }

    for (int i = optind; i < argc; i++)
    {
        fprintf(stderr, "Warning: Non-option argument %s ignored\n", argv[i]);
    }
}


// -----------------------------------------------------------------------------
/**
 * @brief Parses settings file and processes any environment variables to return
 * the DobbySettings object.
 *
 *
 */
static std::shared_ptr<Settings> createSettings()
{
    // create settings from either defaults or JSON settings file
    std::shared_ptr<Settings> settings;

#if defined(ENABLE_OPT_SETTINGS)
    // Enable searching for settings in /opt/dobby.json
    const std::string DEV_SETTINGS_PATH{"/opt/dobby.json"};
    if(access(DEV_SETTINGS_PATH.c_str(), R_OK) == 0)
    {
        AI_LOG_INFO("parsing settings from file @ '%s'", DEV_SETTINGS_PATH.c_str());
        settings = Settings::fromJsonFile(DEV_SETTINGS_PATH);
    }
    else
#endif

    if (!gSettingsFilePath.empty() && (access(gSettingsFilePath.c_str(), R_OK) == 0))
    {
        AI_LOG_INFO("parsing settings from file @ '%s'", gSettingsFilePath.c_str());
        settings = Settings::fromJsonFile(gSettingsFilePath);
    }
    else
    {
        AI_LOG_WARN("missing or inaccessible settings file, using defaults");
        settings = Settings::defaultSettings();
    }

#if (AI_BUILD_TYPE == AI_DEBUG)
    settings->dump();
#endif

    return settings;
}

// -----------------------------------------------------------------------------
/**
 * @brief Redirects stdout/stderr and stdin to /dev/null
 *
 *
 *
 */
static void closeConsole()
{
    int fd = open("/dev/null", O_RDWR, 0);
    if (fd < 0)
    {
        fprintf(stderr, "failed to redirect stdin, stdout and stderr to /dev/null (%d - %s)\n",
                errno, strerror(errno));
    }
    else
    {
        if (dup2(fd, STDIN_FILENO) < 0)
            fprintf(stderr, "failed to redirect stdin (%d - %s)\n", errno, strerror(errno));
        if (dup2(fd, STDOUT_FILENO) < 0)
            fprintf(stderr, "failed to redirect stdout (%d - %s)\n", errno, strerror(errno));
        if (dup2(fd, STDERR_FILENO) < 0)
            fprintf(stderr, "failed to redirect stderr (%d - %s)\n", errno, strerror(errno));
        if (fd != STDIN_FILENO && fd != STDOUT_FILENO && fd != STDERR_FILENO)
            close(fd);
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief Daemonise ourselves
 *
 *
 *
 */
static void daemonise()
{
    pid_t pid, sid;

    // fork off the parent process
    pid = fork();
    if (pid < 0)
    {
        fprintf(stderr, "Error: fork failed (%d - %s)\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (pid > 0)
    {
        // parent, print out the pid if requested
        if (gPrintPidFd >= 0)
        {
            FILE *fp = fdopen(gPrintPidFd, "w");
            if (fp)
            {
                fprintf(fp, "%d", pid);
                fflush(fp);
                fclose(fp);
            }
        }

        exit(EXIT_SUCCESS);
    }

    // change the file mode mask
    umask(0);

    // create a new SID for the child process
    sid = setsid();
    if (sid < 0)
    {
        fprintf(stderr, "setsid failed (%d - %s)\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }

    // change the current working directory
    if ((chdir("/")) < 0)
    {
        fprintf(stderr, "chdir(\"/\") failed (%d - %s)\n", errno, strerror(errno));
        exit(EXIT_FAILURE);
    }


    // close the stdin, stdout and stderr file descriptors and redirect to
    // /dev/null
    closeConsole();


    // and we're done
}

// -----------------------------------------------------------------------------
/**
 *  @brief Debugging function used to find the address of AI dbus(es) at startup
 *
 *  This is only to help with initial testing, on a real box the buses won't
 *  be available till some time after this daemon is up and running.  But for
 *  testing it's convenient to check for them at startup as it saves an extra
 *  step the user needs to do for testing.
 *
 *  @param[in]  privateBus      If true returns the address of the AI private
 *                              bus (if available) otherwise the address of the
 *                              AI public bus.
 */
#if (AI_BUILD_TYPE == AI_DEBUG)
static std::string getAIDbusAddress(bool privateBus)
{
    const std::vector<std::string> possiblePrivatePaths = {
        "/tmp/ai_workspace.*/dbus/socket/private/serverfd",
        "/mnt/nds/tmpfs/APPLICATIONS_WORKSPACE/dbus/socket/private/serverfd"
    };

    const std::vector<std::string> possiblePublicPaths = {
        "/tmp/ai_workspace.*/dbus/socket/public/serverfd",
        "/mnt/nds/tmpfs/APPLICATIONS_WORKSPACE/dbus/socket/public/serverfd"
    };

    const std::vector<std::string>& possiblePaths =
                    privateBus ? possiblePrivatePaths : possiblePublicPaths;

    std::string path;

    for (size_t i = 0; (i < possiblePaths.size()) && path.empty(); i++)
    {
        glob_t globbuf = { 0, nullptr, 0 };
        int rc = glob(possiblePaths[i].c_str(), GLOB_NOSORT, nullptr, &globbuf);

        if ((rc != GLOB_NOMATCH) && (globbuf.gl_pathc > 0))
        {
            path = "unix:path=";
            path += globbuf.gl_pathv[0];
        }

        globfree(&globbuf);
    }

    return path;
}
#endif // (AI_BUILD_TYPE == AI_DEBUG)


// -----------------------------------------------------------------------------
/**
 * @brief Attempt to set up an IPC service and register the Dobby service
 *
 * Will automatically retry connecting to the IPC service up to a set amount
 * with exponential backoff
 *
 * @returns On success - instance of IPCService. On failure - nullptr
 */
static std::shared_ptr<AI_IPC::IIpcService> setupIpcService()
{
    const int maxRetries = 5;
    const int baseBackoffTime = 200; // ms

    std::shared_ptr<AI_IPC::IIpcService> ipcService;
    for (int i = 1; i <= maxRetries; i++)
    {
        try
        {
            // Create IPCServices that attach to the dbus daemons
            if (gDbusAddress.empty())
            {
                ipcService = AI_IPC::createSystemBusIpcService(DOBBY_SERVICE);
            }
            else
            {
                ipcService = AI_IPC::createIpcService(gDbusAddress, DOBBY_SERVICE);
            }
        }
        catch (const std::exception &e)
        {
            AI_LOG_ERROR("failed to create IPC service with error %s. Attempt %d/%d.", e.what(), i, maxRetries);
        }

        if (!ipcService)
        {
            AI_LOG_ERROR("failed to create one of the IPC services. Attempt %d/%d.", i, maxRetries);
        }
        else if (!ipcService->isValid())
        {
            AI_LOG_ERROR("Failed to initialise the IPC service. Attempt %d/%d.", i, maxRetries);
        }
        else
        {
            return ipcService;
        }

        if (i < maxRetries)
        {
            ipcService.reset();
            const int backoffTime = std::pow(2, (i - 1)) * baseBackoffTime;

            AI_LOG_INFO("Retrying in %dms", backoffTime);
            std::this_thread::sleep_for(std::chrono::milliseconds(backoffTime));
        }
    }

    AI_LOG_FATAL("Failed to create IPC Service - max retries hit");
    return nullptr;
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
    try {
        int rc = EXIT_SUCCESS;

        parseArgs(argc, argv);

        // Set our priority if requested
        if (gPriority > 0)
        {
            struct sched_param param;
            param.sched_priority = gPriority;
            sched_setscheduler(0, SCHED_RR, &param);
        }

        // Setup the AI logging stuff
        unsigned logTargets = Dobby::Console;
        if (gUseSyslog)
        {
            logTargets |= Dobby::SysLog;
        }

        // Also log to journald on the RDK builds
        if (gUseJournald)
        {
            logTargets |= Dobby::Journald;
        }


        Dobby::setupLogging(logTargets);
        __ai_debug_log_level = gLogLevel;


        AI_LOG_MILESTONE("starting Dobby daemon");

        // Daemonise ourselves to run in the background
        if (gDaemonise)
        {
            daemonise();

            logTargets &= ~Dobby::Console;
            Dobby::setupLogging(logTargets);
        }
        // Shutdown the console if asked to
        else if (gNoConsole)
        {
            closeConsole();

            logTargets &= ~Dobby::Console;
            Dobby::setupLogging(logTargets);
        }

        // Create object storing Dobby settings
        const std::shared_ptr<Settings> settings = createSettings();


        // Setup signals, this MUST be done in the main thread before any other
        // threads are spawned
        Dobby::configSignals();


        // Initialise tracing on debug builds (warning: this must be done after the
        // Dobby::configSignals() call above, because it spawns threads that mess
        // with the signal masks)
#if defined(AI_ENABLE_TRACING)
        PerfettoTracing::initialise();
#endif

        AI_LOG_INFO("starting dbus service");
#if defined(USE_SYSTEMD)
        AI_LOG_INFO("Dobby built with systemd support - using sd-bus");
#else
        AI_LOG_INFO("Dobby built without systemd support - using libdbus");
#endif
        AI_LOG_INFO("  dbus address '%s'", gDbusAddress.c_str());
        AI_LOG_INFO("  service name '%s'", DOBBY_SERVICE);
        AI_LOG_INFO("  object name '%s'", DOBBY_OBJECT);

        // Create the IPC service and start it, this spawns a thread and runs the dbus
        // event loop inside it.
        std::shared_ptr<AI_IPC::IIpcService> ipcService = setupIpcService();

        if (!ipcService)
        {
            rc = EXIT_FAILURE;
        }
        else if (!ipcService->isServiceAvailable(DOBBY_SERVICE))
        {
            // Double check we did actually make ourselves available on the bus
            AI_LOG_ERROR("IPC Service initialised but service %s is not available on the bus", DOBBY_SERVICE);
            rc = EXIT_FAILURE;
        }
        else
        {
            // Create the dobby object and hook into the IPC service
            Dobby dobby(ipcService->getBusAddress(), ipcService, settings);

            // On debug builds try and detect the AI dbus addresses at startup
#if (AI_BUILD_TYPE == AI_DEBUG)
            dobby.setDefaultAIDbusAddresses(getAIDbusAddress(true),
                                        getAIDbusAddress(false));
#endif // (AI_BUILD_TYPE == AI_DEBUG)

            // Start the service, this spawns a thread and runs the dbus event
            // loop inside it
            ipcService->start();

            // Milestone
            AI_LOG_MILESTONE("started Dobby daemon");

            // Wait till the Dobby service is terminated, this is obviously a
            // blocking call
            dobby.run();

            // Stop the service and fall out
            ipcService->stop();
        }

        // Milestone
        if (rc == EXIT_SUCCESS)
        {
            AI_LOG_MILESTONE("stopped Dobby daemon");
        }

        // And we're done
        AICommon::termLogging();
        return rc;
    } catch (const std::exception& e) {
        AI_LOG_FATAL("Unhandled exception in main: %s", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        AI_LOG_FATAL("Unhandled unknown exception in main");
        return EXIT_FAILURE;
    }
}

