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
 * File:   DobbyConfig.cpp
 *
 */

#include "DobbyConfig.h"

#include <atomic>
#include <glob.h>
#include <sys/stat.h>
#include <fstream>
#include <FileUtilities.h>

#define OCI_VERSION_CURRENT         "1.0.2"         // currently used version of OCI in bundles
#define OCI_VERSION_CURRENT_DOBBY   "1.0.2-dobby"   // currently used version of extended OCI in bundles


// -----------------------------------------------------------------------------
/**
 *  @brief Takes a list of glob patterns corresponding to dev node paths and
 *  returns a list of structs with their details.
 *
 *  If the glob pattern doesn't match a device node then it is ignored, this
 *  is not an error.
 *
 *  @param[in]  devNodes    The list of dev nodes paths (or glob patterns).
 *
 */
std::list<DobbyConfig::DevNode> DobbyConfig::scanDevNodes(const std::list<std::string> &devNodes)
{
    std::list<DobbyConfig::DevNode> nodes;

    // sanity check any dev nodes to add
    if (devNodes.empty())
    {
        return nodes;
    }


    // create a glob structure to hold the list of dev nodes
    glob_t devNodeBuf;
    int globFlags = GLOB_NOSORT;
    for (const std::string& devNode : devNodes)
    {
        glob(devNode.c_str(), globFlags, nullptr, &devNodeBuf);
        globFlags |= GLOB_APPEND;
    }

    if (devNodeBuf.gl_pathc == 0)
    {
        AI_LOG_ERROR("no dev nodes found despite some being listed in the "
                    "JSON config file");
        globfree(&devNodeBuf);
        return nodes;
    }

    struct stat buf;
    std::ostringstream devNodesStream;
    std::ostringstream devNodesPermStream;

    // loop through all the found dev nodes
    for (size_t i = 0; i < devNodeBuf.gl_pathc; ++i)
    {
        const char *devNode = devNodeBuf.gl_pathv[i];
        if (!devNode)
        {
            AI_LOG_ERROR("invalid glob string");
            continue;
        }

        if (stat(devNode, &buf) != 0)
        {
            AI_LOG_SYS_WARN(errno, "failed to stat dev node @ '%s'", devNode);
            continue;
        }

        // Dev Nodes not character special files on vSTB so don't perform check
#if !defined(__i686__)
        if( !S_ISCHR(buf.st_mode) )
            continue;
#endif

        AI_LOG_INFO("found dev node '%s'", devNode);

        nodes.emplace_back(DevNode{ devNode,
                                    major(buf.st_rdev),
                                    minor(buf.st_rdev),
                                    (buf.st_mode & 0666) });
    }

    globfree(&devNodeBuf);

    return nodes;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Public api to allow for adding additional mounts
 *
 *  This can only obviously be called before the config file is persisted to
 *  disk.
 *
 *  @param[in]  source          The mount source
 *  @param[in]  destination     The mount destination
 *  @param[in]  type            The file system type of the mount
 *  @param[in]  mountFlags      The mount flags
 *  @param[in]  mountOptions    The mount options (mount(2) data parameter)
 *
 *  @return true if the mount point was added, otherwise false.
 */
bool DobbyConfig::addMount(const std::string& source,
                           const std::string& destination,
                           const std::string& type,
                           unsigned long mountFlags /*= 0*/,
                           const std::list<std::string>& mountOptions /*= std::list<std::string>()*/)
{
    std::lock_guard<std::mutex> locker(mLock);

    AI_LOG_FN_ENTRY();

    std::shared_ptr<rt_dobby_schema> cfg = config();
    if (cfg == nullptr)
    {
        AI_LOG_ERROR("Invalid bundle config");
        return false;
    }

    // iterate through the mounts to check that the mount doesn't already exist
    for (size_t i=0; i < cfg->mounts_len; i++)
    {
        if (!strcmp(cfg->mounts[i]->source, source.c_str()) && !strcmp(cfg->mounts[i]->destination, destination.c_str()))
        {
            AI_LOG_WARN("mount from source %s to dest %s already exists", source.c_str(), destination.c_str());
            return true;
        }
    }

    std::list<std::string> mountOptionsFinal(mountOptions);

    // we only support the standard flags; bind, ro, sync, nosuid, noexec, etc
    static const std::vector<std::pair<unsigned long, std::string>> mountFlagsNames =
    {
        {   MS_BIND | MS_REC,   "rbind"         },
        {   MS_BIND,            "bind"          },
        {   MS_SILENT,          "silent"        },
        {   MS_RDONLY,          "ro"            },
        {   MS_SYNCHRONOUS,     "sync"          },
        {   MS_NOSUID,          "nosuid"        },
        {   MS_DIRSYNC,         "dirsync"       },
        {   MS_NODIRATIME,      "nodiratime"    },
        {   MS_RELATIME,        "relatime"      },
        {   MS_NOEXEC,          "noexec"        },
        {   MS_NODEV,           "nodev"         },
        {   MS_NOATIME,         "noatime"       },
        {   MS_STRICTATIME,     "strictatime"   },
    };

    // convert the mount flags to their string equivalents
    for (const auto &entry : mountFlagsNames)
    {
        const unsigned long mountFlag = entry.first;
        if ((mountFlag & mountFlags) == mountFlag)
        {
            // add options to options list to be written to the bundle config
            mountOptionsFinal.emplace_back(entry.second);

            // clear the mount flags bit such that we can display a warning
            // if the caller supplies a flag we don't support
            mountFlags &= ~mountFlag;
        }
    }

    // if there was a mount flag we didn't support display a warning
    if (mountFlags != 0)
    {
        AI_LOG_WARN("unsupported mount flag(s) 0x%04lx", mountFlags);
    }

    // allocate memory for mount
    rt_defs_mount *newMount = (rt_defs_mount*)calloc(1, sizeof(rt_defs_mount));
    newMount->options_len = mountOptionsFinal.size();
    newMount->options = (char**)calloc(newMount->options_len, sizeof(char*));

    // add mount options to bundle config
    int i = 0;
    for (const std::string &mountOption : mountOptionsFinal)
    {
        newMount->options[i] = strdup(mountOption.c_str());
        i++;
    }

    newMount->destination = strdup(destination.c_str());
    newMount->type = strdup(type.c_str());
    newMount->source = strdup(source.c_str());

    // allocate memory for new mount and place it in the config struct
    cfg->mounts_len++;
    cfg->mounts = (rt_defs_mount**)realloc(cfg->mounts, sizeof(rt_defs_mount*) * cfg->mounts_len);
    cfg->mounts[cfg->mounts_len-1] = newMount;

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Public api to allow for adding additional env variables
 *
 *  This can only obviously be called before the config file is persisted to
 *  disk.
 *
 *  @param[in]  envVar      The environment variable to set
 *
 *  @return true if the env var was added, otherwise false.
 */
bool DobbyConfig::addEnvironmentVar(const std::string& envVar)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    std::shared_ptr<rt_dobby_schema> cfg = config();
    if (cfg == nullptr)
    {
        AI_LOG_ERROR("Invalid bundle config");
        return false;
    }

    // check if env var already exists in config
    for (size_t i = 0; i < cfg->process->env_len; ++i)
    {
        if (0 == strcmp(cfg->process->env[i], envVar.c_str()))
        {
            return true;
        }
    }

    // Increase the number of environment variables
    cfg->process->env_len += 1;

    // Update env var in OCI bundle config
    cfg->process->env = (char**)realloc(cfg->process->env, sizeof(char*) * cfg->process->env_len);
    cfg->process->env[cfg->process->env_len-1] = strdup(envVar.c_str());

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  Changes the startup command for the container to a custom command.
 *
 *  Will automatically add DobbyInit to run the process to ensure sub-reaping
 *  is handled properly
 *
 *  @param[in]  command     The command to run (including arguments/params)
 */
bool DobbyConfig::changeProcessArgs(const std::string& command)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    std::shared_ptr<rt_dobby_schema> cfg = config();
    if (cfg == nullptr)
    {
        AI_LOG_ERROR("Invalid bundle config");
        return false;
    }

    AI_LOG_DEBUG("Adding custom command %s to config", command.c_str());
    std::vector<std::string> cmd;

    // Always use DobbyInit
    cmd.push_back("/usr/libexec/DobbyInit");

    // Insert space delimited command string into a vector
    std::stringstream ss_cmd(command);
    std::string tmp;
    while (getline(ss_cmd, tmp, ' '))
    {
        cmd.push_back(tmp);
    }

    // Add the args to the config
    cfg->process->args = (char **)realloc(cfg->process->args, sizeof(char *) * cmd.size());
    cfg->process->args_len = cmd.size();

    for (size_t i = 0; i < cmd.size(); i++)
    {
        cfg->process->args[i] = strdup(cmd[i].c_str());
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  Prints startup command for the container.
 */
void DobbyConfig::printCommand() const
{
    std::shared_ptr<rt_dobby_schema> cfg = config();
    if (cfg == nullptr)
    {
        AI_LOG_ERROR("Invalid bundle config");
        return;
    }

    std::stringstream ss;
    ss << "command: '";
    for (size_t i = 0; i < cfg->process->args_len; i++)
    {
        ss << " " << cfg->process->args[i];
    }
    ss << "'";
    AI_LOG_DEBUG("%s", ss.str().c_str());
}

// -----------------------------------------------------------------------------
/**
 *  @brief Enables strace for the container
 *
 *  @param[in]  logsDir      Directory to which strace logs will be written
 *
 *  @return true if strace was sucessfully enabled for the container, otherwise false.
 */
bool DobbyConfig::enableSTrace(const std::string& logsDir)
{
    AI_LOG_FN_ENTRY();

    std::shared_ptr<rt_dobby_schema> cfg = config();
    if (cfg == nullptr)
    {
        AI_LOG_ERROR("Invalid bundle config");
        return false;
    }

    if (!addMount(logsDir, logsDir, "bind", 0, { "bind", "nosuid", "nodev" }))
    {
        AI_LOG_ERROR("Failed to add strace logs mount");
        return false;
    }

    const std::string logsPath = logsDir + "/strace-" + cfg->hostname + ".txt";
    AI_LOG_INFO("Enabling strace for '%s', logs in '%s'", cfg->hostname, logsPath.c_str());

    {
        // add  "/usr/bin/strace -o logs -f " before the rest of command args
        const std::vector<std::string> params{"/usr/bin/strace", "-o", logsPath, "-f"};

        std::lock_guard<std::mutex> locker(mLock);
        size_t new_args_len = cfg->process->args_len + params.size();
        char** new_args = (char **)realloc(cfg->process->args, sizeof(char *) * new_args_len);

        // make place for the new args in front of the args list
        for (size_t i = 0; i < cfg->process->args_len; ++i)
        {
            new_args[new_args_len - 1 - i] = new_args[cfg->process->args_len - 1 - i];
        }

        // copy the new params at the start of new args list
        for (size_t i = 0; i < params.size(); ++i)
        {
            new_args[i] = strdup(params[i].c_str());
        }

        cfg->process->args_len = new_args_len;
        cfg->process->args = new_args;
    }

    printCommand();

    AI_LOG_FN_EXIT();

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  Adds a mount into the container for a westeros socket with the correct
 *  permissions at /tmp/westeros
 *
 *  Sets WAYLAND_DISPLAY and XDG_RUNTIME_DIR environment variables
 *  to ensure container actually uses the display
 *
 *  @param[in]  socketPath     Path to westeros socket on host
 */
bool DobbyConfig::addWesterosMount(const std::string& socketPath)
{
    AI_LOG_FN_ENTRY();

    std::shared_ptr<rt_dobby_schema> cfg = config();
    if (cfg == nullptr)
    {
        AI_LOG_ERROR("Invalid bundle config");
        return false;
    }

    AI_LOG_DEBUG("Adding westeros socket bind mount %s -> /tmp/westeros to config", socketPath.c_str());

    // Mount options
    std::list<std::string> mountOptions = {
        "bind",
        "rw",
        "nosuid",
        "nodev",
        "noexec"
    };

    if (!addMount(socketPath, "/tmp/westeros", "bind", 0, mountOptions))
    {
        AI_LOG_ERROR("Failed to add Westeros mount");
        return false;
    }

    if (!addEnvironmentVar("WAYLAND_DISPLAY=westeros") || !addEnvironmentVar("XDG_RUNTIME_DIR=/tmp"))
    {
        AI_LOG_ERROR("Failed to set westeros environment variables");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Get OCI bundle config json as string
 *
 *  @return OCI config json
 */
const std::string DobbyConfig::configJson() const
{
    std::lock_guard<std::mutex> locker(mLock);

    if (!isValid())
    {
        AI_LOG_ERROR("invalid config");
        return std::string();
    }

    parser_error err;
    std::shared_ptr<rt_dobby_schema> cfg = config();
    if (cfg == nullptr)
    {
        AI_LOG_ERROR("Invalid bundle config");
        return std::string();
    }

    char *json_buf = rt_dobby_schema_generate_json(cfg.get(), 0, &err);
    if (json_buf == nullptr || err)
    {
        AI_LOG_ERROR("Failed to generate json from container config with code '%s'", err);
        return std::string();
    }

    std::string configJson(json_buf);
    free(json_buf);

    return configJson;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Writes bundle config string to a file.
 *
 *  @param[in]  filePath        The name and path to the file to write to.
 *
 *  @return true if the file was written correctly, otherwise false.
 */
bool DobbyConfig::writeConfigJson(const std::string& filePath) const
{
    if (!isValid())
    {
        AI_LOG_ERROR("invalid config");
        return false;
    }

    return this->writeConfigJsonImpl(filePath);
}

bool DobbyConfig::writeConfigJsonImpl(const std::string& filePath) const
{
    std::lock_guard<std::mutex> locker(mLock);

    AI_LOG_FN_ENTRY();

    parser_error err;
    std::shared_ptr<rt_dobby_schema> cfg = config();
    if (cfg == nullptr)
    {
        AI_LOG_ERROR_EXIT("Invalid bundle config");
        return false;
    }

    // generate json for the new config
    FILE *file = fopen(filePath.c_str(), "w");
    if (file == nullptr)
    {
        AI_LOG_ERROR_EXIT("Error opening file '%s'", filePath.c_str());
        return false;
    }

    char *json_buf = rt_dobby_schema_generate_json(cfg.get(), 0, &err);
    if (json_buf == nullptr || err)
    {
        AI_LOG_ERROR_EXIT("Failed to generate json from container config with code '%s'", err);
        fclose(file);
        if (json_buf != nullptr)
        {
            free(json_buf);
        }
        return false;
    }

    // write the new config json to a file
    if (fputs(json_buf, file) == EOF)
    {
        AI_LOG_ERROR_EXIT("Failed to write config file.");
        fclose(file);
        free(json_buf);
        return false;
    }
    fclose(file);
    free(json_buf);

    // set file permissions
    chmod(filePath.c_str(), S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Checks a hook for Dobby plugin launcher entries
 *
 *  @param[in]  hook        pointer to a hook in the bundle config
 *  @param[in]  len         number of entries in the hook
 *
 *  @return true if unexpected Dobby plugin launcher entry found, false if not
 */
bool DobbyConfig::findPluginLauncherHookEntry(rt_defs_hook** hook, int len)
{
    if (hook == nullptr)
    {
        return false;
    }

    // iterate through all hook entries
    for (int i=0; i < len; i++)
    {
        if (!strcmp(hook[i]->args[0], "DobbyPluginLauncher"))
        {
            return true;
        }
    }

    return false;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Populates a hook entry structure with DobbyPluginLauncher data
 *
 *  @param[in]  entry           pointer to hook entry to populate
 *  @param[in]  name            hook name
 *  @param[in]  configPath      path to the config file
 */
void DobbyConfig::setPluginHookEntry(rt_defs_hook* entry, const std::string& name, const std::string& configPath)
{
    std::string verbosity;

    // match plugin launcher verbosity to the daemon's
    switch(__ai_debug_log_level) {
        case AI_DEBUG_LEVEL_DEBUG:
            verbosity = "-vv";
            break;
        case AI_DEBUG_LEVEL_INFO:
            verbosity = "-v";
            break;
        default:
            verbosity = "";
            break;
    }

    std::vector<std::string> args = {
        "DobbyPluginLauncher",
        "-h",
        name,
        "-c",
        configPath
    };

    // add verbosity level if needed
    if (!verbosity.empty())
    {
        args.emplace_back(verbosity);
    }

    entry->path = strdup(PLUGINLAUNCHER_PATH);
    entry->args_len = args.size();;
    entry->args = (char**)calloc(entry->args_len, sizeof(char*));

    for (size_t i = 0; i < args.size(); i++)
    {
        entry->args[i] = strdup(args[i].c_str());
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds OCI hooks to the config
 *
 *  @param[in]  cfg             libocispec config structure instance
 *  @param[in]  bundlePath      path to the container bundle
 *
 *  @return true if hooks added successfully, false if not
 */
void DobbyConfig::addPluginLauncherHooks(std::shared_ptr<rt_dobby_schema> cfg, const std::string& bundlePath)
{
    if (cfg->hooks == nullptr)
    {
        cfg->hooks = (rt_dobby_schema_hooks*)calloc(1, sizeof(rt_dobby_schema_hooks));
    }

    // createRuntime, createContainer, poststart and poststop hook paths must
    // resolve in the runtime namespace - config is in bundle
    std::string configPath = bundlePath + "/config.json";

    // populate createRuntime hook with DobbyPluginLauncher args
    rt_defs_hook *createRuntimeEntry = (rt_defs_hook*)calloc(1, sizeof(rt_defs_hook));
    setPluginHookEntry(createRuntimeEntry, "createRuntime", configPath);
    cfg->hooks->create_runtime = (rt_defs_hook**)realloc(cfg->hooks->create_runtime, sizeof(rt_defs_hook*) * ++cfg->hooks->create_runtime_len);
    cfg->hooks->create_runtime[cfg->hooks->create_runtime_len-1] = createRuntimeEntry;

    // populate createContainer hook with DobbyPluginLauncher args
    rt_defs_hook *createContainerEntry = (rt_defs_hook*)calloc(1, sizeof(rt_defs_hook));
    setPluginHookEntry(createContainerEntry, "createContainer", configPath);
    cfg->hooks->create_container = (rt_defs_hook**)realloc(cfg->hooks->create_container, sizeof(rt_defs_hook*) * ++cfg->hooks->create_container_len);
    cfg->hooks->create_container[cfg->hooks->create_container_len-1] = createContainerEntry;

    // populate poststart hook with DobbyPluginLauncher args
    rt_defs_hook *poststartEntry = (rt_defs_hook*)calloc(1, sizeof(rt_defs_hook));
    setPluginHookEntry(poststartEntry, "poststart", configPath);
    cfg->hooks->poststart = (rt_defs_hook**)realloc(cfg->hooks->poststart, sizeof(rt_defs_hook*) * ++cfg->hooks->poststart_len);
    cfg->hooks->poststart[cfg->hooks->poststart_len-1] = poststartEntry;

    // populate poststop hook with DobbyPluginLauncher args
    rt_defs_hook *poststopEntry = (rt_defs_hook*)calloc(1, sizeof(rt_defs_hook));
    setPluginHookEntry(poststopEntry, "poststop", configPath);
    cfg->hooks->poststop = (rt_defs_hook**)realloc(cfg->hooks->poststop, sizeof(rt_defs_hook*) * ++cfg->hooks->poststop_len);
    cfg->hooks->poststop[cfg->hooks->poststop_len-1] = poststopEntry;

#ifdef USE_STARTCONTAINER_HOOK
    // startContainer hook paths must resolve in the container namespace,
    // config is in container rootdir
    configPath = "/tmp/config.json";

    // populate startContainer hook with DobbyPluginLauncher args
    rt_defs_hook *startContainerEntry = (rt_defs_hook*)calloc(1, sizeof(rt_defs_hook));
    setPluginHookEntry(startContainerEntry, "startContainer", configPath);
    cfg->hooks->start_container = (rt_defs_hook**)realloc(cfg->hooks->start_container, sizeof(rt_defs_hook*) * ++cfg->hooks->start_container_len);
    cfg->hooks->start_container[cfg->hooks->start_container_len-1] = startContainerEntry;
#endif
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the container hostname to the container ID
 *
 *  @param[in]  id              container identifier
 *  @param[in]  cfg             libocispec config structure instance
 *  @param[in]  bundlePath      path to the container bundle
 */
bool DobbyConfig::setHostnameToContainerId(const ContainerId& id, std::shared_ptr<rt_dobby_schema> cfg, const std::string& bundlePath)
{
    // change hostname to container id only if necessary
    if (!strcmp(cfg->hostname, id.c_str()))
    {
        return true;
    }

    free(cfg->hostname);
    cfg->hostname = strdup(id.c_str());

    // write the new config.json to a file
    if (!writeConfigJsonImpl(bundlePath + "/config.json"))
    {
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Convert the input config.json into an OCI compliant bundle config
 *         that adds support for DobbyPluginLauncher to work with rdkPlugins.
 *
 *  @param[in]  id              container identifier
 *  @param[in]  cfg             libocispec config structure instance
 *  @param[in]  bundlePath      path to the container bundle
 */
bool DobbyConfig::updateBundleConfig(const ContainerId& id, std::shared_ptr<rt_dobby_schema> cfg, const std::string& bundlePath)
{
    // update ociVersion to latest supported OCI version
    cfg->oci_version = strdup(OCI_VERSION_CURRENT);

    // change hostname to container id
    cfg->hostname = strdup(id.c_str());

    // if there are any rdk plugins, set up DobbyPluginLauncher in config
    if (cfg->rdk_plugins && cfg->rdk_plugins->plugins_count)
    {
#ifdef USE_STARTCONTAINER_HOOK
        // bindmount DobbyPluginLauncher to container
        if(!addMount(PLUGINLAUNCHER_PATH, PLUGINLAUNCHER_PATH, "bind", 0,
                        { "bind", "ro", "nosuid", "nodev" }))
        {
            return false;
        }

        // bindmount the config file to container
        if(!addMount(bundlePath + "/config.json", "/tmp/config.json", "bind", 0,
                        { "bind", "ro", "nosuid", "nodev" }))
        {
            return false;
        }
#endif

        // set up OCI hooks to use DobbyPluginLauncher
        addPluginLauncherHooks(cfg, bundlePath);
    }

    // release legacyPlugin struct
    if (cfg->legacy_plugins != nullptr)
    {
        free_rt_defs_plugins_legacy_plugins(cfg->legacy_plugins);
        cfg->legacy_plugins = nullptr;
    }

    // write the new config.json to a file
    if (!writeConfigJsonImpl(bundlePath + "/config.json"))
    {
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Check if apparmor profile is loaded
 *
 *  @param[in]  profile  The name of apparmor profile.
 *
 *  @return true if the apparmor profile was loaded in kernel space, otherwise false.
 */

bool DobbyConfig::isApparmorProfileLoaded(const char *profile) const
{
    FILE *fp = nullptr;
    char line[256];
    bool status = false;

    fp = fopen("/sys/kernel/security/apparmor/profiles", "r");

    if (fp == nullptr)
    {
        AI_LOG_ERROR("/sys/kernel/security/apparmor/profiles open failed");
        return status;
    }

    while (fgets(line, sizeof(line), fp))
    {
        if (strstr(line, profile))
        {
            status = true;
            AI_LOG_INFO("Apparmor profile [%s] is loaded", profile);
            break;
        }
    }

    fclose(fp);
    return status;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Set apparmor profile in config.
 *
 *  Checks if profile from config is loaded. If not uses default profile if it
 *  is loaded.
 *
 *  @param[in]  defaultProfileName  The name of the default apparmor profile.
 */
void DobbyConfig::setApparmorProfile(const std::string& defaultProfileName)
{
    std::shared_ptr<rt_dobby_schema> cfg = config();
    if (cfg == nullptr)
    {
        AI_LOG_ERROR("Invalid bundle config");
        return;
    }

    bool status = false;

    if (cfg->process->apparmor_profile)
    {
        status = isApparmorProfileLoaded(cfg->process->apparmor_profile);
    }

    if (!status)
    {
        cfg->process->apparmor_profile = strdup(defaultProfileName.c_str());
        status = isApparmorProfileLoaded(cfg->process->apparmor_profile);
    }

    if (!status)
    {
        cfg->process->apparmor_profile = nullptr;
        AI_LOG_INFO("No apparmor profile is loaded");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Set cgroup pids limit.
 *
 *  Limits the number of processes that containered app can create.
 *
 *  @see https://www.kernel.org/doc/Documentation/cgroup-v1/pids.txt
 *
 *  @param[in]  limit  limit of pids
 */
void DobbyConfig::setPidsLimit(int limit)
{
    std::shared_ptr<rt_dobby_schema> cfg = config();
    if (cfg == nullptr)
    {
        AI_LOG_ERROR("Invalid bundle config");
        return;
    }

    // set pid limit only if it's not set already, we do not override pid limit set in config
    rt_config_linux_resources_pids* pids = cfg->linux->resources->pids;
    if (pids == nullptr)
    {
        pids = (rt_config_linux_resources_pids*)calloc(1, sizeof(rt_config_linux_resources_pids));
        pids->limit = limit;
        pids->limit_present = true;

        cfg->linux->resources->pids = pids;
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Convert the input config.json into an OCI compliant bundle config
 *         that adds support for DobbyPluginLauncher to work with rdkPlugins.
 *
 *  @param[in]  id              container identifier
 *  @param[in]  cfg             libocispec config structure instance
 *  @param[in]  bundlePath      path to the container bundle
 *
 */
bool DobbyConfig::convertToCompliant(const ContainerId& id, std::shared_ptr<rt_dobby_schema> cfg, const std::string& bundlePath)
{
    AI_LOG_FN_ENTRY();

    parser_error err;

    if (cfg == nullptr)
    {
        AI_LOG_ERROR_EXIT("Invalid bundle config");
        return false;
    }

    // check config version and process as needed
    if (!strcmp(cfg->oci_version, OCI_VERSION_CURRENT_DOBBY))
    {
        // Make a backup of the original config, useful for checking whether a new config
        // is available.
        std::ifstream srcCfg(bundlePath + "/config.json", std::ios::binary);
        std::ofstream dstCfg(bundlePath + "/config-dobby.json", std::ios::binary);
        dstCfg << srcCfg.rdbuf();

        if (!updateBundleConfig(id, cfg, bundlePath))
        {
            return false;
        }
    }
    else if (!strcmp(cfg->oci_version, OCI_VERSION_CURRENT))
    {
        // if rdkPlugins are not present, no need to do anything
        if (cfg->rdk_plugins == nullptr)
        {
            return true;
        }

        rt_dobby_schema_hooks *hooks = cfg->hooks;
        // check to see that all OCI hooks have DobbyPluginLauncher set up
        if (hooks == nullptr ||
            !(findPluginLauncherHookEntry(hooks->create_runtime, hooks->create_runtime_len) &&
              findPluginLauncherHookEntry(hooks->create_container, hooks->create_container_len) &&
#ifdef USE_STARTCONTAINER_HOOK
              findPluginLauncherHookEntry(hooks->start_container, hooks->start_container_len) &&
#endif
              findPluginLauncherHookEntry(hooks->poststart, hooks->poststart_len) &&
              findPluginLauncherHookEntry(hooks->poststop, hooks->poststop_len)))
        {
            const std::string extConfigPath = bundlePath + "/config-dobby.json";
            AI_LOG_INFO("rdkPlugins present but hooks aren't set up correctly, attempting "
                        "to parse from config-dobby.json instead");

            // check if config-dobby.json exists
            if (access(extConfigPath.c_str(), F_OK) == -1)
            {
                AI_LOG_ERROR_EXIT("Couldn't find config-dobby.json in bundle directory");
                return false;
            }

            // pick up and deserialise config-dobby.json to parse instead
            cfg.reset(rt_dobby_schema_parse_file(extConfigPath.c_str(), nullptr, &err),
                      free_rt_dobby_schema);
            if (cfg == nullptr || err)
            {
                AI_LOG_ERROR_EXIT("Failed to parse bundle config '%s', err '%s'",
                                  extConfigPath.c_str(), err);
                return false;
            }

            // now, transform the config to set it up for DobbyPluginLauncher
            if (!updateBundleConfig(id, cfg, bundlePath))
            {
                return false;
            }
        }
        else
        {
            // hooks are set up just fine, just need to update the hostname if necessary
            if (!setHostnameToContainerId(id, cfg, bundlePath))
            {
                AI_LOG_ERROR_EXIT("Failed to set container hostname");
                return false;
            }
            return true;
        }
    }
    else
    {
        // OCI version is not OCI_VERSION_CURRENT or OCI_VERSION_CURRENT_DOBBY, no plugin support
        AI_LOG_INFO("Launching container with OCI container version '%s',"
                    "plugins are not used", cfg->oci_version);
        return true;
    }

    AI_LOG_FN_EXIT();
    return true;
}
