/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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

#include <map>
#include <cctype>
#include <string>
#include <vector>
#include <algorithm>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <poll.h>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(exp)            \
  ({                                       \
    decltype(exp) _rc;                     \
    do {                                   \
      _rc = (exp);                         \
    } while (_rc == -1 && errno == EINTR); \
    _rc;                                   \
  })
#endif

/// The log level used for all messages from stdout
static int gDefaultLogLevel = ETHAN_LOG_INFO;

/// The log level used for all messages from stderr
static int gDefaultStderrLogLevel = ETHAN_LOG_WARNING;

/// Set to true if the we automatically parse the log level from the message
static bool gEnableLevelPrefix = true;




// -----------------------------------------------------------------------------
/**
 * @class PipeInput
 * @brief Object that reads a read end of a pipe and sends the data received
 * on it to ethanlog.
 *
 * This attempts to split the data into newline separated log messages.  It also
 * parses the first 3 lines of the message to see if it contains the explicit
 * log level.
 *
 *
 */
class PipeInput
{
public:
    PipeInput(int fd, int logLevel, const char *filename)
        : mFd(fd)
        , mLogLevel(logLevel)
        , mFileName(filename)
        , mValid((fd >= 0))
        , mBufferOffset(0)
    {
    }

    ~PipeInput()
    {
        // if there is still something in the buffer then write it out as well
        // provided it's not empty
        if (mBufferOffset > 0)
        {
            bool isEmpty = true;
            for (int i = 0; i < mBufferOffset; i++)
            {
                if (!::isspace(mBuffer[i]))
                {
                    isEmpty = false;
                    break;
                }
            }

            if (!isEmpty)
            {
                logMessage(mBuffer, mBufferOffset);
            }
        }
    }

public:
    inline bool isValid() const
    {
        return mValid;
    }

    void onReadReady()
    {
        // read as much to fill in the buffer
        ssize_t rc = TEMP_FAILURE_RETRY(read(mFd, mBuffer + mBufferOffset, sizeof(mBuffer) - mBufferOffset));
        if ((rc <= 0) || ((size_t)rc > sizeof(mBuffer) - mBufferOffset))
        {
            mValid = false;
            return;
        }

        // update the offset and process the buffer
        mBufferOffset += rc;
        processBuffer();
    }

private:
    void processBuffer()
    {
        const char *ptr = mBuffer;
        const char* const endPtr = mBuffer + mBufferOffset;
        const char *consumedPtr = ptr;
        const char *lineStart = nullptr;

        // send the buffer contents, split and trimmed of newlines
        while (ptr < endPtr)
        {
            char ch = *ptr++;
            if ((ch == '\n') || (ch == '\r'))
            {
                if (lineStart)
                {
                    ssize_t len = (ptr - lineStart) - 1;
                    if (len > 0)
                    {
                        logMessage(lineStart, len);
                        consumedPtr = ptr;
                    }

                    lineStart = nullptr;
                }
            }
            else if (!lineStart)
            {
                lineStart = ptr - 1;
            }
        }

        // strip any trailing newline and whitespace
        while (consumedPtr < endPtr)
        {
            if (::isspace(*consumedPtr))
                consumedPtr++;
            else
                break;
        }

        // shift the buffer to remove consumed data
        if (consumedPtr > mBuffer)
        {
            mBufferOffset -= (consumedPtr - mBuffer);
            memmove(mBuffer, consumedPtr, mBufferOffset);
        }

        // if we've over the high water mark then just log a bunch of stuff at
        // the start of the buffer and move it along
        if (mBufferOffset > (sizeof(mBuffer) - 256))
        {
            logMessage(mBuffer, 256);

            mBufferOffset -= 256;
            memmove(mBuffer, mBuffer + 256, mBufferOffset);
        }
    }

