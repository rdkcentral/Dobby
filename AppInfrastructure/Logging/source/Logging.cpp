/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2019 Sky UK
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
//
//  Logging.cpp
//  AppInfrastructure
//
//
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE
#endif

#include "Logging.h"

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <climits>
#include <cstdarg>
#include <cctype>
#include <algorithm>
#include <unistd.h>
#include <sys/uio.h>



/* by default print all fatals, errors, warnings & milestones */
int __ai_debug_log_level = AI_DEBUG_LEVEL_MILESTONE;


static void _ai_default_diag_printer(int level, const char *file, const char *func,
                                     int line, const char *message);

static AICommon::diag_printer_t __ai_diag_printer = &_ai_default_diag_printer;


/** ----------------------------------------------------------------------- **/
/**
 *  _ai_default_diag_printer - default log printer if none installed
 *  @level:
 *  @msg:
 *  @msglen:
 *
 *
 */
void _ai_default_diag_printer(int level, const char *file, const char *func,
                              int line, const char *message)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    struct iovec iov[5];
    char tbuf[32];

    iov[0].iov_base = tbuf;
    iov[0].iov_len = snprintf(tbuf, sizeof(tbuf), "%.010lu.%.06lu ",
                              ts.tv_sec, ts.tv_nsec / 1000);
    iov[0].iov_len = std::min(iov[0].iov_len, sizeof(tbuf));

    switch (level) {
        case AI_DEBUG_LEVEL_FATAL:
            iov[1].iov_base = (void*)"FTL: ";
            iov[1].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_ERROR:
            iov[1].iov_base = (void*)"ERR: ";
            iov[1].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_WARNING:
            iov[1].iov_base = (void*)"WRN: ";
            iov[1].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_MILESTONE:
        case AI_DEBUG_LEVEL_PROD_MILESTONE:
            iov[1].iov_base = (void*)"MIL: ";
            iov[1].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_INFO:
            iov[1].iov_base = (void*)"NFO: ";
            iov[1].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_DEBUG:
            iov[1].iov_base = (void*)"DBG: ";
            iov[1].iov_len = 5;
            break;
        default:
            iov[1].iov_base = (void*)": ";
            iov[1].iov_len = 2;
            break;
    }

    char fbuf[160];
    iov[2].iov_base = (void*)fbuf;
    if (!file || !func || (line <= 0))
        iov[2].iov_len = snprintf(fbuf, sizeof(fbuf), "< M:? F:? L:? > ");
    else
        iov[2].iov_len = snprintf(fbuf, sizeof(fbuf), "< M:%.*s F:%.*s L:%d > ",
                                  64, file, 64, func, line);
    iov[2].iov_len = std::min(iov[2].iov_len, sizeof(fbuf));

    iov[3].iov_base = (void*)message;
    iov[3].iov_len = strlen(message);

    iov[4].iov_base = (void*)"\n";
    iov[4].iov_len = 1;


    writev(STDERR_FILENO, iov, 5);
}


/** ----------------------------------------------------------------------- **/
/**
 *  _debug_log_printf - prints a debugging log message at the given level
 *  @level: the level to message is for, should be one of AI_DEBUG_LEVEL_*
 *  @fmt: printf style format string
 *  @...: printf style arguments
 *
 *  Prints the message to an output channel if any is enabled.
 *
 *  If the level is one of the predefined AI_DEBUG_LEVEL_* constants then the
 *  message is prefixed with a string describing the level, i.e. "error: ",
 *  "info: ", etc
 *
 */
static void _ai_debug_log_vprintf(int level, const char *file, const char *func,
                                  int line, const char *fmt, va_list ap,
                                  const char *append)
{
    if (__builtin_expect((level > __ai_debug_log_level), 0))
        return;

    char mbuf[256];
    int len;

    len = vsnprintf(mbuf, sizeof(mbuf), fmt, ap);
    if (__builtin_expect((len < 1), 0))
        return;
    if (__builtin_expect((len > (int)(sizeof(mbuf) - 1)), 0))
        len = sizeof(mbuf) - 1;
    if (__builtin_expect((mbuf[len - 1] == '\n'), 0))
        len--;
    mbuf[len] = '\0';


    if (append && (len < (int)(sizeof(mbuf) - 1))) {
        size_t extra = std::min<size_t>(strlen(append), (sizeof(mbuf) - len - 1));
        memcpy(mbuf + len, append, extra);
        len += extra;
        mbuf[len] = '\0';
    }


    const char *fname = nullptr;
    if (file) {
        if ((fname = strrchr(file, '/')) == nullptr)
            fname = file;
        else
            fname++;
    }

    if (__ai_diag_printer) {
        __ai_diag_printer(level, fname, func, line, mbuf);
    }

}


extern "C" void __ai_debug_log_printf(int level, const char *file,
                                      const char *func, int line,
                                      const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    _ai_debug_log_vprintf(level, file, func, line, fmt, ap, nullptr);
    va_end(ap);
}

extern "C" void __ai_debug_log_sys_printf(int err, int level, const char *file,
                                          const char *func, int line,
                                          const char *fmt, ...)
{
    va_list ap;
    char errbuf[64];
    const char *errmsg;
    char appendbuf[96];
    const char *append = nullptr;

#if defined(__linux__)
    errmsg = strerror_r(err, errbuf, sizeof(errbuf));
#elif defined(__APPLE__)
    if (strerror_r(err, errbuf, sizeof(errbuf)) != 0)
        errmsg = "Unknown error";
    else
        errmsg = errbuf;
#endif

    if (errmsg) {
        snprintf(appendbuf, sizeof(appendbuf), " (%d - %s)", err, errmsg);
        appendbuf[sizeof(appendbuf) - 1] = '\0';
        append = appendbuf;
    }

    va_start(ap, fmt);
    _ai_debug_log_vprintf(level, file, func, line, fmt, ap, append);
    va_end(ap);
}



void AICommon::initLogging(diag_printer_t diagPrinter)
{
    __ai_diag_printer = diagPrinter;
}

void AICommon::termLogging()
{

}



