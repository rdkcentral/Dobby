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
 * File: RefCountFileLock.h
 *
 */
#ifndef REFCOUNTFILELOCK_H
#define REFCOUNTFILELOCK_H

#include "RefCountFile.h"
#include <Logging.h>
#include <memory>

// -----------------------------------------------------------------------------
/**
 *  @class RefCountFileLock
 *  @brief Class that represents a lock on a reference count file
 *
 *  This class is only intended to be used internally by Storage plugin
 *  do not use from external code.
 *
 *  @see Storage
 */
class RefCountFileLock
{
public:
    RefCountFileLock() = delete;
    RefCountFileLock(RefCountFileLock&) = delete;
    RefCountFileLock(RefCountFileLock&&) = delete;

    // Lock the file on construction
    RefCountFileLock(std::unique_ptr<RefCountFile> &file): mRefCountFile(file)
    {
        AI_LOG_FN_ENTRY();
        mRefCountFile->Lock();
        AI_LOG_FN_EXIT();
    }

    // Unlock the file on destruction
    ~RefCountFileLock()
    {
        AI_LOG_FN_ENTRY();
        mRefCountFile->Unlock();
        AI_LOG_FN_EXIT();
    }

private:
    std::unique_ptr<RefCountFile> &mRefCountFile;
};


#endif //REFCOUNTFILELOCK_H
