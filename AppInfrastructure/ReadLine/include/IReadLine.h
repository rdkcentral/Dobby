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
 * File:   IReadLine.h
 *
 */
#ifndef IREADLINE_H
#define IREADLINE_H

#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <list>


class IReadLineContext
{
public:
    virtual ~IReadLineContext() = default;

    virtual void quit() const = 0;
    virtual void printLn(const char *fmt, ...) const __attribute__ ((format (printf, 2, 3))) = 0;
    virtual void printLnError(const char *fmt, ...) const __attribute__ ((format (printf, 2, 3))) = 0;
};


class IReadLine
{
public:
    static std::shared_ptr<IReadLine> create();

public:
    virtual ~IReadLine() = default;

public:
    virtual bool isValid() const = 0;
    virtual void run()= 0;

    virtual std::shared_ptr<const IReadLineContext> getContext() const = 0;

    typedef std::function<void (const std::shared_ptr<const IReadLineContext>& readLine,
                                const std::vector<std::string>& args)> CommandHandler;

    virtual bool addCommand(const std::string& name, CommandHandler handler,
                            const std::string& desc, const std::string& help,
                            const std::string& opts) = 0;

    virtual void runCommand(int argc, char* const *argv) = 0;
};


#endif // !defined(IREADLINE_H)
