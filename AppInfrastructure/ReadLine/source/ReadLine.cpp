/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2015 Sky UK
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
 * File:   ReadLine.cpp
 * Author:
 *
 */
#include "ReadLine.h"

#include <Logging.h>

#include <sstream>
#include <iterator>

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <dlfcn.h>
#include <signal.h>
#include <glob.h>


#ifndef STDIN_FILENO
#  define STDIN_FILENO 0
#endif

// The singleton
std::mutex ReadLine::mInstanceLock;
std::shared_ptr<ReadLine> ReadLine::mInstance;

// The libreadline function pointers, extracted by dlsym
rl_crlf_t ReadLine::_rl_crlf = nullptr;
rl_on_new_line_t ReadLine::_rl_on_new_line = nullptr;
rl_forced_update_display_t ReadLine::_rl_forced_update_display = nullptr;
rl_completion_matches_t ReadLine::_rl_completion_matches = nullptr;
rl_bind_key_t ReadLine::_rl_bind_key = nullptr;
rl_callback_handler_install_t ReadLine::_rl_callback_handler_install = nullptr;
rl_callback_read_char_t ReadLine::_rl_callback_read_char = nullptr;
rl_callback_handler_remove_t ReadLine::_rl_callback_handler_remove = nullptr;
add_history_t ReadLine::_add_history = nullptr;



// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
std::shared_ptr<IReadLine> IReadLine::create()
{
    return ReadLine::instance();
}

// -----------------------------------------------------------------------------
/**
 * @brief 
 *
 *
 *
 */
std::shared_ptr<ReadLine> ReadLine::instance()
{
    std::lock_guard<std::mutex> locker(mInstanceLock);

    if (!mInstance)
    {
        mInstance = std::shared_ptr<ReadLine>(new ReadLine());
    }

    return mInstance;
}


// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
ReadLine::ReadLine()
    : mPollLoop(std::make_shared<AICommon::PollLoop>("JumperClient"))
    , mLibHandle(nullptr)
    , mQuit(false)
{
    AI_LOG_FN_ENTRY();

    // Initialise the interface to libreadline
    initLib();

    // Create a quit command
    addCommand("quit",
               std::bind(&ReadLine::quitCommand, this, std::placeholders::_1, std::placeholders::_2),
               "quit",
               "Quit this interactive terminal.\n",
               "");

    // Create a default help command
    addCommand("help",
               std::bind(&ReadLine::helpCommand, this, std::placeholders::_1, std::placeholders::_2),
               "help [command]",
               "Get general help or help on a specific command\n",
               "command        The command you wish get help for\n");

    AI_LOG_FN_EXIT();
}

ReadLine::~ReadLine()
{
    AI_LOG_FN_ENTRY();

    mPollLoop->stop();

    if (mLibHandle)
    {
        dlclose(mLibHandle);
        mLibHandle = nullptr;
    }
    
    AI_LOG_FN_EXIT();
}


// -----------------------------------------------------------------------------
/**
 * @brief Tests if the class is valid
 *
 *
 *
 */
bool ReadLine::isValid() const
{
    return mPollLoop && mLibHandle;
}


// -----------------------------------------------------------------------------
/**
 * @brief Initialises access to the readline library
 *
 * Because we don't have access to the readline headers of library in the SI
 * build system, we use dlsym to try and get the symbols.
 *
 */
