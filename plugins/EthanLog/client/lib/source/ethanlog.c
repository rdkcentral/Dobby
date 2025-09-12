/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/uio.h>

#define ELOG_RECORD_DELIM               '\x1e'
#define ELOG_FIELD_DELIM                '\x1f'

#define ELOG_MAX_LOG_MSG_LENGTH         512UL
#define ELOG_MAX_FUNC_NAME_LENGTH       ((int)128)
#define ELOG_MAX_FILE_NAME_LENGTH       ((int)64)


#define ELOG_MIN(a,b)   (((a) < (b)) ? (a) : (b))

#define ELOG_PIPE_UNINITIALIZED         -1
#define ELOG_PIPE_ERROR                 -2
#define ELOG_PIPE_REDIRECT_CONSOLE      -3

static int _ethanlog_pipefd = ELOG_PIPE_UNINITIALIZED;


static int ethanlog_init(void)
{
    const char* env;
    int pipe_fd;

    env = getenv("ETHAN_LOGGING_TO_CONSOLE");
    if ((env != NULL) && (env[0] == '1') && (env[1] == '\0')) {
        return ELOG_PIPE_REDIRECT_CONSOLE;
    }

    /* the following environment variable is set by the hypervisor, it
     * tells us the number of open file descriptor to use for logging.
     */
    env = getenv("ETHAN_LOGGING_PIPE");
    if (env == NULL) {
        return ELOG_PIPE_ERROR;
    }

    pipe_fd = atoi(env);
    if ((pipe_fd < 3) || (pipe_fd > 2048)) {
        return ELOG_PIPE_ERROR;
    }

    return pipe_fd;
}

static int ethanlog_console(int level, const char *filename, const char *function, int line,
                            const char *format, va_list ap)
{
    int n = 0;
    struct iovec iov[6];
    char tbuf[32];
    char fbuf[ELOG_MAX_FILE_NAME_LENGTH + ELOG_MAX_FUNC_NAME_LENGTH + 32];
    char mbuf[ELOG_MAX_LOG_MSG_LENGTH];
    int len;
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);

    iov[n].iov_base = tbuf;
    iov[n].iov_len = snprintf(tbuf, sizeof(tbuf), "%.010lu.%.06lu ",
                              ts.tv_sec, ts.tv_nsec / 1000);
    iov[n].iov_len = ELOG_MIN(iov[n].iov_len, sizeof(tbuf));
    n++;

    switch (level) {
        case ETHAN_LOG_FATAL:
            iov[n].iov_base = (void*)"FTL: ";
            iov[n].iov_len = 5;
            break;
        case ETHAN_LOG_ERROR:
            iov[n].iov_base = (void*)"ERR: ";
            iov[n].iov_len = 5;
            break;
        case ETHAN_LOG_WARNING:
            iov[n].iov_base = (void*)"WRN: ";
            iov[n].iov_len = 5;
            break;
        case ETHAN_LOG_MILESTONE:
            iov[n].iov_base = (void*)"MIL: ";
            iov[n].iov_len = 5;
            break;
        case ETHAN_LOG_INFO:
            iov[n].iov_base = (void*)"NFO: ";
            iov[n].iov_len = 5;
            break;
        case ETHAN_LOG_DEBUG:
            iov[n].iov_base = (void*)"DBG: ";
            iov[n].iov_len = 5;
            break;
        default:
            iov[n].iov_base = (void*)": ";
            iov[n].iov_len = 2;
            break;
    }
    n++;

    len = snprintf(fbuf, sizeof(fbuf), "< S:%.*s F:%.*s L:%d > ",
                   ELOG_MAX_FILE_NAME_LENGTH, filename ?: "?",
                   ELOG_MAX_FUNC_NAME_LENGTH, function ?: "?",
                   line);
    if (len > 0 ) {
        iov[n].iov_base = (void *) fbuf;
        iov[n].iov_len = ELOG_MIN(len, sizeof(fbuf));
        n++;
    }

    len = vsnprintf(mbuf, sizeof(mbuf), format, ap);
    if (len > 0) {
        iov[n].iov_base = (void *) mbuf;
        iov[n].iov_len = ELOG_MIN(len, sizeof(mbuf));
        n++;
    }

    iov[n].iov_base = (void*)"\n";
    iov[n].iov_len = 1;
    n++;

    if (level <= ETHAN_LOG_WARNING) {
        return TEMP_FAILURE_RETRY(writev(STDERR_FILENO, iov, n));
    } else {
        return TEMP_FAILURE_RETRY(writev(STDOUT_FILENO, iov, n));
    }
}

