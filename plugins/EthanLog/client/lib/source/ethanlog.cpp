/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 Sky UK
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

#include "ethanlog.h"

#include <atomic>
#include <deque>
#include <string>
#include <mutex>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <algorithm>

#include <unistd.h>
#include <time.h>


static constexpr char ETHANLOG_RECORD_DELIM = '\x1e';
static constexpr char ETHANLOG_FIELD_DELIM = '\x1f';

static constexpr char ETHANLOG_WRAP_INDICATOR[] = "\xe2\x86\xb5"; // '↵' character

static constexpr size_t ETHANLOG_MAX_LOG_MSG_LENGTH = 512;
static constexpr int ETHANLOG_MAX_FUNC_NAME_LENGTH = 128;
static constexpr int ETHANLOG_MAX_FILE_NAME_LENGTH = 64;
static constexpr size_t ETHANLOG_MAX_WRAP_INDICATOR_LEN = 32;

static constexpr int ETHANLOG_PIPE_UNINITIALIZED = -1;
static constexpr int ETHANLOG_PIPE_REDIRECT_CONSOLE = -2;
static constexpr int ETHANLOG_PIPE_ERROR = -3;

static constexpr char ETHANLOG_LEVEL_FATAL_CHAR = '1';
static constexpr char ETHANLOG_LEVEL_ERROR_CHAR = '2';
static constexpr char ETHANLOG_LEVEL_WARNING_CHAR = '3';
static constexpr char ETHANLOG_LEVEL_INFO_CHAR = '4';
static constexpr char ETHANLOG_LEVEL_DEBUG_CHAR = '5';
static constexpr char ETHANLOG_LEVEL_MILESTONE_CHAR = '6';

static_assert(ETHANLOG_MAX_LOG_MSG_LENGTH >
             (64 + ETHANLOG_MAX_FUNC_NAME_LENGTH + ETHANLOG_MAX_FILE_NAME_LENGTH + ETHANLOG_MAX_WRAP_INDICATOR_LEN),
              "Max log message length must be large enough to hold the max size of all fields");

static std::atomic<int> gEthanlogPipefd{ETHANLOG_PIPE_UNINITIALIZED};

// The number of log messages to internally queue before we start dropping messages,
// this is disabled by default (max is 0) but can be enabled by setting the
// ETHAN_LOGGING_QUEUE_SIZE environment variable to a positive integer value
// (up to 1024).
static size_t gEthanLogMaxQueuedMessages = 0;

// Log wrapping is disabled by default, it can be enabled by setting the
// ETHAN_LOGGING_ENABLE_WRAP environment variable to "1".
static bool gEthanLogWrapEnabled = false;

// The wrap indicator is the set of characters to append to the end of a
// log message when it is too long and has been truncated.
static char gEthanLogWrapIndicator[ETHANLOG_MAX_WRAP_INDICATOR_LEN + 1] = { 0 };
static int gEthanLogWrapIndicatorLen = 0;


static size_t ethanlogPopulateMsgPrefix(int level, const char *filename,
                                        const char *function, int line,
                                        char* const buf);

// -----------------------------------------------------------------------------
/**
 * @brief Initialises the logging system, this is called on the first call to
 * log a message and initialises the pipe file descriptor to write log messages
 * to.
 *
 * This is also checks some environment variables to determine if log messages
 * should be written to the console instead of the pipe, and also to determine
 * if log line wrapping should be enabled and what the wrap indicator should be.
 *
 */
