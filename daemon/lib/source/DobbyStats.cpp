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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <sstream>

DobbyStats::DobbyStats(const ContainerId& id,
                       const std::shared_ptr<IDobbyEnv>& env)
    : mStats(getStats(id, env))
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
                                 const std::shared_ptr<IDobbyEnv>& env)
{
    AI_LOG_FN_ENTRY();

    Json::Value stats(Json::objectValue);

    const std::string cpuCgroupPath(env->cgroupMountPath(IDobbyEnv::Cgroup::CpuAcct));
    if (!cpuCgroupPath.empty())
    {
        // the pids entry should be the same for all cgroups, so we might as well
        // use the cpuacct cgroup to get the pids from
        stats["pids"] =
            readMultipleCgroupValues(id, cpuCgroupPath, "cgroup.procs");

        // get the cpu usage values
        stats["cpu"]["usage"]["total"] =
            readSingleCgroupValue(id, cpuCgroupPath, "cpuacct.usage");
        stats["cpu"]["usage"]["percpu"] =
            readMultipleCgroupValues(id, cpuCgroupPath, "cpuacct.usage_percpu");
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

    AI_LOG_FN_EXIT();
    return stats;
}

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
Json::Value DobbyStats::readMultipleCgroupValues(const ContainerId& id,
                                                 const std::string& cgroupMntPath,
                                                 const std::string& cgroupfileName)
{
    char buf[4096];

    if (readCgroupFile(id, cgroupMntPath, cgroupfileName, buf, sizeof(buf)) <= 0)
        return Json::Value::null;

    Json::Value array(Json::arrayValue);

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
        else if (value == ULONG_LONG_MAX)
        {
            array.append(-1);
        }
        else
        {
            array.append(static_cast<Json::LargestUInt>(value));
        }

        token = strtok_r(nullptr, delims, &saveptr);
    }

    return array;
}