    void logMessage(const char *message, int messageLen) const
    {
        int level = mLogLevel;

        if (gEnableLevelPrefix &&
            (messageLen >= 3) &&
            (message[0] == '<') &&
            (message[1] >= '1' && message[1] <= '6') &&
            (message[2] == '>'))
        {
            level = message[1] - '0';

            message += 3;
            messageLen -= 3;
        }

        if (messageLen > 0)
        {
            ethanlog(level, mFileName.c_str(), nullptr, -1, "%.*s", messageLen, message);
        }
    }

private:
    const int mFd;
    const int mLogLevel;
    const std::string mFileName;

    bool mValid;

    char mBuffer[1024];
    size_t mBufferOffset;
};




// -----------------------------------------------------------------------------
/**
 * @brief Simply prints the version string on stdout
 *
 *
 *
 */
static void displayVersion()
{
    printf("Version: " DOBBY_VERSION "\n");
}

// -----------------------------------------------------------------------------
/**
 * @brief Simply prints the usage options to stdout
 *
 *
 *
 */
static void displayUsage()
{
    printf("Usage: ethanlog-cat <option(s)>\n");
    printf("  Execute process with stdout/stderr connected to the ethanlog.\n");
    printf("  Typical usage:\n");
    printf("       ./run-something.sh 2>&1 | ethanlog-cat \n");
    printf("\n");
    printf("  -h, --help                      Print this help and exit\n");
    printf("  -V, --version                   Display this program's version number\n");
    printf("\n");
    printf("  -p, --priority=PRIORITY         Set the priority value (1..6) [%d]\n", gDefaultLogLevel);
    // printf("      --stderr-priority=PRIORITY  Set the priority value of messages from stderr (1..6) [%d]\n", gDefaultStderrLogLevel);
    printf("      --level-prefix=BOOL         Control whether level prefix shall be parsed [%s]\n", gEnableLevelPrefix ? "true" : "false");
    printf("\n");
}

// -----------------------------------------------------------------------------
/**
 * @brief Parses the log level string, which may be either a number or a string
 * representing the log level.
 *
 * Returns -1 on failure, if a failure then it's logged.
 *
 */
static int logLevelFromString(const char *level)
{
    const std::map<std::string, int> levels = {
        { "fatal",     ETHAN_LOG_FATAL      },
        { "crit",      ETHAN_LOG_FATAL      },
        { "1",         ETHAN_LOG_FATAL      },
        { "error",     ETHAN_LOG_ERROR      },
        { "err",       ETHAN_LOG_ERROR      },
        { "2",         ETHAN_LOG_ERROR      },
        { "warning",   ETHAN_LOG_WARNING    },
        { "3",         ETHAN_LOG_WARNING    },
        { "info",      ETHAN_LOG_INFO       },
        { "4",         ETHAN_LOG_INFO       },
        { "debug",     ETHAN_LOG_DEBUG      },
        { "5",         ETHAN_LOG_DEBUG      },
        { "notice",    ETHAN_LOG_MILESTONE  },
        { "milestone", ETHAN_LOG_MILESTONE  },
        { "6",         ETHAN_LOG_MILESTONE  },
    };

    std::string levelLowerCase(level);
    transform(levelLowerCase.begin(), levelLowerCase.end(), levelLowerCase.begin(), ::tolower);

    auto it = levels.find(levelLowerCase);
    if (it == levels.end())
    {
        fprintf(stderr, "Error: invalid log priority level argument '%s'\n", level);
        return -1;
    }

    return it->second;
}

// -----------------------------------------------------------------------------
/**
 * @brief Parses the command line args
 *
 *
 *
 */