void ReadLine::initLib()
{
    AI_LOG_FN_ENTRY();

    (void)dlerror();

    // try and find the readline library in the standard paths
    glob_t buf;
    glob("/lib/libreadline.so*", 0, nullptr, &buf);
    glob("/usr/lib/libreadline.so*", GLOB_APPEND, nullptr, &buf);

    for (size_t i = 0; i < buf.gl_pathc; i++)
    {
        const char *path = buf.gl_pathv[i];
        if (path && (path[0] != '\0'))
        {
            mLibHandle = dlopen(path, RTLD_NOW);
            if (mLibHandle)
                break;
        }
    }

    globfree(&buf);

    if (!mLibHandle)
    {
        AI_LOG_ERROR("failed to find / open readline library (%s)", dlerror());
        goto failed;
    }

    #define GET_RL_FUNC(f) \
        do { \
            _ ## f = reinterpret_cast<f ## _t>(dlsym(mLibHandle, "" #f "")); \
            if (! _ ## f) { \
                AI_LOG_ERROR("failed to get symbol '" #f "' (%s)", dlerror()); \
                goto failed; \
            } \
        } while(0)

    GET_RL_FUNC(rl_crlf);
    GET_RL_FUNC(rl_on_new_line);
    GET_RL_FUNC(rl_forced_update_display);
    GET_RL_FUNC(rl_completion_matches);
    GET_RL_FUNC(rl_bind_key);
    GET_RL_FUNC(rl_callback_handler_install);
    GET_RL_FUNC(rl_callback_read_char);
    GET_RL_FUNC(rl_callback_handler_remove);

    GET_RL_FUNC(add_history);

    #undef GET_RL_FUNC


    {
        void** rl_attempted_completion_function = reinterpret_cast<void**>(
                        dlsym(mLibHandle, "rl_attempted_completion_function"));
        if (rl_attempted_completion_function)
        {
            *rl_attempted_completion_function = reinterpret_cast<void*>(_completionCallback);
        }

        rl_command_func_t* rl_complete = reinterpret_cast<rl_command_func_t*>(
                        dlsym(mLibHandle, "rl_complete"));
        if (rl_complete)
        {
            _rl_bind_key('\t', rl_complete);
        }
    }


    AI_LOG_FN_EXIT();
    return;

failed:
    if (mLibHandle)
    {
        dlclose(mLibHandle);
        mLibHandle = nullptr;
    }

    AI_LOG_FN_EXIT();
}




// -----------------------------------------------------------------------------
/**
 * @brief Completion function for libreadline
 *
 * Generator function for command completion.  STATE lets us know whether
 * to start from scratch; without any state (i.e. STATE == 0), then we
 * start at the top of the list.
 *
 */
char* ReadLine::commandGenerator(const char *text, int state)
{
    static size_t len = 0, index = 0;

    std::lock_guard<std::mutex> locker(mLock);

    // If this is a new word to complete, initialize now.  This includes
    // saving the length of TEXT for efficiency, and initializing the index
    // variable to 0.
    if (!state)
    {
        len = strlen(text);
        index = 0;
    }

    // Return the next name which partially matches from the command list.
    char * cmd = nullptr;
    for (; index < mCommands.size(); index++)
    {
        if (mCommands[index].name.compare(0, len, text) == 0)
        {
            cmd = strdup(mCommands[index].name.c_str());
            index++;
            break;
        }
    }

    // If no names matched, then return NULL.
    return cmd;
}


// -----------------------------------------------------------------------------
/**
 * @brief Callback handler for the tab completion callback
 *
 *
 *
 */
char* ReadLine::_commandGenerator(const char *text, int state)
{
    std::lock_guard<std::mutex> locker(mInstanceLock);
    return mInstance->commandGenerator(text, state);

}


// -----------------------------------------------------------------------------
/**
 * @brief Callback handler for the tab completion callback
 *
 *
 *
 */
char** ReadLine::_completionCallback(const char *text, int start, int end)
{
    (void)end;

    char **matches = nullptr;

    // If this word is at the start of the line, then it is a command to complete.
    if (start == 0)
        matches = _rl_completion_matches(text, _commandGenerator);

    return matches;
}


// -----------------------------------------------------------------------------
/**
 * @brief Callback handler from the readline library
 *
 *
 *
 */
void ReadLine::commandLineHandler(const char *line)
{
    std::lock_guard<std::mutex> locker(mLock);

    if (!line)
    {
        quit();
        AI_LOG_FN_EXIT();
        return;
    }

    std::string _line(line);

    // trim leading spaces
    size_t startpos = _line.find_first_not_of(" \t");
    if (startpos != std::string::npos)
        _line = _line.substr(startpos);

    // trim trailing spaces
    size_t endpos = _line.find_last_not_of(" \t");
    if (endpos != std::string::npos)
        _line = _line.substr(0, endpos + 1);


    if (!_line.empty())
    {
        // construct a stream from the string
        std::stringstream strstr(_line);

        // use stream iterators to copy the stream to the vector as whitespace
        // separated strings
        std::istream_iterator<std::string> it(strstr);
        std::istream_iterator<std::string> end;
        std::vector<std::string> args(it, end);

        if (!args.empty())
        {
            std::string cmdStr = args[0];
            args.erase(args.begin());
            commandExecute(cmdStr, args);
        }

        // add the command to the history
        _add_history(_line.c_str());
    }
}


// -----------------------------------------------------------------------------
/**
 * @brief Callback handler from the readline library
 *
 *
 *
 */
void ReadLine::_commandLineHandler(char *line)
{
    std::lock_guard<std::mutex> locker(mInstanceLock);
    mInstance->commandLineHandler(line);
}


// -----------------------------------------------------------------------------
/**
 * @brief Executes the given command, called from readline callback handler
 *
 *
 *
 */
void ReadLine::commandExecute(const std::string& cmdStr,
                              const std::vector<std::string>& args)
{
    std::string errStr;

    std::vector<ReadLine::ReadLineCommand>::const_iterator cmdRef = mCommands.end();
    std::vector<ReadLine::ReadLineCommand>::const_iterator it = mCommands.begin();
    for (; it != mCommands.end(); ++it)
    {
        if (it->name.compare(0, cmdStr.length(), cmdStr) == 0)
        {
            // exact matches always work
            if (it->name.length() == cmdStr.length())
            {
                cmdRef = it;
                break;
            }

            // check if we don't already have a match, if we do then we have
            // multiple matches and have to report an error.
            if (cmdRef == mCommands.end())
            {
                cmdRef = it;
            }
            else
            {
                errStr += it->name + " ";
            }
        }
    }

    //
    if (cmdRef == mCommands.end())
    {
        fprintf(stderr, "%s: No such command.\n", cmdStr.c_str());
    }
    else if (!errStr.empty())
    {
        fprintf(stderr, "Ambiguous command '%s', possible commands: %s %s\n",
                cmdStr.c_str(), cmdRef->name.c_str(), errStr.c_str());
    }
    else if (cmdRef->handler != nullptr)
    {
        // Call the command handler, if it returns false we stop the poll loop
        // which effectively quits the app
        cmdRef->handler(shared_from_this(), args);
    }
}


// -----------------------------------------------------------------------------
/**
 * @brief Callback from the PollLoop that is listening on stdin
 *
 *
 *
 */
void ReadLine::process(const std::shared_ptr<AICommon::IPollLoop>& pollLoop, epoll_event event)
{
    if (event.events & EPOLLIN)
    {
        _rl_callback_read_char();
    }
}


// -----------------------------------------------------------------------------
/**
 * @brief Signal handler for capturing ctrl-c and restoring the terminal state
 *
 *
 *
 */
void ReadLine::signalHandler(int)
{
    std::lock_guard<std::mutex> locker(mInstanceLock);
    mInstance->quit();
}


// -----------------------------------------------------------------------------
/**
 * @brief Run loop, blocks until quit is called
 *
 *
 *
 */
void ReadLine::run()
{
    AI_LOG_FN_ENTRY();

    // reset the quit var
    std::unique_lock<std::mutex> quitLocker(mQuitLock);
    mQuit = false;

    // install sighandler so can do proper cleanup for libreadline
    signal(SIGINT, signalHandler);

    // setup readline
    _rl_callback_handler_install("> ", _commandLineHandler);

    // add ourselves as a source trigger from input from stdin
    if (!mPollLoop->addSource(shared_from_this(), STDIN_FILENO, EPOLLIN))
    {
        AI_LOG_ERROR_EXIT("failed to add stdin source to poll loop");
        return;
    }

    // run the poll loop
    mPollLoop->start();

    // block until quit is called
    while (!mQuit)
    {
        mQuitConditional.wait(quitLocker);
    }

    // stop the poll loop
    mPollLoop->delSource(shared_from_this());
    mPollLoop->stop();

    // uninstall the handler
    _rl_callback_handler_remove();

    AI_LOG_FN_ENTRY();
}

// -----------------------------------------------------------------------------
/**
 * @brief Returns the print context.
 *
 *
 *
 */
std::shared_ptr<const IReadLineContext> ReadLine::getContext() const
{
    return std::dynamic_pointer_cast<const IReadLineContext>(shared_from_this());
}

// -----------------------------------------------------------------------------
/**
 * @brief Triggers an exit from the readline loop
 *
 *
 *
 */
void ReadLine::quit() const
{
    AI_LOG_FN_ENTRY();

    std::unique_lock<std::mutex> quitLocker(mQuitLock);

    mQuit = true;
    mQuitConditional.notify_all();

    AI_LOG_FN_ENTRY();
}

// -----------------------------------------------------------------------------
/**
 * @brief Prints out the message from the command handler
 *
 *
 *
 */
void ReadLine::printLn(const char *fmt, ...) const
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");

    _rl_on_new_line();
}

