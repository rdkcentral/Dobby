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
 * File:   DobbyStats.cpp
 *
 */
#include "DobbyStats.h"

#include <Logging.h>

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#ifndef LONG_LONG_MAX
#define LONG_LONG_MAX __LONG_LONG_MAX__
#endif
#ifndef ULONG_LONG_MAX
#define ULONG_LONG_MAX (LONG_LONG_MAX * 2ULL + 1)
#endif

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#include <set>
#include <regex>

#include <sstream>
#include <ext/stdio_filebuf.h>

DobbyStats::DobbyStats(const ContainerId& id,
                       const std::shared_ptr<IDobbyEnv>& env,
                       const std::shared_ptr<IDobbyUtils> &utils)
    : mStats(getStats(id, env, utils))
{
}

DobbyStats::~DobbyStats()
{
}

const Json::Value& DobbyStats::stats() const
{
    return mStats;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets the stats for the container
 *
 *  This is primarily a debugging method, used to get statistics on the
 *  container and roughly correlates to the 'runc events --stats <id>' call.
 *
 *  The reply is a json formatted string containing some info, it's form may
 *  change over time.
 *
 *      {
 *          "timestamp": 348134887768,
 *          "pids": [ 2046, 2064 ],
 *          "cpu": {
 *              "usage": {
 *                  "total":734236982,
 *                  "percpu":[348134887,386102095]
 *              }
 *          },
 *          "memory":{
 *              "user": {
 *                  "limit":41943040,
 *                  "usage":356352,
 *                  "max":524288,
 *                  "failcnt":0
 *              }
 *          }
 *          "gpu":{
 *              "memory": {
 *                  "limit":41943040,
 *                  "usage":356352,
 *                  "max":524288,
 *                  "failcnt":0
 *              }
 *          }
 *          ...
 *      }
 *
 *  @param[in]  id      The container id, assumed to also be the name of the
 *                      cgroups.
 *  @param[in]  env     The environment setup, used to get the mount point(s)
 *                      of the various cgroups.
 *
 *  @return The populated json stats, may be an empty object if no cgroups
 *  could be found.
 */
Json::Value DobbyStats::getStats(const ContainerId& id,
                                 const std::shared_ptr<IDobbyEnv>& env,
                                 const std::shared_ptr<IDobbyUtils> &utils)
{
    AI_LOG_FN_ENTRY();

    Json::Value stats(Json::objectValue);

    const std::string cpuCgroupPath(env->cgroupMountPath(IDobbyEnv::Cgroup::CpuAcct));
    if (!cpuCgroupPath.empty())
    {
        // the pids entry should be the same for all cgroups, so we might as well
        // use the cpuacct cgroup to get the pids from
        stats["pids"] =
            readMultipleCgroupValuesJson(id, cpuCgroupPath, "cgroup.procs");

        stats["processes"] =
            getProcessTree(id, cpuCgroupPath, utils);

        // get the cpu usage values
        stats["cpu"]["usage"]["total"] =
            readSingleCgroupValue(id, cpuCgroupPath, "cpuacct.usage");
        stats["cpu"]["usage"]["percpu"] =
            readMultipleCgroupValuesJson(id, cpuCgroupPath, "cpuacct.usage_percpu");
    }

    // the timestamp value is generally used to calculate the cpu usage, so set
    // the timestamp as close to the cpuacct cgroup read as possible
    struct timespec tp;
    if (clock_gettime(CLOCK_MONOTONIC, &tp) == 0)
    {
        stats["timestamp"] =
            static_cast<Json::Int64>((static_cast<int64_t>(tp.tv_sec) * 1000000000LL) +
                                      static_cast<int64_t>(tp.tv_nsec));
    }

    const std::string memCgroupPath(env->cgroupMountPath(IDobbyEnv::Cgroup::Memory));
    if (!memCgroupPath.empty())
    {
        // get the userspace memory consumed
        stats["memory"]["user"]["limit"] =
            readSingleCgroupValue(id, memCgroupPath, "memory.limit_in_bytes");
        stats["memory"]["user"]["usage"] =
            readSingleCgroupValue(id, memCgroupPath, "memory.usage_in_bytes");
        stats["memory"]["user"]["max"] =
            readSingleCgroupValue(id, memCgroupPath, "memory.max_usage_in_bytes");
        stats["memory"]["user"]["failcnt"] =
            readSingleCgroupValue(id, memCgroupPath, "memory.failcnt");
    }

    const std::string gpuCgroupPath(env->cgroupMountPath(IDobbyEnv::Cgroup::Gpu));
    if (!gpuCgroupPath.empty())
    {
        // get the gpu memory consumed
        stats["gpu"]["memory"]["limit"] =
            readSingleCgroupValue(id, gpuCgroupPath, "gpu.limit_in_bytes");
        stats["gpu"]["memory"]["usage"] =
            readSingleCgroupValue(id, gpuCgroupPath, "gpu.usage_in_bytes");
        stats["gpu"]["memory"]["max"] =
            readSingleCgroupValue(id, gpuCgroupPath, "gpu.max_usage_in_bytes");
        stats["gpu"]["memory"]["failcnt"] =
            readSingleCgroupValue(id, gpuCgroupPath, "gpu.failcnt");
    }

#if defined(RDK)
    const std::string ionCgroupPath(env->cgroupMountPath(IDobbyEnv::Cgroup::Ion));
    if (!ionCgroupPath.empty())
    {
        stats["ion"]["heaps"] = readIonCgroupHeaps(id, ionCgroupPath);
    }
#endif


    AI_LOG_FN_EXIT();
    return stats;
}

#if defined(RDK)
// -----------------------------------------------------------------------------
/**
 *  @brief Reads the cgroup values for all the ION heaps and returns as a JSON
 *  object.
 *
 *  The path to read is made up like: <cgroupMntPath>/<id>/<cgroupfileName>
 *
 *  @param[in]  id              The string id of the container.
 *  @param[in]  ionCgroupPath   The path to the ion cgroup mount point.
 *
 *  @return A JSON object value contain the ion heaps values.
 */
Json::Value DobbyStats::readIonCgroupHeaps(const ContainerId& id,
                                           const std::string &ionCgroupPath)
{
    AI_LOG_FN_ENTRY();

    Json::Value heaps(Json::objectValue);

    // first get all the possible heaps in the cgroup
    std::ostringstream dirPath;
    dirPath << ionCgroupPath << "/" << id.str() << "/";
    DIR *dir = opendir(dirPath.str().c_str());
    if (!dir)
    {
        return heaps;
    }

    std::set<std::string> heapNames;
    const std::regex limitRegex(R"regex((^ion\.)(\w+)(\.limit_in_bytes$))regex");

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // only care about sysfs files
        if (entry->d_type != DT_REG)
            continue;

        // check if it is a heap's limit file, if so extract the heap name
        // and add to the set
        std::cmatch matches;
        if (std::regex_match(entry->d_name, matches, limitRegex) &&
            (matches.size() == 4))
        {
            heapNames.insert(matches.str(1));
        }
    }

    // clean the dir iterator
    closedir(dir);

    // read all every heaps cgroup values
    for (const std::string &heapName : heapNames)
    {
        Json::Value heap(Json::objectValue);

        heap["limit"] =
            readSingleCgroupValue(id, ionCgroupPath, "ion." + heapName + ".limit_in_bytes");
        heap["usage"] =
            readSingleCgroupValue(id, ionCgroupPath, "ion." + heapName + ".usage_in_bytes");
        heap["max"] =
            readSingleCgroupValue(id, ionCgroupPath, "ion." + heapName + ".max_usage_in_bytes");
        heap["failcnt"] =
            readSingleCgroupValue(id, ionCgroupPath, "ion." + heapName + ".failcnt");

        heaps[heapName] = std::move(heap);
    }

    AI_LOG_FN_EXIT();
    return heaps;
}
#endif