static void ethanLogInit()
{
    static std::once_flag initFlag;
    std::call_once(initFlag, []()
    {
        const char *env = getenv("ETHAN_LOGGING_TO_CONSOLE");
        if ((env != nullptr) && (env[0] == '1') && (env[1] == '\0'))
        {
            gEthanlogPipefd = ETHANLOG_PIPE_REDIRECT_CONSOLE;
            return;
        }

        // check if log line wrapping should be enabled
        env = getenv("ETHAN_LOGGING_ENABLE_WRAP");
        if ((env != nullptr) && (env[0] == '1') && (env[1] == '\0'))
        {
            gEthanLogWrapEnabled = true;
        }

        if (gEthanLogWrapEnabled)
        {
            // check if a custom log wrapping indicator is set, if not use the default
            env = getenv("ETHAN_LOGGING_WRAP_INDICATOR");
            if ((env != nullptr) && (env[0] != '\0'))
            {
                strncpy(gEthanLogWrapIndicator, env, ETHANLOG_MAX_WRAP_INDICATOR_LEN);
                gEthanLogWrapIndicator[ETHANLOG_MAX_WRAP_INDICATOR_LEN] = '\0';
            }
            else
            {
                strcpy(gEthanLogWrapIndicator, ETHANLOG_WRAP_INDICATOR);
            }
            gEthanLogWrapIndicatorLen = strlen(gEthanLogWrapIndicator);
        }

        // check if want to enable the log message queue
        env = getenv("ETHAN_LOGGING_QUEUE_SIZE");
        if (env != nullptr)
        {
            int queueSize = atoi(env);
            if (queueSize > 0)
            {
                gEthanLogMaxQueuedMessages = std::min<size_t>(queueSize, 1024UL);
            }
        }

        // the following environment variable is set by the hypervisor, it
        // tells us the number of open file descriptor to use for logging.
        env = getenv("ETHAN_LOGGING_PIPE");
        if (env == nullptr)
        {
            gEthanlogPipefd = ETHANLOG_PIPE_ERROR;
            return;
        }

        // the actual pipe fd
        int pipeFd = atoi(env);
        if ((pipeFd < 3) || (pipeFd > 2048))
        {
            gEthanlogPipefd = ETHANLOG_PIPE_ERROR;
        }
        else
        {
            gEthanlogPipefd = pipeFd;
        }
    });
}

// -----------------------------------------------------------------------------
/**
 * @brief Simple logging function that writes the log message to the console.
 *
 * This function is used if ETHAN_LOGGING_TO_CONSOLE=1 is set in the console.
 *
 */
