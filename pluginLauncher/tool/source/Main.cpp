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

// Can override the plugin path at build time by setting -DPLUGIN_PATH=/path/to/plugins/
#ifndef PLUGIN_PATH
    #ifdef DEV_VM
        #define PLUGIN_PATH "/usr/local/lib/plugins/dobby"
    #else
        #define PLUGIN_PATH "/usr/lib/plugins/dobby"
    #endif
#endif

static std::string configPath;
static std::string hookName;

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
            hookName = reinterpret_cast<const char *>(optarg);
            break;
        case 'c':
            configPath = reinterpret_cast<const char *>(optarg);
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
            {"preCreation", IDobbyRdkPlugin::HintFlags::PreCreationFlag},
            {"createruntime", IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag},
            {"createcontainer", IDobbyRdkPlugin::HintFlags::CreateContainerFlag},
            {"startcontainer", IDobbyRdkPlugin::HintFlags::StartContainerFlag},
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
bool runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint, std::shared_ptr<rt_dobby_schema> containerConfig, const std::string &rootfsPath)
{
    AI_LOG_INFO("Loading plugins from %s", PLUGIN_PATH);

    // Create an instance of pluginManager to load the plugins
    std::shared_ptr<DobbyRdkPluginUtils> rdkPluginUtils = std::make_shared<DobbyRdkPluginUtils>();
    DobbyRdkPluginManager pluginManager(containerConfig, rootfsPath, PLUGIN_PATH, rdkPluginUtils);

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

    bool success = pluginManager.runPlugins(hookPoint);

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
    const std::string rootfsRelativePath = containerConfig->root->path;

    configPath.replace(configPath.find(configName), configName.length(), rootfsRelativePath);

    return configPath;
}

/**
 * @brief Entrypoint
 */
int main(int argc, char *argv[])
{
    parseArgs(argc, argv);

    if (hookName.empty())
    {
        AI_LOG_ERROR_EXIT("Must give a hook name to execute");
        exit(EXIT_FAILURE);
    }
    if (configPath.empty())
    {
        AI_LOG_ERROR("Path to container's OCI config is required");
        exit(EXIT_FAILURE);
    }

    AI_LOG_INFO("Running hook %s", hookName.c_str());

    // Work out which hook we need to run
    IDobbyRdkPlugin::HintFlags hookPoint = determineHookPoint(hookName);

    if (hookPoint == IDobbyRdkPlugin::Unknown)
    {
        AI_LOG_ERROR("Unknown hook point %s", hookName.c_str());
        exit(EXIT_FAILURE);
    }

    // Create a libocispec object for the container's config
    char *absPath = realpath(configPath.c_str(), NULL);
    if (absPath == nullptr)
    {
        AI_LOG_ERROR("Couldn't find config at %s", configPath.c_str());
        exit(EXIT_FAILURE);
    }
    configPath = std::string(absPath);
    AI_LOG_INFO("Loading container config from file: '%s'", configPath.c_str());

    parser_error err;
    std::shared_ptr<rt_dobby_schema> containerConfig(
        rt_dobby_schema_parse_file(configPath.c_str(), NULL, &err),
        free_rt_dobby_schema);


    if (containerConfig == nullptr)
    {
        AI_LOG_ERROR("Failed to parse OCI config with error: %s", err);
        exit(EXIT_FAILURE);
    }

    // Get the path of the container rootfs to give to plugins
    const std::string rootfsPath = getRootfsPath(configPath, containerConfig);

    const int rdkPluginCount = containerConfig->rdk_plugins->plugins_count;

    // Nothing to do
    if (rdkPluginCount == 0)
    {
        AI_LOG_WARN("No plugins listed in config to run");
        exit(EXIT_SUCCESS);
    }

    AI_LOG_DEBUG("The following plugins are specified in the container config:");
    const auto pluginsInConfig = containerConfig->rdk_plugins->names_of_plugins;
    for (size_t i = 0; i < rdkPluginCount; i++)
    {
        AI_LOG_DEBUG("\t %s", pluginsInConfig[i]);
    }

    // Everything looks good, try to run the plugins
    bool success = runPlugins(hookPoint, containerConfig, rootfsPath);

    if (success)
    {
        AI_LOG_INFO("Plugins run successfully");
        return EXIT_SUCCESS;
    }

    AI_LOG_WARN("Plugins ran with errors");
    return EXIT_FAILURE;
}