// -----------------------------------------------------------------------------
/**
 *  @brief Reads a maximum of 4096 bytes from the given cgroup file.
 *
 *  The path to read is made up like: <cgroupMntPath>/<id>/<cgroupfileName>
 *
 *  @param[in]  id              The string id of the container.
 *  @param[in]  cgroupMntPath   The path to the cgroup mount point.
 *  @param[in]  cgroupfileName  The name of the cgroup file.
 *  @param[out] buf             Buffer to store the file contents in
 *  @param[in]  bufLen          The size of the buffer.
 *
 *  @return The number of characters copied, or
 */
ssize_t DobbyStats::readCgroupFile(const ContainerId& id,
                                   const std::string& cgroupMntPath,
                                   const std::string& cgroupfileName,
                                   char* buf, size_t bufLen)
{
    std::ostringstream filePath;
    filePath << cgroupMntPath << "/" << id.str() << "/" << cgroupfileName;

    std::string contents;

    int fd = open(filePath.str().c_str(), O_CLOEXEC | O_RDONLY);
    if (fd < 0)
    {
        return -1;
    }

    ssize_t rd = TEMP_FAILURE_RETRY(read(fd, buf, bufLen - 1));
    if (rd > 0)
    {
        buf[rd] = '\0';
    }

    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close '%s'", filePath.str().c_str());
    }

    return rd;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Reads a single value from the given cgroup file
 *
 *  The path to read is made up like:
 *
 *      <cgroupMntPath>/<id>/<cgroupfileName>
 *
 *  @param[in]  id              The string id of the container.
 *  @param[in]  cgroupMntPath   The path to the cgroup mount point.
 *  @param[in]  cgroupfileName  The name of the cgroup file.
 *
 *  @return The value read as a json object, typically an integer value.
 */