int ethanlog_vprint(int level, const char *filename, const char *function, int line,
                    const char *format, va_list ap)
{
    struct timespec ts;
    char buf[ELOG_MAX_LOG_MSG_LENGTH];
    char *p, *end;
    char *basename;
    int len;

    /* run the sanity checks first */
    if ((level < ETHAN_LOG_FATAL) || (level > ETHAN_LOG_MILESTONE)) {
        errno = EINVAL;
        return -1;
    }

    /* initialise the pipe if we haven't already */
    if (__builtin_expect((_ethanlog_pipefd == ELOG_PIPE_UNINITIALIZED), 0)) {
        _ethanlog_pipefd = ethanlog_init();
    }

    /* check managed to initialise the pipe */
    if (__builtin_expect((_ethanlog_pipefd == ELOG_PIPE_ERROR), 0)) {
        errno = EPIPE;
        return -1;
    }

    /* check if environment variable is set to redirect stderr/stdout */
    if (_ethanlog_pipefd == ELOG_PIPE_REDIRECT_CONSOLE) {
        return ethanlog_console(level, filename, function, line, format, ap);
    }


    p = buf;
    end = buf + ELOG_MAX_LOG_MSG_LENGTH;


    *p++ = ELOG_RECORD_DELIM;

    /* level field */
    *p++ = ELOG_FIELD_DELIM;
    *p++ = 'L';
    *p++ = '0' + level;

    /* timestamp */
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        ts.tv_sec = ts.tv_nsec = 0;
    p += sprintf(p, "%cT%08lx.%08lx", ELOG_FIELD_DELIM, ts.tv_sec, ts.tv_nsec);

    /* source file basename (limited to 64 characters) */
    if (filename) {
        *p++ = ELOG_FIELD_DELIM;
        if ((basename = strrchr(filename, '/')) != NULL)
            filename = (basename + 1);
        p += sprintf(p, "S%.*s", ELOG_MAX_FILE_NAME_LENGTH, filename);
    }

    /* function (limited to 128 characters) */
    if (function) {
        *p++ = ELOG_FIELD_DELIM;
        p += sprintf(p, "F%.*s", ELOG_MAX_FUNC_NAME_LENGTH, function);
    }

    /* line number */
    if (line > 0) {
        *p++ = ELOG_FIELD_DELIM;
        p += sprintf(p, "N%d", line);
    }


    /* process id */
    *p++ = ELOG_FIELD_DELIM;
    p += sprintf(p, "P%x", getpid());


    /* apply the message, limit it to the buffer size */
    *p++ = ELOG_FIELD_DELIM;
    *p++ = 'M';
    if (format) {
        len = vsnprintf(p, (end - p) - 1, format, ap);
        p += ELOG_MIN(len, ((end - p) - 1));
    }

    /* set the terminator and we're done */
    *p++ = ELOG_RECORD_DELIM;


    /* finally we need to send the message */
    return TEMP_FAILURE_RETRY(write(_ethanlog_pipefd, buf, (p - buf)));
}

void vethanlog(int level, const char *filename, const char *function,
               int line, const char *format, va_list ap)
{
    ethanlog_vprint(level, filename, function, line, format, ap);
}

void ethanlog(int level, const char *filename, const char *function, int line,
              const char *format, ...)
{
    va_list vl;
    
    va_start(vl, format);
    ethanlog_vprint(level, filename, function, line, format, vl);
    va_end(vl);
}

