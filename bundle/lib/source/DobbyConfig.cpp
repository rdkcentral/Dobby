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
 * File:   DobbyConfig.cpp
 *
 * Copyright (C) BSKYB 2016+
 */

#include "DobbyConfig.h"

#include <glob.h>
#include <sys/stat.h>


#define OCI_VERSION_CURRENT         "1.0.2"         // currently used version of OCI in bundles
#define OCI_VERSION_CURRENT_DOBBY   "1.0.2-dobby"   // currently used version of extended OCI in bundles

std::mutex DobbyConfig::mGpuDevNodesLock;
std::string DobbyConfig::mGpuDevNodes;
std::string DobbyConfig::mGpuDevNodesPerms;
bool DobbyConfig::mInitialisedGpuDevNodes = false;

/**
 *  @brief Map of RDK plugins currently in development.
 *
 *  Contains RDK plugins with matching Dobby syshooks used until
 *  development is finished. If an RDK plugin is in development,
 *  (i.e. in this map), its respective syshooks are used instead.
 *
 *  TODO: Remove entry when RDK plugin development is finalised.
 */
const std::map<std::string, std::list<std::string>> DobbyConfig::mRdkPluginsInDevelopment =
{
    { RDK_GPU_PLUGIN_NAME,
    {
#if !defined(RDK)
        "GpuMemHook"
#endif
    }}
};

// -----------------------------------------------------------------------------
/**
 *  @brief Populates the static strings used for setting the GPU container
 *  mappings.
 *
 *  This function is only expected to be run once the first time it is required,
 *  it then stores the strings in static fields and uses them for all
 *  subsequent container starts.
 *
 *  @param[in]  devNodes    The list of dev nodes paths (or glob patterns).
 *
 */
