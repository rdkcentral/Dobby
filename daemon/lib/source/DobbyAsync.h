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
 * File:   DobbyAsync.h
 *
 * Copyright (C) Sky UK 2017+
 */
#ifndef DOBBYASYNC_H
#define DOBBYASYNC_H


#include <string>
#include <memory>
#include <functional>


class DobbyAsyncResult;
class IDobbyAsyncResultPrivate;


std::shared_ptr<DobbyAsyncResult> DobbyAsyncImpl(const std::string &name,
                                                 const std::function<bool()>& func);
std::shared_ptr<DobbyAsyncResult> DobbyDeferredImpl(const std::function<bool()>& func);



// -----------------------------------------------------------------------------
/**
 *  @class DobbyAsyncResult
 *  @brief Result object for async and deferred results.
 *
 *  The behaviour of this object is different depending on how it was created;
 *  for DobbyAsync objects, the getResults() method will wait for the function
 *  to complete in a separate thread before returning the results. For
 *  DobbyDeferred objects, the function is executed in the current thread when
 *  the getResults() method is called.
 *
 */
class DobbyAsyncResult
{
private:
    friend std::shared_ptr<DobbyAsyncResult> DobbyAsyncImpl(const std::string &name,
                                                            const std::function<bool()>& func);
    friend std::shared_ptr<DobbyAsyncResult> DobbyDeferredImpl(const std::function<bool()>& func);
    explicit DobbyAsyncResult(IDobbyAsyncResultPrivate *priv);

public:
    ~DobbyAsyncResult();

public:
    bool getResult();

private:
    IDobbyAsyncResultPrivate* const mPrivate;
};




// -----------------------------------------------------------------------------
/**
 *  @fn DobbyAsync
 *  @brief Spawns a thread to execute the given function.
 *
 *  You MUST call the getResult() method on the returned object to join the
 *  thread and clean-up.  A fatal error will be logged if you don't, and it
 *  will likely be followed by a crash.
 *
 *  The thread will be given the @a name.
 *
 */
template< class Function >
static inline std::shared_ptr<DobbyAsyncResult> DobbyAsync(const std::string &name,
                                                           Function func)
{
    return DobbyAsyncImpl(name, func);
}

template< class Function, class... Args >
static inline std::shared_ptr<DobbyAsyncResult> DobbyAsync(const std::string &name,
                                                           Function&& f, Args&&... args)
{
    return DobbyAsyncImpl(name, std::bind(std::forward<Function>(f),
                                          std::forward<Args>(args)...));
}


// -----------------------------------------------------------------------------
/**
 *  @fn DobbyDeferred
 *  @brief Stores the suppied function and executes it when the results are
 *  requested.
 *
 *  You MUST call the getResult() method on the returned object to join the
 *  thread and clean-up.  A fatal error will be logged if you don't.
 *
 *
 */
template< class Function >
static inline std::shared_ptr<DobbyAsyncResult> DobbyDeferred(Function func)
{
    return DobbyDeferredImpl(func);
}

template< class Function, class... Args >
static inline std::shared_ptr<DobbyAsyncResult> DobbyDeferred(Function&& f, Args&&... args)
{
    return DobbyDeferredImpl(std::bind(std::forward<Function>(f),
                                       std::forward<Args>(args)...));
}


#endif // !defined(DOBBYASYNC_H)