Json::Value DobbyStats::readSingleCgroupValue(const ContainerId& id,
                                              const std::string& cgroupMntPath,
                                              const std::string& cgroupfileName)
{
    char buf[128];

    if (readCgroupFile(id, cgroupMntPath, cgroupfileName, buf, sizeof(buf)) <= 0)
        return Json::Value::null;

    char* saveptr = nullptr;
    char *token = strtok_r(buf, " \t\n\r", &saveptr);
    if (!token)
        return Json::Value::null;

    unsigned long long value = strtoull(token, nullptr, 0);
    if ((value == 0) && (errno == EINVAL))
    {
        AI_LOG_SYS_ERROR(errno, "failed to convert '%s' contents to uint64_t",
                         cgroupfileName.c_str());
        return Json::Value::null;
    }

    if (value == ULONG_LONG_MAX)
        return Json::Value(-1);
    else
        return Json::Value(static_cast<Json::LargestUInt>(value));
}

// -----------------------------------------------------------------------------
/**
 *  @brief Reads multiple values from the given cgroup file
 *
 *  The path to read is made up like:
 *
 *      <cgroupMntPath>/<id>/<cgroupfileName>
 *
 *  Each value is expected to be delimited with either a space, tab or newline.
 *
 *  @param[in]  id              The string id of the container.
 *  @param[in]  cgroupMntPath   The path to the cgroup mount point.
 *  @param[in]  cgroupfileName  The name of the cgroup file.
 *
 *  @return The value read as a json object, typically an integer value.
 */
std::vector<int64_t> DobbyStats::readMultipleCgroupValues(const ContainerId& id,
                                                 const std::string& cgroupMntPath,
                                                 const std::string& cgroupfileName)
{
    char buf[4096];

    if (readCgroupFile(id, cgroupMntPath, cgroupfileName, buf, sizeof(buf)) <= 0)
        return {};

    std::vector<int64_t> values;

    char* saveptr = nullptr;
    const char delims[] = " \t\n\r";
    char *token = strtok_r(buf, delims, &saveptr);
    while (token)
    {
        unsigned long long value = strtoull(token, nullptr, 0);
        if ((value == 0) && (errno == EINVAL))
        {
            AI_LOG_SYS_ERROR(errno, "failed to convert '%s' contents to uint64_t",
                             cgroupfileName.c_str());
        }
        else if (value == LONG_LONG_MAX)
        {
            values.emplace_back(-1);
        }
        else
        {
            values.emplace_back(value);
        }

        token = strtok_r(nullptr, delims, &saveptr);
    }

    return values;
}

