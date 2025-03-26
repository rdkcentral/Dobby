/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2024 Sky UK
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
 * File: AnroidHelper.h
 *
 */
#ifndef ANDROID_HELPER_H
#define ANDROID_HELPER_H

#include <Logging.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>
#include <memory>
#include <map>
#include <mutex>
#include <list>

//#define ENABLE_TESTS 1

/**
 *  @brief Help functions for Storage related things
 *
 *  All low level help functions that doesn't rely on current state
 */
class AndroidHelper
{
public:
    static int loopDeviceAssociate(int fileFd, std::string* loopDevPath);
    static int openLoopDevice(std::string* loopDevice);
    static bool attachFileToLoopDevice(int loopFd, int fileFd);
    static int attachLoopDevice(const std::string& sourceFile,
                                std::string* loopDevice);
    static int getMountOptions(const std::list<std::string> &mountOptions);

    // -------------------------------------------------------------------------
    /**
     *  @brief Removes a directory and all it's contents.
     *
     *  This is equivalent to the 'rm -rf <dir>' command.
     *
     *  If the pathname given in pathname is relative, then it is interpreted
     *  relative to the directory referred to by the file descriptor dirFd, if
     *  dirFd is not supplied then it's relative to the cwd.
     *
     *  @warning This function only supports deleting directories with contents
     *  that are less than 128 levels deep, this is to avoid running out of
     *  file descriptors.
     *
     *  @param[in]  dirFd           If specified the path should be relative to
     *                              to this directory.
     *  @param[in]  path            The path to the directory to create.
     *
     *  @return true on success, false on failure.
     */
    static bool rmdirRecursive(int dirFd, const std::string& path);

    // -------------------------------------------------------------------------
    /**
     *  @brief Removes the contents of a directory but leave the actual
     *  directory in place.
     *
     *  This is equivalent to the 'rm -rf <dir>/ *' command.
     *
     *  If the pathname given in pathname is relative, then it is interpreted
     *  relative to the directory referred to by the file descriptor dirFd, if
     *  dirFd is not supplied then it's relative to the cwd.
     *
     *  @warning This function only supports deleting directories with contents
     *  that are less than 128 levels deep, this is to avoid running out of
     *  file descriptors.
     *
     *  @param[in]  dirFd           If specified the path should be relative to
     *                              to this directory.
     *  @param[in]  path            The path to the directory to create.
     *
     *  @return true on success, false on failure.
     */
    static bool rmdirContents(int dirFd, const std::string& path);
    static bool deleteRecursive(int dirfd, int depth);

    // Tests
#ifdef ENABLE_TESTS
    static bool Test_mkdirRecursive(const std::string& rootfsPath);
    static bool Test_openLoopDevice();
    static bool Test_attachLoopDevice(std::string& imagePath);
    static bool Test_cleanMountLostAndFound(const std::string& rootfsPath);
    static bool Test_checkWriteReadMount(const std::string& tmpPath);
#endif // ENABLE_TESTS

};

#endif // !defined(ANDROID_HELPER_H)