static void parseArgs(int argc, char **argv)
{
    struct option longopts[] = {
        { "help",           no_argument,        nullptr,    (int)'h' },
        { "version",        no_argument,        nullptr,    (int)'V' },
        { "priority",       required_argument,  nullptr,    (int)'p' },
        { "stderr-priority",required_argument,  nullptr,    (int)'s' },
        { "level-prefix",   required_argument,  nullptr,    (int)'l' },
        { nullptr,          0,                  nullptr,    0        }
    };

    opterr = 0;

    int c;
    int longindex;
    while ((c = getopt_long(argc, argv, "+hVp:", longopts, &longindex)) != -1)
    {
        switch (c)
        {
            case 'h':
                displayUsage();
                exit(EXIT_SUCCESS);

            case 'V':
                displayVersion();
                exit(EXIT_SUCCESS);

            case 'p':
                gDefaultLogLevel = logLevelFromString(optarg);
                if (gDefaultLogLevel < 0)
                {
                    // failure already logged in the logLevelFromString parser
                    exit(EXIT_FAILURE);
                }
                break;

            case 's':
                gDefaultStderrLogLevel = logLevelFromString(optarg);
                if (gDefaultStderrLogLevel < 0)
                {
                    // failure already logged in the logLevelFromString parser
                    exit(EXIT_FAILURE);
                }
                break;

            case 'l':
                gEnableLevelPrefix = (strcasecmp(optarg, "1") == 0) ||
                                     (strcasecmp(optarg, "yes") == 0) ||
                                     (strcasecmp(optarg, "true") == 0);
                break;

            case '?':
                if (optopt == 'c')
                    fprintf(stderr, "Warning: Option -%c requires an argument.\n", optopt);
                else if (isprint(optopt))
                    fprintf(stderr, "Warning: Unknown option `-%c'.\n", optopt);
                else
                    fprintf(stderr, "Warning: Unknown option character `\\x%x'.\n", optopt);
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief Reads from the supplied fds and sends any contents to the ethanlog
 * pipe formatted.
 *
 * This is a blocking call and only returns if one of the write side of the
 * pipes is closed.
 *
 *
 *
 */
static void redirectInputToEthanLog(int stdinFd, int stderrFd)
{
    std::vector<struct pollfd> fds;
    std::map<int, PipeInput> inputs;

    if (stdinFd >= 0)
    {
        struct pollfd fd;
        fd.fd = stdinFd;
        fd.events = POLLIN | POLLHUP;
        fd.revents = 0;

        fds.emplace_back(fd);
        inputs.emplace(stdinFd, PipeInput{ stdinFd, gDefaultLogLevel, "stdout" });
    }

    if (stderrFd >= 0)
    {
        struct pollfd fd;
        fd.fd = stderrFd;
        fd.events = POLLIN | POLLHUP;
        fd.revents = 0;

        fds.emplace_back(fd);
        inputs.emplace(stderrFd, PipeInput{stderrFd, gDefaultStderrLogLevel, "stderr" });
    }


    // loop while pipe(s) are valid
    while (true)
    {
        int rc = TEMP_FAILURE_RETRY(poll(fds.data(), fds.size(), 1000));
        if (rc < 0)
        {
            fprintf(stderr, "Error: poll failed (%d - %s)\n",
                    errno, strerror(errno));
            return;
        }

        for (const struct pollfd &fd : fds)
        {
            // check for input
            if (fd.revents & (POLLIN | POLLHUP))
            {
                PipeInput &input = inputs.at(fd.fd);

                input.onReadReady();

                if (!input.isValid())
                    return;
            }

            // on any pipe error we give up and abort sending to ethanlog
            if (fd.revents & POLLERR)
            {
                return;
            }

        }
    }
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
int forkExecCommand(const char *file, char *const argv[])
{
    // TODO: not implemented yet
    return EXIT_FAILURE;
}

// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
int main(int argc, char * argv[])
{
    // disable SIGPIPE
    signal(SIGPIPE, SIG_IGN);

    // parse all args
    parseArgs(argc, argv);

    // sanity check the pipe env var is set - it'll only be sent inside a
    // container so log a helpful message
    if (getenv("ETHAN_LOGGING_PIPE") == nullptr)
    {
        fprintf(stderr, "Error: no ethanlog pipe found, are you running "
                        "this in a Dobby container?\n");
        return EXIT_FAILURE;
    }

#if 0
    // check if a command was added to the end of the command line
    if (argc <= optind)
    {
        return forkExecCommand(argv[optind], argv + optind);
    }
#endif

    // otherwise just run a loop to read stdin and send to the log
    redirectInputToEthanLog(STDIN_FILENO, -1);

    return EXIT_SUCCESS;
}


