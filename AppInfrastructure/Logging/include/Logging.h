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
//
//  Logging.h
//  AppInfrastructure
//
//  Copyright © 2019 Sky UK. All rights reserved.
//
#ifndef LOGGING_H
#define LOGGING_H


/**
 * One of the following should be set by the SI build system, if they aren't
 * we're in trouble.
 */
#if !defined(AI_BUILD_TYPE) || !defined(AI_RELEASE) || !defined(AI_DEBUG)
#  warning "No build type defined, expected AI_BUILD_TYPE to be defined to either AI_RELEASE or AI_DEBUG"
#endif
#if (AI_BUILD_TYPE != AI_RELEASE) && (AI_BUILD_TYPE != AI_DEBUG)
#  warning "AI_BUILD_TYPE is not equal to AI_RELEASE or AI_DEBUG"
#endif


#ifdef __cplusplus
extern "C" {
#endif



extern int __ai_debug_log_level;
extern void __ai_debug_log_printf(int level, const char *file, const char *func,
                                  int line, const char *fmt, ...)
    __attribute__ ((format (printf, 5, 6)));
extern void __ai_debug_log_sys_printf(int err, int level, const char *file,
                                      const char *func, int line,
                                      const char *fmt, ...)
    __attribute__ ((format (printf, 6, 7)));


#define AI_DEBUG_LEVEL_PROD_MILESTONE  -1
#define AI_DEBUG_LEVEL_FATAL            0
#define AI_DEBUG_LEVEL_ERROR            1
#define AI_DEBUG_LEVEL_WARNING          2
#define AI_DEBUG_LEVEL_MILESTONE        3
#define AI_DEBUG_LEVEL_INFO             4
#define AI_DEBUG_LEVEL_DEBUG            5



/**
 * Debugging macros.
 *
 * Primatives for debugging.
 *
 */
#define __AI_LOG_PRINTF(level, fmt, ...) \
    do {  \
        if (__builtin_expect(((level) <= __ai_debug_log_level),0)) \
            __ai_debug_log_printf((level), __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__); \
    } while(0)

#define __AI_LOG_SYS_PRINTF(err, level, fmt, ...) \
    do {  \
        if (__builtin_expect(((level) <= __ai_debug_log_level),0)) \
            __ai_debug_log_sys_printf((err), (level), __FILE__, __FUNCTION__, __LINE__, fmt, ##__VA_ARGS__); \
    } while(0)


/* In all builds we support production milestone logging */
#define AI_LOG_PROD_MILESTONE(fmt,...) \
    __AI_LOG_PRINTF(AI_DEBUG_LEVEL_PROD_MILESTONE, fmt, ##__VA_ARGS__)


/* In release builds only enable the milestone, warning, error and fatal messages */
#if (AI_BUILD_TYPE == AI_RELEASE)
#   define AI_LOG_FN_ENTRY()
#   define AI_LOG_FN_EXIT()
#   define AI_LOG_DEBUG(fmt,...)
#   define AI_LOG_INFO(fmt,...)
#   define AI_LOG_MILESTONE(fmt,...)
#   define AI_LOG_WARN(fmt,...)
#   define AI_LOG_SYS_WARN(err,fmt,...)
#   define AI_LOG_ERROR(fmt,...)
#   define AI_LOG_SYS_ERROR(err, fmt,...)
#   define AI_LOG_ERROR_EXIT(fmt,...)
#   define AI_LOG_SYS_ERROR_EXIT(err,fmt,...)
#   define AI_LOG_FATAL(fmt,...)
#   define AI_LOG_SYS_FATAL(err,fmt,...)
#   define AI_LOG_FATAL_EXIT(fmt,...)
#   define AI_LOG_SYS_FATAL_EXIT(err,fmt,...)
#   define AI_LOG_EXCEPTION(fmt,...)

/* debug and release_dbg builds can print the following but the log level actually
 * sets what is printed.
 */
#else /* (AI_BUILD_TYPE == AI_RELEASE) */

#   define AI_LOG_FN_ENTRY() \
        __AI_LOG_PRINTF(AI_DEBUG_LEVEL_DEBUG, "entry")
#   define AI_LOG_FN_EXIT() \
        __AI_LOG_PRINTF(AI_DEBUG_LEVEL_DEBUG, "exit")
#   define AI_LOG_DEBUG(fmt,...) \
        __AI_LOG_PRINTF(AI_DEBUG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#   define AI_LOG_INFO(fmt,...) \
        __AI_LOG_PRINTF(AI_DEBUG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#   define AI_LOG_MILESTONE(fmt,...) \
        __AI_LOG_PRINTF(AI_DEBUG_LEVEL_MILESTONE, fmt, ##__VA_ARGS__)
#   define AI_LOG_WARN(fmt,...) \
        __AI_LOG_PRINTF(AI_DEBUG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#   define AI_LOG_SYS_WARN(err,fmt,...) \
        __AI_LOG_SYS_PRINTF(err, AI_DEBUG_LEVEL_WARNING, fmt, ##__VA_ARGS__)
#   define AI_LOG_ERROR(fmt,...) \
        __AI_LOG_PRINTF(AI_DEBUG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#   define AI_LOG_SYS_ERROR(err, fmt,...) \
        __AI_LOG_SYS_PRINTF(err, AI_DEBUG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#   define AI_LOG_ERROR_EXIT(fmt,...) \
        do { \
            AI_LOG_ERROR(fmt, ##__VA_ARGS__); \
            AI_LOG_FN_EXIT(); \
        } while(0)
#   define AI_LOG_SYS_ERROR_EXIT(err,fmt,...) \
        do { \
            AI_LOG_SYS_ERROR(err, fmt, ##__VA_ARGS__); \
            AI_LOG_FN_EXIT(); \
        } while(0)
#   define AI_LOG_FATAL(fmt,...) \
        __AI_LOG_PRINTF(AI_DEBUG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#   define AI_LOG_SYS_FATAL(err,fmt,...) \
        __AI_LOG_SYS_PRINTF(err, AI_DEBUG_LEVEL_FATAL, fmt, ##__VA_ARGS__)
#   define AI_LOG_FATAL_EXIT(fmt,...) \
        do { \
            AI_LOG_FATAL(fmt, ##__VA_ARGS__); \
            AI_LOG_FN_EXIT(); \
        } while(0)
#   define AI_LOG_SYS_FATAL_EXIT(err,fmt,...) \
        do { \
            AI_LOG_SYS_FATAL(err, fmt, ##__VA_ARGS__); \
            AI_LOG_FN_EXIT(); \
        } while(0)
#   define AI_LOG_EXCEPTION(fmt,...) \
        __AI_LOG_PRINTF(AI_DEBUG_LEVEL_FATAL, fmt, ##__VA_ARGS__)

#   include <assert.h>
#   define AI_DEBUG_ASSERT(condition) \
        do { \
            if (! (condition) ) \
                __AI_LOG_PRINTF(AI_DEBUG_LEVEL_FATAL, "ASSERT - " #condition ); \
            assert( condition ); \
        } while(0)

#endif /* (AI_BUILD_TYPE != AI_RELEASE) */



#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <functional>


namespace AICommon
{

    typedef std::function<void (int level, const char *file, const char *func, int line, const char *message)> diag_printer_t;

    void initLogging(diag_printer_t diagPrinter = nullptr);
    void termLogging();

} // namespace AICommon

#endif // defined(__cplusplus)



#endif /* LOGGING_H */

