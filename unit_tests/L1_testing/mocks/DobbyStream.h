/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Synamedia
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
    int dupWriteFD(int newFd, bool closeExec) const
    {
        return -1;
    }
};


class DobbyBufferStreamImpl
{
public:
    virtual ~DobbyBufferStreamImpl() = default;
    virtual std::vector<char> getBuffer() const = 0;
    virtual int getMemFd() const = 0;
};
// -----------------------------------------------------------------------------
class DobbyBufferStream : public IDobbyStream
{
protected:
    static DobbyBufferStreamImpl *impl;
public:
    DobbyBufferStream(ssize_t limit = -1);
    ~DobbyBufferStream();

public:
    int dupWriteFD(int newFd, bool closeExec) const;
public:
    static void setImpl(DobbyBufferStreamImpl* newImpl);
    static DobbyBufferStream* getInstance();
    static std::vector<char> getBuffer();
    static int getMemFd();

};

#endif // !defined(DOBBYSTREAM_H)