// -----------------------------------------------------------------------------
/**
 * @brief Prints out an error message from the command handler
 *
 *
 *
 */
void ReadLine::printLnError(const char *fmt, ...) const
{
    va_list args;

    printf("error - ");
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");

    _rl_on_new_line();
}



// -----------------------------------------------------------------------------
/**
 * @brief
 *
 *
 *
 */
void ReadLine::runCommand(int argc, char* const *argv)
{
    std::vector<std::string> args;
    for (int i = 0; i < argc; i++)
    {
        args.emplace_back(argv[i]);
    }

    if (!args.empty())
    {
        std::lock_guard<std::mutex> locker(mLock);

        std::string cmdStr = args[0];
        args.erase(args.begin());
        commandExecute(cmdStr, args);
    }
}


// -----------------------------------------------------------------------------
/**
 * @brief Adds a new command to the readline loop
 *
 *
 *
 */
bool ReadLine::addCommand(const std::string& name, CommandHandler handler,
                          const std::string& desc, const std::string& help,
                          const std::string& opts)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    ReadLineCommand cmd = { name, std::move(handler), desc, help, opts };
    mCommands.push_back(std::move(cmd));

    AI_LOG_FN_ENTRY();
    return true;
}


// -----------------------------------------------------------------------------
/**
 * @brief Called when the user types quit
 *
 *
 *
 */