void DobbyConfig::initGpuDevNodes(const std::list<std::string>& devNodes)
{
    std::lock_guard<std::mutex> locker(mGpuDevNodesLock);

    // just in case we have multi-threaded container start
    if (mInitialisedGpuDevNodes)
    {
        return;
    }

    // sanity check any dev nodes to add
    if (devNodes.empty())
    {
        mInitialisedGpuDevNodes = true;
        return;
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
        AI_LOG_ERROR("no GPU dev nodes found despite some being listed in the "
                    "JSON config file");
        globfree(&devNodeBuf);
        return;
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

        AI_LOG_INFO("adding gpu dev node '%s' to the template", devNode);

        // the following creates some json telling crun to create the nodes
        devNodesStream << "{ \"path\": \"" << devNode << "\","
                    << "  \"type\": \"c\","
                    << "  \"major\": "       << major(buf.st_rdev)   << ","
                    << "  \"minor\": "       << minor(buf.st_rdev)   << ","
                    << "  \"fileMode\": "    << (buf.st_mode & 0666) << ","
                    << "  \"uid\": 0,"
                    << "  \"gid\": 0 },\n";

        // and this creates the json for the devices cgroup to tell it that
        // the graphics nodes are readable and writeable
        devNodesPermStream << ",\n{ \"allow\": true, "
                        <<      "\"access\": \"rw\", "
                        <<      "\"type\": \"c\","
                        <<      "\"major\": " << major(buf.st_rdev) << ", "
                        <<      "\"minor\": " << minor(buf.st_rdev) << " }";

    }

    globfree(&devNodeBuf);


    // trim off the final comma (',') and newline
    std::string devNodesString = devNodesStream.str();
    if (!devNodesString.empty())
        devNodesString.pop_back();
    if (!devNodesString.empty())
        devNodesString.pop_back();

    // and finally set the global template value
    mGpuDevNodes = devNodesString;
    mGpuDevNodesPerms = devNodesPermStream.str();

    mInitialisedGpuDevNodes = true;
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
    for (int i=0; i < cfg->mounts_len; i++)
    {
        if (!strcmp(cfg->mounts[i]->source, source.c_str()) && !strcmp(cfg->mounts[i]->destination, destination.c_str()))
        {
            AI_LOG_WARN("mount from source %s to dest %s already exists", source.c_str(), destination.c_str());
            return true;
        }
    }

    std::list<std::string> mountOptionsFinal(mountOptions);

    // we only support the standard flags; bind, ro, sync, nosuid, noexec, etc
    static const std::map<unsigned long, std::string> mountFlagsNames =
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
    std::map<unsigned long, std::string>::const_iterator it = mountFlagsNames.begin();
    for (; it != mountFlagsNames.end(); ++it)
    {
        const unsigned long& mountFlag = it->first;
        if ((mountFlag & mountFlags) == mountFlag)
        {
            // add options to options list to be written to the bundle config
            mountOptionsFinal.emplace_back(it->second);

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
    for (int i = 0; i < cfg->process->env_len; ++i)
    {
        if (0 == strcmp(cfg->process->env[i], envVar.c_str()))
        {
            return true;
        }
    }

    // Increase the number of enviromental variables
    cfg->process->env_len += 1;

    // Update env var in OCI bundle config
    cfg->process->env = (char**)realloc(cfg->process->env, sizeof(char*) * cfg->process->env_len);
    cfg->process->env[cfg->process->env_len-1] = strdup(envVar.c_str());

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
        return false;
    }

    // write the new config json to a file
    if (fputs(json_buf, file) == EOF)
    {
        AI_LOG_ERROR_EXIT("Failed to write config file.");
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
#if (AI_BUILD_TYPE == AI_DEBUG)
    int args_len = 6;
    std::string args[args_len] = {
        "DobbyPluginLauncher",
        "-v",
#else
    int args_len = 5;
    std::string args[args_len] = {
        "DobbyPluginLauncher",
#endif
        "-h",
        name,
        "-c",
        configPath
    };

    entry->path = strdup(PLUGINLAUNCHER_PATH);
    entry->args_len = args_len;
    entry->args = (char**)calloc(entry->args_len, sizeof(char*));

    for (int i = 0; i < args_len; i++)
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


    // startContainer hook paths must resolve in the container namespace,
    // config is in container rootdir
    configPath = "/tmp/config.json";

    // populate startContainer hook with DobbyPluginLauncher args
    rt_defs_hook *startContainerEntry = (rt_defs_hook*)calloc(1, sizeof(rt_defs_hook));
    setPluginHookEntry(startContainerEntry, "startContainer", configPath);
    cfg->hooks->start_container = (rt_defs_hook**)realloc(cfg->hooks->start_container, sizeof(rt_defs_hook*) * ++cfg->hooks->start_container_len);
    cfg->hooks->start_container[cfg->hooks->start_container_len-1] = startContainerEntry;
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
    if (cfg->rdk_plugins->plugins_count && findRdkPlugins(cfg->rdk_plugins))
    {
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
 *  @brief Checks if a matching rdkPlugin shared library is available for all
 *  defined rdkPlugins in config. If false is returned, DobbyPluginLauncher
 *  hooks should not be added to config.
 *
 *  @param[in]  rdkPlugins      rdkPlugins in config file
 *
 *  TODO: remove this function once rdkPlugin development has finalised
 *
 */
bool DobbyConfig::findRdkPlugins(rt_defs_plugins_rdk_plugins *rdkPlugins)
{
    // if plugin isn't found in mRdkPluginsInDevelopment, we can expect it to
    // exist as a shared library i.e. as an rdkPlugin, not as a syshook. If
    // even one rdkPlugin can be used, we can return true
    for (int i = 0; i < rdkPlugins->plugins_count; i++)
    {
        if (mRdkPluginsInDevelopment.find(rdkPlugins->names_of_plugins[i]) ==
            mRdkPluginsInDevelopment.end())
        {
            // didn't find plugin in mRdkPluginsInDevelopment, hooks needed
            return true;
        }
    }

    return false;
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
        // copy config.json to config-dobby.json
        if (!writeConfigJsonImpl(bundlePath + "/config-dobby.json"))
        {
            return false;
        }

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
              findPluginLauncherHookEntry(hooks->start_container, hooks->start_container_len) &&
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
            // hooks are set up just fine, so we can start
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
