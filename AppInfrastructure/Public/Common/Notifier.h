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
 * File:   Notifier.h
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 26 June 2014
 *
 */
#ifndef NOTIFIER_H
#define	NOTIFIER_H

#include "Polymorphic.h"
#include "IDispatcher.h"
#include <memory>
#include <deque>
#include <functional>
#include <mutex>
#include <algorithm>
#include <utility>
#include <vector>
#include <thread>
#include <condition_variable>
#include <stdexcept>

namespace AICommon
{

/**
 * @brief A template of observable objects that send notifications defined in interface T.
 *
 * @note Inherit to use, call notify() to send an update.
 */
template<typename T>
class Notifier : virtual public Polymorphic
{
public:

    Notifier() : notifyingObservers(false), waiteeCount(0) {}

    /**
     * @brief Register interest in receiving updates.
     */
    void addObserver(const std::shared_ptr<T>& observer)
    {
        std::lock_guard<std::mutex> lock(m);
        observers.push_back(observer);
    }

    /**
     * @brief Unregister from updates.
     */
    void removeObserver(const std::shared_ptr<T>& observer)
    {
        std::unique_lock<std::mutex> lock(m);

#if (AI_BUILD_TYPE == AI_DEBUG)
        if (dispatcher && dispatcher->invokedFromDispatcherThread())
        {
            throw std::logic_error("AI notifier: potential deadlock as this method should not be called from the dispatcher call");
        }
#endif
        //find matching observers
        for(size_t i = 0; i < observers.size(); ++i)
        {
            if(observers[i].lock() == observer)
            {
                observers.erase(observers.begin() + i);
                //if addObserver was called 3 times for one object, you need
                //to call remove 3 times too.
                break;
            }
        }

        if (notifyingObservers)
        {
            waiteeCount++;

            do {
                cv.wait(lock);
            } while(notifyingObservers);

            waiteeCount--;
        }
    }

    /**
     * Set dispatcher which will be used for notification callbacks.
     */
    void setDispatcher(const std::shared_ptr<IDispatcher>& dispatcher_)
    {
        std::lock_guard<std::mutex> lock(m);
        dispatcher = dispatcher_;
    }

protected:

    template<typename F, typename... Args>
    void notify(F f, Args&&... args)
    {
        notify_impl(std::bind(f, std::placeholders::_1, std::forward<Args>(args)...));
    }

    template<typename F>
    void notify(F f)
    {
        notify_impl(f);
    }

private:

    void notify_impl(std::function<void (const std::shared_ptr<T>&)> fun)
    {
        std::unique_lock<std::mutex> lock(m);

        if(!dispatcher)
        {
            throw std::logic_error("You must set a dispatcher before you can produce events.");
        }

        //don't want to lock adding new observers while callbacks are executed - will make a copy instead.
        decltype(observers) observersCopy;
        //scrub them for expired listeners. Only copy if (bool)use_count == true. Happens when use_count is positive.
        //(wishing for C++ lambdas here)
        using namespace std;
        using namespace std::placeholders;
        copy_if(observers.begin(), observers.end(), back_inserter(observersCopy), bind(&weak_ptr<T>::use_count, _1));
        //in the unlikely event that there were some expired observers, replace
        //the original observers with the updated copy.
        if(observers.size() != observersCopy.size())
        {
            observers = observersCopy;
        }

        notifyingObservers = true;

        lock.unlock();

		//--------------------------------------------------------------------------------------------
		//---------------------------------------- NOTE ----------------------------------------------
		//We maintain vector of strong pointers pointing to observer objects as otherwise bad things
		//can happen. Lets consider, the observer object point backs to the notifier object itself.
		//That means, there is a circular dependency between the notifier and the observer, but we
		//break that by using a combination of shared and weak pointers. However, imagine, within the
		//notify_impl() method, we gets a shared pointer of observer object out of weak_ptr. After the
		//shared pointer is constructed (bit still in use), now the owner of the observer resets its
		//pointer that is pointing to the observer object. This might result one to one references
		//between the notifier and the observer, i.e., as soon as the observer will be destroyed the
		//notifier will also be destroyed. It means, if now the observer object is destroyed from
		//the notify_imp() method, it will cause the notifier object itself to be destroyed, where
		//the notify_impl can still continue to access its member variable (e.g. dispatcher). This
		//might result an undefined behaviour.
		//--------------------------------------------------------------------------------------------
		//--------------------------------------------------------------------------------------------
        std::vector<std::shared_ptr<T>> observerStrongPtrs;

        for(auto o = observersCopy.cbegin(); o != observersCopy.cend(); ++o)
        {
            std::shared_ptr<T> strong = o->lock();
            if(strong)
            {
                dispatcher->post(std::bind(fun, strong));
            }
            observerStrongPtrs.push_back(std::move(strong));
        }

        lock.lock();
        if (dispatcher && (waiteeCount > 0))
        {
            // We are unregistering an observer so make sure we will not notify unregistered observers
            lock.unlock();
            /* coverity[missing_lock : FALSE] */
            dispatcher->sync();
            lock.lock();
        }

        notifyingObservers = false;

        if (waiteeCount > 0)
        {
            cv.notify_all();
        }
    }

protected:
    std::shared_ptr<IDispatcher> dispatcher;

private:
    std::mutex m;
    std::deque<std::weak_ptr<T>> observers;

    std::condition_variable cv;
    bool notifyingObservers;
    unsigned int waiteeCount;
};

} //AICommon

#endif	/* NOTIFIER_H */

