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
 * File:   DobbyTemplate.cpp
 *
 */
#include "DobbyTemplate.h"
#include "IDobbySettings.h"

#include <Logging.h>

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <mntent.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

#include <sstream>
#include <glob.h>
#include <unordered_set>

// Use the extended OCI bundle format with RDK plugins
#if defined(DEV_VM)
#   include "templates/OciConfigJsonVM1.0.2-dobby.template"
#else
#   include "templates/OciConfigJson1.0.2-dobby.template"
#endif



DobbyTemplate* DobbyTemplate::mInstance = nullptr;
pthread_rwlock_t DobbyTemplate::mInstanceLock = PTHREAD_RWLOCK_INITIALIZER;

DobbyTemplate::DobbyTemplate()
    : mTemplateKey("oci")
    , mTemplateCache(new ctemplate::TemplateCache())
{
    AI_LOG_FN_ENTRY();

    atexit(DobbyTemplate::cleanUp);

    // load the above template string into the cache
    if (!mTemplateCache->StringToTemplateCache(mTemplateKey,
                                               ociJsonTemplate,
                                               ctemplate::STRIP_WHITESPACE))
    {
        AI_LOG_ERROR("failed to insert default template into ctemplate cache");
    }

    // we get guarantees about ctemplate not hitting the filesystem only if the
    // cache is frozen.
    mTemplateCache->Freeze();

    // set the template platform identifier environment variables
    setTemplatePlatformEnvVars();

    // set the template enable/disable for the cpu RT sched cgroups
    setTemplateCpuRtSched();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called at shutdown time to clean up the singleton
 *
 *  Callback installed as atexit(), frees the instance pointer.
 *
 */
void DobbyTemplate::cleanUp()
{
    pthread_rwlock_wrlock(&mInstanceLock);

    if (mInstance != nullptr)
    {
        delete mInstance;
        mInstance = nullptr;
    }

    pthread_rwlock_unlock(&mInstanceLock);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns / creates singleton instance.
 *
 *
 */
DobbyTemplate* DobbyTemplate::instance()
{
    pthread_rwlock_rdlock(&mInstanceLock);

    if (mInstance == nullptr)
    {
        // Need to upgrade to a write lock and then check the instance again
        // to be sure that someone else hasn't jumped in and allocated it
        // while we were unlocked.
        pthread_rwlock_unlock(&mInstanceLock);
        pthread_rwlock_wrlock(&mInstanceLock);

        if (mInstance == nullptr)
        {
            mInstance = new DobbyTemplate;
        }
    }

    DobbyTemplate* result = mInstance;
    pthread_rwlock_unlock(&mInstanceLock);

    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Applies the settings to the global template values.
 *
 *
 */
void DobbyTemplate::setSettings(const std::shared_ptr<const IDobbySettings>& settings)
{
    instance()->_setSettings(settings);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Applies the settings to the global template values.
 *
 *  This includes settings for the environment variables and GPU.
 */
void DobbyTemplate::_setSettings(const std::shared_ptr<const IDobbySettings>& settings)
{
    // DEPRECATED: removed the following and instead now do it in DobbyConfig
    // const std::list<std::string> gpuDevNodes = settings->gpuDeviceNodes();
    // if (!gpuDevNodes.empty())
    //    setTemplateDevNodes(gpuDevNodes);

    // set the extra env vars and apply to the template
    const std::map<std::string, std::string> extraEnvVars = settings->extraEnvVariables();
    if (!extraEnvVars.empty())
    {
        for (const auto& envVar : extraEnvVars)
            mExtraEnvVars[envVar.first] = envVar.second;

        setTemplateEnvVars(mExtraEnvVars);
    }
}

#if 0
// -----------------------------------------------------------------------------
/**
 *  @brief Called by setTemplateDevNodes to get a list of dev nodes to map
 *
 *  Append required dev nodes to the vector
 *
 */
std::vector<std::string> DobbyTemplate::getTemplateDevNodes()
{
    //Ensure devNodes vector is empty
    std::vector<std::string> devNodes;

    // for ST platforms we need to map in the following
    //   /dev/mali
    //   /dev/xeglhelper
    devNodes.emplace_back("/dev/mali");
    devNodes.emplace_back("/dev/xeglhelper");

    // for broadcom onebox platforms
    devNodes.emplace_back("/dev/nexus");
    devNodes.emplace_back("/dev/bcm_moksha_loader");
    devNodes.emplace_back("/dev/dri/card0");

    // create a glob structure to hold the list of dev nodes
    glob_t devNodeBuf;

    // for broadcom platforms we need to map in the following
    //   /dev/nds/opengl0
    //   /dev/nds/xeglstreamX   ( where X => { 0 : 11 } )
    glob("/dev/nds/opengl*", GLOB_NOSORT, nullptr, &devNodeBuf);
    glob("/dev/nds/xeglstream*", GLOB_NOSORT | GLOB_APPEND, nullptr, &devNodeBuf);

    // add support for g
#if defined(__i686__)
    // for vSTB platforms we need to map in the following
    //   /dev/nds/xeglhelper/xeglcrossprocess
    //   /dev/nds/xeglhelper/eglimageX   ( where X => { 0 : 254 } )
    //   /dev/nds/xeglhelper/eglstreamX  ( where X => { 0 : 254 } )
    devNodes.push_back("/dev/nds/xeglhelper/xeglcrossprocess");
    glob("/dev/nds/xeglhelper/eglimage*", GLOB_NOSORT | GLOB_APPEND, NULL, &devNodeBuf);
    glob("/dev/nds/xeglhelper/eglstream*", GLOB_NOSORT | GLOB_APPEND, NULL, &devNodeBuf);
#endif //__i686__

    // loop through result vector and assign
    for( uint32_t i = 0; i < devNodeBuf.gl_pathc; ++i )
    {
        devNodes.emplace_back( std::string( devNodeBuf.gl_pathv[i] ) );
    }

    // free up any memory allocated by glob
    globfree(&devNodeBuf);

    return devNodes;
}
#endif

// -----------------------------------------------------------------------------
/**
 *  @deprecated This is now down lazily in the DobbyConfig component when
 *  starting the first container that requires the GPU.
 *
 *  @brief Sets up the global template values for the device nodes
 *
 *  Currently the device nodes are only for the xegl/opengl.
 *
 *  We need to get the device numbers from the file system as runc won't do that
 *  automatically for us.  We could hard code them, but it's perfectly
 *  reasonable that they could change between driver releases.
 *
 *  Nb: this feels like the wrong place to be doing this, however for now
 *  it is convenient and the most efficient.
 *
 */
void DobbyTemplate::setTemplateDevNodes(const std::list<std::string>& devNodes)
{
    AI_LOG_FN_ENTRY();

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
        AI_LOG_WARN("no GPU dev nodes found despite some being listed in the "
                    "JSON config file");
        globfree(&devNodeBuf);
        AI_LOG_FN_EXIT();
        return;
    }

    //
    int devDirFd = open("/dev", O_CLOEXEC | O_DIRECTORY);
    if (devDirFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open /dev directory");
        globfree(&devNodeBuf);
        return;
    }

    // Get the dev nodes
    struct stat buf;
    std::ostringstream devNodesStream;
    std::ostringstream devNodesPermStream;

    // for the sake of simplicity we map in all of the above if we find
    // them in the rootfs
    // loop through result vector and assign
    for (size_t i = 0; i < devNodeBuf.gl_pathc; ++i)
    {
        const char *devNode = devNodeBuf.gl_pathv[i];
        if (!devNode)
        {
            AI_LOG_ERROR("invalid glob string");
            continue;
        }

        if (fstatat(devDirFd, devNode, &buf, 0) != 0)
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

        // the following creates some json telling runc to create the nodes
        devNodesStream << "{ \"path\": \"" << devNode << "\","
                       << "  \"type\": \"c\","
                       << "  \"major\": "       << major(buf.st_rdev)   << ","
                       << "  \"minor\": "       << minor(buf.st_rdev)   << ","
                       << "  \"fileMode\": "    << (buf.st_mode & 0666) << ","
                       << "  \"uid\": 0,"
                       << "  \"gid\": 0 },\n";

        // and this creates the json for the devices cgroup to tell it that
        // the graphics nodes are readable and writeable
        devNodesPermStream << "{ \"allow\": true, "
                           << "    \"access\": \"rw\", "
                           << "    \"type\": \"c\","
                           << "    \"major\": " << major(buf.st_rdev) << ", "
                           << "    \"minor\": " << minor(buf.st_rdev) << " },\n";

    }

    if (close(devDirFd) != 0)
        AI_LOG_SYS_ERROR(errno, "failed to close dirfd");

    globfree(&devNodeBuf);


    // trim off the final comma (',') and newline
    std::string devNodesString = devNodesStream.str();
    while (!devNodesString.empty() &&
           ((devNodesString.back() == '\n') || (devNodesString.back() == ',')))
    {
        devNodesString.pop_back();
    }

    std::string devNodesPermString = devNodesPermStream.str();
    while (!devNodesPermString.empty() &&
           ((devNodesPermString.back() == '\n') || (devNodesPermString.back() == ',')))
    {
        devNodesPermString.pop_back();
    }


    // and finally set the global template value
    ctemplate::TemplateDictionary::SetGlobalValue("GPU_DEV_NODES",
                                                  devNodesString);
    ctemplate::TemplateDictionary::SetGlobalValue("GPU_DEV_NODES_PERMS",
                                                  devNodesPermString);


    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Converts the envVars map to json formatted string and sets it as the
 *  EXTRA_ENV_VARS template value.
 *
 *
 *
 */
void DobbyTemplate::setTemplateEnvVars(const std::map<std::string, std::string>& envVars)
{
    // create the string of new values
    std::ostringstream envVarsStream;
    for (const auto& envVar : envVars)
    {
        envVarsStream << "\"" << envVar.first << "=" << envVar.second << "\",";
    }

    // assign the string to the template var
    ctemplate::TemplateDictionary::SetGlobalValue("EXTRA_ENV_VARS", envVarsStream.str());
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the environment variables used to identify the platform.
 *
 *  All containers get two environment variables that define the platform;
 *
 *      ETHAN_STB_TYPE  = [ "GW" | "MR" | "HIP" ]
 *      ETHAN_STB_MODEL = [ "ES140" | "ES130" | "EM150" | "ES240" | "ESi240" | ... ]
 *
 *  For a list of model names refer to the following confluence page
 *
 *      https://www.stb.bskyb.com/confluence/display/2016/STB+HW+ID
 *
 *  To determine platform we check for the AI_PLATFORM_IDENT environment
 *  variable.  This should have been set by the user (or upstart script) that
 *  started this daemon.  If it's not present then we set both templates
 *  as empty and log an error.
 */
void DobbyTemplate::setTemplatePlatformEnvVars()
{
    AI_LOG_FN_ENTRY();

    const char* platformType = getenv("AI_PLATFORM_TYPE");
    if ((platformType == nullptr) || (platformType[0] == '\0'))
    {
        AI_LOG_INFO("missing AI_PLATFORM_TYPE environment var, will "
                    "set empty container platform env vars");
        AI_LOG_FN_EXIT();
        return;
    }

    static const std::string mrType = "MR";
    static const std::string gwType = "GW";
    static const std::string ipType = "HIP";
    if (!((mrType == platformType) || (gwType == platformType) || (ipType == platformType)))
    {
        AI_LOG_ERROR_EXIT("Platform type is invalid %s", platformType);
        return;
    }

    const char* platformModel = getenv("AI_PLATFORM_MODEL");
    if ((platformModel == nullptr) || (platformModel[0] == '\0'))
    {
        AI_LOG_INFO("missing AI_PLATFORM_MODEL environment var, will "
                    "set empty container platform env vars");
        AI_LOG_FN_EXIT();
        return;
    }

    static const std::unordered_set<std::string> availablePlatformModels = {
        "ES140", "ES130", "EM150", "ES240", "ES340", "ESi240", "vSTB", "ES160"
    };

    if (availablePlatformModels.count(platformModel) < 1)
    {
        AI_LOG_ERROR_EXIT("Platform model is invalid %s", platformModel);
        return;
    }

    mExtraEnvVars["ETHAN_STB_TYPE"] = platformType;
    mExtraEnvVars["ETHAN_STB_MODEL"] = platformModel;
    setTemplateEnvVars(mExtraEnvVars);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Determines if the kernel's CONFIG_RT_GROUP_SCHED is set or not.
 *
 *  If CONFIG_RT_GROUP_SCHED is set in the kernel config we need to give all
 *  containers a slice of runtime scheduler.  So we check if the kernel is
 *  configured that way and if it is we enabled CGROUP_RTSCHD_ENABLED section in
 *  the template.
 *
 */
void DobbyTemplate::setTemplateCpuRtSched()
{
    AI_LOG_FN_ENTRY();

    // try and open /proc/mounts for scanning the current mount table
    FILE* procMounts = setmntent("/proc/mounts", "r");
    if (procMounts == nullptr)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open '/proc/mounts' file");
        return;
    }

    // loop over all the mounts
    struct mntent mntBuf;
    struct mntent* mnt;
    char buf[PATH_MAX + 256];

    long cpuRtRuntime = 0;
    long cpuRtPeriod = 0;
    while ((mnt = getmntent_r(procMounts, &mntBuf, buf, sizeof(buf))) != nullptr)
    {
        // skip entries that don't have a mount point, type or options
        if (!mnt->mnt_type || !mnt->mnt_dir || !mnt->mnt_opts)
            continue;

        // skip non-cgroup mounts
        if (strcmp(mnt->mnt_type, "cgroup") != 0)
            continue;

        // check if a cpu cgroup mount
        char* mntopt = hasmntopt(mnt, "cpu");
        if (!mntopt || (strncmp(mntopt, "cpu", 3) != 0))
            continue;

        // check for the presence of the 'cpu.rt_runtime_us' file
        char filePath[PATH_MAX];
        snprintf(filePath, sizeof(filePath), "%s/cpu.rt_runtime_us", mnt->mnt_dir);
        if (access(filePath, F_OK) == 0)
        {
            cpuRtRuntime = 1000;
            cpuRtPeriod = 1000000;
        }

        break;
    }

    endmntent(procMounts);

    char cpuRtRuntimeStr[32], cpuRtPeriodStr[32];
    // convert to strings (because some of toolchains don't support std::to_string)
    if (cpuRtPeriod == 0 && cpuRtRuntime == 0)
    {
        // In newer crun versions, 0 is considered a defined value, so set to
        // null if kernel doesn't support this feature.
        sprintf(cpuRtRuntimeStr, "null");
        sprintf(cpuRtPeriodStr, "null");
    }
    else
    {
        sprintf(cpuRtRuntimeStr, "%ld", cpuRtRuntime);
        sprintf(cpuRtPeriodStr, "%ld", cpuRtPeriod);
    }

    // update the template values
    ctemplate::TemplateDictionary::SetGlobalValue("CPU_RT_RUNTIME", cpuRtRuntimeStr);
    ctemplate::TemplateDictionary::SetGlobalValue("CPU_RT_PERIOD", cpuRtPeriodStr);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Applies the dictionary to the template
 *
 *
 *  @param[in]  dictionary      The dictionary to apply
 *
 *  @return the expanded template string
 */
std::string DobbyTemplate::_apply(const ctemplate::TemplateDictionaryInterface* dictionary,
                                  bool prettyPrint) const
{
    AI_LOG_FN_ENTRY();

    std::string result;

    if (!mTemplateCache->ExpandNoLoad(mTemplateKey,
                                      prettyPrint ? ctemplate::STRIP_WHITESPACE
                                                  : ctemplate::STRIP_WHITESPACE,
                                      dictionary, nullptr, &result))
    {
        AI_LOG_ERROR("template cache expand on load failed");
        result.clear();
    }

    AI_LOG_FN_EXIT();

    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Applies the dictionary to the template
 *
 *
 *  @param[in]  dictionary      The dictionary to apply
 *  @param[in]  prettyPrint     Format the json file using pretty printer.
 *
 *  @return the expanded template string
 */
std::string DobbyTemplate::apply(const ctemplate::TemplateDictionaryInterface* dictionary,
                                 bool prettyPrint)
{
    return instance()->_apply(dictionary, prettyPrint);
}

// -----------------------------------------------------------------------------
/**
 *  @class DobbyFileEmitter
 *  @brief Small utility class to emit data to a file rather than string
 *
 */
class DobbyFileEmitter : public ctemplate::ExpandEmitter
{
public:
    explicit DobbyFileEmitter(int fd)
        : mFd(fd)
    { }
    ~DobbyFileEmitter() final = default;

public:
    void Emit(char c) override
    {
        this->Emit(&c, 1);
    }
    void Emit(const std::string& s) override
    {
        this->Emit(s.data(), s.length());
    }
    void Emit(const char* s) override
    {
        this->Emit(s, strlen(s));
    }
    void Emit(const char* s, size_t n) override
    {
        while (n > 0)
        {
            ssize_t written = TEMP_FAILURE_RETRY(write(mFd, s, n));
            if (written < 0)
            {
                AI_LOG_SYS_ERROR(errno, "failed to write to file");
                break;
            }
            else if (written == 0)
            {
                break;
            }

            s += written;
            n -= written;
        }
    }

private:
    int mFd;
};

// -----------------------------------------------------------------------------
/**
 *  @brief Applies the dictionary to the template and writes the output into
 *  the file.
 *
 *  If the pathname given in @a fileName is relative, then it is interpreted
 *  relative to the directory referred to by the file descriptor dirfd. If
 *  @a fileName is relative and dirfd is the special value AT_FDCWD, then
 *  @a fileName is interpreted relative to the current working directory
 *
 *  @param[in]  dirFd           The directory of the file.
 *  @param[in]  fileName        The path to the file to write to.
 *  @param[in]  dictionary      The dictionary to apply.
 *
 *  @return true on success, false on failure.
 */
bool DobbyTemplate::_applyAt(int dirFd, const std::string& fileName,
                             const ctemplate::TemplateDictionaryInterface* dictionary,
                             bool prettyPrint) const
{
    AI_LOG_FN_ENTRY();

    // open / create the file to write to
    const int flags = O_CLOEXEC | O_CREAT | O_WRONLY | O_TRUNC;
    int fd = openat(dirFd, fileName.c_str(), flags, 0600);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open/create file '%s'",
                              fileName.c_str());
        return false;
    }

    // wrap the fd in an emitter object
    DobbyFileEmitter emitter(fd);

    // expand the template using the emitter
    bool success = mTemplateCache->ExpandNoLoad(mTemplateKey,
                                                prettyPrint ?
                                                    ctemplate::STRIP_WHITESPACE :
                                                    ctemplate::STRIP_WHITESPACE,
                                                dictionary, nullptr, &emitter);
    if (!success)
    {
        AI_LOG_ERROR("template cache expand on load failed");
    }

    // close the file created
    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close file");
    }

    // if there was an error make sure to delete the file
    if (!success && (unlinkat(dirFd, fileName.c_str(), 0) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to delete file");
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Applies the dictionary to the template and writes the output into
 *  the file.
 *
 *  If the pathname given in @a fileName is relative, then it is interpreted
 *  relative to the directory referred to by the file descriptor dirfd. If
 *  @a fileName is relative and dirfd is the special value AT_FDCWD, then
 *  @a fileName is interpreted relative to the current working directory
 *
 *  @param[in]  dirFd           The directory of the file.
 *  @param[in]  fileName        The path to the file to write to.
 *  @param[in]  dictionary      The dictionary to apply.
 *  @param[in]  prettyPrint     Pretty print the json output.
 *
 *  @return true on success, false on failure.
 */
bool DobbyTemplate::applyAt(int dirFd, const std::string& fileName,
                            const ctemplate::TemplateDictionaryInterface* dictionary,
                            bool prettyPrint)
{
    return instance()->_applyAt(dirFd, fileName, dictionary, prettyPrint);
}
