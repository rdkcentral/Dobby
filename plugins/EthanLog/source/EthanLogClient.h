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
 * File:   EthanLogClient.h
 *
 * Copyright (C) Sky UK 2016+
 */

#ifndef ETHANLOGCLIENT_H
#define ETHANLOGCLIENT_H

#include "ContainerId.h"

#include <chrono>
#include <string>
#include <cstdint>

#include <sys/uio.h>


typedef struct sd_event sd_event;
typedef struct sd_event_source sd_event_source;


class EthanLogClient
{
public:
    EthanLogClient(sd_event *loop, ContainerId &&id, std::string &&name, int fd,
                   unsigned allowedLevels, unsigned rate, unsigned burstSize);
    ~EthanLogClient();

    EthanLogClient(const EthanLogClient &other) = delete;
    EthanLogClient(EthanLogClient &&other) = delete;

    EthanLogClient &operator=(const EthanLogClient &other) = delete;
    EthanLogClient &operator=(EthanLogClient &&other) = delete;

public:
    inline bool closed() const
    {
        return (mSource == nullptr);
    }

    inline ContainerId id() const
    {
        return mContainerId;
    }

    inline void setBasePid(pid_t pid)
    {
        mPidOffset = (pid - 1);
    }

public:
    static constexpr unsigned LOG_LEVEL_FATAL       = (0x1 << 0);
    static constexpr unsigned LOG_LEVEL_ERROR       = (0x1 << 1);
    static constexpr unsigned LOG_LEVEL_WARNING     = (0x1 << 2);
    static constexpr unsigned LOG_LEVEL_MILESTONE   = (0x1 << 3);
    static constexpr unsigned LOG_LEVEL_INFO        = (0x1 << 4);
    static constexpr unsigned LOG_LEVEL_DEBUG       = (0x1 << 5);

private:
    static int pipeFdHandler(sd_event_source *source, int fd,
                             uint32_t revents, void *userData);

    int pipeFdHandler(uint32_t revents);

    void processLogData();
    int processLogLevel(const char *field, ssize_t len, struct iovec *iov) const;
    int processPid(const char *field, ssize_t len, struct iovec *iov) const;
    int processTimestamp(const char *field, ssize_t len, struct iovec *iov) const;
    int processThreadName(const char *field, ssize_t len, struct iovec *iov) const;
    int processCodeFile(const char *field, ssize_t len, struct iovec *iov) const;
    int processCodeFunction(const char *field, ssize_t len, struct iovec *iov) const;
    int processCodeLine(const char *field, ssize_t len, struct iovec *iov) const;
    int processMessage(const char *field, ssize_t len, struct iovec *iov) const;

    bool shouldDrop();

    static constexpr ssize_t MAX_LOG_MSG_LENGTH = 512;

    static constexpr char RECORD_DELIM = '\x1e';
    static constexpr char FIELD_DELIM = '\x1f';


private:
    const ContainerId mContainerId;
    const std::string mName;
    const int mPipeFd;
    const unsigned mAllowedLevels;

    pid_t mPidOffset;

    sd_event_source *mSource;

    std::string mIdentifier;

    char mMsgBuf[(MAX_LOG_MSG_LENGTH * 2)];
    size_t mMsgLen;

    bool mRateLimitingEnabled;

    struct TokenBucket
    {
        const unsigned int rate;
        const unsigned int burstSize;

        unsigned int tokens;
        std::chrono::steady_clock::time_point lastFill;

        TokenBucket(unsigned int rate_, unsigned int burstSize_)
            : rate(rate_)
            , burstSize(burstSize_)
            , tokens(0)
            , lastFill(std::chrono::steady_clock::time_point::min())
        { }

    } mTokenBucket;

    int mDropped;
    std::chrono::steady_clock::time_point mFirstDropped;
    std::chrono::steady_clock::time_point mLastDropped;
};


#endif // ETHANLOGCLIENT_H