void ReadLine::quitCommand(const std::shared_ptr<const IReadLineContext>& readLine,
                           const std::vector<std::string>& args)
{
    // Ignore all args, just return false which will stop the loop
    readLine->quit();
}


// -----------------------------------------------------------------------------
/**
 * @brief Called when the user types help
 *
 *
 *
 */
void ReadLine::helpCommand(const std::shared_ptr<const IReadLineContext>& readLine,
                           const std::vector<std::string>& args)
{
    // Nb: the mLock is already held because we're in the body of a processing
    // callback (i.e. ReadLine::commandExecute)

    std::vector<ReadLine::ReadLineCommand>::const_iterator it = mCommands.begin();
    if (args.empty())
    {
        for (; it != mCommands.end(); ++it)
        {
            readLine->printLn("%-16s  %s", it->name.c_str(), it->desc.c_str());
        }
    }
    else
    {
        for (; it != mCommands.end(); ++it)
        {
            if (it->name == args[0])
            {
                readLine->printLn("%-16s  %s\n", it->name.c_str(), it->desc.c_str());
                if (!it->help.empty())
                    readLine->printLn("%s\n", it->help.c_str());
                if (!it->opts.empty())
                    readLine->printLn("%s\n", it->opts.c_str());
                break;
            }
        }
    }
}
