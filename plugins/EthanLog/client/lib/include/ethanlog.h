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

#ifndef ETHANLOG_H
#define ETHANLOG_H

#include <stdarg.h>

#define ETHAN_LOG_FATAL     (1)
#define ETHAN_LOG_ERROR     (2)
#define ETHAN_LOG_WARNING   (3)
#define ETHAN_LOG_INFO      (4)
#define ETHAN_LOG_DEBUG     (5)
#define ETHAN_LOG_MILESTONE (6)


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @fn ethanlog, vethanlog
 * @brief Send messages to the Ethan / Fusion logger
 * @param level The log level to send the message at
 * @param filename The name of the file associated with the message
 * @param function The name of the function associated with the message
 * @param line The line number associated with the message
 * @param format printf style format string for the message
 *
 * This function generates a message that is sent back to APP_Process for adding
 * to the system DIAG log.
 *
 * These functions will only work when called from inside a container, as they
 * require a pre-created pipe with which to send the messages across.  The
 * file descriptor number of the pipe is automatically set in an environment
 * variable called ETHAN_LOGGING_PIPE.
 *
 */
void ethanlog(int level, const char *filename, const char *function, int line,
              const char *format, ...) __attribute__ ((format (printf, 5, 6)));

void vethanlog(int level, const char *filename, const char *function, int line,
               const char *format, va_list ap) __attribute__ ((format (printf, 5, 0)));

/**
 * @fn ethanlog_vprint
 * @brief Updated version of vethanlog that returns the number of bytes written.
 * @param level The log level to send the message at
 * @param filename The name of the file associated with the message
 * @param function The name of the function associated with the message
 * @param line The line number associated with the message
 * @param text The constant string to send
 *
 * This is identical to vethanlog, except that it returns the number of bytes
 * written to the logging pipe.
 *
 * On success returns the number of bytes written to the logging pipe, this
 * includes the meta data (level, filename, etc), framing bytes as well as
 * text itself.  On failure returns -1 and sets errno to indicate the error.
 *
 * If the pipe is full, EAGAIN is returned, in which case the caller should
 * retry later.
 */
int ethanlog_vprint(int level, const char *filename, const char *function, int line,
                    const char *format, va_list ap) __attribute__ ((format (printf, 5, 0)));

#ifdef __cplusplus
}
#endif

#endif /* !defined(ETHANLOG_H) */
