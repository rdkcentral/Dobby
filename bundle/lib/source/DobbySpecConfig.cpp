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
 * File:   DobbySpecConfig.cpp
 *
 */

#include "DobbySpecConfig.h"
#include "DobbyTemplate.h"
#include "IDobbyUtils.h"

#include <array>
#include <atomic>
#include <algorithm>
#include <grp.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/sysinfo.h>
#include <sys/capability.h>
#include <sys/stat.h>
#include <fstream>

// Compile time generated strings that (in theory) speeds up the processing
// of ctemplate expanding
//  (see https://google-ctemplate.googlecode.com/svn/trunk/doc/reference.html)

static const ctemplate::StaticTemplateString ENV_VAR_SECTION =
    STS_INIT(ENV_VAR_SECTION, "ENV_VAR_SECTION");
static const ctemplate::StaticTemplateString ENV_VAR_VALUE =
    STS_INIT(ENV_VAR_VALUE, "ENV_VAR_VALUE");

static const ctemplate::StaticTemplateString ARGS_VAR_SECTION =
    STS_INIT(ARGS_VAR_SECTION, "ARGS_VAR_SECTION");
static const ctemplate::StaticTemplateString ARGS_VAR_VALUE =
    STS_INIT(ARGS_VAR_VALUE, "ARGS_VAR_VALUE");

static const ctemplate::StaticTemplateString USER_ID =
    STS_INIT(USER_ID, "USER_ID");
static const ctemplate::StaticTemplateString GROUP_ID =
    STS_INIT(GROUP_ID, "GROUP_ID");
static const ctemplate::StaticTemplateString USERNS_ENABLED =
    STS_INIT(USERNS_ENABLED, "USERNS_ENABLED");
static const ctemplate::StaticTemplateString USERNS_DISABLED =
    STS_INIT(USERNS_DISABLED, "USERNS_DISABLED");

static const ctemplate::StaticTemplateString MEM_LIMIT =
    STS_INIT(MEM_LIMIT, "MEM_LIMIT");

static const ctemplate::StaticTemplateString CPU_SHARES_ENABLED =
    STS_INIT(CPU_SHARES_ENABLED, "CPU_SHARES_ENABLED");
static const ctemplate::StaticTemplateString CPU_SHARES_VALUE =
    STS_INIT(CPU_SHARES_VALUE, "CPU_SHARES_VALUE");
static const ctemplate::StaticTemplateString CPU_CPUS_ENABLED =
    STS_INIT(CPU_CPUS_ENABLED, "CPU_CPUS_ENABLED");
static const ctemplate::StaticTemplateString CPU_CPUS_VALUE =
    STS_INIT(CPU_CPUS_VALUE, "CPU_CPUS_VALUE");

static const ctemplate::StaticTemplateString NETNS_ENABLED =
    STS_INIT(NETNS_ENABLED, "NETNS_ENABLED");

static const ctemplate::StaticTemplateString ADDITIONAL_GIDS =
    STS_INIT(ADDITIONAL_GIDS, "ADDITIONAL_GIDS");
static const ctemplate::StaticTemplateString ADDITIONAL_GID =
    STS_INIT(ADDITIONAL_GID, "ADDITIONAL_GID");

static const ctemplate::StaticTemplateString ADDITIONAL_DEVICE_NODES =
    STS_INIT(ADDITIONAL_DEVICE_NODES, "ADDITIONAL_DEVICE_NODES");
static const ctemplate::StaticTemplateString DEVICE_PATH =
    STS_INIT(DEVICE_PATH, "DEVICE_PATH");
static const ctemplate::StaticTemplateString DEVICE_MAJOR =
        STS_INIT(DEVICE_MAJOR, "DEVICE_MAJOR");
static const ctemplate::StaticTemplateString DEVICE_MINOR =
    STS_INIT(DEVICE_MINOR, "DEVICE_MINOR");
static const ctemplate::StaticTemplateString DEVICE_FILE_MODE =
    STS_INIT(DEVICE_FILE_MODE, "DEVICE_FILE_MODE");
static const ctemplate::StaticTemplateString DEVICE_ACCESS =
    STS_INIT(DEVICE_ACCESS, "DEVICE_ACCESS");

static const ctemplate::StaticTemplateString MOUNT_SECTION =
    STS_INIT(MOUNT_SECTION, "MOUNT_SECTION");
static const ctemplate::StaticTemplateString MOUNT_DST =
    STS_INIT(MOUNT_DST, "MOUNT_DST");
static const ctemplate::StaticTemplateString MOUNT_SRC =
    STS_INIT(MOUNT_SRC, "MOUNT_SRC");
static const ctemplate::StaticTemplateString MOUNT_TYPE =
    STS_INIT(MOUNT_TYPE, "MOUNT_TYPE");
static const ctemplate::StaticTemplateString MOUNT_OPT_SECTION =
    STS_INIT(MOUNT_OPT_SECTION, "MOUNT_OPT_SECTION");
static const ctemplate::StaticTemplateString MOUNT_OPT =
    STS_INIT(MOUNT_OPT, "MOUNT_OPT");

static const ctemplate::StaticTemplateString SYSLOG_SECTION =
    STS_INIT(SYSLOG_SECTION, "SYSLOG_SECTION");

static const ctemplate::StaticTemplateString RTLIMIT_ENABLED =
    STS_INIT(RTLIMIT_ENABLED, "RTLIMIT_ENABLED");
static const ctemplate::StaticTemplateString RLIMIT_RTPRIO =
    STS_INIT(RLIMIT_RTPRIO, "RLIMIT_RTPRIO");

static const ctemplate::StaticTemplateString DEV_WHITELIST_SECTION =
    STS_INIT(DEV_WHITELIST_SECTION, "DEV_WHITELIST_SECTION");
static const ctemplate::StaticTemplateString DEV_WHITELIST_MAJOR =
    STS_INIT(DEV_WHITELIST_MAJOR, "DEV_WHITELIST_MAJOR");
static const ctemplate::StaticTemplateString DEV_WHITELIST_MINOR =
    STS_INIT(DEV_WHITELIST_MINOR, "DEV_WHITELIST_MINOR");
static const ctemplate::StaticTemplateString DEV_WHITELIST_ACCESS =
    STS_INIT(DEV_WHITELIST_ACCESS, "DEV_WHITELIST_ACCESS");

static const ctemplate::StaticTemplateString EXTRA_CAPS_SECTION =
    STS_INIT(EXTRA_CAPS_SECTION, "EXTRA_CAPS_SECTION");
static const ctemplate::StaticTemplateString EXTRA_CAPS_VALUE =
    STS_INIT(EXTRA_CAPS_VALUE, "EXTRA_CAPS_VALUE");

static const ctemplate::StaticTemplateString NO_NEW_PRIVS =
    STS_INIT(NO_NEW_PRIVS, "NO_NEW_PRIVS");


static const ctemplate::StaticTemplateString ENABLE_RDK_PLUGINS =
    STS_INIT(ENABLE_RDK_PLUGINS, "ENABLE_RDK_PLUGINS");
static const ctemplate::StaticTemplateString RDK_PLUGIN_SECTION =
    STS_INIT(RDK_PLUGIN_SECTION, "RDK_PLUGIN_SECTION");
static const ctemplate::StaticTemplateString RDK_PLUGIN_NAME =
    STS_INIT(RDK_PLUGIN_NAME, "RDK_PLUGIN_NAME");
static const ctemplate::StaticTemplateString RDK_PLUGIN_DATA =
    STS_INIT(RDK_PLUGIN_DATA, "RDK_PLUGIN_DATA");
static const ctemplate::StaticTemplateString RDK_PLUGIN_REQUIRED =
    STS_INIT(RDK_PLUGIN_REQUIRED, "RDK_PLUGIN_REQUIRED");
static const ctemplate::StaticTemplateString RDK_PLUGIN_DEPENDS_ON =
    STS_INIT(RDK_PLUGIN_DEPENDS_ON, "RDK_PLUGIN_DEPENDS_ON");

static const ctemplate::StaticTemplateString ENABLE_LEGACY_PLUGINS =
    STS_INIT(ENABLE_LEGACY_PLUGINS, "ENABLE_LEGACY_PLUGINS");
static const ctemplate::StaticTemplateString DOBBY_PLUGIN_SECTION =
    STS_INIT(DOBBY_PLUGIN_SECTION, "DOBBY_PLUGIN_SECTION");
static const ctemplate::StaticTemplateString PLUGIN_NAME =
    STS_INIT(PLUGIN_NAME, "PLUGIN_NAME");
static const ctemplate::StaticTemplateString PLUGIN_DATA =
    STS_INIT(PLUGIN_DATA, "PLUGIN_DATA");

static const ctemplate::StaticTemplateString SECCOMP_ENABLED =
    STS_INIT(SECCOMP_ENABLED, "SECCOMP_ENABLED");
static const ctemplate::StaticTemplateString SECCOMP_DEFAULT_ACTION =
    STS_INIT(SECCOMP_DEFAULT_ACTION, "SECCOMP_DEFAULT_ACTION");
static const ctemplate::StaticTemplateString SECCOMP_ACTION =
    STS_INIT(SECCOMP_ACTION, "SECCOMP_ACTION");
static const ctemplate::StaticTemplateString SECCOMP_SYSCALLS =
    STS_INIT(SECCOMP_SYSCALLS, "SECCOMP_SYSCALLS");


// Flags that are set as various parts of the json spec file is parsed
#define JSON_FLAG_ENV               (0x1U << 1)
#define JSON_FLAG_ARGS              (0x1U << 2)
#define JSON_FLAG_CWD               (0x1U << 3)
#define JSON_FLAG_USER              (0x1U << 4)
#define JSON_FLAG_USERNS            (0x1U << 5)
#define JSON_FLAG_CONSOLE           (0x1U << 6)
#define JSON_FLAG_ETC               (0x1U << 7)
#define JSON_FLAG_MOUNTS            (0x1U << 8)
#define JSON_FLAG_PLUGINS           (0x1U << 9)
#define JSON_FLAG_MEMLIMIT          (0x1U << 10)
#define JSON_FLAG_GPU               (0x1U << 11)
#define JSON_FLAG_NETWORK           (0x1U << 12)
#define JSON_FLAG_RTPRIORITY        (0x1U << 13)
#define JSON_FLAG_RESTARTONCRASH    (0x1U << 14)
#define JSON_FLAG_DBUS              (0x1U << 15)
#define JSON_FLAG_SYSLOG            (0x1U << 16)
#define JSON_FLAG_CPU               (0x1U << 17)
#define JSON_FLAG_DEVICES           (0x1U << 18)
#define JSON_FLAG_CAPABILITIES      (0x1U << 19)
#define JSON_FLAG_FILECAPABILITIES  (0x1U << 20)
#define JSON_FLAG_VPU               (0x1U << 21)
#define JSON_FLAG_SECCOMP           (0x1U << 22)

