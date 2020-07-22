/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2014 Sky UK
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
 * File:   IDispatcher.h
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 26 June 2014
 *
 */
#ifndef IDISPATCHER_H
#define	IDISPATCHER_H

#include "Polymorphic.h"
#include <functional>
#include <thread>
#include <memory>

namespace AICommon
{

/**
 * @brief A dispatcher interface
 */
class IDispatcher : public Polymorphic
{
public:
    /**
     * Post an item of work to be executed.
     */
    virtual void post(std::function<void ()> work) = 0;

    /**
     * @brief Ensures that anything that was in the queue before the call has been
     * executed before returning.
     */
    virtual void sync() = 0;

    /**
     * @brief If it is a threaded dispatcher then the dispatcher thread id, otherwise nullptr.
     */
    virtual bool invokedFromDispatcherThread() = 0;
};

} //AICommon

#endif	/* IDISPATCHER_H */

