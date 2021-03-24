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
 * File:   EthanLogClient.cpp
 *
 */

#include "EthanLogClient.h"
#include "EthanLogLoop.h"

#include <Logging.h>

#define SD_JOURNAL_SUPPRESS_LOCATION

#include <systemd/sd-event.h>
#include <systemd/sd-journal.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include <climits>
#include <sstream>
#include <ext/stdio_filebuf.h>

#define ETHANLOG_MAX_LOG_MSG_LENGTH       512UL

// #define ETHANLOG_DEBUG_DUMP



// -----------------------------------------------------------------------------
/**
 *  @brief Constructs a logging client which represents one pipe.
 *
 *  @param[in]  loop    The systemd event loop the plugin is running
 *  @param[in]  name    The name of the container, used to tag all log messages
 *  @param[in]  fd      The pipe fd, this class takes ownership of the pipe and
 *                      closes it in the destructor
 *  @param[in]  allowedLevels    Bitmask of the allowed log levels.
 *  @param[in]  rate    The number of log messages allowed per second.
 *  @param[in]  burst   The maximum number of messages allowed in a burst.
 *  @param[in]  memCgrpMountPoint   Used to lookup the pids within a container
 *                                  for mapping namespaced pids to real pids.
 *
 */
EthanLogClient::EthanLogClient(sd_event *loop, ContainerId &&id,
                               std::string &&name, int fd,
                               unsigned allowedLevels,
                               unsigned rate, unsigned burstSize,
                               const std::string& memCgrpMountPoint)
    : mContainerId(std::move(id))
    , mName(std::move(name))
    , mPipeFd(fd)
    , mAllowedLevels(allowedLevels)
    , mSource(nullptr)
    , mMsgLen(0)
    , mRateLimitingEnabled(false)
    , mTokenBucket(1, burstSize)
    , mDropped(0)
    , mFirstDropped(std::chrono::steady_clock::time_point::min())
    , mLastDropped(std::chrono::steady_clock::time_point::min())
{
    AI_LOG_DEBUG("created logging pipe for '%s' with read fd %d",
                 mName.c_str(), mPipeFd);

    // create the path to the cgroup.procs file, used to get a list of all pids
    // inside the client container
    mCgroupPidsPath = memCgrpMountPoint + "/" + mContainerId.str() + "/cgroup.procs";

    // set the identifier tag for journald
    mIdentifier = "SYSLOG_IDENTIFIER=" + mName;

    // add the pipe to the event loop
    int rc = sd_event_add_io(loop, &mSource, fd, EPOLLIN,
                             &EthanLogClient::pipeFdHandler, this);
    if ((rc < 0) || !mSource)
    {
        AI_LOG_SYS_ERROR(-rc, "failed to create source for pipe fd");
    }

}

