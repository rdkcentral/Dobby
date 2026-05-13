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
/**
 * Dobby Plugin Launcher tool
 */
#include <rt_dobby_schema.h>
#include <rt_state_schema.h>

#include "IDobbyRdkPlugin.h"
#include "DobbyRdkPluginManager.h"
#include "DobbyRdkPluginUtils.h"

#include <Logging.h>

#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <iostream>
#include <sstream>
#include <set>
#include <unordered_map>
#include <mutex>
#include <sys/syscall.h>
#include <sys/uio.h>

#ifdef USE_SYSTEMD
    #define SD_JOURNAL_SUPPRESS_LOCATION
    #include <systemd/sd-journal.h>
    #include <systemd/sd-daemon.h>
#endif // USE_SYSTEMD



// Can override the plugin path at build time by setting -DPLUGIN_PATH=/path/to/plugins/
#ifndef PLUGIN_PATH
    #define PLUGIN_PATH "/usr/lib/plugins/dobby"
#endif

static std::string gConfigPath;
static std::string gHookName;
static std::string gContainerId;

// -----------------------------------------------------------------------------
/**
 * @brief Shows usage/help info
 */
static void displayUsage()
{
    printf("Usage: DobbyPluginLauncher <option(s)>\n");
    printf("  Tool to run Dobby plugins loaded from %s\n", PLUGIN_PATH);
    printf("\n");
    printf("  -H, --help                    Print this help and exit\n");
    printf("  -v, --verbose                 Increase the log level\n");
    printf("\n");
    printf("  -h, --hook                    Specify the hook to run\n");
    printf("  -c, --config=PATH             Path to container OCI config\n");
    printf("\n");
}

// -----------------------------------------------------------------------------
/**
 * @brief Read and parse the command-line arguments
 */