Json::Value DobbyStats::readMultipleCgroupValuesJson(const ContainerId& id,
                                                    const std::string& cgroupMntPath,
                                                    const std::string& cgroupfileName)
{
    std::vector<int64_t> cgroupValues = readMultipleCgroupValues(id, cgroupMntPath, cgroupfileName);

    Json::Value array(Json::arrayValue);

    for(const auto& value : cgroupValues)
    {
        array.append(static_cast<Json::LargestInt>(value));
    }

    return array;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Builds a json array of the processes running in the container
 *
 *  Will return a json array with all the processes inside the container with
 *  their filename, cmdline and PID (both in the host and container namespace)
 *
 * "processes": [
 *           {
 *               "pid": "2345",
 *               "nsPid": "1"
 *               "file": "/usr/libexec/DobbyInit",
 *               "cmdline": "/usr/libexec/DobbyInit sleep 30",
 *           }
 *       ]
 *
 *  @param[in]  id              The string id of the container.
 *
 *  @return Json array with all container processes
 */
Json::Value DobbyStats::getProcessTree(const ContainerId& id,
                                       const std::string& cpuCgroupMntPath,
                                       const std::shared_ptr<IDobbyUtils> &utils)
{
    std::vector<int64_t> cgroupValues = readMultipleCgroupValues(id, cpuCgroupMntPath, "cgroup.procs");

    Json::Value array(Json::arrayValue);

    for (auto pid : cgroupValues)
    {
        if (pid > std::numeric_limits<pid_t>::max() || pid < 0)
        {
            AI_LOG_WARN("Invalid PID found: %lld", static_cast<long long>(pid));
            continue;
        }

        Json::Value processJson;
        getProcessInfo(pid, utils).Serialise(processJson);
        array.append(processJson);
    };

    return array;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns information about a given PID
 *
 *  @param[in]  pid     The PID to get information about
 *
 *  @returns    Process struct containing information about the specified
 *              process
 */
DobbyStats::Process DobbyStats::getProcessInfo(pid_t pid,
                                               const std::shared_ptr<IDobbyUtils> &utils)
{
    AI_LOG_FN_ENTRY();

    // Get the path to the executable (resolving symlinks)
    char exePath[32];
    snprintf(exePath, sizeof(exePath), "/proc/%d/exe", pid);

    char processPathBuf[PATH_MAX];
    std::string processPath;
    ssize_t len = readlink(exePath, processPathBuf, sizeof(processPathBuf));
    if (len <= 0)
    {
        AI_LOG_SYS_ERROR(errno, "readlink failed on %s", exePath);
    }
    else
    {
        processPath = std::string(processPathBuf, len);
    }

    // Get the full process command line including arguments given
    char cmdlinePath[32];
    snprintf(cmdlinePath, sizeof(cmdlinePath), "/proc/%d/cmdline", pid);

    std::string processCmdline;
    processCmdline = utils->readTextFile(cmdlinePath);
    std::replace( processCmdline.begin(), processCmdline.end(), '\0', ' ');

    // Get the PID of the process from the perspective of the container
    pid_t nsPid = readNsPidFromProc(pid);

    Process process {
        pid,
        nsPid,
        std::move(processPath),
        std::move(processCmdline)
    };

    AI_LOG_FN_EXIT();
    return process;
}

// -----------------------------------------------------------------------------
/**
 * @brief Given a pid (in global namespace) tries to find what it's namespace
 * pid is.
 *
 * This reads the /proc/<pid>/status file, line NStgid.
 *
 * @param[in] pid      The real pid of the process to lookup.
 * @returns            Set of all the real pids within the container.
 */
pid_t DobbyStats::readNsPidFromProc(pid_t pid)
{
    char filePathBuf[32];
    sprintf(filePathBuf, "/proc/%d/status", pid);

    // get the list of all pids within the container
    int fd = open(filePathBuf, O_CLOEXEC | O_RDONLY);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open %s'", filePathBuf);
        return -1;
    }

    // nb: stdio_filebuf closes the fd on destruction
    __gnu_cxx::stdio_filebuf<char> statusFileBuf(fd, std::ios::in);
    std::istream statusFileStream(&statusFileBuf);

    std::string line;
    while (std::getline(statusFileStream, line))
    {
        if (line.compare(0, 7, "NStgid:") == 0)
        {
            int realPid = -1, nsPid = -1;

            // skip the row header and read the next two integer (pid) values
            if (sscanf(line.c_str(), "NStgid:\t%d\t%d", &realPid, &nsPid) != 2)
            {
                AI_LOG_WARN("failed to parse NStgid field, '%s' -> %d %d",
                            line.c_str(), realPid, nsPid);
                nsPid = -1;
            }

            // the first pid should be the one in the global namespace
            else if ((realPid != pid) || (nsPid < 1))
            {
                AI_LOG_WARN("failed to parse NStgid field, '%s' -> %d %d",
                            line.c_str(), realPid, nsPid);
                nsPid = -1;
            }

            return nsPid;
        }
    }

    AI_LOG_WARN("failed to find the NStgid field in the '%s' file", filePathBuf);
    return -1;
}
