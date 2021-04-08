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
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/syscall.h>

#define ELOG_RECORD_DELIM               '\x1e'
#define ELOG_FIELD_DELIM                '\x1f'

#define ELOG_MAX_LOG_MSG_LENGTH         512UL
#define ELOG_MAX_FUNC_NAME_LENGTH       ((int)128)
#define ELOG_MAX_FILE_NAME_LENGTH       ((int)64)


#define ELOG_MIN(a,b)   (((a) < (b)) ? (a) : (b))



static int _ethanlog_pipefd = -1;


static void ethanlog_sendmsg(const char *msg, size_t len)
{
    if (__builtin_expect((_ethanlog_pipefd < 0),0)) {
        const char* env;
        
        /* the fd will be set to -2 if we've tried this already */
        if (_ethanlog_pipefd == -2)
            return;
            
        /* the following environment variable is set by the hypervisor, it
         * tells us the number of open file descriptor to use for logging.
         */
        env = getenv("ETHAN_LOGGING_PIPE");
        if (env == NULL) {
            _ethanlog_pipefd = -2;
            return;
        }
        
        _ethanlog_pipefd = atoi(env);
        if ((_ethanlog_pipefd < 3) || (_ethanlog_pipefd > 2048)) {
            _ethanlog_pipefd = -2;
            return;
        }
    }
    
    /* do we care if write fails ? what shall we do ? */
    write(_ethanlog_pipefd, msg, len);
}


void vethanlog(int level, const char *filename, const char *function,
               int line, const char *format, va_list ap)
{
    struct timespec ts;
    char buf[ELOG_MAX_LOG_MSG_LENGTH];
    char *p, *end;
    char *basename;
    int len;
    
    /* run the sanity checks first */
    if ((level < ETHAN_LOG_FATAL) || (level > ETHAN_LOG_MILESTONE))
        return;
    
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
    ethanlog_sendmsg(buf, (p - buf));
}



void ethanlog(int level, const char *filename, const char *function, int line,
              const char *format, ...)
{
    va_list vl;
    
    va_start(vl, format);
    vethanlog(level, filename, function, line, format, vl);
    va_end(vl);
}