static int ethanlogToConsole(int level, const char *filename, const char *function,
                             int line, const char *format, va_list ap)
{
    char tbuf[32];
    timespec ts = { };
    clock_gettime(CLOCK_MONOTONIC, &ts);
    snprintf(tbuf, sizeof(tbuf), "%.010lu.%.06lu", ts.tv_sec, ts.tv_nsec / 1000);

    const char *prefix;
    switch (level)
    {
        case ETHAN_LOG_FATAL:
            prefix = "FTL";
            break;
        case ETHAN_LOG_ERROR:
            prefix = "ERR";
            break;
        case ETHAN_LOG_WARNING:
            prefix = "WRN";
            break;
        case ETHAN_LOG_MILESTONE:
            prefix = "MIL";
            break;
        case ETHAN_LOG_INFO:
            prefix = "NFO";
            break;
        case ETHAN_LOG_DEBUG:
            prefix = "DBG";
            break;
        default:
            prefix = " ";
            break;
    }

    char mbuf[2048];
    vsnprintf(mbuf, sizeof(mbuf), format, ap);

    const int stream = (level <= ETHAN_LOG_WARNING) ? STDERR_FILENO : STDOUT_FILENO;
    return dprintf(stream, "%s %s: < S:%s F:%s L:%d > %s\n",
                   tbuf, prefix,
                   filename ? filename : "?",
                   function ? function : "?",
                   line, mbuf);
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 */
static bool ethanlogWriteToPipe(const char* buf, size_t len)
{
    const int fd = gEthanlogPipefd;
    if (fd < 0)
    {
        errno = EBADF;
        return false;
    }

    ssize_t written = TEMP_FAILURE_RETRY(write(fd, buf, len));
    if (written != static_cast<ssize_t>(len))
    {
        if ((errno == EWOULDBLOCK) || (errno == EAGAIN))
        {
            // The pipe is blocked, we will handle this in the caller by queuing the message
            return false;
        }
        else
        {
            // If we fail to write to the pipe, we mark it as an error and drop future messages.
            gEthanlogPipefd = ETHANLOG_PIPE_ERROR;
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 * @brief Helper function to create a log message indicating that messages have
 * been dropped due to backpressure on the pipe.
 *
 */
static std::string ethanlogCreateDroppedMessage(int count)
{
    char buf[ETHANLOG_MAX_LOG_MSG_LENGTH];
    const size_t prefixLen =
        ethanlogPopulateMsgPrefix(ETHAN_LOG_WARNING, nullptr, nullptr, -1, buf);
    const size_t maxMsgLen = ETHANLOG_MAX_LOG_MSG_LENGTH - prefixLen - 1;

    int msgLen = snprintf(buf + prefixLen, maxMsgLen,
                          "Dropped %d log messages due to backpressure", count);
    msgLen = std::min<int>(msgLen, (maxMsgLen - 1));

    buf[prefixLen + msgLen] = ETHANLOG_RECORD_DELIM;

    return std::string(buf, prefixLen + msgLen + 1);
}

// -----------------------------------------------------------------------------
/**
 * @brief Helper function to create a log message indicating that messages have
 * been dropped due to backpressure on the pipe.
 *
 */
static int ethanlogWriteMessageWithQueuing(const char* buf, size_t len)
{
    static std::atomic<int> droppedMessages{0};

    static std::mutex queueMessageLock;
    static std::deque<std::string> queuedMessages;
    static std::atomic<int> queuedMessageCount{0};

    // First check if we have queued messages, if we do then we need to write
    // those out before writing the new message, to ensure messages are written
    // in order (although this is not guaranteed if another thread is writing
    // messages at the same time, but it is better than always writing messages
    // out of order).
    if (queuedMessageCount > 0)
    {
        std::lock_guard<std::mutex> locker(queueMessageLock);

        while (!queuedMessages.empty())
        {
            const auto &msg = queuedMessages.front();
            if (ethanlogWriteToPipe(msg.data(), msg.size()))
            {
                queuedMessages.pop_front();
                queuedMessageCount--;
            }
            else
            {
                break;
            }
        }

        const size_t availableQueueSpace = gEthanLogMaxQueuedMessages - queuedMessages.size();
        const size_t requiredQueueSpace = (droppedMessages > 0) ? 2 : 1;

        // If we still have messages in the queue, then either queue up the new
        // message or drop it if we've reached the maximum queue size, to prevent
        // unbounded memory growth.
        if (availableQueueSpace < requiredQueueSpace)
        {
            droppedMessages++;
            errno = EWOULDBLOCK;
            return -1;
        }

        // If the queue is not empty then just add the new message to the end of
        // the queue, and we're done.
        if (!queuedMessages.empty())
        {
            // Add "dropped messages" message if needed before the new message,
            // to ensure it is logged before any new messages in the queue
            // (already checked for space in the queue above).
            if (droppedMessages > 0)
            {
                const auto msg = ethanlogCreateDroppedMessage(droppedMessages);
                queuedMessages.emplace_back(msg);
                queuedMessageCount++;
                droppedMessages = 0;
            }

            // Also queue the new message
            queuedMessages.emplace_back(buf, len);
            queuedMessageCount++;
            return 0;
        }

        // If we have dropped messages but the queue is now empty, then we can try
        // writing the dropped message directly to the pipe, if that fails then we
        // will add it to the queue and add the new message to the queue as well.
        if (droppedMessages > 0)
        {
            const auto msg = ethanlogCreateDroppedMessage(droppedMessages);
            if (!ethanlogWriteToPipe(msg.data(), msg.size()))
            {
                queuedMessages.emplace_back(msg);
                queuedMessageCount++;
            }

            droppedMessages = 0;
        }
    }

    // At this point we've successfully written all queued messages, however
    // the pipe _may_ still be blocked on this write, or another thread may come
    // in and filled the pipe and the queued messages again.  So we try and write,
    // if fails then we try and queue the message again (which may fail if another
    // thread has filled the queue in the meantime).
    if (!ethanlogWriteToPipe(buf, len))
    {
        std::lock_guard<std::mutex> locker(queueMessageLock);

        if (queuedMessages.size() < gEthanLogMaxQueuedMessages)
        {
            queuedMessages.emplace_back(buf, len);
            queuedMessageCount++;
            return 0;
        }

        droppedMessages++;
        errno = EWOULDBLOCK;
        return -1;
    }

    return len;
}

// -----------------------------------------------------------------------------
/**
 * @brief Attempts to write the given log message to the pipe, if the pipe is
 * blocked then the message may be queued if queueing is enabled, otherwise the
 * message will be dropped.
 *
 *
 */
static int ethanlogWriteMessage(const char* buf, size_t len)
{
    const int fd = gEthanlogPipefd;
    if (fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    if (gEthanLogMaxQueuedMessages > 0)
        return ethanlogWriteMessageWithQueuing(buf, len);

    if (!ethanlogWriteToPipe(buf, len))
        return -1;
    else
        return len;
}

// -----------------------------------------------------------------------------
/**
 * @brief Populates a log line prefix with the given log level, filename,
 * function name and line number,
 *
 * The end result is the \a buf filled with the prefix up to the 'M' field, and
 * the return value is the length of the prefix in bytes.  The caller can then
 * append the log message after the prefix, and then add the record delimiter at
 * the end before writing the message to the pipe.
 *
 */
static size_t ethanlogPopulateMsgPrefix(int level, const char *filename,
                                        const char *function, int line,
                                        char* const buf)
{
    char *p = buf;

    *p++ = ETHANLOG_RECORD_DELIM;

    // Log level
    *p++ = ETHANLOG_FIELD_DELIM;
    *p++ = 'L';
    switch (level)
    {
        case ETHAN_LOG_DEBUG:
            *p++ = ETHANLOG_LEVEL_DEBUG_CHAR;
            break;
        case ETHAN_LOG_INFO:
            *p++ = ETHANLOG_LEVEL_INFO_CHAR;
            break;
        case ETHAN_LOG_MILESTONE:
            *p++ = ETHANLOG_LEVEL_MILESTONE_CHAR;
            break;
        case ETHAN_LOG_WARNING:
            *p++ = ETHANLOG_LEVEL_WARNING_CHAR;
            break;
        case ETHAN_LOG_ERROR:
            *p++ = ETHANLOG_LEVEL_ERROR_CHAR;
            break;
        case ETHAN_LOG_FATAL:
            *p++ = ETHANLOG_LEVEL_FATAL_CHAR;
            break;
        default:
            *p++ = ETHANLOG_LEVEL_INFO_CHAR;
    }

    // gcc complains about the use of sprintf, but all writes in this code are carefully bounds checked to ensure we
    // don't overflow the buffer, so we can safely ignore these warnings.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

    // Timestamp for the log
    timespec ts = {};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        ts.tv_sec = ts.tv_nsec = 0;
    p += sprintf(p, "%cT%08lx.%08lx", ETHANLOG_FIELD_DELIM, ts.tv_sec, ts.tv_nsec);

    // Filename (limited to 64 characters)
    if (filename)
    {
        const char *basename = strrchr(filename, '/');
        if (basename != nullptr)
            filename = (basename + 1);

        p += sprintf(p, "%cS%.*s", ETHANLOG_FIELD_DELIM, ETHANLOG_MAX_FILE_NAME_LENGTH, filename);
    }

    // Function name (limited to 128 characters)
    if (function)
        p += sprintf(p, "%cF%.*s", ETHANLOG_FIELD_DELIM, ETHANLOG_MAX_FUNC_NAME_LENGTH, function);

    // We deliberately don't include the pid, it is disabled on PROD builds in
    // Dobby (the consumer of the pipe) due to the overhead of aligning the pids
    // inside and outside the containers.
    // p += sprintf(p, "%cP%x", ETHANLOG_FIELD_DELIM, getpid());

    // Line number
    if (line > 0)
        p += sprintf(p, "%cN%d", ETHANLOG_FIELD_DELIM, line);

    // Finally the start of the message - "<file>:<line>:"
    p += sprintf(p, "%cM", ETHANLOG_FIELD_DELIM);

#pragma GCC diagnostic pop

    return p - buf;
}

// -----------------------------------------------------------------------------
/**
 * @brief Main logging function, this constructs the message to send in the
 * pipe and sends it.
 *
 * It log line wrapping is enabled, then multiple message packages could be sent
 * for a single log message, with the wrap indicator appended to all but the last
 * message package.  If wrapping is disabled (the default) then the message will
 * be truncated if it exceeds the maximum.
 *
 * Messages may also be queued if the pipe is blocked, if the
 * ETHAN_LOGGING_QUEUE_SIZE environment variable is set to a non-zero value,
 * to prevent log messages from being dropped due to backpressure on the pipe.
 * If the queue is full then messages will be dropped and the code will attempt
 * to add a log message indicating that messages have been dropped due to
 * backpressure.
 *
 * Returns the 0 on success and -1 on error with errno set to indicate the error.
 */
extern "C" int ethanlog_vprint(int level, const char *filename,
                               const char *function, int line,
                               const char *format, va_list ap)
{
    // Run the sanity checks first
    if ((level < ETHAN_LOG_FATAL) || (level > ETHAN_LOG_MILESTONE))
    {
        errno = EINVAL;
        return -1;
    }

    // Initialise the pipe if we haven't already
    if (gEthanlogPipefd == ETHANLOG_PIPE_UNINITIALIZED)
    {
        ethanLogInit();
    }
    if (gEthanlogPipefd == ETHANLOG_PIPE_ERROR)
    {
        errno = EPIPE;
        return -1;
    }
    if (gEthanlogPipefd == ETHANLOG_PIPE_REDIRECT_CONSOLE)
    {
        return ethanlogToConsole(level, filename, function, line, format, ap);
    }

    // Populate the start of the log message buffer with prefix information
    // (timestamp, file, function, line number etc) this is constant for all
    // log lines even when wrapping is enabled.
    char buf[ETHANLOG_MAX_LOG_MSG_LENGTH];
    const size_t wrapIndicatorLen = gEthanLogWrapEnabled ? gEthanLogWrapIndicatorLen : 0;
    const size_t prefixLen = ethanlogPopulateMsgPrefix(level, filename, function, line, buf);
    const size_t maxLineLen = (ETHANLOG_MAX_LOG_MSG_LENGTH - prefixLen - wrapIndicatorLen - 1);

    // If wrapping is not enabled then just write as much of the message as
    // we can into the buffer
    if (!gEthanLogWrapEnabled)
    {
        int messageLen = vsnprintf(buf + prefixLen, maxLineLen, format, ap);
        if (messageLen < 0)
            return -1;

        messageLen = std::min<int>(messageLen, (maxLineLen - 1));
        buf[prefixLen + messageLen] = ETHANLOG_RECORD_DELIM;
        return ethanlogWriteMessage(buf, prefixLen + messageLen + 1);
    }
    else
    {
        // If wrapping is enabled then need to format the message into
        // a temporary buffer first so we can split it across multiple log
        // lines if needed.
        char messageBuf[4096];
        int messageLen = vsnprintf(messageBuf, sizeof(messageBuf), format, ap);
        if (messageLen < 0)
            return -1;

        messageLen = std::min<int>(messageLen, sizeof(messageBuf) - 1);

        int totalWritten = 0;

        // Write the log message in chunks of maxLineLen, appending the wrap
        // indicator and record delimiter as needed.
        int pos = 0;
        while (pos < messageLen)
        {
            int lineLen = std::min<int>(messageLen - pos, maxLineLen);

            // Copy the message into the buffer
            memcpy(buf + prefixLen, messageBuf + pos, lineLen);
            pos += lineLen;

            // Check if we should append the wrap indicator character(s)
            if ((pos < messageLen) && (wrapIndicatorLen > 0))
            {
                memcpy(buf + prefixLen + lineLen, gEthanLogWrapIndicator, wrapIndicatorLen);
                lineLen += wrapIndicatorLen;
            }

            // Append the log record delimiter at the end of the message
            buf[prefixLen + lineLen] = ETHANLOG_RECORD_DELIM;
            int written = ethanlogWriteMessage(buf, prefixLen + lineLen + 1);
            if (written < 0)
                return -1;

            totalWritten += written;
        }

        return totalWritten;
    }
}

extern "C" void vethanlog(int level, const char *filename, const char *function,
                          int line, const char *format, va_list ap)
{
    (void) ethanlog_vprint(level, filename, function, line, format, ap);
}

extern "C" void ethanlog(int level, const char *filename, const char *function,
                         int line, const char *format, ...)
{
    va_list vl;
    va_start(vl, format);
    (void) ethanlog_vprint(level, filename, function, line, format, vl);
    va_end(vl);
}