EthanLogClient::~EthanLogClient()
{
    // clean up the event source if still open
    if (mSource)
    {
        sd_event_source_unref(mSource);
        mSource = nullptr;
    }

    // and close the pipe
    if ((mPipeFd >= 0) && (close(mPipeFd) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close pipe fd");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the base pid for the client's container.
 *
 *  This is used as the default pid to put in the log message if the client
 *  hasn't supplied a pid, or we couldn't resolve their pid in the global
 *  pid namespace.
 *
 *  @param pid    The base pid of the container.
 *
 */
void EthanLogClient::setContainerPid(pid_t pid)
{
    if (pid <= 0)
        return;

    // set the defaults for journald
    mDefaultSyslogPid = "SYSLOG_PID=" + std::to_string(pid);
    mDefaultObjectPid = "OBJECT_PID=" + std::to_string(pid);

    // also we know that within the container that pid will be given the value
    // 1 in the container's pid_namespace, so can add that to the mapping
    mNsToRealPidMapping[1] = pid;
    AI_LOG_INFO("added mapping for container pid %d to real pid %d", 1, pid);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Callback called when data to read on the logging pipe.
 *
 *  Checks if the pipe is closed, if not then reads a block from the pipe.
 *
 */
int EthanLogClient::pipeFdHandler(sd_event_source *source, int fd,
                                  uint32_t revents, void *userData)
{
    EthanLogClient *self = reinterpret_cast<EthanLogClient*>(userData);
    AI_DEBUG_ASSERT(fd == self->mPipeFd);

    return self->pipeFdHandler(revents);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Callback called when data to read on the logging pipe.
 *
 *  Checks if the pipe is closed, if not then reads a block from the pipe.
 *
 */
int EthanLogClient::pipeFdHandler(uint32_t revents)
{
    // the other end of the pipe closed so
    if (revents & (EPOLLHUP | EPOLLERR))
    {
        AI_LOG_INFO("detected close of logging pipe for '%s'", mName.c_str());

        sd_event_source_unref(mSource);
        mSource = nullptr;
        return 0;
    }

    // read all the data from the pipe
    if (revents & EPOLLIN)
    {
        while (true)
        {
            ssize_t amount = TEMP_FAILURE_RETRY(read(mPipeFd, mMsgBuf + mMsgLen,
                                                     MAX_LOG_MSG_LENGTH));
            if (amount < 0)
            {
                // non-blocking is set so this just means we've read everything
                if (errno != EAGAIN)
                {
                    AI_LOG_SYS_ERROR(errno, "failed to read from logging pipe");

                    sd_event_source_unref(mSource);
                    mSource = nullptr;
                    return -1;
                }

                break;
            }
            else if (amount == 0)
            {
                // pipe closed
                AI_LOG_INFO("detected close of logging pipe for '%s'", mName.c_str());

                sd_event_source_unref(mSource);
                mSource = nullptr;
                return 0;
            }
            else
            {
                // increment the message length in the mMsgBuf
                mMsgLen += amount;

                // process the content of the pipe
                processLogData();

                // sanity check the message length, shouldn't be needed
                if (mMsgLen > ETHANLOG_MAX_LOG_MSG_LENGTH) {
                    AI_LOG_ERROR("serious internal error parsing log msg");
                    mMsgLen = 0;
                }

            }
        }
    }

    return 0;
}

#if defined(ETHANLOG_DEBUG_DUMP)

// -----------------------------------------------------------------------------
/**
 *  @brief Debugging function, used to print out the message string with
 *  non-printable characters converted to "\x??" strings.
 *
 *  @param[in]  buf     Buffer to dump
 *  @param[in]  len     The size of the buffer
 *
 */
static void dumpMessage(const char *buf, size_t len)
{
    const char hexChars[] = "0123456789abcdef";

    if (!len)
        return;

    char *tmp = (char*) alloca((len * 4) + 1);
    if (!tmp)
        return;

    char *ptr = tmp;

    for (size_t i = 0; i < len; i++)
    {
        if (std::isprint(buf[i]))
        {
            *ptr++ = buf[i];
        }
        else
        {
            *ptr++ = '\\';
            *ptr++ = 'x';
            *ptr++ = hexChars[(buf[i] >> 4)];
            *ptr++ = hexChars[(buf[i] & 0xf)];
        }
    }

    *ptr++ = '\0';

    fprintf(stderr, ">>>> [%s] <<<<\n", tmp);
}

static void dumpMessage(struct iovec *fields, size_t len)
{
    if (!len)
        return;

    fprintf(stderr, ">>>>\n");
    for (size_t i = 0; i < len; i++)
    {
        fprintf(stderr, "\t%.*s\n", int(fields[i].iov_len), fields[i].iov_base);
    }

    fprintf(stderr, "<<<<\n");
}

#endif // defined(ETHANLOG_DEBUG_DUMP)

// -----------------------------------------------------------------------------
/**
 * @brief Returns true if the message should be dropped due to rate limiting.
 *
 * The rate limits are set in the constructor and the algorithm used is a
 * token bucket type setup.
 *
 */
bool EthanLogClient::shouldDrop()
{
    if (!mRateLimitingEnabled)
        return false;

    // FIXME:
    const unsigned tokensPerMessage = 1;
    const auto now = std::chrono::steady_clock::now();

    // check we have enough tokens to log the message
    if (mTokenBucket.tokens < tokensPerMessage)
    {
        // check if need to refill the bucket
        if (now > mTokenBucket.lastFill)
        {
            const std::chrono::milliseconds msecsElapsed =
                std::chrono::duration_cast<std::chrono::milliseconds>
                    (now - mTokenBucket.lastFill);

            const long long tokens =
                mTokenBucket.tokens + std::min<long long>(UINT_MAX, msecsElapsed.count());

            mTokenBucket.tokens =
                static_cast<unsigned>(std::min<long long>(tokens, mTokenBucket.burstSize));

            mTokenBucket.lastFill = now;
        }

        // check once again if we have enough tokens
        if (mTokenBucket.tokens < tokensPerMessage)
        {
            if (!mDropped)
                mFirstDropped = now;

            mLastDropped = now;
            mDropped++;
            return true;
        }
    }

    // removed tokens from the bucket
    mTokenBucket.tokens -= tokensPerMessage;

    // if previously dropped frames log the message
    if (mDropped)
    {
        auto firstDropped = std::chrono::duration_cast<std::chrono::seconds>(now - mFirstDropped);
        auto lastDropped = std::chrono::duration_cast<std::chrono::seconds>(now - mLastDropped);
        mDropped = 0;

        char messageBuf[128];
        snprintf(messageBuf, sizeof(messageBuf),
                 "MESSAGE=Dropped %u log messages in last %" PRId64 " seconds (most "
                 "recently, %" PRId64 " seconds ago) due to excessive rate",
                 mDropped, firstDropped.count(), lastDropped.count());

        sd_journal_send("PRIORITY=4", mIdentifier.c_str(), messageBuf, nullptr);
    }

    return false;
}

// -----------------------------------------------------------------------------
/**
 * @brief Process some log data from a client pipe
 * @param app The app/client that has generated the data
 * @param data The string (or part of a string) sent by the client
 * @param datalen The number of characters received
 *
 * The log string(s) sent by the client library are formatted using ASCII
 * separators, the format is (temporarily) described on the following
 * confluence page:
 *   https://www.stb.bskyb.com/confluence/display/~grayb/Unified+Logging+on+the+STB
 *
 * But for those that can't be bothered opening a browser, the following are
 * the cliff notes:
 *
 *      \x1e  - Character used to start and terminate a log message
 *      \x1f  - Character used to delimit fields within the message string
 *
 *      Each field within the message is prefixed with one of the following
 *      upper case characters that define the field type.
 *
 *      L   - Log level
 *      P   - PID of app in hexadecimal (without 0x prefix)
 *      T   - Timestamp from monotonic clock in hexadecimal (without 0x prefix)
 *      R   - Name of the thread
 *      S   - Name of the source file containing the log message
 *      F   - Name of the function producing the log message
 *      N   - The line number of the log producer
 *      M   - The log message (mandatory but can be empty)
 */
void EthanLogClient::processLogData()
{
    enum MessageFlags
    {
        FLAG_HAVE_LOG_LEVEL      = (0x1UL << 0),
        FLAG_HAVE_PID            = (0x1UL << 1),
        FLAG_HAVE_TIMESTAMP      = (0x1UL << 2),
        FLAG_HAVE_THREAD         = (0x1UL << 3),
        FLAG_HAVE_SRCFILE        = (0x1UL << 4),
        FLAG_HAVE_FUNCTION       = (0x1UL << 5),
        FLAG_HAVE_LINENO         = (0x1UL << 6),
        FLAG_HAVE_MESSAGE        = (0x1UL << 7)
    };


    // if all logging is disabled then just jump out now, no point doing any
    // processing
    if (mAllowedLevels == 0)
    {
        mMsgLen = 0;
        return;
    }

    // fields to pass to journald for logging, the first one is always the
    // container / app identifier
    const int maxFields = 16;
    struct iovec fields[maxFields + 2];
    fields[0].iov_base = const_cast<char*>(mIdentifier.c_str());
    fields[0].iov_len = mIdentifier.size();

    // get out early if the message is obviously too short (start/stop delims +
    // 4 mandatory fields times 3 minimum characters)
    while (mMsgLen >= (2 + (3 * 3)))
    {
        // find the message start point, if no start found discard everything */
        char *msgStart = (char*)memchr(mMsgBuf, RECORD_DELIM, mMsgLen);
        if (!msgStart)
        {
            mMsgLen = 0;
            break;
        }

        /* wipe out everything before the start */
        if (msgStart != mMsgBuf)
        {
            mMsgLen -= (msgStart - mMsgBuf);
            memmove(mMsgBuf, mMsgBuf, mMsgLen);
        }


        /* sanity check there is enough in the buffer to make a valid message */
        if (mMsgLen < (3 * 3))
            break;

        /* try and find the end, if not found we're done */
        char *msgEnd = (char*)memchr((mMsgBuf + 1), RECORD_DELIM, (mMsgLen - 1));
        if (!msgEnd)
            break;


        /* discard messages that are obviously too short (4 mandatory fields
         * times 3 minimum characters for each).
         */
        if (((msgEnd - mMsgBuf) > (3 * 3)) && !shouldDrop())
        {
#if defined(ETHANLOG_DEBUG_DUMP)
            dumpMessage(mMsgBuf, (msgEnd - mMsgBuf));
#endif

            // null terminate the message and start tokenising it based on the
            // field delimiter.
            *msgEnd++ = '\0';

            // counter of iov fields, we start at 1 because the first one is
            // always the constant identifier
            int numFields = 1;


            char *thisField, *nextField = nullptr;

            // find the first field and null terminate it
            thisField = (char*)memchr((mMsgBuf + 1), FIELD_DELIM, ((msgEnd - mMsgBuf) - 1));
            if (thisField)
            {
                // move to the first character after the delimiter
                thisField += 1;

                // find the location of the next delimiter
                nextField = (char*)memchr(thisField, FIELD_DELIM, (msgEnd - thisField));
                if (nextField)
                {
                    *nextField = '\0';
                    nextField += 1;
                }
            }

            int ret = -1;
            unsigned msgFlags = 0;

            while (thisField && (numFields < maxFields))
            {
                // calculate the size of the data in the field
                ssize_t fieldLen = nextField ? (nextField - thisField)
                                             : (msgEnd - thisField);
                fieldLen -= 2;

                // skip empty fields
                if (fieldLen > 0)
                {
                    switch (*thisField++)
                    {
                        case 'L':
                            if (!(msgFlags & FLAG_HAVE_LOG_LEVEL))
                            {
                                ret = processLogLevel(thisField, fieldLen, &fields[numFields]);
                                msgFlags |= FLAG_HAVE_LOG_LEVEL;
                            }
                            break;
                        case 'T':
                            if (!(msgFlags & FLAG_HAVE_TIMESTAMP))
                            {
                                ret = processTimestamp(thisField, fieldLen, &fields[numFields]);
                                msgFlags |= FLAG_HAVE_TIMESTAMP;
                            }
                            break;
                        case 'P':
#if (AI_BUILD_TYPE == AI_DEBUG)
                            // processing container pids can be expensive and prone
                            // to abuse by a client, so disable this on release builds
                            if (!(msgFlags & FLAG_HAVE_PID))
                            {
                                ret = processPid(thisField, fieldLen, &fields[numFields]);
                                msgFlags |= FLAG_HAVE_PID;
                            }
#endif
                            break;
                        case 'R':
                            if (!(msgFlags & FLAG_HAVE_THREAD))
                            {
                                ret = processThreadName(thisField, fieldLen, &fields[numFields]);
                                msgFlags |= FLAG_HAVE_THREAD;
                            }
                            break;
                        case 'S':
                            if (!(msgFlags & FLAG_HAVE_SRCFILE))
                            {
                                ret = processCodeFile(thisField, fieldLen, &fields[numFields]);
                                msgFlags |= FLAG_HAVE_SRCFILE;
                            }
                            break;
                        case 'F':
                            if (!(msgFlags & FLAG_HAVE_FUNCTION))
                            {
                                ret = processCodeFunction(thisField, fieldLen, &fields[numFields]);
                                msgFlags |= FLAG_HAVE_FUNCTION;
                            }
                            break;
                        case 'N':
                            if (!(msgFlags & FLAG_HAVE_LINENO))
                            {
                                ret = processCodeLine(thisField, fieldLen, &fields[numFields]);
                                msgFlags |= FLAG_HAVE_LINENO;
                            }
                            break;
                        case 'M':
                            if (!(msgFlags & FLAG_HAVE_MESSAGE))
                            {
                                ret = processMessage(thisField, fieldLen, &fields[numFields]);
                                msgFlags |= FLAG_HAVE_MESSAGE;
                            }
                            break;
                        default:
                            // we're strict, any message that doesn't have the correct
                            // prefix results in the entire message being ignored.
                            ret = -1;
                            break;
                    }

                    if (ret < 0)
                        break;

                    numFields += ret;
                }

                thisField = nextField;
                if (thisField)
                {
                    nextField = (char*)memchr(thisField, FIELD_DELIM, (msgEnd - thisField));
                    if (nextField)
                    {
                        *nextField = '\0';
                        nextField += 1;
                    }
                }

            }

            // if no pid was set then ensure we set a default one to stop
            // journald from mistakenly grouping with the dobby service logs
            if (!(msgFlags & FLAG_HAVE_PID))
            {
                if (!mDefaultSyslogPid.empty() && (numFields < maxFields))
                {
                    fields[numFields].iov_base = const_cast<char*>(mDefaultSyslogPid.data());
                    fields[numFields].iov_len = mDefaultSyslogPid.length();
                    numFields++;
                }

                if (!mDefaultObjectPid.empty() && (numFields < maxFields))
                {
                    fields[numFields].iov_base = const_cast<char*>(mDefaultObjectPid.data());
                    fields[numFields].iov_len = mDefaultObjectPid.length();
                    numFields++;
                }
            }

            // if not aborted and have all the mandatory fields, then send the
            // message to journald
            if ((ret >= 0) && (numFields > 1))
            {
#if defined(ETHANLOG_DEBUG_DUMP)
                dumpMessage(fields, numFields);
#endif
                int rc = sd_journal_sendv(fields, numFields);
                if (rc < 0)
                    AI_LOG_SYS_ERROR(-rc, "failed to write to journald");
            }
        }

        // regardless of whether we successfully parsed the message or not, jump
        // over the message and if any left try to parse some more.
        mMsgLen -= (msgEnd - mMsgBuf);
        memmove(mMsgBuf, msgEnd, mMsgLen);
    }


    // sanity check the message length, this may exceed the maximum length if
    //no terminator was found
    if (mMsgLen >= ETHANLOG_MAX_LOG_MSG_LENGTH)
        mMsgLen = 0;

}

// -----------------------------------------------------------------------------
/**
 *  @brief Process the log level field
 *
 *  @param[in]  field           The field minus the leading 'L' character
 *  @param[out] iov             The formatted io vector for journald
 *
 *  @returns                    the number of fields added to iov on success,
 *                              -1 on failure
 */
int EthanLogClient::processLogLevel(const char *field, ssize_t len, struct iovec *iov) const
{
    // there should only be a single character in the field
    if (len != 1)
        return -1;

    // logging levels are character values from '1' to '6', the following
    // maps them to the syslog priority levels for journald
    unsigned ethanLogLevel = 0;
    switch (field[0])
    {
        case '1':
            ethanLogLevel = LOG_LEVEL_FATAL;
            iov->iov_base = (void*)"PRIORITY=2";
           break;
        case '2':
            ethanLogLevel = LOG_LEVEL_ERROR;
            iov->iov_base = (void*)"PRIORITY=3";
            break;
        case '3':
            ethanLogLevel = LOG_LEVEL_WARNING;
            iov->iov_base = (void*)"PRIORITY=4";
            break;
        case '4':
            ethanLogLevel = LOG_LEVEL_INFO;
            iov->iov_base = (void*)"PRIORITY=6";
            break;
        case '5':
            ethanLogLevel = LOG_LEVEL_DEBUG;
            iov->iov_base = (void*)"PRIORITY=7";
            break;
        case '6':
            ethanLogLevel = LOG_LEVEL_MILESTONE;
            iov->iov_base = (void*)"PRIORITY=5";
            break;
        default:
            // abort
            return -1;
    }

    iov->iov_len = 10;

    // check if we should be logging this level
    return (mAllowedLevels & ethanLogLevel) ? 1 : -1;
}

// -----------------------------------------------------------------------------
/**
 * @brief Process the timestamp field
 * @param[in]  tok   The field minus the leading 'T' character
 * @param[out] ts    The calculated timestamp
 * @returns          the number of fields added to iov on success, -1 on failure
 */
int EthanLogClient::processTimestamp(const char *field, ssize_t, struct iovec *iov) const
{
    static char buf[64];
    int len = snprintf(buf, sizeof(buf), "MONOTONIC_TS=%s", field);

    iov->iov_base = buf;
    iov->iov_len = std::min<size_t>(len, sizeof(buf));
    return 1;
}

// -----------------------------------------------------------------------------
/**
 * @brief Process the message field
 * @param[in]  tok     The field minus the leading 'F' character
 * @param[out] message Upon return will point to the message string
 * @returns            the number of fields added to iov on success, -1 on failure
 */
int EthanLogClient::processMessage(const char *field, ssize_t len, struct iovec *iov) const
{
    static char buf[8 + ETHANLOG_MAX_LOG_MSG_LENGTH];
    memcpy(buf, "MESSAGE=", 8);

    len = std::min<size_t>(ETHANLOG_MAX_LOG_MSG_LENGTH, len);
    memcpy(buf + 8, field, len);

    iov->iov_base = buf;
    iov->iov_len = 8 + len;
    return 1;
}

#if (AI_BUILD_TYPE == AI_DEBUG)
// -----------------------------------------------------------------------------
/**
 * @brief Process the pid field
 * @param[in]  field   The field minus the leading 'P' character
 * @param[in]  len     The length of the field.
 * @returns            The number of fields added to iov on success, -1 on failure
 */
int EthanLogClient::processPid(const char *field, ssize_t len, struct iovec *iov) const
{
    // convert hex digits to number
    char *end = nullptr;
    long pid = strtol(field, &end, 16);
    if ((pid < 1) || (pid == LONG_MAX) || (end != (field + len)))
    {
        pid = -1;
    }
    else
    {
        pid = findRealPid(pid);
    }

    // if didn't find a pid or field was badly formatted then use the defaults
    int n = 0;
    if (pid <= 0)
    {
        if (!mDefaultObjectPid.empty())
        {
            iov->iov_base = const_cast<char*>(mDefaultObjectPid.data());
            iov->iov_len = mDefaultObjectPid.size();
            iov++;
            n++;
        }
        if (!mDefaultSyslogPid.empty())
        {
            iov->iov_base = const_cast<char*>(mDefaultSyslogPid.data());
            iov->iov_len = mDefaultSyslogPid.size();
            n++;
        }
    }
    else
    {
        {
            static char syslogBuf[48];
            iov->iov_base = syslogBuf;
            iov->iov_len = sprintf(syslogBuf, "SYSLOG_PID=%ld", pid);
            iov++;
            n++;
        }

        {
            static char objBuf[48];
            iov->iov_base = objBuf;
            iov->iov_len = sprintf(objBuf, "OBJECT_PID=%ld", pid);
            n++;
        }
    }

    return n;
}
#endif // (AI_BUILD_TYPE == AI_DEBUG)

// -----------------------------------------------------------------------------
/**
 * @brief Process the thread name field
 * @param[in]  tok     The field minus the leading 'R' character
 * @param[out] thread  Upon return will point to the thread name
 * @returns            The number of fields added to iov on success, -1 on failure
 */
int EthanLogClient::processThreadName(const char *field, ssize_t len, struct iovec *iov) const
{
    static char buf[32];
    memcpy(buf, "THREAD_NAME=", 12);

    len = std::min<size_t>(sizeof(buf) - 12, len);
    memcpy(buf + 12, field, len);

    iov->iov_base = buf;
    iov->iov_len = 12 + len;
    return 1;
}

// -----------------------------------------------------------------------------
/**
 * @brief Process the line number field
 * @param[in]  tok     The field minus the leading 'N' character
 * @param[out] lineno  Upon return will container the line number
 * @returns            The number of fields added to iov on success, -1 on failure
 */
int EthanLogClient::processCodeLine(const char *field, ssize_t len, struct iovec *iov) const
{
    // check all the field contains numeric characters
    const char *ptr = field;
    while (*ptr != '\0')
    {
        if (!std::isdigit(*ptr))
            return -1;

        ptr++;
    }

    static char buf[32];
    memcpy(buf, "CODE_LINE=", 10);

    len = std::min<size_t>(sizeof(buf) - 10, len);
    memcpy(buf + 10, field, len);

    iov->iov_base = buf;
    iov->iov_len = 10 + len;
    return 1;
}

// -----------------------------------------------------------------------------
/**
 * @brief Process the function name field
 * @param[in]  tok     The field minus the leading 'F' character
 * @param[out] func    Upon return will point to the function name string
 * @returns            The number of fields added to iov on success, -1 on failure
 */
int EthanLogClient::processCodeFunction(const char *field, ssize_t len, struct iovec *iov) const
{
    static char buf[128];
    memcpy(buf, "CODE_FUNC=", 10);

    len = std::min<size_t>(sizeof(buf) - 10, len);
    memcpy(buf + 10, field, len);

    iov->iov_base = buf;
    iov->iov_len = 10 + len;
    return 1;
}

// -----------------------------------------------------------------------------
/**
 * @brief Process the function name field
 * @param[in]  tok     The field minus the leading 'S' character
 * @param[out] func    Upon return will point to the function name string
 * @returns            The number of fields added to iov on success, -1 on failure
 */
int EthanLogClient::processCodeFile(const char *field, ssize_t len, struct iovec *iov) const
{
    static char buf[128];
    memcpy(buf, "CODE_FILE=", 10);

    len = std::min<size_t>(sizeof(buf) - 10, len);
    memcpy(buf + 10, field, len);

    iov->iov_base = buf;
    iov->iov_len = 10 + len;
    return 1;
}

#if (AI_BUILD_TYPE == AI_DEBUG)
// -----------------------------------------------------------------------------
/**
 * @brief Attempts to find the pid number in the root pid namespace from a pid
 * in the containers namespace.
 *
 * It's tricky getting the real pid of a process in a namespace, see the below
 * link for more information:
 * https://blogs.oracle.com/linux/translating-process-id-between-namespaces
 *
 * However we can make some assumptions about our containers which make it
 * slightly easier; the first is that we always have a memory cgroup setup for
 * them, and secondly there aren't typically going to be lots of processes in
 * our containers.
 * So what we do is read the /sys/fs/cgroup/memory/<id>/cgroup.procs file to
 * get all the processes within the container, then we read each of their
 * /proc/<pid>/status files to extract the NSpid fields and then match them up.
 *
 * To speed up the process, everytime this method is called and we don't have
 * an existing mapping, then we re-create the full mapping.  This helps flush
 * out dead processes from the cache and also speed up subsequent lookups.
 * However this could result in a bit of load in Dobby, if the client constantly
 * sent invalid pid numbers to us ... or more likely there are lots of transient
 * processes that log just a single line, like a shell script or something ...
 * not sure what the solution for that is.
 *
 * @param[in]  nsPid   The pid in the namespace of the container.
 * @returns            The 'real' pid in the global namespace.
 */
pid_t EthanLogClient::findRealPid(pid_t nsPid) const
{
    // check the mapping table
    auto it = mNsToRealPidMapping.find(nsPid);
    if (it != mNsToRealPidMapping.end())
        return it->second;


    // get the list of all pids within the container
    std::set<pid_t> realPids = getAllContainerPids();

    // harmonise the real pids list, ie. remove any pids that are no longer in
    // the container
    it = mNsToRealPidMapping.begin();
    while (it != mNsToRealPidMapping.end())
    {
        const int realPid = it->second;
        if (realPids.count(realPid) == 0)
        {
            it = mNsToRealPidMapping.erase(it);
        }
        else
        {
            realPids.erase(realPid);
            ++it;
        }
    }

    // so now mNsToRealPidMapping should only have pids that are still in the
    // container, and realPids should only have pids that are not already in
    // the mNsToRealPidMapping map
    for (const pid_t &realPid : realPids)
    {
        pid_t namespacedPid = readNsPidFromProc(realPid);
        if (namespacedPid > 0)
        {
            mNsToRealPidMapping.emplace(namespacedPid, realPid);
            AI_LOG_INFO("added mapping for container pid %d to real pid %d",
                        namespacedPid, realPid);
        }
    }

    // now look again for the requested pid
    it = mNsToRealPidMapping.find(nsPid);
    if (it == mNsToRealPidMapping.end())
        return -1;
    else
        return it->second;
}

// -----------------------------------------------------------------------------
/**
 * @brief Reads the set of all pids within the client's container.
 *
 * This reads the cgroup.pids file from the memory cgroup for the container.
 *
 * @returns            Set of all the real pids within the container.
 */
std::set<pid_t> EthanLogClient::getAllContainerPids() const
{
    std::set<pid_t> realPids;

    // get the list of all pids within the container
    int fd = open(mCgroupPidsPath.c_str(), O_CLOEXEC | O_RDONLY);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open container cgroup file @ '%s'",
                         mCgroupPidsPath.c_str());
        return realPids;
    }

    // nb: stdio_filebuf closes the fd on destruction
    __gnu_cxx::stdio_filebuf<char> cgrpPidsBuf(fd, std::ios::in);
    std::istream cgrpPidsStream(&cgrpPidsBuf);

    std::string pidLine;
    while (std::getline(cgrpPidsStream, pidLine))
    {
        try
        {
            const pid_t pid = std::stol(pidLine);
            realPids.emplace(pid);
        }
        catch (std::exception &e)
        {
            AI_LOG_ERROR("failed to convert pid '%s' to long (%s)",
                         pidLine.c_str(), e.what());
        }
    }

    return realPids;
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
pid_t EthanLogClient::readNsPidFromProc(pid_t pid) const
{
    char filePathBuf[32];
    sprintf(filePathBuf, "/proc/%d/status", pid);

    // get the list of all pids within the container
    int fd = open(filePathBuf, O_CLOEXEC | O_RDONLY);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open procfs file @ '%s'", filePathBuf);
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

#endif // (AI_BUILD_TYPE == AI_DEBUG)
