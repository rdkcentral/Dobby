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
 * File:   Settings.cpp
 *
 */

#include "Settings.h"

#include <DobbyProtocol.h>

#include <Logging.h>
#include <FileUtilities.h>

#include <regex>
#include <cstring>
#include <cerrno>
#include <fstream>
#include <ext/stdio_filebuf.h>

#include <grp.h>
#include <fcntl.h>
#include <unistd.h>
#include <wordexp.h>
#include <sys/stat.h>
#include <sys/mount.h>


// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
std::shared_ptr<Settings> Settings::defaultSettings()
{
    return std::shared_ptr<Settings>(new Settings());
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the default values for all settings.
 *
 */
std::shared_ptr<Settings> Settings::fromJsonFile(const std::string& filePath)
{
    // try and open the config file
    int configFileFd = open(filePath.c_str(), O_CLOEXEC | O_RDONLY);
    if (configFileFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open config file @ '%s'",
                         filePath.c_str());
        return nullptr;
    }

    // wrap the fd in a c++ file buf, it will close the fd on destruction
    __gnu_cxx::stdio_filebuf<char> fileBuf(configFileFd, std::ios::in);
    std::istream fileStream(&fileBuf);

    // create the parse builder
    Json::CharReaderBuilder builder;
    builder["allowComments"] = true;
    builder["collectComments"] = false;

    // parse the file
    Json::Value root;
    std::string errs;
    if (!Json::parseFromStream(builder, fileStream, &root, &errs))
    {
        AI_LOG_ERROR("failed to parse JSON config file @ '%s' due to - %s",
                     filePath.c_str(), errs.c_str());
        return nullptr;
    }

    return std::shared_ptr<Settings>(new Settings(root));
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the settings object with the default settings.
 *
 */
Settings::Settings()
    : mGpuGroupId(-1)
{
    setDefaults();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the settings source the data from the supplied JSON
 *  object.
 *
 */
Settings::Settings(const Json::Value& settings)
    : mGpuGroupId(-1)
{
    // defaults first
    setDefaults();

    // process dbus settings
    {
        Json::Value serviceName = Json::Path(".dbus.serviceName").resolve(settings);
        if (!serviceName.isNull())
        {
            if (!serviceName.isString())
                AI_LOG_ERROR("invalid dbus service name in JSON settings file");
            else
                mDBusServiceName = serviceName.asString();
        }

        Json::Value objectPath = Json::Path(".dbus.objectPath").resolve(settings);
        if (!objectPath.isNull())
        {
            if (!objectPath.isString())
                AI_LOG_ERROR("invalid dbus object path in JSON settings file");
            else
                mDBusObjectPath = objectPath.asString();
        }
    }

    // process the paths
    {
        Json::Value workspaceDir = Json::Path(".paths.workspaceDir").resolve(settings);
        if (!workspaceDir.isNull())
        {
            const std::list<std::string> workspaceDirs = getPathsFromJson(workspaceDir);
            if (!workspaceDirs.empty())
            {
                if (!AICommon::mkdirRecursive(workspaceDirs.front(), 01755))
                    AI_LOG_ERROR("invalid or inaccessible workspace path in JSON file");
                else
                    mWorkspaceDir = workspaceDirs.front();
            }
        }

        Json::Value persistentDir = Json::Path(".paths.persistentDir").resolve(settings);
        if (!persistentDir.isNull())
        {
            const std::list<std::string> persistentDirs = getPathsFromJson(persistentDir);
            if (!persistentDirs.empty())
            {
                if (!AICommon::mkdirRecursive(persistentDirs.front(), 0755))
                    AI_LOG_ERROR("invalid or inaccessible persistent path in JSON file");
                else
                    mPersistentDir = persistentDirs.front();
            }
        }

        Json::Value consoleSocketPath = Json::Path(".logging.consoleSocket").resolve(settings);
        if (!consoleSocketPath.isNull())
        {
            mConsoleSocketPath = getPathsFromJson(consoleSocketPath).front();
            // Socket will be created later by DobbyLogger
        }
    }

    // process the extra env variables
    {
        mExtraEnvVars =
            getEnvVarsFromJson(settings, Json::Path(".extraEnvVariables"));
    }

    // process the gpu settings
    {
        Json::Value gpuGroupId = Json::Path(".gpu.groupId").resolve(settings);
        if (!gpuGroupId.isNull())
        {
            if (gpuGroupId.isIntegral())
                mGpuGroupId = gpuGroupId.asInt();
            else if (gpuGroupId.isString())
                mGpuGroupId = getGroupId(gpuGroupId.asString());
            else
                AI_LOG_ERROR("invalid gpu group id value in JSON settings file");
        }

        // Nb: validation that the paths are actually dev nodes is done in the
        // DobbyTemplate code
        mGpuDevNodes = getGpuDevNodes(settings, Json::Path(".gpu.devNodes"));

        //
        mGpuExtraMounts = getGpuExtraMounts(settings, Json::Path(".gpu.extraMounts"));
    }

    // process the network settings
    {
        Json::Value externalIfaces = Json::Path(".network.externalInterfaces").resolve(settings);
        if (externalIfaces.isString())
        {
            mExternalInterfaces.insert(externalIfaces.asString());
        }
        else if (externalIfaces.isArray())
        {
            for (const Json::Value &iface : externalIfaces)
            {
                if (iface.isString())
                    mExternalInterfaces.insert(iface.asString());
                else
                    AI_LOG_ERROR("invalid entry in network externalInterfaces array in JSON settings file");
            }
        }
        else
        {
            AI_LOG_ERROR("invalid or missing network externalInterfaces in JSON settings file");
        }
    }

}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the default values for all settings.
 *
 */
void Settings::setDefaults()
{
    mDBusServiceName = DOBBY_SERVICE;
    mDBusObjectPath = DOBBY_OBJECT;
    mConsoleSocketPath = "/tmp/dobbyPty.sock";

#if defined(RDK)
    mWorkspaceDir = getPathFromEnv("AI_WORKSPACE_PATH", "/var/volatile/sky");
    mPersistentDir = getPathFromEnv("AI_PERSISTENT_PATH", "/opt/persistent/sky");
#else
    mWorkspaceDir = getPathFromEnv("AI_WORKSPACE_PATH", "/tmp/ai-workspace-fallback");
    mPersistentDir = getPathFromEnv("AI_PERSISTENT_PATH", "/tmp/ai-flash-fallback");
#endif

    mGpuGroupId = -1;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
void Settings::setDBusServiceName(const std::string& name)
{
    mDBusServiceName = name;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
std::string Settings::dbusServiceName() const
{
    return mDBusServiceName;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
void Settings::setDBusObjectPath(const std::string& path)
{
    mDBusObjectPath = path;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
std::string Settings::dbusObjectPath() const
{
    return mDBusObjectPath;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
std::string Settings::workspaceDir() const
{
    return mWorkspaceDir;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
std::string Settings::persistentDir() const
{
    return mPersistentDir;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
std::string Settings::consoleSocketPath() const
{
    return mConsoleSocketPath;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
std::map<std::string, std::string> Settings::extraEnvVariables() const
{
    return mExtraEnvVars;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
std::list<std::string> Settings::gpuDeviceNodes() const
{
    return mGpuDevNodes;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
int Settings::gpuGroupId() const
{
    return mGpuGroupId;
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
bool Settings::gpuHasExtraMounts() const
{
    return !mGpuExtraMounts.empty();
}

// -----------------------------------------------------------------------------
/**
 *  @brief
 *
 */
std::list<Settings::GpuExtraMount> Settings::gpuExtraMounts() const
{
    return mGpuExtraMounts;
}

 // -----------------------------------------------------------------------------
 /**
 *  @brief
 *
 */
std::set<std::string> Settings::externalInterfaces() const
{
    return mExternalInterfaces;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Debugging function to dump the settings to the log - info level.
 *
 *
 */
void Settings::dump(int aiLogLevel) const
{
    if (aiLogLevel < 0)
        aiLogLevel = AI_DEBUG_LEVEL_INFO;

    __AI_LOG_PRINTF(aiLogLevel, "settings.dbus.serviceName='%s'", mDBusServiceName.c_str());
    __AI_LOG_PRINTF(aiLogLevel, "settings.dbus.objectPath='%s'", mDBusObjectPath.c_str());

    __AI_LOG_PRINTF(aiLogLevel, "settings.paths.workspaceDir='%s'", mWorkspaceDir.c_str());
    __AI_LOG_PRINTF(aiLogLevel, "settings.paths.persistentDir='%s'", mPersistentDir.c_str());
    __AI_LOG_PRINTF(aiLogLevel, "settings.paths.consoleSocket='%s'", mConsoleSocketPath.c_str());

    unsigned int i = 0;
    for (const auto& envVar : mExtraEnvVars)
    {
        __AI_LOG_PRINTF(aiLogLevel, "settings.extraEnvVariables[%u]='%s=%s'",
                        i++, envVar.first.c_str(), envVar.second.c_str());
    }

    __AI_LOG_PRINTF(aiLogLevel, "settings.gpu.groupId=%d", mGpuGroupId);

    i = 0;
    for (const auto& devNode : mGpuDevNodes)
    {
        __AI_LOG_PRINTF(aiLogLevel, "settings.gpu.devNode[%u]='%s'",
                        i++, devNode.c_str());
    }

    i = 0;
    for (const auto& extraMount : mGpuExtraMounts)
    {
        std::ostringstream flags;
        for (const std::string& flag : extraMount.flags)
            flags << flag << ", ";

        __AI_LOG_PRINTF(aiLogLevel, "settings.gpu.extraMounts[%u]={ src='%s' dst='%s' type='%s' flags=[%s] }",
                        i++,
                        extraMount.source.c_str(), extraMount.target.c_str(),
                        extraMount.type.c_str(), flags.str().c_str());
    }

    i = 0;
    for (const auto& extIface : mExternalInterfaces)
    {
        __AI_LOG_PRINTF(aiLogLevel, "settings.network.externalInterfaces[%u]='%s'",
                        i++, extIface.c_str());
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Checks if path is a directory and has the given access flags.
 *
 *
 *
 *  @return true if the path is a directory and accessible.
 */
bool Settings::isDir(const std::string& path, int accessFlags) const
{
    struct stat buf;
    if (stat(path.c_str(), &buf) != 0)
    {
        return false;
    }

    if (!S_ISDIR(buf.st_mode))
    {
        return false;
    }

    if ((accessFlags != 0) && (access(path.c_str(), accessFlags) != 0))
    {
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the group id associated with the name.
 *
 *
 *
 *  @return the group id on success, -1 on error.
 */
int Settings::getGroupId(const std::string& name) const
{
    struct group grp, *result = nullptr;
    char groupBuf[256];

    if (getgrnam_r(name.c_str(), &grp, groupBuf, sizeof(groupBuf), &result) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to get gid of '%s' group", name.c_str());
        return -1;
    }
    else if (result == nullptr)
    {
        AI_LOG_ERROR("invalid results");
        return -1;
    }

    return result->gr_gid;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to get and validate a path from environment vars.
 *
 *  If the env var is not set or points to an non-existing directory then the
 *  function falls back to @a fallbackPath.
 *
 *  Either way this function guarantees that the returned string will point
 *  to a valid directory.
 *
 *  @return a string containing the path to the directory.
 */
std::string Settings::getPathFromEnv(const char* env,
                                     const char* fallbackPath) const
{
    AI_LOG_FN_ENTRY();

    // check for the platform environment var
    const char* envVar = getenv(env);
    if ((envVar == nullptr) || (envVar[0] == '\0'))
    {
        AI_LOG_INFO("missing '%s' environment var, falling back to '%s'",
                    env, fallbackPath);
    }
    else if (!isDir(envVar, R_OK | W_OK | X_OK))
    {
        AI_LOG_WARN("failed to access dir @ '%s', falling back to '%s'",
                    envVar, fallbackPath);
    }
    else
    {
        AI_LOG_FN_EXIT();
        return std::string(envVar);
    }

    // if we're arrived here then we need to use the fallback path
    if ((mkdir(fallbackPath, 0755) != 0) && (errno != EEXIST))
    {
        AI_LOG_SYS_FATAL(errno, "failed to create fallback workspace path @ '%s'",
                         fallbackPath);
    }

    AI_LOG_FN_EXIT();
    return fallbackPath;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to read a path from the JSON object.
 *
 *  The path(s) are expanded using the wordexp() function, meaning that glob
 *  and environment variable expansion are performed on the string stored in
 *  the json object.
 *
 *  @return a list of expanded paths in the json.
 */
std::list<std::string> Settings::getPathsFromJson(const Json::Value& value) const
{
    // sanity check the json value is a string
    if (!value.isString())
    {
        AI_LOG_ERROR("JSON value in settings file is not a string");
        return std::list<std::string>();
    }

    // perform path expansion (without the $(command) processing)
    wordexp_t exp;
    bzero(&exp, sizeof(exp));

    int rc = wordexp(value.asCString(), &exp, WRDE_NOCMD | WRDE_UNDEF);
    if (rc != 0)
    {
        AI_LOG_ERROR("failed to expand settings path string '%s'",
                     value.asCString());
        return std::list<std::string>();
    }

    // copy all expanded paths back
    std::list<std::string> paths;
    for (int i = 0; i < exp.we_wordc; i++)
    {
        paths.emplace_back(exp.we_wordv[i]);
    }

    wordfree(&exp);

    return paths;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns a map of strings to strings as read from the JSON.
 *
 *  This expects the json value to contain an array of strings, each string in
 *  the array should be formatted as a "<NAME>=<VALUE>" and follow the same
 *  rules as for standard environment variables.
 *
 *
 *  @return a map of key value environment variable pairs.
 */
std::map<std::string, std::string> Settings::getEnvVarsFromJson(const Json::Value& root,
                                                                const Json::Path& path) const
{
    // get the string value from the json
    const Json::Value &envVars = Json::Path(path).resolve(root);
    if (envVars.isNull())
    {
        // it's not an error if the value does not exist in the JSON
        return std::map<std::string, std::string>();
    }
    else if (!envVars.isArray())
    {
        AI_LOG_ERROR("JSON value in settings file is not an array (of strings)");
        return std::map<std::string, std::string>();
    }

    // simple regex for checking and splitting the string
    const std::regex envVarRegex("(\\w+)=(\\w+)", std::regex_constants::ECMAScript |
                                                     std::regex_constants::icase);

    // process each entry
    std::map<std::string, std::string> result;
    for (const Json::Value &envVar : envVars)
    {
        // verify the value in an array is a string
        if (!envVar.isString())
        {
            AI_LOG_ERROR("invalid JSON value in extra env var array in settings file");
            return std::map<std::string, std::string>();
        }

        // check and split the string to key value pairs
        std::smatch matches;
        const std::string envVarStr = envVar.asString();
        if (!std::regex_match(envVarStr, matches, envVarRegex) ||
            (matches.size() != 3))
        {
            AI_LOG_ERROR("invalid env var string '%s' in settings file",
                         envVar.asCString());
            return std::map<std::string, std::string>();
        }

        // add to the map
        result[matches[1].str()] = matches[2].str();
    }

    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to read the list of GPU device nodes that are needed for
 *  apps.
 *
 *
 *
 *  @return
 */
std::list<std::string> Settings::getGpuDevNodes(const Json::Value& root,
                                                const Json::Path& path) const
{
    // get the array value from the json
    const Json::Value &devNodes = Json::Path(path).resolve(root);
    if (devNodes.isNull())
    {
        // it's not an error if the value does not exist in the JSON
        return std::list<std::string>();
    }
    else if (!devNodes.isArray())
    {
        AI_LOG_ERROR("JSON value in settings file is not an array (of dev nodes)");
        return std::list<std::string>();
    }

    std::list<std::string> result;
    for (const Json::Value &devNode : devNodes)
    {
        // verify the value in an array is a string
        if (!devNode.isString())
        {
            AI_LOG_ERROR("invalid JSON value in dev nodes array in settings file");
            return std::list<std::string>();
        }

        // append any expanded files paths to the list
        std::list<std::string> files = getPathsFromJson(devNode);
        if (!files.empty())
        {
            result.splice(result.end(), files);
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to read the mount JSON structure(s) from the object.
 *
 *
 *
 *  @return
 */
std::list<Settings::GpuExtraMount> Settings::getGpuExtraMounts(const Json::Value& root,
                                                               const Json::Path& path) const
{
    // get the string value from the json
    const Json::Value &extraMounts = Json::Path(path).resolve(root);
    if (extraMounts.isNull())
    {
        // it's not an error if the value does not exist in the JSON
        return std::list<GpuExtraMount>();
    }
    else if (!extraMounts.isArray())
    {
        AI_LOG_ERROR("JSON value in settings file is not an array (of mount objects)");
        return std::list<GpuExtraMount>();
    }

    // process each entry
    std::list<GpuExtraMount> result;
    for (const Json::Value &extraMount : extraMounts)
    {
        // verify the value in an array is a string
        if (!extraMount.isObject())
        {
            AI_LOG_ERROR("invalid JSON value in extra gpu mount var array in settings file");
            return std::list<GpuExtraMount>();
        }

        // add the extra mount to the list
        GpuExtraMount mount;
        if (processMountObject(extraMount, &mount))
        {
            result.emplace_back(mount);
        }
    }

    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes a json 'mount' object.
 *
 *
 *  @return
 */
bool Settings::processMountObject(const Json::Value& value, GpuExtraMount* mount) const
{
    const Json::Value& source = value["source"];
    const Json::Value& destination = value["destination"];
    const Json::Value& type = value["type"];
    const Json::Value& options = value["options"];

    if (!source.isString() || !destination.isString() || !type.isString())
    {
        AI_LOG_ERROR("invalid 'source', 'destination' or 'type' JSON field");
        return false;
    }

    if (!options.isNull() && !options.isArray())
    {
        AI_LOG_ERROR("invalid 'options' JSON field");
        return false;
    }

    mount->source = source.asString();
    mount->target = destination.asString();
    mount->type = type.asString();
    mount->flags.clear();


    // we only support the standard flags; bind, ro, sync, nosuid, noexec, etc
    static const std::set<std::string> mountFlags =
        {
            "rbind", "bind", "silent", "ro", "sync", "nosuid", "dirsync",
            "nodiratime", "relatime", "noexec", "nodev", "noatime", "strictatime"
        };

    // convert the mount flags
    if (!options.isNull())
    {
        for (const Json::Value &option : options)
        {
            if (!option.isString())
            {
                AI_LOG_ERROR("invalid JSON value in gpu mount options array");
                return false;
            }

            if (mountFlags.count(option.asString()) <= 0)
            {
                AI_LOG_ERROR("unknown mount option '%s' in settings JSON",
                             option.asCString());
                return false;
            }

            mount->flags.insert(option.asString());
        }
    }

    return true;
}

