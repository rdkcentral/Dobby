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
 * File:   IDobbyStartState.h
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef IDOBBYSTARTSTATE_H
#define IDOBBYSTARTSTATE_H

#include <list>
#include <string>

#include <sys/mount.h>

// -----------------------------------------------------------------------------
/**
 *  @class IDobbyStartState
 *  @brief Utility interface passed in at the post-construction phase, to allow
 *  some final tweaking of the container before it's launched.
 *
 *
 */
class IDobbyStartState
{
public:
    virtual ~IDobbyStartState() = default;

public:
    // -------------------------------------------------------------------------
    /**
     *  @brief Adds another file descriptor to be passed into the container
     *
     *  The number of the file descriptor in the container namespace is
     *  returned, unless there was an error in which case a negative value is
     *  returned.  File descriptors start at 3.
     *
     *  The method dups the supplied file descriptor so it can be closed
     *  immmediatly after the call.  The file descriptor will be closed
     *  after the container is started and handed over.
     *
     *  Lastly to help find issues, this function will log an error and reject
     *  the file descriptor if it doesn't have the FD_CLOEXEC bit set.
     *
     *  @param[in]  fd      The file descriptor to pass to the container
     *
     *  @return the number of the file descriptor inside the container on
     *  success, on failure -1
     */
    virtual int addFileDescriptor(int fd) = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Adds an environment variable to the container
     *
     *  Simple appends another environment variable to the container
     *
     *  @param[in]  envVar      The environment variable to set
     *
     *  @return true on success, false on failure
     */
    virtual bool addEnvironmentVariable(const std::string& envVar) = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Adds a new mount to the container
     *
     *  Adds a mount entry to the config.json for the container.
     *
     *  @warning this can't be used to add loopback mounts, only standard /dev
     *  mounts or bind mounts of directories and files.
     *
     *  @param[in]  source          The source of the mount
     *  @param[in]  target          The target mount point
     *  @param[in]  fsType          The filesystem type of the mount
     *  @param[in]  mountFlags      The mount flags (i.e. MS_BIND, MS_??)
     *  @param[in]  mountOptions    Any additional mount options.
     *
     *  @return true on success, false on failure
     */
    virtual bool addMount(const std::string& source,
                          const std::string& target,
                          const std::string& fsType,
                          unsigned long mountFlags = 0,
                          const std::list<std::string>& mountOptions = std::list<std::string>()) = 0;

};


#endif // !defined(IDOBBYSTARTSTATE_H)