static void parseArgs(const int argc, char **argv)
{
    struct option longopts[] =
        {
            {"help", no_argument, nullptr, (int)'H'},
            {"hook", required_argument, nullptr, (int)'h'},
            {"verbose", no_argument, nullptr, (int)'v'},
            {"config", required_argument, nullptr, (int)'c'},
            {nullptr, 0, nullptr, 0}};

    int opt;
    int index;

    // Read through all the options and set variables accordingly
    while ((opt = getopt_long(argc, argv, "+Hvh:c:", longopts, &index)) != -1)
    {
        switch (opt)
        {
        case 'H':
            displayUsage();
            exit(EXIT_SUCCESS);
            break;
        case 'v':
            __ai_debug_log_level++;
            break;
        case 'h':
            gHookName = reinterpret_cast<const char *>(optarg);
            break;
        case 'c':
            gConfigPath = reinterpret_cast<const char *>(optarg);
            break;
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
    return;
}

/**
 * @brief Convert the name of a hook to the necessary bitflag
 *
 * @param[in]   hookName    Name of the hook to run
 */
IDobbyRdkPlugin::HintFlags determineHookPoint(const std::string &hookName)
{
    std::unordered_map<std::string, IDobbyRdkPlugin::HintFlags> const hookMappings =
        {
            {"postinstallation", IDobbyRdkPlugin::HintFlags::PostInstallationFlag},
            {"precreation", IDobbyRdkPlugin::HintFlags::PreCreationFlag},
            {"createruntime", IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag},
            {"createcontainer", IDobbyRdkPlugin::HintFlags::CreateContainerFlag},
#ifdef USE_STARTCONTAINER_HOOK
            {"startcontainer", IDobbyRdkPlugin::HintFlags::StartContainerFlag},
#endif
            {"poststart", IDobbyRdkPlugin::HintFlags::PostStartFlag},
            {"posthalt", IDobbyRdkPlugin::HintFlags::PostHaltFlag},
            {"poststop", IDobbyRdkPlugin::HintFlags::PostStopFlag}};

    // Convert hook name to lowercase
    std::string lowercase;
    lowercase.resize(hookName.length());
    std::transform(hookName.begin(), hookName.end(), lowercase.begin(), ::tolower);

    auto it = hookMappings.find(lowercase);
    if (it != hookMappings.end())
    {
        return it->second;
    }
    // If we can't find it, return unknown
    return IDobbyRdkPlugin::HintFlags::Unknown;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets the state of the container as defined in the OCI spec here:
 *  https://github.com/opencontainers/runtime-spec/blob/master/runtime.md#state
 *
 *  Only available with OCI container hooks.
 *
 *  @return State object. nullptr if not available
 */
std::shared_ptr<const rt_state_schema> getContainerState()
{
    std::mutex lock;
    std::lock_guard<std::mutex> locker(lock);

    char buf[4096];
    bzero(buf, sizeof(buf));

    ssize_t bytesRead = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (bytesRead < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to read stdin");
        return nullptr;
    }
    else if (bytesRead == 0)
    {
        AI_LOG_WARN("No data read from stdin");
        return nullptr;
    }
    buf[bytesRead] = '\0'; 
    
    // Occasionally, there's some extra special characters after the json.
    // We need to clear them out of the string.
    std::string hookStdin(buf);
    if (!hookStdin.empty() && hookStdin[hookStdin.length() - 1] != '}')
    {
        size_t pos = hookStdin.rfind('}');
        if (pos != std::string::npos)
        {
            // clear any characters after the last '}'
            hookStdin.erase(pos + 1);
        }
    }

    parser_error err = nullptr;
    auto state = std::shared_ptr<const rt_state_schema>(
        rt_state_schema_parse_data(hookStdin.c_str(), nullptr, &err),
        free_rt_state_schema);

    if (state.get() == nullptr || err)
    {
        if (hookStdin.length() == sizeof(buf) - 1)
        {
            AI_LOG_ERROR("Most probably the read buffer is too small and causes the parse error below!");
        }

        if (err)
        {
            free(err);
            err = nullptr;
        }

        AI_LOG_ERROR_EXIT("Failed to parse container state");
        return nullptr;
    }

    return state;
}

/**
 * @brief Run the plugins
 *
 * @param[in]   hookPoint           The hook to run
 * @param[in]   containerConfig     Pointer to libocispec struct containing the
 *                                  container config
 * @param[in]   rootfsPath          Path to the container rootfs
 *
 * @return True/false for success/failure for each plugin
 */
bool runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint, std::shared_ptr<rt_dobby_schema> containerConfig, const std::string &rootfsPath, std::shared_ptr<const rt_state_schema> state)
{
    AI_LOG_DEBUG("Loading plugins from %s", PLUGIN_PATH);

    std::shared_ptr<DobbyRdkPluginUtils> rdkPluginUtils;
    rdkPluginUtils = std::make_shared<DobbyRdkPluginUtils>(containerConfig, state, state->id);

    DobbyRdkPluginManager pluginManager(std::move(containerConfig), rootfsPath, PLUGIN_PATH, rdkPluginUtils);

    std::vector<std::string> loadedPlugins = pluginManager.listLoadedPlugins();
    std::vector<std::string> loadedLoggers = pluginManager.listLoadedLoggers();
    AI_LOG_DEBUG("Successfully loaded %zd plugins\n", loadedPlugins.size());
    AI_LOG_DEBUG("Successfully loaded %zd loggers\n", loadedLoggers.size());

    // We've got plugins to run, but nothing is loaded - that's not good
    if (loadedPlugins.size() == 0)
    {
        AI_LOG_ERROR("No plugins were loaded - are there any plugins installed?");
        return {};
    }

    bool success = pluginManager.runPlugins(hookPoint, 4000);

    if (!success)
    {
        AI_LOG_ERROR("Error running plugins");
        return false;
    }

    return true;
}

/**
 * @brief Converts the path given to the config.json file to the path to the
 * container rootfs
 *
 * @param[in]   configPath      Path to config.json file
 * @param[in]   containerConfig libocispec container config pointer
 *
 * @return string with the rootfs path
 */
std::string getRootfsPath(std::string configPath, std::shared_ptr<rt_dobby_schema> containerConfig)
{
    const std::string configName = "config.json";
    const std::string rootfsPath = containerConfig->root->path;

    // check if root path is absolute
    if (rootfsPath.front() == '/')
    {
        return std::move(rootfsPath);
    }

    configPath.replace(configPath.find(configName), configName.length(), rootfsPath);

    return configPath;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Writes logging output to the console.
 *
 *  This duplicates code in the Logging component, but unfortunately we can't
 *  use the function there without messing up the API for all other things
 *  that use it.
 *
 */
void logConsolePrinter(int level, const char *file, const char *func,
                              int line, const char *message)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    struct iovec iov[6];
    char tbuf[32];

    iov[0].iov_base = tbuf;
    iov[0].iov_len = snprintf(tbuf, sizeof(tbuf), "%.010lu.%.06lu ",
                              ts.tv_sec, ts.tv_nsec / 1000);
    iov[0].iov_len = std::min<size_t>(iov[0].iov_len, sizeof(tbuf));


    char threadbuf[32];
    iov[1].iov_base = threadbuf;
    iov[1].iov_len = snprintf(threadbuf, sizeof(threadbuf), "<T-%lu> ", syscall(SYS_gettid));
    iov[1].iov_len = std::min<size_t>(iov[1].iov_len, sizeof(threadbuf));

    switch (level)
    {
        case AI_DEBUG_LEVEL_FATAL:
            iov[2].iov_base = (void*)"FTL: ";
            iov[2].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_ERROR:
            iov[2].iov_base = (void*)"ERR: ";
            iov[2].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_WARNING:
            iov[2].iov_base = (void*)"WRN: ";
            iov[2].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_MILESTONE:
            iov[2].iov_base = (void*)"MIL: ";
            iov[2].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_INFO:
            iov[2].iov_base = (void*)"NFO: ";
            iov[2].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_DEBUG:
            iov[2].iov_base = (void*)"DBG: ";
            iov[2].iov_len = 5;
            break;
        default:
            iov[2].iov_base = (void*)": ";
            iov[2].iov_len = 2;
            break;
    }

    char fbuf[160];
    iov[3].iov_base = (void*)fbuf;
    if (!file || !func || (line <= 0))
        iov[3].iov_len = snprintf(fbuf, sizeof(fbuf), "< M:? F:? L:? > ");
    else
        iov[3].iov_len = snprintf(fbuf, sizeof(fbuf), "< M:%.*s F:%.*s L:%d > ",
                                  64, file, 64, func, line);
    iov[3].iov_len = std::min<size_t>(iov[3].iov_len, sizeof(fbuf));

    iov[4].iov_base = const_cast<char*>(message);
    iov[4].iov_len = strlen(message);

    iov[5].iov_base = (void*)"\n";
    iov[5].iov_len = 1;


    writev(fileno((level <= __ai_debug_log_level) ? stderr : stdout), iov, 6);
}

#ifdef USE_SYSTEMD
// -----------------------------------------------------------------------------
/**
 *  @brief Writes logging output to journald.
 *
 */
void JournaldPrinter(int level, const char *file, const char *func,
                     int line, const char *message)
{
    int priority;
    switch (level)
    {
        case AI_DEBUG_LEVEL_FATAL:          priority = LOG_CRIT;      break;
        case AI_DEBUG_LEVEL_ERROR:          priority = LOG_ERR;       break;
        case AI_DEBUG_LEVEL_WARNING:        priority = LOG_WARNING;   break;
        case AI_DEBUG_LEVEL_MILESTONE:      priority = LOG_NOTICE;    break;
        case AI_DEBUG_LEVEL_INFO:           priority = LOG_INFO;      break;
        case AI_DEBUG_LEVEL_DEBUG:          priority = LOG_DEBUG;     break;
        default:
            return;
    }

    sd_journal_send("SYSLOG_IDENTIFIER=%s", gContainerId.c_str(),
                    "PRIORITY=%i", priority,
                    "CODE_FILE=%s", file,
                    "CODE_LINE=%i", line,
                    "CODE_FUNC=%s", func,
                    "MESSAGE=%s", message,
                    nullptr);
}
#endif

// -----------------------------------------------------------------------------
/**
 *  @brief Logging callback, called every time a log message needs to be emitted
 *
 */
void logPrinter(int level, const char *file, const char *func,
                              int line, const char *message)

{
    // Write to both stdout/err and journald
    logConsolePrinter(level, file, func, line, message);

#ifdef USE_SYSTEMD
    // TODO:: Add command-line argument to enable/disable this based on Dobby
    // log settings (i.e. only do this if Dobby launched with --journald flag)
    JournaldPrinter(level, file, func, line, message);
#endif
}

/**
 * @brief Entrypoint
 */
int main(int argc, char *argv[])
{
    parseArgs(argc, argv);

    AICommon::initLogging(logPrinter);

    if (gHookName.empty())
    {
        AI_LOG_ERROR_EXIT("Must give a hook name to execute");
        return EXIT_FAILURE;
    }
    if (gConfigPath.empty())
    {
        AI_LOG_ERROR_EXIT("Path to container's OCI config is required");
        return EXIT_FAILURE;
    }

    // Work out which hook we need to run
    IDobbyRdkPlugin::HintFlags hookPoint = determineHookPoint(gHookName);

    if (hookPoint == IDobbyRdkPlugin::Unknown)
    {
        AI_LOG_ERROR("Unknown hook point %s", gHookName.c_str());
        return EXIT_FAILURE;
    }

    // Create a libocispec object for the container's config
    char *absPath = realpath(gConfigPath.c_str(), NULL);
    if (absPath == nullptr)
    {
        AI_LOG_ERROR("Couldn't find config at %s", gConfigPath.c_str());
        return EXIT_FAILURE;
    }
    const std::string fullConfigPath = std::string(absPath);
    free(absPath);
    AI_LOG_DEBUG("Loading container config from file: '%s'", fullConfigPath.c_str());

    parser_error err;
    std::shared_ptr<rt_dobby_schema> containerConfig(
        rt_dobby_schema_parse_file(fullConfigPath.c_str(), NULL, &err),
        free_rt_dobby_schema);

    if (containerConfig == nullptr)
    {
        AI_LOG_ERROR("Failed to parse OCI config with error: %s", err);
        return EXIT_FAILURE;
    }

    // Get container id from state (using hostname may be incorrect if we
    // launch multiple containers from same bundle)
    std::shared_ptr<const rt_state_schema> state = getContainerState();
    if (state)
    {
        gContainerId = std::string(state->id);
    }
    else
    {
        AI_LOG_WARN("Failed to get container state from stdin");
        return false;
    }

    AI_LOG_MILESTONE("Running hook %s for container '%s'", gHookName.c_str(), gContainerId.c_str());

    // Get the path of the container rootfs to give to plugins
    const std::string rootfsPath = getRootfsPath(std::move(fullConfigPath), containerConfig);

    const int rdkPluginCount = containerConfig->rdk_plugins->plugins_count;

    // Nothing to do
    if (rdkPluginCount == 0)
    {
        AI_LOG_WARN("No plugins listed in config - nothing to do");
        return EXIT_SUCCESS;
    }

#ifdef DEBUG
    AI_LOG_DEBUG("The following plugins are specified in the container config:");
    const auto pluginsInConfig = containerConfig->rdk_plugins->names_of_plugins;
    for (size_t i = 0; i < rdkPluginCount; i++)
    {
        AI_LOG_DEBUG("\t %s", pluginsInConfig[i]);
    }
#endif // DEBUG

    // Everything looks good, try to run the plugins
    bool success = runPlugins(hookPoint, std::move(containerConfig), rootfsPath, std::move(state));

    if (success)
    {
        AI_LOG_INFO("Hook %s completed", gHookName.c_str());
        return EXIT_SUCCESS;
    }

    AI_LOG_WARN("Hook %s failed - plugin(s) ran with errors", gHookName.c_str());
    return EXIT_FAILURE;
}
