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
* Dobby Bundle Generator tool
*
*/

#include "IReadLine.h"
#include "Logging.h"
#include "DobbySpecConfig.h"
#include "IDobbyUtils.h"
#include "DobbyBundle.h"
#include "DobbyRootfs.h"
#include "IDobbyUtils.h"
#include "DobbyUtils.h"
#include "Settings.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <memory.h>

#include <iostream>
#include <fstream>

#define DEFAULT_SETTINGS_PATH "/etc/dobby.json"

// Some globals variables for command line arguments
static std::string inputPath;
static std::string outputDirectory;
static std::string settingsPath;

// -----------------------------------------------------------------------------
/**
 * @brief Shows usage/help info
 */
static void displayUsage()
{
    printf("Usage: DobbyBundleGenerator <option(s)>\n");
    printf("  Tool to convert Dobby JSON spec to OCI bundle without needing a running Dobby Daemon\n");
    printf("\n");
    printf("  -h, --help                    Print this help and exit\n");
    printf("  -v, --verbose                 Increase the log level\n");
    printf("\n");
    printf("  -s, --settings=PATH           Path to Dobby Settings file for STB \n");
    printf("  -i, --inputpath=PATH          Path to Dobby JSON Spec for container\n");
    printf("  -o, --outputDirectory=PATH    Where to save the generated OCI bundle\n");
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
            {"help", no_argument, nullptr, (int)'h'},
            {"verbose", no_argument, nullptr, (int)'v'},
            {"settings", required_argument, nullptr, (int)'s'},
            {"inputpath", required_argument, nullptr, (int)'i'},
            {"outputDirectory", required_argument, nullptr, (int)'o'},
            {nullptr, 0, nullptr, 0}};

    int opt;
    int index;

    // Read through all the options and set variables accordingly
    while ((opt = getopt_long(argc, argv, "+hvVi:o:s:", longopts, &index)) != -1)
    {
        switch (opt)
        {
        case 'h':
            displayUsage();
            exit(EXIT_SUCCESS);
            break;
        case 'v':
            __ai_debug_log_level++;
            break;
        case 'i':
            inputPath = reinterpret_cast<const char *>(optarg);
            break;
        case 'o':
            outputDirectory = reinterpret_cast<const char *>(optarg);
            break;
        case 's':
            settingsPath = reinterpret_cast<const char *>(optarg);
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

// -----------------------------------------------------------------------------
/**
 * @brief   Load settings from the provided path
 *          Loads default settings if no JSON file is provided
 */
static std::shared_ptr<Settings> readSettings()
{
    std::shared_ptr<Settings> settings;
    if (!settingsPath.empty() && (access(settingsPath.c_str(), R_OK) == 0))
    {
        AI_LOG_INFO("parsing settings from file @ '%s'", settingsPath.c_str());
        settings = Settings::fromJsonFile(settingsPath);
    }
    else if(access(DEFAULT_SETTINGS_PATH, R_OK) == 0)
    {
        AI_LOG_INFO("parsing settings from default file path @ '%s'", DEFAULT_SETTINGS_PATH);
        settings = Settings::fromJsonFile(DEFAULT_SETTINGS_PATH);
    }
    else
    {
        AI_LOG_WARN("missing or inaccessible settings file, using defaults");
        settings = Settings::defaultSettings();
    }

    return settings;
}

// -----------------------------------------------------------------------------
/**
 * @brief Reads a Dobby spec file into a string
 *
 * @returns String containing Dobby JSON spec
 */
static std::string readSpecFromFile(const std::string &path)
{
    if (path.empty())
    {
        return std::string();
    }

    std::ifstream file(path, std::ifstream::binary);

    if (!file.is_open())
    {
        AI_LOG_ERROR("Failed to open file at %s", path.c_str());
    }

    // Read our file into a string
    std::string jsonSpecString;

    file.seekg(0, std::ifstream::end);
    jsonSpecString.resize(file.tellg());
    file.seekg(0, std::ifstream::beg);

    file.read(&jsonSpecString[0], jsonSpecString.size());

    return jsonSpecString;
}

static bool generateOciBundle(std::shared_ptr<IDobbySettings> settings,
                              std::shared_ptr<IDobbyUtils> utils,
                              std::string specPath,
                              std::string bundlePath)
{
    // Parse the JSON config
    auto jsonSpec = readSpecFromFile(specPath);
    if (jsonSpec.empty())
    {
        AI_LOG_ERROR("Failed to load spec from path %s", specPath.c_str());
        return false;
    }

    // Need somewhere to save our bundle
    auto bundle = std::make_shared<DobbyBundle>(utils, bundlePath, true);
    if (!bundle || !bundle->isValid())
    {
        AI_LOG_ERROR("Failed to create bundle directory %s", outputDirectory.c_str());
        return false;
    }

    // Make sure we've got a config we can use
    auto config = std::make_shared<DobbySpecConfig>(utils, settings, bundle, jsonSpec);
    if (!config || !config->isValid())
    {
        AI_LOG_ERROR("Invalid Dobby config");
        return false;
    }

    // Build our rootfs (from hardcoded template for now...)
    auto rootfs = std::make_shared<DobbyRootfs>(utils, bundle, config);
    if (!rootfs || !rootfs->isValid())
    {
        AI_LOG_ERROR("failed to create rootfs");
        return false;
    }
    rootfs->setPersistence(true);

    return true;
}

// -----------------------------------------------------------------------------
/**
 * @brief Entrypoint
 */
int main(int argc, char *argv[])
{
    printf("Dobby Bundle Generator Tool\n");
    parseArgs(argc, argv);

    // Set up so we can read commands
    auto readLine = IReadLine::create();
    if (!readLine || !readLine->isValid())
    {
        AI_LOG_ERROR_EXIT("failed to create ReadLine object");
        exit(EXIT_FAILURE);
    }

    // Can't do any work without a file to process or somewhere to put the
    // output
    if (inputPath.empty())
    {
        AI_LOG_ERROR("Must provide a Dobby spec as an input");
        exit(EXIT_FAILURE);
    }
    else if (access(inputPath.c_str(), R_OK) != 0)
    {
        AI_LOG_ERROR("Cannot access Dobby spec file %s", inputPath.c_str());
        exit(EXIT_FAILURE);
    }

    // We'll create the directory if it's missing later, so no need to check
    // if we can read it
    if (outputDirectory.empty())
    {
        AI_LOG_ERROR("Must provide an output directory");
        exit(EXIT_FAILURE);
    }

    // Dobby uses a JSON settings file to provide STB-specific settings (e.g GPU)
    // Can be left blank (defaylt settings will be used)
    else if (!settingsPath.empty() && access(settingsPath.c_str(), R_OK) != 0)
    {
        AI_LOG_ERROR("Cannot access settings file %s", inputPath.c_str());
        exit(EXIT_FAILURE);
    }

    AI_LOG_INFO("Parsing Dobby spec file %s\n", inputPath.c_str());
    AI_LOG_INFO("Generating Bundle in directory: %s\n", outputDirectory.c_str());

    // Get settings from the provided json file
    auto settings = readSettings();
    auto utils = std::make_shared<DobbyUtils>();

    // Now we can do some actual work
    generateOciBundle(settings, utils, inputPath, outputDirectory);

    // And we're done
    AICommon::termLogging();
    return EXIT_SUCCESS;
}