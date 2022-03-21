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
 * File: RefCountFile.cpp
 *
 */
#include "RefCountFile.h"
#include <stdio.h>
#include <unistd.h>

RefCountFile::RefCountFile(std::string file): mFilePath(std::move(file)), mFd(0), mOpen(false)
{
    AI_LOG_FN_ENTRY();

    mFd = open(mFilePath.c_str(),
               O_CREAT|O_CLOEXEC|O_RDWR,
               S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if (mFd < 0)
    {
        AI_LOG_ERROR("failed to open reference count file '%s'",
                     mFilePath.c_str());
    }
    else
    {
         mOpen = true;
    }

    AI_LOG_FN_EXIT();
}

RefCountFile::~RefCountFile()
{
    AI_LOG_FN_ENTRY();

    Close();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Close the file descriptor if it's not already closed.
 *
 */
void RefCountFile::Close()
{
    AI_LOG_FN_ENTRY();

    if (mFd)
    {
        close(mFd);
        mFd = 0;
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Increment the reference count in the file.
 *
 */
int RefCountFile::Increment()
{
    AI_LOG_FN_ENTRY();

    int ref = Read();
    ref = Write(++ref);
    AI_LOG_DEBUG("ref count: %d", ref);

    return ref;
    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Decrement the reference count in the file.
 *
 */
int RefCountFile::Decrement()
{
    AI_LOG_FN_ENTRY();

    int ref = Read();
    if (ref > 0) {
        ref = Write(--ref);
    }
    AI_LOG_DEBUG("ref count: %d", ref);
    
    // If reference count is 0, delete the file
    if (ref == 0)
    {
        AI_LOG_DEBUG("deleting ref count file %s", mFilePath.c_str());
        unlink(mFilePath.c_str());
    }

    return ref;
    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Read the reference count file.
 *
 *  @return reference count
 */
int RefCountFile::Read()
{
    AI_LOG_FN_ENTRY();
    int ref(0);

    lseek(mFd, 0, SEEK_SET);
    read(mFd, &ref, sizeof(ref));

    return ref;
    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Write the reference count to file.
 *
 *  @return reference count
 */
int RefCountFile::Write(int ref)
{
    AI_LOG_FN_ENTRY();

    lseek(mFd, 0, SEEK_SET);
    write(mFd, &ref, sizeof(ref));

    return ref;
    AI_LOG_FN_EXIT();
}