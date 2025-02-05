/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2025 Sky UK
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
 * File:   DobbyHibernate.h
 *
 */
#pragma once

#include <sys/types.h>
#include <stdint.h>
#include <string>

class DobbyHibernateImpl;

class DobbyHibernate
{
    public:

    enum Error
    {
        ErrorNone = 0,
        ErrorGeneral = 1,
        ErrorTimeout = 2
    };

    enum CompressionAlg
    {
        AlgNone = 0,
        AlgLz4 = 1,
        AlgZstd = 2,
        AlgDefault = 3
    };

    static const std::string DFL_LOCATOR;
    static const uint32_t DFL_TIMEOUTE_MS;

    static void setImpl(DobbyHibernateImpl* newImpl);

    static Error HibernateProcess(const pid_t pid, const uint32_t timeout = DFL_TIMEOUTE_MS,
        const std::string &locator = DFL_LOCATOR, const std::string &dumpDirPath = std::string(), CompressionAlg compression = AlgDefault);
    static Error WakeupProcess(const pid_t pid, const uint32_t timeout = DFL_TIMEOUTE_MS, const std::string &locator = DFL_LOCATOR);

    protected:
    static DobbyHibernateImpl* impl;
};


class DobbyHibernateImpl
{
    public:

    virtual ~DobbyHibernateImpl() = default;

    virtual DobbyHibernate::Error HibernateProcess(const pid_t pid, const uint32_t timeout,
        const std::string &locator, const std::string &dumpDirPath, DobbyHibernate::CompressionAlg compression) = 0;

    virtual DobbyHibernate::Error WakeupProcess(const pid_t pid, const uint32_t timeout, const std::string &locator) = 0;
};