int DobbySpecConfig::mNumCores = -1;

// TODO: should we only allowed these if a network namespace is enabled ?
const std::map<std::string, int> DobbySpecConfig::mAllowedCaps =
{
    { "CAP_NET_BIND_SERVICE",   CAP_NET_BIND_SERVICE    },
    { "CAP_NET_BROADCAST",      CAP_NET_BROADCAST       },
    { "CAP_NET_RAW",            CAP_NET_RAW             },
};

// -----------------------------------------------------------------------------
/**
 *  @brief Constructor used to parse a Dobby spec file into an OCI config file.
 *
 *  @param[in]  utils               The daemon utils object.
 *  @param[in]  settings            Dobby settings object.
 *  @param[in]  id                  Container ID.
 *  @param[in]  bundle              an instance of the DobbyBundle object
 *  @param[in]  specJson            JSON containing the Dobby spec
 */
DobbySpecConfig::DobbySpecConfig(const std::shared_ptr<IDobbyUtils> &utils,
                                 const std::shared_ptr<const IDobbySettings>& settings,
                                 const ContainerId& id,
                                 const std::shared_ptr<const DobbyBundle>& bundle,
                                 const std::string& specJson)
    : mUtilities(utils)
    , mGpuSettings(settings->gpuAccessSettings())
    , mVpuSettings(settings->vpuAccessSettings())
    , mDefaultPlugins(settings->defaultPlugins())
    , mRdkPluginsData(settings->rdkPluginsData())
    , mDictionary(nullptr)
    , mConf(nullptr)
    , mSpecVersion(SpecVersion::Unknown)
    , mUserId(-1)
    , mGroupId(-1)
    , mRestartOnCrash(false)
    , mSystemDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mSessionDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mDebugDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mConsoleDisabled(true)
    , mConsoleLimit(-1)
    , mRootfsPath("rootfs")
{
    // get the number of (online) cpu cores on the system if we haven't already
    if (mNumCores <= 0)
    {
        mNumCores = get_nprocs();
        AI_LOG_INFO("current platform has %d cores", mNumCores);
    }

    // create a dictionary object
    mDictionary = new ctemplate::TemplateDictionary("spec");

    // because jsoncpp can throw exceptions if we fail to check the json types
    // before performing conversions we wrap the whole parse operation in a
    // try / catch
    try
    {
        // go and parse dobby spec into OCI config using a template dictionary
        mValid = parseSpec(mDictionary, specJson, bundle->dirFd());

        // bundle persistence is set to default when starting from a spec,
        // so we can go ahead and finalise container config preparation!
        if(!bundle->getPersistence())
        {
            // deserialise config.json
            parser_error err = nullptr;
            std::string configPath = bundle->path() + "/config.json";
            mConf = std::shared_ptr<rt_dobby_schema>(
                        rt_dobby_schema_parse_file(configPath.c_str(), nullptr, &err),
                        free_rt_dobby_schema);

            if (mConf.get() == nullptr || err)
            {
                AI_LOG_ERROR_EXIT("Failed to parse bundle config, err '%s'", err);
                if (err)
                {
                    free(err);
                    err = nullptr;
                }
                mValid = false;
            }
            else
            {
                // convert OCI config to compliant using libocispec
                mValid &= DobbyConfig::convertToCompliant(id, mConf, bundle->path());
            }
        }
    }
    catch (const Json::Exception& e)
    {
        AI_LOG_ERROR("exception thrown during spec parsing - %s", e.what());
        mValid = false;
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructor used to parse a Dobby spec file into an OCI config file.
 *  Used with bundle generation.
 *
 *  @param[in]  utils               The daemon utils object.
 *  @param[in]  settings            Dobby settings object.
 *  @param[in]  bundle              an instance of the DobbyBundle object
 *  @param[in]  specJson            JSON containing the Dobby spec
 */
DobbySpecConfig::DobbySpecConfig(const std::shared_ptr<IDobbyUtils> &utils,
                                 const std::shared_ptr<const IDobbySettings>& settings,
                                 const std::shared_ptr<const DobbyBundle>& bundle,
                                 const std::string& specJson)
    : mUtilities(utils)
    , mGpuSettings(settings->gpuAccessSettings())
    , mVpuSettings(settings->vpuAccessSettings())
    , mDictionary(nullptr)
    , mConf(nullptr)
    , mSpecVersion(SpecVersion::Unknown)
    , mUserId(-1)
    , mGroupId(-1)
    , mRestartOnCrash(false)
    , mSystemDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mSessionDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mDebugDbus(IDobbyIPCUtils::BusType::NoneBus)
    , mConsoleDisabled(true)
    , mConsoleLimit(-1)
    , mRootfsPath("rootfs")
{
    // get the number of (online) cpu cores on the system if we haven't already
    if (mNumCores <= 0)
    {
        mNumCores = get_nprocs();
        AI_LOG_INFO("current platform has %d cores", mNumCores);
    }

    // create a dictionary object
    mDictionary = new ctemplate::TemplateDictionary("spec");

    // because jsoncpp can throw exceptions if we fail to check the json types
    // before performing conversions we wrap the whole parse operation in a
    // try / catch
    try
    {
        // go and parse dobby spec into OCI config using a template dictionary
        mValid = parseSpec(mDictionary, specJson, bundle->dirFd());
    }
    catch (const Json::Exception& e)
    {
        AI_LOG_ERROR("exception thrown during spec parsing - %s", e.what());
        mValid = false;
    }
}

DobbySpecConfig::~DobbySpecConfig()
{
    if (mDictionary != nullptr)
    {
        delete mDictionary;
    }
}

bool DobbySpecConfig::isValid() const
{
    return mValid;
}

const std::string DobbySpecConfig::spec() const
{
    Json::StreamWriterBuilder builder;
    builder["indentation"] = "";

    return Json::writeString(builder, mSpec);
}

uid_t DobbySpecConfig::userId() const
{
    return mUserId;
}

gid_t DobbySpecConfig::groupId() const
{
    return mGroupId;
}

const std::string& DobbySpecConfig::rootfsPath() const
{
    return mRootfsPath;
}

bool DobbySpecConfig::restartOnCrash() const
{
    return mRestartOnCrash;
}

IDobbyIPCUtils::BusType DobbySpecConfig::systemDbus() const
{
    return mSystemDbus;
}

IDobbyIPCUtils::BusType DobbySpecConfig::sessionDbus() const
{
    return mSessionDbus;
}

IDobbyIPCUtils::BusType DobbySpecConfig::debugDbus() const
{
    return mDebugDbus;
}

bool DobbySpecConfig::consoleDisabled() const
{
    return mConsoleDisabled;
}

ssize_t DobbySpecConfig::consoleLimit() const
{
    return mConsoleLimit;
}

const std::string& DobbySpecConfig::consolePath() const
{
    return mConsolePath;
}

const std::map<std::string, Json::Value>& DobbySpecConfig::legacyPlugins() const
{
    return mLegacyPlugins;
}

const std::map<std::string, Json::Value>& DobbySpecConfig::rdkPlugins() const
{
    return mRdkPlugins;
}

std::vector<DobbySpecConfig::MountPoint> DobbySpecConfig::mountPoints() const
{
    std::lock_guard<std::mutex> locker(mLock);
    return mMountPoints;
}

const std::string& DobbySpecConfig::etcHosts() const
{
    return mEtcHosts;
}

const std::string& DobbySpecConfig::etcServices() const
{
    return mEtcServices;
}

const std::string& DobbySpecConfig::etcPasswd() const
{
    return mEtcPasswd;
}

const std::string& DobbySpecConfig::etcGroup() const
{
    return mEtcGroup;
}

const std::string& DobbySpecConfig::etcLdSoPreload() const
{
    return mEtcLdSoPreload;
}

std::shared_ptr<rt_dobby_schema> DobbySpecConfig::config() const
{
    return mValid ? std::shared_ptr<rt_dobby_schema>(mConf) : nullptr;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Generates the OCI
 *
 *  This function will only work once, subsequent attempts to set the workspace
 *  path will fail.  The function is atomic, therefore it returns true you can
 *  guarantee it suck and will be set for the lifetime of the function.
 *
 *  @param[in]  dictionary      Pointer to the OCI dictionary to populate
 *  @param[in]  json            The json spec document from the client
 *  @param[in]  bundleFd        File descriptor of the bundle directory
 *
 *  @return true if the path was set, otherwise false.
 */
bool DobbySpecConfig::parseSpec(ctemplate::TemplateDictionary* dictionary,
                                const std::string& json,
                                int bundleFd)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // constant static std::map of json fields to function processors
    typedef bool (DobbySpecConfig::*ProcessorFunc)(const Json::Value&, ctemplate::TemplateDictionary*);
    typedef std::map<std::string, std::pair<unsigned, ProcessorFunc>> ProcessorMap;

    static const ProcessorMap processors =
    {
        { "env",            {   JSON_FLAG_ENV,              &DobbySpecConfig::processEnv            }   },
        { "args",           {   JSON_FLAG_ARGS,             &DobbySpecConfig::processArgs           }   },
        { "cwd",            {   JSON_FLAG_CWD,              &DobbySpecConfig::processCwd            }   },
        { "user",           {   JSON_FLAG_USER,             &DobbySpecConfig::processUser           }   },
        { "userNs",         {   JSON_FLAG_USERNS,           &DobbySpecConfig::processUserNs         }   },
        { "console",        {   JSON_FLAG_CONSOLE,          &DobbySpecConfig::processConsole        }   },
        { "etc",            {   JSON_FLAG_ETC,              &DobbySpecConfig::processEtc            }   },
        { "network",        {   JSON_FLAG_NETWORK,          &DobbySpecConfig::processNetwork        }   },
        { "rtPriority",     {   JSON_FLAG_RTPRIORITY,       &DobbySpecConfig::processRtPriority     }   },
        { "restartOnCrash", {   JSON_FLAG_RESTARTONCRASH,   &DobbySpecConfig::processRestartOnCrash }   },
        { "mounts",         {   JSON_FLAG_MOUNTS,           &DobbySpecConfig::processMounts         }   },
        { "plugins",        {   JSON_FLAG_PLUGINS,          &DobbySpecConfig::processLegacyPlugins  }   },
        { "memLimit",       {   JSON_FLAG_MEMLIMIT,         &DobbySpecConfig::processMemLimit       }   },
        { "gpu",            {   JSON_FLAG_GPU,              &DobbySpecConfig::processGpu            }   },
        { "vpu",            {   JSON_FLAG_VPU,              &DobbySpecConfig::processVpu            }   },
        { "dbus",           {   JSON_FLAG_DBUS,             &DobbySpecConfig::processDbus           }   },
        { "syslog",         {   JSON_FLAG_SYSLOG,           &DobbySpecConfig::processSyslog         }   },
        { "cpu",            {   JSON_FLAG_CPU,              &DobbySpecConfig::processCpu            }   },
        { "devices",        {   JSON_FLAG_DEVICES,          &DobbySpecConfig::processDevices        }   },
        { "capabilities",   {   JSON_FLAG_CAPABILITIES,     &DobbySpecConfig::processCapabilities   }   },
        { "seccomp",        {   JSON_FLAG_SECCOMP,          &DobbySpecConfig::processSeccomp        }   }
    };

    // step 1 - parse the 'dobby' spec document
    mSpec.clear();
    if (json.empty())
    {
        AI_LOG_ERROR_EXIT("invalid string");
        return false;
    }
    else
    {
        Json::Reader reader;
        if (!reader.parse(json, mSpec))
        {
            AI_LOG_ERROR_EXIT("failed to parse json - %s",
                              reader.getFormattedErrorMessages().c_str());
            return false;
        }
    }

    // step 2 - get the version number of the spec first, it may determine how
    // subsequent fields are processed
    const Json::Value version = mSpec["version"];
    if (!version.isString())
    {
        AI_LOG_ERROR_EXIT("json spec string doesn't have valid version field");
        return false;
    }

    const char *versionStr = version.asCString();
    if (strcmp(versionStr, "1.0") == 0)
    {
        mSpecVersion = SpecVersion::Version1_0;
    }
    else if (strcmp(versionStr, "1.1") == 0)
    {
        mSpecVersion = SpecVersion::Version1_1;
    }
    else
    {
        AI_LOG_ERROR_EXIT("json spec version number '%s' is not recognised",
                          versionStr);
        return false;
    }

    // step 3 - process all the fields, each processor should populate the
    // dictionary, or in some cases populate internal fields
    unsigned flags = 0;
    bool success = false;
    Json::Value::const_iterator it = mSpec.begin();
    for (; it != mSpec.end(); ++it)
    {
        ProcessorMap::const_iterator fn = processors.find(it.name());
        if (fn != processors.end())
        {
            AI_LOG_DEBUG("Processing %s", fn->first.c_str());
            // the following is super ugly, but allows us to call the function
            // pointers as methods within our own class
            const ProcessorFunc& method = fn->second.second;
            success = (this->*(method))(*it, dictionary);
            if (!success)
                break;

            // set the flag indicating we've processed the field
            flags |= fn->second.first;
        }
    }

    // step 4 - if successful check if all the mandatory fields were present
    // in the supplied json
    if (success)
    {
        const unsigned mandatoryFlags = JSON_FLAG_ARGS |
                                        JSON_FLAG_USER | JSON_FLAG_MEMLIMIT;
        if ((flags & mandatoryFlags) != mandatoryFlags)
        {
            // create a list of all the missing json fields to make life
            // easier for debugging
            std::string message;
            const unsigned missing = (flags ^ mandatoryFlags) & mandatoryFlags;
            for (unsigned bit = 0x1; bit != 0; bit <<= 1)
            {
                if (missing & bit)
                {
                    ProcessorMap::const_iterator jt = processors.begin();
                    for (; jt != processors.end(); ++jt)
                    {
                        if (jt->second.first == bit)
                        {
                            if (!message.empty())
                                message += ",";
                            message += jt->first;
                            break;
                        }
                    }
                }
            }

            AI_LOG_ERROR("missing the following mandatory field(s); %s "
                         "(flags:0x%06x, mandatory:0x%06x)",
                         message.c_str(), flags, mandatoryFlags);
            success = false;
        }
    }

    // step 5 - for any fields that haven't been set, ensure we set the defaults
    if (!(flags & JSON_FLAG_USERNS))
    {
        dictionary->ShowSection(USERNS_ENABLED);
    }

    if (!(flags & JSON_FLAG_NETWORK))
    {
        dictionary->ShowSection(NETNS_ENABLED);
    }

    if (!(flags & JSON_FLAG_RTPRIORITY))
    {
        dictionary->ShowSection(RTLIMIT_ENABLED);
        dictionary->SetIntValue(RLIMIT_RTPRIO, 0);
    }

    if (!(flags & JSON_FLAG_CAPABILITIES))
    {
        dictionary->SetValue(NO_NEW_PRIVS, "true");
    }

    // step 6 - enable the RDK plugins section
    dictionary->ShowSection(ENABLE_RDK_PLUGINS);

    // step 6.5 - add any default plugins in the settings file
    Json::Value rdkPluginData = mRdkPluginsData;
    for (const auto& pluginName : mDefaultPlugins)
    {
        mRdkPluginsJson[pluginName]["data"] = rdkPluginData[pluginName];
        mRdkPluginsJson[pluginName]["required"] = false;
    }

    // step 7 - process RDK plugins json into dictionary
    if (!processRdkPlugins(mSpec["rdkPlugins"], mDictionary))
    {
        AI_LOG_ERROR_EXIT("failed to process rdkPlugins");
        return false;
    }

    // step 8 - write dictionary to config file so that libocispec can continue
    // processing the config from here on out
    if (!DobbyTemplate::applyAt(bundleFd, "config.json", mDictionary, false))
    {
        AI_LOG_ERROR_EXIT("Failed to apply and write dictionary to config");
        return false;
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 * @brief Use the JsonCpp streamwriter builder to convert a Json object into
 * a string for use in ctemplate
 *
 * @param[in]   jsonObject  object to convert to string
 *
 * @return JSON string of the input object
 *
 */
std::string DobbySpecConfig::jsonToString(const Json::Value& jsonObject)
{
    Json::StreamWriterBuilder builder;
     // Do not indent json
    builder["indentation"] = "";

    return Json::writeString(builder, jsonObject);
}


// -----------------------------------------------------------------------------
/**
 *  @brief Processes the environment variable field of the json spec
 *
 *  Example json:
 *
 *      "env": [
 *          "ADDITIONAL_DATA_URL=monkey",
 *          "full_screen_opacity=1"
 *      ]
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processEnv(const Json::Value& value,
                             ctemplate::TemplateDictionary* dictionary)
{
    if (!value.isArray())
    {
        AI_LOG_ERROR("invalid env field");
        return false;
    }

    Json::Value::const_iterator it = value.begin();
    for (; it != value.end(); ++it)
    {
        const Json::Value& entry = *it;
        if (!entry.isString())
        {
            AI_LOG_ERROR("invalid env entry at index %u", it.index());
            return false;
        }

        ctemplate::TemplateDictionary* subDict = dictionary->AddSectionDictionary(ENV_VAR_SECTION);
        subDict->SetValue(ENV_VAR_VALUE, entry.asString());
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the args field of the json spec
 *
 *  Example json:
 *
 *          "args": [
 *              "sh"
 *          ]
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processArgs(const Json::Value& value,
                              ctemplate::TemplateDictionary* dictionary)
{
    if (!value.isArray())
    {
        AI_LOG_ERROR("invalid args field");
        return false;
    }

    Json::Value::const_iterator it = value.begin();
    for (; it != value.end(); ++it)
    {
        const Json::Value& entry = *it;
        if (!entry.isString())
        {
            AI_LOG_ERROR("invalid args entry at index %u", it.index());
            return false;
        }

        ctemplate::TemplateDictionary* subDict = dictionary->AddSectionDictionary(ARGS_VAR_SECTION);
        subDict->SetValue(ARGS_VAR_VALUE, entry.asString());
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the working directory field of the json spec
 *
 *  Example json:
 *
 *          "cwd": "/home"
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processCwd(const Json::Value& value,
                             ctemplate::TemplateDictionary* dictionary)
{
    static const ctemplate::TemplateString cwdValue("WORKING_DIRECTORY");

    if (!value.isString())
    {
        AI_LOG_ERROR("invalid cwd field");
        return false;
    }

    dictionary->SetValue(cwdValue, value.asString());
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the user field of the json spec
 *
 *  Example json:
 *
 *          "user": {
 *              "uid": 30001,
 *              "gid": 30000
 *          }
 *
 *  Any uid or gid is valid, except root (uid = 0).
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processUser(const Json::Value& value,
                              ctemplate::TemplateDictionary* dictionary)
{
    if (!value.isObject())
    {
        AI_LOG_ERROR("invalid user field");
        return false;
    }

    const Json::Value& uid = value["uid"];
    const Json::Value& gid = value["gid"];
    if (!uid.isIntegral() || !gid.isIntegral())
    {
        AI_LOG_ERROR("invalid uid or gid field");
        return false;
    }

    mUserId = uid.asUInt();
    mGroupId = gid.asUInt();

    // sanity check the uid and gid are valid, and make sure we aren't being
    // asked to start the container as root
    if (mUserId == 0)
    {
        AI_LOG_ERROR("the user.uid cannot be root (0)");
        return false;
    }
    if ((mUserId >= 65535) || (mGroupId >= 65535))
    {
        AI_LOG_ERROR("invalid uid or gid field, values must be less than 65535");
        return false;
    }

    dictionary->SetIntValue(USER_ID, mUserId);
    dictionary->SetIntValue(GROUP_ID, mGroupId);

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the userNs field of the json spec
 *
 *  Example json:
 *
 *          "userNs": true
 *          "userNs": false
 *
 *  This field controls whether to enable user namespacing or not, by default
 *  userns is enabled, it must explicitly be disabled.
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processUserNs(const Json::Value& value,
                                ctemplate::TemplateDictionary* dictionary)
{
    bool enabled;
    if (value.isBool())
    {
        enabled = value.asBool();
    }
    else if (value.isNull())
    {
        enabled = false;
    }
    else
    {
        AI_LOG_ERROR("invalid userNs field");
        return false;
    }

    dictionary->ShowSection(enabled ? USERNS_ENABLED : USERNS_DISABLED);

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the rtPriority field of the json spec
 *
 *  Example json (version 1.0 of spec)
 *
 *          "rtPriority": 4
 *
 *  Example json (version 1.1 of spec)
 *
 *          "rtPriority": {
 *              "default" : 4,
 *              "limit"   : 6
 *          }
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processRtPriority(const Json::Value& value,
                                        ctemplate::TemplateDictionary* dictionary)
{
    int rtPriorityDefault = 0;
    int rtPriorityLimit = 0;

    std::lock_guard<std::mutex> locker(mLock);
    if (mSpecVersion == SpecVersion::Version1_0)
    {
        if (!value.isIntegral())
        {
            AI_LOG_ERROR("invalid rtPriority field");
            return false;
        }

        rtPriorityDefault = value.asInt();
    }
    else if (mSpecVersion == SpecVersion::Version1_1)
    {
        if (!value.isObject())
        {
            AI_LOG_ERROR("invalid rtPriority field");
            return false;
        }

        const Json::Value& default_ = value["default"];
        if (default_.isIntegral())
        {
            rtPriorityDefault = default_.asInt();
        }
        else if (!default_.isNull())
        {
            AI_LOG_ERROR("invalid rtPriority.default field");
            return false;
        }

        const Json::Value& limit = value["limit"];
        if (limit.isIntegral())
        {
            rtPriorityLimit = limit.asInt();
        }
        else if (!limit.isNull())
        {
            AI_LOG_ERROR("invalid rtPriority.limit field");
            return false;
        }
    }

    // Write values to the rdk plugin
    Json::Value rdkPluginData;
    rdkPluginData["rtlimit"] = rtPriorityLimit;
    rdkPluginData["rtdefault"] = rtPriorityDefault;
    mRdkPluginsJson[RDK_RTSCHEDULING_PLUGIN_NAME]["data"] = rdkPluginData;
    mRdkPluginsJson[RDK_RTSCHEDULING_PLUGIN_NAME]["required"] = false;

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the console field of the json spec
 *
 *  Example json:
 *
 *          "console": {
 *              "path": "/mnt/apps/console.log",
 *              "limit": 1024
 *          }
 *
 *  Or
 *
 *          "console": null
 *
 *  The console settings aren't given to the crun, instead it determines the
 *  type of stream that we attach to read the console output.
 *
 *  Typically on debug builds the console will be redirected to a file on the
 *  flash.  On release builds it is redirected to /dev/null.
 *
 *  If console is null, then the terminal stdin / stdout & stderr are all
 *  redirected to /dev/null.  If not null then stdout & stderr are redirected
 *  into the supplied file, with an optional size limit on it.
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processConsole(const Json::Value& value,
                                 ctemplate::TemplateDictionary* dictionary)
{
    if (value.isNull())
    {
        mConsoleDisabled = true;

        // Even though console is disabled, we must still add the logging plugin so that
        // the container ptty is configured correctly and there is something on the receiving
        // end to drain it
        Json::Value rdkPluginData;
        rdkPluginData["sink"] = "devnull";
        mRdkPluginsJson[RDK_LOGGING_PLUGIN_NAME]["data"] = rdkPluginData;
        mRdkPluginsJson[RDK_LOGGING_PLUGIN_NAME]["required"] = false;

        return true;
    }

    if (!value.isObject())
    {
        AI_LOG_ERROR("invalid console field");
        return false;
    }

    const Json::Value& path = value["path"];
    if (path.isNull())
    {
        AI_LOG_WARN("Console option set but no path provided - cannot enable console redirection");

        mConsoleDisabled = true;

        // Even though console is disabled, we must still add the logging plugin so that
        // the container ptty is configured correctly and there is something on the receiving
        // end to drain it
        Json::Value rdkPluginData;
        rdkPluginData["sink"] = "devnull";
        mRdkPluginsJson[RDK_LOGGING_PLUGIN_NAME]["data"] = rdkPluginData;
        mRdkPluginsJson[RDK_LOGGING_PLUGIN_NAME]["required"] = false;

        return true;
    }
    else if (path.isString())
    {
        mConsolePath = path.asString();
    }
    else
    {
        AI_LOG_ERROR("invalid console.path field");
        return false;
    }

    const Json::Value& limit = value["limit"];
    if (limit.isNull())
    {
        mConsoleLimit = -1;
    }
    else if (limit.isIntegral())
    {
        mConsoleLimit = limit.asInt();
        mConsoleLimit = std::max<ssize_t>(mConsoleLimit, -1);
    }
    else
    {
        AI_LOG_ERROR("invalid console.limit field");
        return false;
    }

    mConsoleDisabled = false;

    // We've got this far, so console is enabled - set console settings in
    // the RDK logging plugin
    Json::Value rdkPluginData;
    rdkPluginData["sink"] = "file";
    rdkPluginData["fileOptions"]["path"] = mConsolePath;
    rdkPluginData["fileOptions"]["limit"] = static_cast<int>(mConsoleLimit);
    mRdkPluginsJson[RDK_LOGGING_PLUGIN_NAME]["data"] = rdkPluginData;
    mRdkPluginsJson[RDK_LOGGING_PLUGIN_NAME]["required"] = false;

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the dbus field
 *
 *  Example json:
 *
 *      "dbus": {
 *          "session": "ai-public",
 *          "system": "system",
 *          "debug": "ai-private"
 *      }
 *
 *      "dbus": {
 *          "system": "ai-private",
 *          "session": "ai-public"
 *      }
 *
 *
 *  This config options allow you to specify what bus to map into the container
 *  and what to call it inside the container.
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Ignored
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processDbus(const Json::Value& value,
                              ctemplate::TemplateDictionary* dictionary)
{
    static const std::map<std::string, IDobbyIPCUtils::BusType> busTypes =
    {
        {   "system",       IDobbyIPCUtils::BusType::SystemBus     },
        {   "ai-public",    IDobbyIPCUtils::BusType::AIPublicBus   },
        {   "ai-private",   IDobbyIPCUtils::BusType::AIPrivateBus  },
    };

    bool enableDbusPlugin = false;
    Json::Value rdkPluginData;

    // process the system dbus field
    {
        const Json::Value& system = value["system"];
        if (system.isString())
        {
            std::map<std::string, IDobbyIPCUtils::BusType>::const_iterator it = busTypes.find(system.asString());
            if (it == busTypes.end())
            {
                AI_LOG_ERROR("invalid 'dbus.system' field");
                return false;
            }

            mSystemDbus = it->second;

            rdkPluginData["system"] = system.asString();
            enableDbusPlugin = true;
        }
        else if (!system.isNull())
        {
            AI_LOG_ERROR("invalid 'dbus.system' field");
            return false;
        }
    }

    // process the session dbus field
    {
        const Json::Value& session = value["session"];
        if (session.isString())
        {
            std::map<std::string, IDobbyIPCUtils::BusType>::const_iterator it = busTypes.find(session.asString());
            if (it == busTypes.end())
            {
                AI_LOG_ERROR("invalid 'dbus.session' field");
                return false;
            }

            mSessionDbus = it->second;

            rdkPluginData["session"] = session.asString();
            enableDbusPlugin = true;
        }
        else if (!session.isNull())
        {
            AI_LOG_ERROR("invalid 'dbus.session' field");
            return false;
        }
    }

#if (AI_BUILD_TYPE == AI_DEBUG)
    // process the debug dbus field
    {
        const Json::Value& debug = value["debug"];
        if (debug.isString())
        {
            std::map<std::string, IDobbyIPCUtils::BusType>::const_iterator it = busTypes.find(debug.asString());
            if (it == busTypes.end())
            {
                AI_LOG_ERROR("invalid 'dbus.debug' field");
                return false;
            }

            mDebugDbus = it->second;

            rdkPluginData["debug"] = debug.asString();
            enableDbusPlugin = true;
        }
        else if (!debug.isNull())
        {
            AI_LOG_ERROR("invalid 'dbus.debug' field");
            return false;
        }
    }
#endif

    // If we have any buses, set up the IPC RDK plugin
    if (enableDbusPlugin)
    {
        mRdkPluginsJson[RDK_IPC_PLUGIN_NAME]["data"] = rdkPluginData;
        mRdkPluginsJson[RDK_IPC_PLUGIN_NAME]["required"] = false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the restart on crash field
 *
 *  Example json:
 *
 *      "restartOnCrash": true
 *
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Ignored
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processRestartOnCrash(const Json::Value& value,
                                        ctemplate::TemplateDictionary* dictionary)
{
    if (value.isNull())
    {
        mRestartOnCrash = false;
    }
    else if (value.isBool())
    {
        mRestartOnCrash = value.asBool();
    }
    else
    {
        AI_LOG_ERROR("invalid restartOnCrash field");
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the memory limit field
 *
 *  Example json:
 *
 *      "memLimit": 1024564
 *
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processMemLimit(const Json::Value& value,
                                  ctemplate::TemplateDictionary* dictionary)
{
    if (!value.isIntegral())
    {
        AI_LOG_ERROR("invalid memLimit field");
        return false;
    }

    // the memory limit should ideally be a 64-bit value, but ctemplate doesn't
    // support that and all boxes have <= 3GB of memory so meh
    unsigned memLimit = value.asUInt();

    if (memLimit < (256 * 1024))
    {
        AI_LOG_WARN("memory limit looks dangerously low");
    }

    dictionary->SetIntValue(MEM_LIMIT, memLimit);

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds the GPU device nodes (if any) to supplied dictionary.
 *
 *  This function gathers the dev node details from the settings and system the
 *  first time it runs, for all subsequent times it uses the initial cached
 *  details.
 *
 *  @param[in]  settings    The settings containing the list of dev nodes paths
 *                          or glob patterns.  Only used the first time called.
 *  @param[in]  dictionary  The dictionary to add the details to.
 *
 */
void DobbySpecConfig::addGpuDevNodes(const std::shared_ptr<const IDobbySettings::HardwareAccessSettings> &settings,
                                 ctemplate::TemplateDictionary *dictionary)
{
    // device nodes should be static, so get the details once and store for
    // all subsequent calls
    static std::mutex scanningLock;
    static std::atomic<bool> scannedDevNodes(false);
    static std::list<DobbyConfig::DevNode> devNodes;

    if (!scannedDevNodes)
    {
        std::lock_guard<std::mutex> locker(scanningLock);

        if (!scannedDevNodes)
        {
            devNodes = scanDevNodes(settings->deviceNodes);
            scannedDevNodes = true;
        }
    }

    // add to the additional device node section
    for (const DobbyConfig::DevNode &devNode : devNodes)
    {
        ctemplate::TemplateDictionary *subDict = dictionary->AddSectionDictionary(ADDITIONAL_DEVICE_NODES);
        subDict->SetValue(DEVICE_PATH, devNode.path);
        subDict->SetIntValue(DEVICE_MAJOR, devNode.major);
        subDict->SetIntValue(DEVICE_MINOR, devNode.minor);
        subDict->SetIntValue(DEVICE_FILE_MODE, devNode.mode);
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds the VPU device nodes (if any) to supplied dictionary.
 *
 *  This function gathers the dev node details from the settings and system the
 *  first time it runs, for all subsequent times it uses the initial cached
 *  details.
 *
 *  @param[in]  settings    The settings containing the list of dev nodes paths
 *                          or glob patterns.  Only used the first time called.
 *  @param[in]  dictionary  The dictionary to add the details to.
 *
 */
void DobbySpecConfig::addVpuDevNodes(const std::shared_ptr<const IDobbySettings::HardwareAccessSettings> &settings,
                                 ctemplate::TemplateDictionary *dictionary)
{
    // device nodes should be static, so get the details once and store for
    // all subsequent calls
    static std::mutex scanningLock;
    static std::atomic<bool> scannedDevNodes(false);
    static std::list<DobbyConfig::DevNode> devNodes;

    if (!scannedDevNodes)
    {
        std::lock_guard<std::mutex> locker(scanningLock);

        if (!scannedDevNodes)
        {
            devNodes = scanDevNodes(settings->deviceNodes);
            scannedDevNodes = true;
        }
    }

    // add to the additional device node section
    for (const DobbyConfig::DevNode &devNode : devNodes)
    {
        ctemplate::TemplateDictionary *subDict = dictionary->AddSectionDictionary(ADDITIONAL_DEVICE_NODES);
        subDict->SetValue(DEVICE_PATH, devNode.path);
        subDict->SetIntValue(DEVICE_MAJOR, devNode.major);
        subDict->SetIntValue(DEVICE_MINOR, devNode.minor);
        subDict->SetIntValue(DEVICE_FILE_MODE, devNode.mode);
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the gpu field, which contains enable and memLimit values.
 *
 *  Example json:
 *
 *      "gpu": {
 *          "enable": true,
 *          "memLimit": 1024564
 *      }
 *
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processGpu(const Json::Value& value,
                             ctemplate::TemplateDictionary* dictionary)
{
    bool gpuEnabled;
    Json::Value rdkPluginData;

    const Json::Value& enable = value["enable"];
    const Json::Value& memLimit = value["memLimit"];

    if (enable.isBool())
    {
        gpuEnabled = enable.asBool();
    }
    else if (enable.isNull())
    {
        gpuEnabled = false;
    }
    else
    {
        AI_LOG_ERROR("invalid 'gpu.enable' field");
        return false;
    }

    if (memLimit.isIntegral())
    {
        rdkPluginData["memory"] = memLimit.asUInt();
    }
    else if (memLimit.isNull())
    {
        // use default memory limit
        rdkPluginData["memory"] = 64 * 1024 * 1024;
    }
    else
    {
        AI_LOG_ERROR("invalid 'gpu.memLimit' field");
        return false;
    }

    if (gpuEnabled)
    {
        // lazily init the GPU dev nodes mapping - we use to do this at start-up
        // but hit an issue on broadcom platforms where the dev nodes aren't
        // created until the gpu library is used
        addGpuDevNodes(mGpuSettings, dictionary);

        // check if a special 'GPU' group id is needed
        for (const int gid : mGpuSettings->groupIds)
        {
            dictionary->
                    AddSectionDictionary(ADDITIONAL_GIDS)->
                    SetIntValue(ADDITIONAL_GID, gid);
        }

        // add any extra mounts (ie ipc sockets, shared memory files, etc)
        for (const IDobbySettings::ExtraMount &extraMount : mGpuSettings->extraMounts)
        {
            ctemplate::TemplateDictionary *subDict = dictionary->AddSectionDictionary(MOUNT_SECTION);
            subDict->SetValue(MOUNT_SRC, extraMount.source);
            subDict->SetValue(MOUNT_DST, extraMount.target);
            subDict->SetValue(MOUNT_TYPE, extraMount.type);

            for (const std::string& flag : extraMount.flags)
            {
                ctemplate::TemplateDictionary *optSubDict = subDict->AddSectionDictionary(MOUNT_OPT_SECTION);
                optSubDict->SetValue(MOUNT_OPT, flag);
            }

            // store the mount point for rootfs construction
            storeMountPoint(extraMount.type, extraMount.source, extraMount.target);
        }


        // Enable the RDK GPU plugin to set gpu memory limit
        mRdkPluginsJson[RDK_GPU_PLUGIN_NAME]["data"] = rdkPluginData;
        mRdkPluginsJson[RDK_GPU_PLUGIN_NAME]["required"] = false;
    }

    // and any extra environment variables
    for (const auto& extraEnvVar : mGpuSettings->extraEnvVariables)
    {
        ctemplate::TemplateDictionary *envSubDict = dictionary->AddSectionDictionary(ENV_VAR_SECTION);
        envSubDict->SetFormattedValue(ENV_VAR_VALUE, "%s=%s",
                                      extraEnvVar.first.c_str(),
                                      extraEnvVar.second.c_str());
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the vpu field, which is used to enable access to the VPU.
 *
 *  Example json:
 *
 *      "vpu": {
 *          "enable": true,
 *      }
 *
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processVpu(const Json::Value& value,
                             ctemplate::TemplateDictionary* dictionary)
{
    // check VPU access should be enabled
    const Json::Value& enable = value["enable"];
    if (enable.isBool())
    {
        if (!enable.asBool())
        {
            // vpu not enabled, just return
            return true;
        }
    }
    else if (enable.isNull())
    {
        // not an error but means vpu is not enabled
        return true;
    }
    else
    {
        AI_LOG_ERROR("invalid 'vpu.enable' field");
        return false;
    }


    // add the VPU dev nodes
    addVpuDevNodes(mVpuSettings, dictionary);

    // check if a special 'VPU' group id(s) are needed
    for (const int gid : mVpuSettings->groupIds)
    {
        dictionary->
                AddSectionDictionary(ADDITIONAL_GIDS)->
                SetIntValue(ADDITIONAL_GID, gid);
    }

    // add any extra mounts (ie ipc sockets, shared memory files, etc)
    for (const auto& extraMount : mVpuSettings->extraMounts)
    {
        ctemplate::TemplateDictionary *subDict = dictionary->AddSectionDictionary(MOUNT_SECTION);
        subDict->SetValue(MOUNT_SRC, extraMount.source);
        subDict->SetValue(MOUNT_DST, extraMount.target);
        subDict->SetValue(MOUNT_TYPE, extraMount.type);

        for (const std::string& flag : extraMount.flags)
        {
            ctemplate::TemplateDictionary *optSubDict = subDict->AddSectionDictionary(MOUNT_OPT_SECTION);
            optSubDict->SetValue(MOUNT_OPT, flag);
        }

        // store the mount point for rootfs construction
        storeMountPoint(extraMount.type, extraMount.source, extraMount.target);
    }

    // and any extra environment variables
    for (const auto& extraEnvVar : mVpuSettings->extraEnvVariables)
    {
        ctemplate::TemplateDictionary *envSubDict = dictionary->AddSectionDictionary(ENV_VAR_SECTION);
        envSubDict->SetFormattedValue(ENV_VAR_VALUE, "%s=%s",
                                      extraEnvVar.first.c_str(),
                                      extraEnvVar.second.c_str());
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the network field
 *
 *  Example json:
 *
 *      "network": "nat"
 *      "network": "open"
 *      "network": "private"
 *
 *  The network can be either 'nat', 'open' or 'private', the default is
 *  private. 'private' is translated to 'none' to match Networking plugin.
 *
 *  @see refer to the Networking plugin for more details.
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processNetwork(const Json::Value& value,
                                 ctemplate::TemplateDictionary* dictionary)
{
    Json::Value rdkPluginData;

    if (value.isNull())
    {
        rdkPluginData["type"] = "none";

        return true;
    }
    else if (!value.isString())
    {
        AI_LOG_ERROR("invalid network field, should be a string type");
        return false;
    }

    const std::string type = value.asString();
    if (type == "nat")
    {
        rdkPluginData["type"] = "nat";
#if !defined(DEV_VM)
        rdkPluginData["dnsmasq"] = true;
#endif
        rdkPluginData["ipv4"] = true;
    }
    else if (type == "open")
    {
        rdkPluginData["type"] = "open";
#if !defined(DEV_VM)
        rdkPluginData["dnsmasq"] = true;
#endif
    }
    else if (type == "private")
    {
        rdkPluginData["type"] = "none";
    }
    else
    {
        AI_LOG_ERROR("invalid network field value '%s'", type.c_str());
        return false;
    }

    // Enable the RDK Networking plugin
    mRdkPluginsJson[RDK_NETWORK_PLUGIN_NAME]["data"] = rdkPluginData;
    mRdkPluginsJson[RDK_NETWORK_PLUGIN_NAME]["required"] = false;

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the syslog field, which should be a boolean
 *
 *  Example json:
 *
 *      "syslog": true
 *      "syslog": false
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processSyslog(const Json::Value& value,
                                ctemplate::TemplateDictionary* dictionary)
{
    if (value.isNull())
    {
        // a null value is equivalent to false
        return true;
    }
    else if (!value.isBool())
    {
        AI_LOG_ERROR("invalid 'syslog' field, should be a boolean type");
        return false;
    }

    if (value.asBool() == true)
    {
        dictionary->ShowSection(SYSLOG_SECTION);
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Parses a string to create a bitset for the appropriate bits set
 *
 *  @note This code has been borrowed from the linux kernel's __bitmap_parselist
 *  function.
 *
 *  Input format is a comma-separated list of decimal numbers and ranges.
 *  Consecutively set bits are shown as two hyphen-separated decimal numbers,
 *  the smallest and largest bit numbers set in the range.
 *
 *  @param[in]  str         The string containing the bits set
 *
 *  @return the set of bits in the string, in case of an error an empty bitset
 *  will be returned.
 */
template <std::size_t N>
std::bitset<N> DobbySpecConfig::parseBitset(const std::string& str) const
{
    const char *buf = str.c_str();
    size_t buflen = str.length();

    size_t a, b;
    int c, old_c, totaldigits;
    bool exp_digit, in_range;

    std::bitset<N> bits;

    totaldigits = c = 0;
    do
    {
        exp_digit = true;
        in_range = false;
        a = b = 0;

        // get the next cpu# or a range of cpu#'s
        while (buflen)
        {
            old_c = c;
            c = *buf++;
            buflen--;
            if (isspace(c))
                continue;

            // if the last character was a space and the current character isn't
            // '\0', we've got embedded whitespace. This is a no-no, so throw
            // an error.
            if (totaldigits && c && isspace(old_c))
                return std::bitset<N>();

            // A '\0' or a ',' signal the end of a cpu# or range
            if (c == '\0' || c == ',')
                break;

            if (c == '-')
            {
                if (exp_digit || in_range)
                    return std::bitset<N>();
                b = 0;
                in_range = true;
                exp_digit = true;
                continue;
            }

            if (!isdigit(c))
                return -EINVAL;

            b = b * 10 + (c - '0');
            if (!in_range)
                a = b;
            exp_digit = false;
            totaldigits++;
        }

        if (!(a <= b))
            return std::bitset<N>();
        if (b >= N)
            return std::bitset<N>();
        while (a <= b)
        {
            bits.set(a);
            a++;
        }

    } while (buflen && c == ',');

    return bits;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the cpu field, which is an optional object json type
 *
 *  Example json:
 *
 *      "cpu": {
 *          "shares": 50,
 *          "cores": "0-1,3"
 *      }
 *
 *  'shares' specifies a relative share of CPU time available to the tasks in a
 *  cgroup as a percentage, must be equal or less than 100.
 *
 *  'cores" list of CPUs the container will run in. This is a comma-separated
 *  list, with dashes ("-") to represent ranges.
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processCpu(const Json::Value& value,
                             ctemplate::TemplateDictionary* dictionary)
{
    const Json::Value& shares = value["shares"];
    const Json::Value& cores = value["cores"];


    // get the shares value which should be a value between 1 and 100 (percentage)
    int sharesValue = -1;
    if (shares.isIntegral())
    {
        sharesValue = shares.asInt();
        if ((sharesValue <= 0) || (sharesValue > 100))
        {
            AI_LOG_ERROR("invalid 'shares' value %d (0 < shares <= 100)",
                        sharesValue);
            return false;
        }
    }
    else if (!shares.isNull())
    {
        AI_LOG_ERROR("invalid 'shares' field");
        return false;
    }

    // enable the option in the OCI spec template
    if (sharesValue > 0)
    {
        // the shares value is relative to all other cgroups, by default the
        // root cgroup (which contains everything outside a container) has
        // a share value of 1024, meaning we take the percentage value above
        // convert it to a value between 2 and 1024
        float actualShare = (1024.0f / 100.0f) * static_cast<float>(sharesValue);

        char sharesString[32];
        sprintf(sharesString, "%d", static_cast<int>(actualShare));

        dictionary->SetValueAndShowSection(CPU_SHARES_VALUE, sharesString,
                                           CPU_SHARES_ENABLED);
    }



    // parses the core string and convert to a bitmask
    std::string cpus;
    if (cores.isString())
    {
        const size_t nMaxCores = 8;

        std::bitset<nMaxCores> coreBits = parseBitset<nMaxCores>(cores.asString());
        if (coreBits.none())
        {
            AI_LOG_ERROR("invalid 'cores' value '%s' (empty bitset)",
                         cores.asCString());
            return false;
        }

        // trim the number of cores based on the actual number on the current
        // processor
        std::ostringstream cpusStream;
        for (int n = 0; n < std::min<int>(nMaxCores, mNumCores); n++)
        {
            if (coreBits.test(n))
                cpusStream << n << ",";
        }

        // the output should be a comma delimited list of cores to run the
        // container on
        cpus = cpusStream.str();
        if (cpus.length() > 1)
            cpus.pop_back();
    }
    else if (!cores.isNull())
    {
        AI_LOG_ERROR("invalid 'cores' field");
        return false;
    }

    // write the comma delimited list of cores
    if (!cpus.empty())
    {
        dictionary->SetValueAndShowSection(CPU_CPUS_VALUE, cpus,
                                           CPU_CPUS_ENABLED);
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Stores the mount point internally so it can be created by in the
 *  rootfs component.
 *
 *
 *  @param[in]  type        The type of the mount.
 *  @param[in]  source      The mount source.
 *  @param[in]  source      The mount destination within the rootfs.
 *
 */
void DobbySpecConfig::storeMountPoint(const std::string &type,
                                  const std::string &source,
                                  const std::string &destination)
{
    MountPoint::Type mountType = MountPoint::Directory;

    // most mount points are directories, but if bind mounting a file then the
    // mount point should be a file
    if ((type == "bind") || (type == "rbind"))
    {
        struct stat buf;
        if (stat(source.c_str(), &buf) != 0)
        {
            AI_LOG_SYS_WARN(errno, "failed to stat source of mount '%s'",
                            source.c_str());
        }
        else
        {
            mountType = ((buf.st_mode & S_IFMT) == S_IFDIR) ?
                        MountPoint::Directory : MountPoint::File;
        }
    }

    // store the mount point internally
    mMountPoints.emplace_back(MountPoint{ mountType, destination });
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the mounts field of the json spec
 *
 *  Example json:
 *
 *      "mounts": [
 *          {
 *              "destination": "/home/private",
 *              "type": "loop",
 *              "source": "/mnt/apps/data.img",
 *              "options": [ "nosuid", "strictatime", "mode=755", "size=65536k" ]
 *          }
 *      ]
 *
 *  If mount type is "loop" then the mount is implemented as a prestart hook as
 *  crun doesn't support loop back mounts by default.
 *
 *  For non-loop mounts we just pass the parameters through to the OCI spec
 *  json for crun to handle.
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processMounts(const Json::Value& value,
                                ctemplate::TemplateDictionary* dictionary)
{
    if (!value.isArray())
    {
        AI_LOG_ERROR("invalid mounts field");
        return false;
    }

    // We only need to enable the RDK Storage plugin for loop mounts here
    // Everything else can be handled by OCI
    int numLoopMounts = 0;
    Json::Value rdkPluginData;

    Json::Value::const_iterator it = value.begin();
    for (; it != value.end(); ++it)
    {
        const Json::Value& mount = *it;
        if (!mount.isObject())
        {
            AI_LOG_ERROR("invalid mounts entry at index %u", it.index());
            return false;
        }

        const Json::Value& src = mount["source"];
        const Json::Value& dest = mount["destination"];
        const Json::Value& type = mount["type"];

        if (!src.isString() || !dest.isString() || !type.isString())
        {
            AI_LOG_ERROR("invalid mounts entry at index %u", it.index());
            return false;
        }

        const std::string strSrc = src.asString();
        const std::string strDest = dest.asString();
        const std::string strType = type.asString();

        // loop mounts are special, they aren't handled by crun, instead we
        // install a pre-start hook that does the mounting inside the mount
        // namespace of app.
        if (strType == "loop")
        {
            Json::Value loopMountData;
            if (!processLoopMount(mount, dictionary, loopMountData))
            {
                // if failed to parse the loop mount details then also pass
                // the failure on up the chain
                return false;
            }

            // Create an array item for the RDK Storage plugin
            rdkPluginData["loopback"][numLoopMounts] = loopMountData;
            numLoopMounts++;
        }
        else
        {
            // just a regular mount so add all the mount options to the OCI
            // dictionary so crun can do the mounting for us
            ctemplate::TemplateDictionary* subDict = dictionary->AddSectionDictionary(MOUNT_SECTION);
            subDict->SetValue(MOUNT_SRC, strSrc);
            subDict->SetValue(MOUNT_DST, strDest);
            subDict->SetValue(MOUNT_TYPE, strType);

            // add any extra mount options
            const Json::Value& opts = mount["options"];
            if (opts.isArray())
            {
                Json::Value::const_iterator jt = opts.begin();
                for (; jt != opts.end(); ++jt)
                {
                    const Json::Value& opt = *jt;
                    if (!opt.isString())
                    {
                        AI_LOG_ERROR("invalid mounts option entry at index %u:%u",
                                     it.index(), jt.index());
                        return false;
                    }

                    ctemplate::TemplateDictionary* optSubDict = subDict->AddSectionDictionary(MOUNT_OPT_SECTION);
                    optSubDict->SetValue(MOUNT_OPT, opt.asString());
                }
            }
        }

        // store the mount point for the rootfs construction
        storeMountPoint(strType, strSrc, strDest);
    }

    // If we need the storage plugin
    if (numLoopMounts > 0)
    {
        mRdkPluginsJson[RDK_STORAGE_PLUGIN_NAME]["data"] = rdkPluginData;
        mRdkPluginsJson[RDK_STORAGE_PLUGIN_NAME]["required"] = false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes a loop mount field of the json spec
 *
 *  Example json:
 *
 *      {
 *          "destination": "/home/private",
 *          "type": "loop",
 *          "fstype": "ext4",
 *          "source": "/mnt/apps/data.img",
 *          "options": [ "nosuid", "nodev", "noexec", "strictatime" ]
 *      }
 *
 *
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Ignored
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processLoopMount(const Json::Value& value,
                                    ctemplate::TemplateDictionary* dictionary,
                                    Json::Value& loopMntData)
{
    if (!value.isObject())
    {
        AI_LOG_ERROR("invalid loop mount field");
        return false;
    }

    // check the mandatory string fields
    const Json::Value& source = value["source"];
    const Json::Value& destination = value["destination"];
    const Json::Value& fstype = value["fstype"];

    if (!source.isString() || !destination.isString() || !fstype.isString())
    {
        AI_LOG_ERROR("one or more of the manadatory loop mount data fields is"
                     " missing or invalid");
        return false;
    }

    // create a loop mount object
    LoopMount mount;
    mount.fsImagePath = source.asString();
    mount.fsImageType = fstype.asString();
    mount.destination = destination.asString();
    mount.mountFlags  = 0;

    // sanity/security check that the destination directory is canonicalised
    // (i.e. doesn't have any '../' in it)
    if (mount.destination.find("..") != std::string::npos)
    {
        AI_LOG_ERROR("loop mount destination path not canonicalised");
        return false;
    }

    // check for any options
    const Json::Value& options = value["options"];

    Json::Value rdkMountOpts;

    if (options.isArray())
    {
        // we only support the standard options; ro, sync, nosuid, noexec, etc
        static const std::map<std::string, unsigned long> mountFlags =
        {
            {   "ro",           MS_RDONLY       },
            {   "sync",         MS_SYNCHRONOUS  },
            {   "nosuid",       MS_NOSUID       },
            {   "dirsync",      MS_DIRSYNC      },
            {   "nodiratime",   MS_NODIRATIME   },
            {   "relatime",     MS_RELATIME     },
            {   "noexec",       MS_NOEXEC       },
            {   "nodev",        MS_NODEV        },
            {   "noatime",      MS_NOATIME      },
            {   "strictatime",  MS_STRICTATIME  },
        };

        // iterate through the options adding them to the bitfield, if they
        // are not recognised then add to the mount data string
        int index = 0;
        Json::Value::const_iterator it = options.begin();
        for (; it != options.end(); ++it)
        {
            const Json::Value& option = *it;
            if (!option.isString())
            {
                AI_LOG_ERROR("invalid loop mount option entry at index %u",
                             it.index());
                return false;
            }

            std::map<std::string, unsigned long>::const_iterator opt =
                mountFlags.find(option.asString());
            if (opt != mountFlags.end())
            {
                mount.mountFlags |= opt->second;
            }
            else
            {
                mount.mountOptions.push_back(option.asString());
                rdkMountOpts[index] = option.asString();
                index++;
            }
        }
    }
    else if (!options.isNull())
    {
        AI_LOG_ERROR("invalid options field, it should be an array or null");
        return false;
    }

    // Set up the RDK Storage plugin data
    loopMntData["source"] = mount.fsImagePath;
    loopMntData["destination"] = mount.destination;
    loopMntData["fstype"] = mount.fsImageType;

    // TODO:: Depending on the exact plugin implementation, we may want to copy over
    // the mounts directly from the dobby spec, rather than using flags.
    Json::UInt64 flags = mount.mountFlags;
    loopMntData["flags"] = flags;
    loopMntData["options"] = rdkMountOpts;

    // disable management of the image to maintain backwards compatibly
    loopMntData["imgmanagement"] = false;

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the hooks field of the json spec
 *
 *  Example json:
 *
 *      "plugins": [
 *          {
 *              "name": "jumper",
 *              "data": [
 *                  {
 *                      "src": {
 *                          "ip": "127.0.0.1",
 *                          "port": 9001,
 *                          "inContainer": false
 *                      },
 *                      "dst": {
 *                          "ip": "127.0.0.1",
 *                          "port": 9001,
 *                          "inContainer": true
 *                      }
 *                  }
 *              ]
 *          },
 *          {
 *              "name": "filemapper",
 *              "data": {
 *                  "port": 1234
 *              }
 *          }
 *      ],
 *
 *  This adds prestart and poststop hooks into the container, the name of the
 *  hook should refer to file name in the hooks directory.
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processLegacyPlugins(const Json::Value& value,
                                 ctemplate::TemplateDictionary* dictionary)
{
    if (!value.isArray())
    {
        AI_LOG_ERROR("invalid hooks field");
        return false;
    }

    Json::Value::const_iterator it = value.begin();
    for (; it != value.end(); ++it)
    {
        const Json::Value& plugin = *it;
        if (!plugin.isObject())
        {
            AI_LOG_ERROR("invalid hook entry at index %u", it.index());
            return false;
        }

        // the name field must be a string
        const Json::Value& name = plugin["name"];
        if (!name.isString())
        {
            AI_LOG_ERROR("invalid hook.name entry at index %u", it.index());
            return false;
        }

        // the data can be anything, we don't place any restrictions on it since
        // it's just passed to the hook library for processing
        const Json::Value& data = plugin["data"];


        // add the hook to the list
        mLegacyPlugins.emplace(name.asString(), data);

        // Convert the explicit plugins into RDK extended OCI bundle syntax
        dictionary->ShowSection(ENABLE_LEGACY_PLUGINS);
        ctemplate::TemplateDictionary* pluginDict = dictionary->AddSectionDictionary(DOBBY_PLUGIN_SECTION);
        pluginDict->SetValue(PLUGIN_NAME, name.asString());

        std::string pluginDataString = jsonToString(data);
        pluginDict->SetValue(PLUGIN_DATA, pluginDataString);
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the etc field of the json spec
 *
 *  Example json:
 *
 *      "etc": {
 *          "services": [
 *              {
 *                  "name": "as",
 *                  "port": 1234,
 *                  "protocol": "tcp",
 *                  "aliases": [ "appservices" ]
 *              }
 *          ],
 *          "hosts": [
 *              {
 *                  "ip": "127.0.0.1",
 *                  "hostname": "as",
 *                  "aliases": [ "something" ]
 *              }
 *          ],
 *          "passwd": [
 *              {
 *                  "name": "30000",
 *                  "password": null,
 *                  "uid": 30000,
 *                  "gid": 30000,
 *                  "gecos": null,
 *                  "directory": null,
 *                  "shell": null
 *              }
 *          ],
 *          "group": [
 *              {
 *                  "name": "30000",
 *                  "password": null,
 *                  "gid": 30000,
 *                  "users": [ "30000", "30001" ]
 *              }
 *          ]
 *      }
 *
 *  None of the parsed etc values get put in the template dictionary as the
 *  OCI spec doesn't have any notion of /etc files. So instead the contents
 *  get stored internally and it's up to users of this class to call one of
 *  the etcXXXX functions to get the contents for those files.
 *
 *  None of the top level fields are manadatory (i.e. services, hosts, etc)
 *  but if they are present then some of the sub-fields may be manadatory.
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Ignore
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processEtc(const Json::Value& value,
                             ctemplate::TemplateDictionary*)
{
#if 0
    // constant static std::map of json fields to function processors
    typedef bool (DobbySpecConfig::*EtcProcessorFunc)(const Json::Value&, ctemplate::TemplateDictionary*);
    static const std::map<std::string, EtcProcessorFunc> etcProcessors =
    {
        { "services",       &DobbySpecConfig::processEtcServices    },
        { "hosts",          &DobbySpecConfig::processEtcHosts       },
        { "passwd",         &DobbySpecConfig::processEtcPasswd      },
        { "group",          &DobbySpecConfig::processEtcGroup       },
        { "ld-preload",     &DobbySpecConfig::processEtcLdPreload   },
    };
#else
    const std::map<std::string, std::string&> fileMap =
    {
        { "services",       mEtcServices    },
        { "hosts",          mEtcHosts       },
        { "passwd",         mEtcPasswd      },
        { "group",          mEtcGroup       },
        { "ld-preload",     mEtcLdSoPreload },
    };
#endif

    if (!value.isObject())
    {
        AI_LOG_ERROR("invalid etc field");
        return false;
    }

    Json::Value::const_iterator it = value.begin();
    for (; it != value.end(); ++it)
    {
        // check the field matches one of our known etc types
        std::map<std::string, std::string&>::const_iterator file = fileMap.find(it.name());
        if (file == fileMap.end())
            continue;

        // every field within the etc object should be an array type containing
        // strings, each string in the array is one line (terminated with a
        // newline) to string into the etc file
        const Json::Value& lines = *it;
        if (!lines.isArray())
        {
            AI_LOG_ERROR("invalid etc.%s field", file->first.c_str());
            return false;
        }

        // iterate through every array entry and append it to the string
        Json::Value::const_iterator jt = lines.begin();
        for (; jt != lines.end(); ++jt)
        {
            const Json::Value& line = *jt;
            if (!line.isString())
            {
                AI_LOG_ERROR("invalid line at index %u in etc.%s",
                             jt.index(), file->first.c_str());
                return false;
            }

            file->second.append(line.asString());
            file->second.push_back('\n');
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the devices field of the json spec
 *
 *  Example json:
 *
 *      "devices": [
 *          {
 *              "major": "hidraw",
 *              "minor": 0,
 *              "access": "r"
 *          },
 *          {
 *              "major": 13,
 *              "minor": 64,
 *              "access": "rw"
 *          }
 *      ]
 *
 *  This adds extra devices to the whitelist used by the container.
 *
 *  @see https://github.com/opencontainers/runtime-spec/blob/master/config-linux.md
 *  @see https://www.kernel.org/doc/Documentation/cgroup-v1/devices.txt
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processDevices(const Json::Value& value,
                                 ctemplate::TemplateDictionary* dictionary)
{
    if (!value.isArray())
    {
        AI_LOG_ERROR("invalid 'devices' field");
        return false;
    }

    Json::Value::const_iterator it = value.begin();
    for (; it != value.end(); ++it)
    {
        const Json::Value& device = *it;
        if (!device.isObject())
        {
            AI_LOG_ERROR("invalid device entry at index %u", it.index());
            return false;
        }

        // get the fields
        const Json::Value& major = device["major"];
        const Json::Value& minor = device["minor"];
        const Json::Value& access = device["access"];

        // the access field must be a string and contain either; "r", "w", "wr" or "rw"
        if (!access.isString())
        {
            AI_LOG_ERROR("invalid device.access entry at index %u", it.index());
            return false;
        }

        std::string accessStr = access.asString();
        std::transform(accessStr.begin(), accessStr.end(), accessStr.begin(), ::tolower);
        if ((accessStr != "r") && (accessStr != "w") &&
            (accessStr != "wr") && (accessStr != "rw"))
        {
            AI_LOG_ERROR("invalid device.access entry at index %u", it.index());
            return false;
        }

        // the major field can be a name or a number
        unsigned int majorNum = 0;
        if (major.isString())
        {
            majorNum = mUtilities->getDriverMajorNumber(major.asString());
        }
        else if (major.isIntegral())
        {
            majorNum = major.asUInt();
        }

        if ((majorNum < 1) || (majorNum > 1024))
        {
            AI_LOG_ERROR("invalid device.major entry at index %u", it.index());
            return false;
        }

        // the minor field must be a number
        unsigned int minorNum = UINT_MAX;
        if (minor.isIntegral())
        {
            minorNum = minor.asUInt();
        }

        if (minorNum > 1024)
        {
            AI_LOG_ERROR("invalid device.minor entry at index %u", it.index());
            return false;
        }

        // check the device requested is in our master global whitelist
        if (!mUtilities->deviceAllowed(majorNum, minorNum))
        {
            AI_LOG_ERROR("device at index %u with major:minor %u:%u is not allowed",
                         it.index(), majorNum, minorNum);
            return false;
        }

        // finally add the device to the template
        ctemplate::TemplateDictionary* subDict = dictionary->AddSectionDictionary(DEV_WHITELIST_SECTION);
        subDict->SetIntValue(DEV_WHITELIST_MAJOR, majorNum);
        subDict->SetIntValue(DEV_WHITELIST_MINOR, minorNum);
        subDict->SetValue(DEV_WHITELIST_ACCESS, accessStr);
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the capabilities field of the json spec
 *
 *  Example json:
 *
 *      "capabilities" : [
 *              "CAP_NET_RAW",
 *              "CAP_SYS_NICE"
 *      ]
 *
 *  This adds extra capabilities to the container.
 *
 *
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processCapabilities(const Json::Value& value,
                                      ctemplate::TemplateDictionary* dictionary)
{
    if (!value.isArray())
    {
        AI_LOG_ERROR("invalid 'capabilities' field");
        return false;
    }

    Json::Value::const_iterator it = value.begin();
    for (; it != value.end(); ++it)
    {
        const Json::Value& capability = *it;
        if (!capability.isString())
        {
            AI_LOG_ERROR("invalid capability entry at index %u", it.index());
            return false;
        }

        // check the capability is recognised and allowed
        const std::string capString = capability.asString();
        if (mAllowedCaps.count(capString) == 0)
        {
            AI_LOG_ERROR("capability '%s' is invalid or not allowed",
                         capString.c_str());
            return false;
        }

        // add to the capability to the template
        dictionary->SetValueAndShowSection(EXTRA_CAPS_VALUE, capString,
                                           EXTRA_CAPS_SECTION);
    }

#if !defined(RDK)
    // allow the containered apps to inherit any file base capabilities, this
    // is needed if wanting to execute programs that have the file capabilities
    // that match the capabilities we've given the container
    dictionary->SetValue(NO_NEW_PRIVS, "false");
#endif // !defined(RDK)

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the seccomp field of the json spec
 *
 *  Example json:
 *
 *      "seccomp": {
 *          "defaultAction": "SCMP_ACT_ALLOW",
 *          "syscalls": {
 *              "names": [
 *                  "getcwd",
 *                  "chmod"
 *              ],
 *              "action": "SCMP_ACT_ERRNO"
 *          }
 *      }
 *
 *  This adds extra capabilities to the container.
 *
 *  @param[in]  value       The json spec document from the client
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processSeccomp(const Json::Value& value,
                                    ctemplate::TemplateDictionary* dictionary)
{
    if (!value.isObject())
    {
        AI_LOG_ERROR("invalid 'seccomp' field");
        return false;
    }

    const Json::Value& defaultAction = value["defaultAction"];
    if (!validateSeccompAction(defaultAction))
    {
        AI_LOG_ERROR("invalid 'seccomp.defaultAction' field");
        return false;
    }

    const Json::Value& syscalls = value["syscalls"];
    if (!syscalls.isObject())
    {
        AI_LOG_ERROR("invalid 'seccomp.syscalls' field");
        return false;
    }

    const Json::Value& action = syscalls["action"];
    if (!validateSeccompAction(action))
    {
        AI_LOG_ERROR("invalid 'seccomp.syscalls.action' field");
        return false;
    }

    const Json::Value& names = syscalls["names"];
    if (!names.isArray())
    {
        AI_LOG_ERROR("invalid 'seccomp.syscalls.names' field");
        return false;
    }

    std::stringstream ss;
    if (names.size() > 0)
    {
        for (int i = 0; i < (int)names.size() - 1; ++i)
        {
            const Json::Value& entry = names[i];
            if (!entry.isString())
            {
                AI_LOG_ERROR("invalid 'seccomp.syscalls.names[%d]' field", i);
                return false;
            }

            ss << "\"" << entry.asString() << "\", ";
        }

        ss << "\"" << names[names.size() - 1].asString() << "\"";
    }
    else
    {
        AI_LOG_ERROR("empty 'seccomp.syscalls.names' array");
        return false;
    }

    dictionary->SetValue(SECCOMP_DEFAULT_ACTION, defaultAction.asString());
    dictionary->SetValue(SECCOMP_ACTION, action.asString());
    dictionary->SetValue(SECCOMP_SYSCALLS, ss.str());
    dictionary->ShowSection(SECCOMP_ENABLED);

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Validates the seccomp action field value
 *
 *  @param[in]  value       seccomp action field value
 *
 *  @return true if correct seccomp action value, otherwise false.
 */
bool DobbySpecConfig::validateSeccompAction(const Json::Value& value) const
{
    if (!value.isString())
    {
        return false;
    }

    static const std::vector<std::string> actions{"SCMP_ACT_ERRNO", "SCMP_ACT_ALLOW"};

    if (std::find(actions.begin(), actions.end(), value.asString()) == actions.end())
    {
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Inserts rdkPlugin json into existing json
 *
 *  Instead of blindly overwriting the plugin data, we only overwrite sections
 *  of the plugin data that have been set in the rdkPlugin field of the spec.
 *
 *  This allows us to set smaller portions of the plugin data and merge them
 *  with the data set by the processor methods.
 *
 *  @param[in]  value       The rdkPlugins field from the json spec
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
void DobbySpecConfig::insertIntoRdkPluginJson(const std::string& pluginName,
                                              const Json::Value& pluginData)
{
    Json::Value& existingData = mRdkPluginsJson[pluginName]["data"];

    // iterate through all data members in the RDK plugin's data field
    for (const auto& dataMember : pluginData.getMemberNames())
    {
        if (!pluginData[dataMember].isArray())
        {
            // if plugin data member is not an array, we can use the data from
            // the spec's rdkPlugin section to overwrite the member.
            existingData[dataMember] = pluginData[dataMember];
        }
        else
        {
            // plugin member is an array, so instead of overwriting, we should
            // append the new array members to the existing array if there is one
            if (!existingData[dataMember].isNull())
            {
                for (const auto& arrayElement : pluginData[dataMember])
                {
                    existingData[dataMember].append(arrayElement);
                }
            }
            else
            {
                existingData[dataMember] = pluginData[dataMember];
            }
        }
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes the rdkPlugins field of the json spec
 *
 *  The format is a 1-to-1 match with the actual OCI config file's rdkPlugin
 *  section.
 *
 *  If any rdkPlugin has been added to mRdkPluginsJson by the processX methods,
 *  the plugin's data fields will be overwritten if the same data member exists
 *  in the rdkPlugins field.
 *
 *  @param[in]  value       The rdkPlugins field from the json spec
 *  @param[in]  dictionary  Pointer to the OCI dictionary to populate
 *
 *  @return true if correctly processed the value, otherwise false.
 */
bool DobbySpecConfig::processRdkPlugins(const Json::Value& value,
                                        ctemplate::TemplateDictionary* dictionary)
{
    // if the rdkPlugins field is not empty, process it
    if (!value.isNull())
    {
        if (!value.isObject())
        {
            AI_LOG_ERROR_EXIT("invalid rdkPlugins field");
            return false;
        }

        for (const auto& pluginName : value.getMemberNames())
        {
            // insert the rdkPlugins field into the json parsed from the spec
            insertIntoRdkPluginJson(pluginName, value[pluginName]["data"]);

            // if the required field was given, overwrite existing
            if (!value[pluginName]["required"].isNull())
            {
                mRdkPluginsJson[pluginName]["required"] = value[pluginName]["required"];
            }
            // write the "dependsOn" field
            if (!value[pluginName]["dependsOn"].isNull())
            {
                mRdkPluginsJson[pluginName]["dependsOn"] = value[pluginName]["dependsOn"];
            }
        }
    }

    // process the final rdkPlugins from mRdkPluginsJson
    for (const auto& pluginName : mRdkPluginsJson.getMemberNames())
    {
        const Json::Value pluginJson = mRdkPluginsJson[pluginName];
        const std::string pluginData = jsonToString(pluginJson["data"]);
        bool pluginRequired = pluginJson["required"].asBool();
        const std::string pluginDependsOn = (pluginJson["dependsOn"].isNull() ? "[]" : jsonToString(pluginJson["dependsOn"]));

        // add parsed rdkPlugin into mRdkPlugins for Dobby hooks
        mRdkPlugins.emplace(pluginName, pluginJson);

        // add parsed rdkPlugin into dictionary to be written to OCI config
        ctemplate::TemplateDictionary* subDict = mDictionary->AddSectionDictionary(RDK_PLUGIN_SECTION);
        subDict->SetValue(RDK_PLUGIN_NAME, pluginName.c_str());
        subDict->SetValue(RDK_PLUGIN_DATA, pluginData.c_str());
        subDict->SetValue(RDK_PLUGIN_REQUIRED, pluginRequired ? "true": "false");
        subDict->SetValue(RDK_PLUGIN_DEPENDS_ON, pluginDependsOn.c_str());
    }

    // we no longer need mRdkPluginsJson, so we can safely clear it
    mRdkPluginsJson.clear();

    return true;
}
