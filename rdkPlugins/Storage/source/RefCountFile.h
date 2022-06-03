/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2022 Sky UK
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
 * File: RefCountFile.h
 *
 */
#ifndef REFCOUNTFILE_H
#define REFCOUNTFILE_H

#include <Logging.h>
#include <sys/file.h>
#include <string>

// -----------------------------------------------------------------------------
/**
 *  @class RefCountFile
 *  @brief Class that represents a reference count file
 *
 *  This class is only intended to be used internally by Storage plugin
 *  do not use from external code.
 *
 *  @see Storage
 */
class RefCountFile
{
public:
    RefCountFile(std::string file);
    ~RefCountFile();

    bool IsOpen() const
    {
        AI_LOG_FN_ENTRY();

        AI_LOG_FN_EXIT();
        return mOpen;
    }

    const std::string& GetFilePath() const
    {
        AI_LOG_FN_ENTRY();
        
        AI_LOG_FN_EXIT();
        return mFilePath;
    }
    
    inline void Lock()
    {
        AI_LOG_FN_ENTRY();

        flock(mFd, LOCK_EX);  // lock file exclusively

        AI_LOG_FN_EXIT();
    }

    inline void Unlock()
    {
        AI_LOG_FN_ENTRY();

        flock(mFd, LOCK_UN);  // unlock file

        AI_LOG_FN_EXIT();
    }

    void Reset();
    int Increment();
    int Decrement();

private:
    int Read();
    void Close();
    int Write(int ref);

private:
    std::string mFilePath;
    int mFd;
    bool mOpen;
};


#endif //REFCOUNTFILE_H
