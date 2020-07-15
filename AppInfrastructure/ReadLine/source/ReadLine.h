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
 * File:   ReadLine.h
 *
 * Copyright (C) BSKYB 2015+
 */
#ifndef READLINE_H
#define READLINE_H

#include "IReadLine.h"
#include <PollLoop.h>

#include <condition_variable>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <list>


typedef char *rl_compentry_func_t(const char *, int);
typedef int rl_command_func_t(int, int);
typedef void rl_vcpfunc_t(char *);

typedef int (*rl_crlf_t)();
typedef int (*rl_on_new_line_t)();
typedef int (*rl_forced_update_display_t)();
typedef char** (*rl_completion_matches_t)(const char *, rl_compentry_func_t *);
typedef int (*rl_bind_key_t)(int, rl_command_func_t *);
typedef void (*rl_callback_handler_install_t)(const char *, rl_vcpfunc_t *);
typedef void (*rl_callback_read_char_t)();
typedef void (*rl_callback_handler_remove_t)();

typedef void (*add_history_t)(const char *);



class ReadLine : public AICommon::IPollSource
               , public IReadLine
               , public IReadLineContext
               , public std::enable_shared_from_this<ReadLine>
{
public:
    static std::shared_ptr<ReadLine> instance();
    ~ReadLine() final;

private:
    ReadLine();

public:
    bool isValid() const override ;
    void run() override;
    bool addCommand(const std::string& name, CommandHandler handler,
                    const std::string& desc, const std::string& help,
                    const std::string& opts) override;

public:
    void runCommand(int argc, char* const *argv) override;

public:
    void process(const std::shared_ptr<AICommon::IPollLoop>& pollLoop, uint32_t events) override;

public:
    std::shared_ptr<const IReadLineContext> getContext() const override;

    void quit() const override;
    void printLn(const char *fmt, ...) const override;
    void printLnError(const char *fmt, ...) const override;

private:
    static std::mutex mInstanceLock;
    static std::shared_ptr<ReadLine> mInstance;

private:
    std::mutex mLock;
    std::shared_ptr<AICommon::PollLoop> mPollLoop;

private:
    void initLib();

    void* mLibHandle;

    static rl_crlf_t _rl_crlf;
    static rl_on_new_line_t _rl_on_new_line;
    static rl_forced_update_display_t _rl_forced_update_display;
    static rl_completion_matches_t _rl_completion_matches;
    static rl_bind_key_t _rl_bind_key;
    static rl_callback_handler_install_t _rl_callback_handler_install;
    static rl_callback_read_char_t _rl_callback_read_char;
    static rl_callback_handler_remove_t _rl_callback_handler_remove;
    static add_history_t _add_history;

    static char* _commandGenerator(const char *text, int state);
    static char** _completionCallback(const char *text, int start, int end);
    static void _commandLineHandler(char *text);

    char* commandGenerator(const char *text, int state);
    void commandLineHandler(const char *text);
    void commandExecute(const std::string& cmdStr,
                        const std::vector<std::string>& args);

private:
    void helpCommand(const std::shared_ptr<const IReadLineContext>& context,
                     const std::vector<std::string>& args);
    void quitCommand(const std::shared_ptr<const IReadLineContext>& context,
                     const std::vector<std::string>& args);

private:
    mutable bool mQuit;
    mutable std::mutex mQuitLock;
    mutable std::condition_variable mQuitConditional;

    static void signalHandler(int dummy);

private:
    typedef struct _ReadLineCommand
    {
        const std::string name;
        const CommandHandler handler;
        const std::string desc;
        const std::string help;
        const std::string opts;
    } ReadLineCommand;

    std::vector<ReadLine::ReadLineCommand> mCommands;
};



#endif // !defined(IPOLLLOOP_H)
