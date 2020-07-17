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
 * File:   ObserverTest.cpp
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 26 June 2014
 *
 * Copyright (C) Sky UK 2014
 */

#include <Common/Observer.h>
#include <Common/Notifier.h>
#include <CallerThreadDispatcher.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace std;
using namespace std::placeholders;
using namespace testing;
using namespace AICommon;

//a basic callback interface with two types of callbacks
class StateEvents
{
public:
    virtual void stateChanged(int newState) = 0;
    virtual void nameChanged(std::string newName) = 0;
    virtual void keyAndValueChanged(std::string newKey, std::string newValue) = 0;
    virtual void eventOccured() = 0;
};

//a basic observee which emits events defined in StateEvents
class Observee : public Notifier<StateEvents>
{
public:
    void setState(int state)
    {
        notify(bind(&StateEvents::stateChanged, _1, state));
    }

    void setName(std::string name)
    {
        notify(&StateEvents::nameChanged, name);
    }

    void generateEvent()
    {
        notify(&StateEvents::eventOccured);
    }

    void setKeyAndValue(std::string key, std::string value)
    {
        notify(&StateEvents::keyAndValueChanged, key, value);
    }
};

//an observer expressed as gmock mock so that we can use EXPECT_CALL to express
//callback expectations.
class TestObserver : public Observer<StateEvents>
{
public:
    MOCK_METHOD1(stateChanged, void (int));
    MOCK_METHOD1(nameChanged, void (string));
    MOCK_METHOD2(keyAndValueChanged, void (string,string));
    MOCK_METHOD0(eventOccured, void ());
};

TEST(NotifierTest, testSendNotification)
{
    Observee notifier;
    shared_ptr<TestObserver> observer = make_shared<TestObserver>();

    notifier.setDispatcher(make_shared<CallerThreadDispatcher>());
    notifier.addObserver(observer);

    EXPECT_CALL(*observer, stateChanged(5)).Times(1);

    notifier.setState(5);
}

TEST(NotifierTest, testSendNotificationManyArgs)
{
    Observee notifier;
    shared_ptr<TestObserver> observer = make_shared<TestObserver>();

    notifier.setDispatcher(make_shared<CallerThreadDispatcher>());
    notifier.addObserver(observer);

    EXPECT_CALL(*observer, keyAndValueChanged("key", "value")).Times(1);

    notifier.setKeyAndValue("key", "value");
}

TEST(NotifierTest, testSendNotificationZeroArg)
{
    Observee notifier;
    shared_ptr<TestObserver> observer = make_shared<TestObserver>();

    notifier.setDispatcher(make_shared<CallerThreadDispatcher>());
    notifier.addObserver(observer);

    EXPECT_CALL(*observer, eventOccured()).Times(1);

    notifier.generateEvent();
}

TEST(NotifierTest, testMultipleObservers)
{
    Observee notifier;
    vector<shared_ptr<TestObserver>> observers(10);
    generate(observers.begin(), observers.end(), bind(&make_shared<TestObserver>));

    notifier.setDispatcher(make_shared<CallerThreadDispatcher>());
    for(size_t i = 0; i < observers.size(); ++i)
    {
        notifier.addObserver(observers[i]);
        EXPECT_CALL(*observers[i], stateChanged(5)).Times(1);
    }
    notifier.setState(5);
}

TEST(NotifierTest, testMultipleObserversMultipleNotifications)
{
    Observee notifier;
    vector<shared_ptr<TestObserver>> observers(10);
    generate(observers.begin(), observers.end(), bind(&make_shared<TestObserver>));

    notifier.setDispatcher(make_shared<CallerThreadDispatcher>());
    for(size_t i = 0; i < observers.size(); ++i)
    {
        notifier.addObserver(observers[i]);
        EXPECT_CALL(*observers[i], stateChanged(5)).Times(1);
        EXPECT_CALL(*observers[i], stateChanged(6)).Times(1);
    }
    notifier.setState(5);
    notifier.setState(6);
}

TEST(NotifierTest, testMultipleObserversMultipleTypesOfNotifications)
{
    Observee notifier;
    vector<shared_ptr<TestObserver>> observers(10);
    generate(observers.begin(), observers.end(), bind(&make_shared<TestObserver>));

    notifier.setDispatcher(make_shared<CallerThreadDispatcher>());
    for(size_t i = 0; i < observers.size(); ++i)
    {
        notifier.addObserver(observers[i]);
        EXPECT_CALL(*observers[i], stateChanged(5)).Times(1);
        EXPECT_CALL(*observers[i], nameChanged("name")).Times(1);
    }
    notifier.setState(5);
    notifier.setName("name");
}

TEST(NotifierTest, testZeroObservers)
{
    Observee notifier;
    notifier.setDispatcher(make_shared<CallerThreadDispatcher>());
    notifier.setState(5);
}

TEST(NotifierTest, testRemovedObserversDontGetAnyMoreNotifications)
{
    Observee notifier;
    shared_ptr<TestObserver> observer = make_shared<TestObserver>();
    shared_ptr<TestObserver> removedObserver = make_shared<TestObserver>();

    notifier.setDispatcher(make_shared<CallerThreadDispatcher>());
    notifier.addObserver(observer);
    notifier.addObserver(removedObserver);
    notifier.removeObserver(removedObserver);

    EXPECT_CALL(*observer, stateChanged(5)).Times(1);

    notifier.setState(5);
}

TEST(NotifierTest, testRemovedObserverThatWasNeverAddedDoesntCauseACrash)
{
    Observee notifier;
    shared_ptr<TestObserver> observer = make_shared<TestObserver>();

    notifier.setDispatcher(make_shared<CallerThreadDispatcher>());
    notifier.removeObserver(observer);
}
