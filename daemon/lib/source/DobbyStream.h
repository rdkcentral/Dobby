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
 * File:   DobbyStream.h
 *
 * Copyright (C) Sky UK 2016+
 */
#ifndef DOBBYSTREAM_H
#define DOBBYSTREAM_H

#include <sys/types.h>

#include <string>
#include <thread>
#include <vector>

// -----------------------------------------------------------------------------
/**
 *  @interface IDobbyStream
 *  @brief Interface for all character streams used in the daemon
 *
 *
 *
 */
class IDobbyStream
{
public:
    virtual ~IDobbyStream() = default;

public:
    virtual int dupWriteFD(int newFd = -1, bool closeExec = true) const = 0;
};


// -----------------------------------------------------------------------------
/**
 *  @class DobbyDevNullStream
 *  @brief Stream that just redirects all the input to /dev/null
 *
 *  This simply returns the fd for /dev/null in the dupWriteFD call.
 *
 */
class DobbyDevNullStream : public IDobbyStream
{
public:
    ~DobbyDevNullStream() final = default;

public:
    int dupWriteFD(int newFd, bool closeExec) const override;
};


// -----------------------------------------------------------------------------
/**
 *  @class DobbyBufferStream
 *  @brief Stream that just redirects all the input to an internal memory buffer.
 *
 *  This is useful for capturing the stderr output, or other small bits of
 *  text output from a command line tool.
 *
 *  @note This object is not very efficient and should only be used for small
 *  amounts of text data.
 *
 */
class DobbyBufferStream : public IDobbyStream
{
public:
    explicit DobbyBufferStream(ssize_t limit = -1);
    ~DobbyBufferStream() final;

public:
    int dupWriteFD(int newFd, bool closeExec) const override;

public:
    std::vector<char> getBuffer() const;
    int getMemFd() const;

private:
    int mMemFd;
};

#endif // !defined(DOBBYSTREAM_H)
