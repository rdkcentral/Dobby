/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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
 * IIpcServiceTests.cpp
 *
 *  Created on: 22 Jun 2015
 *      Author: riyadh
 */


#include <memory>
#include <utility>
#include <string>
#include <future>
#include <functional>
#include <chrono>
#include <ctime>
#include <set>
#include <list>
#include <algorithm>
#include <cstdint>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <dbus/dbus.h>

#include <Logging.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "IIpcService.h"
#include "IpcCommon.h"
#include "IpcFactory.h"
#include <IDbusServer.h>

using namespace AI_IPC;
using namespace testing;

#define IPCTEST_SERVICE_COMMON              "test.ipc.common"
#define IPCTEST_SERVICE_PROCESS_CLIENT      "test.ipc.client"
#define IPCTEST_SERVICE_PROCESS_SERVER      "test.ipc.service"
#define IPCTEST_OBJECT_PATH                 "/test/ipc/ai"
#define IPCTEST_INTERFACE_NAME              "test.ipc.ai.interface"
#define IPCTEST_METHOD_NAME                 "testMethod"
#define IPCTEST_SIGNAL_NAME                 "testSignal"

#define IPCTEST_METHOD_NO_RESPONSE_NAME         "testMethodDelayedResponse"
#define IPCTEST_METHOD_DELAYED_RESPONSE_NAME    "testMethodNoResponse"


class MockDbusServer : public AI_DBUS::IDbusServer
{
public:
    MockDbusServer(const std::string& address)
        : mAddress(address) { }

public:
    virtual std::string getBusAddress() const override
    {
        return mAddress;
    }

    virtual std::string getSocketFolder() const override
    {
        return std::string();
    }

private:
    const std::string mAddress;
};



namespace {

VariantList getVariantListUint8()
{
    VariantList args= {0x01};

    return args;
}

VariantList getVariantListUint16()
{
    VariantList args = { uint16_t(1) };

    return args;
}

VariantList getVariantListInt32()
{
    VariantList args = {1};

    return args;
}

VariantList getVariantListUint32()
{
    VariantList args = {1U};

    return args;
}

VariantList getVariantListUint64()
{
    VariantList args = { uint64_t(1ULL) };

    return args;
}

VariantList getVariantListBool()
{
    VariantList args = {true};

    return args;
}

VariantList getVariantListUnixFd(const std::string& fileName, const std::string& fileContent)
{
    VariantList args;

    int fd = open(fileName.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    EXPECT_TRUE(fd > -1);
    ssize_t count = write(fd, fileContent.c_str(), fileContent.size() );
    EXPECT_EQ(count, (ssize_t)fileContent.size());
    close(fd);

    UnixFd unixFd;
    unixFd.fd = open(fileName.c_str(), O_RDONLY);
    EXPECT_TRUE(fd > -1);

    args.push_back(unixFd);

    return args;
}

VariantList getVariantListString()
{
    VariantList args = {"One"};

    return args;
}

VariantList getVariantListUint8Vec()
{
    VariantList args = { std::vector<uint8_t>(1024, 0x08) };
    return args;
}

VariantList getVariantListUint16Vec()
{
    VariantList args = { std::vector<uint16_t>(0x10000, 0xdead) };
    return args;
}

VariantList getVariantListInt32Vec()
{
    VariantList args = { std::vector<int32_t>( {1, 2, 3, 4, 5, 6, 7} ) };

    return args;
}

VariantList getVariantListUint32Vec()
{
    VariantList args = { std::vector<uint32_t>( {1, 2, 3, 4, 5, 6, 7} ) };

    return args;
}

VariantList getVariantListUint64Vec()
{
    VariantList args = { std::vector<uint64_t>( {1, 2, 3, 4, 5, 6, 7} ) };

    return args;
}

#if 0 // boolean vectors no longer supported
VariantList getVariantListBoolVec()
{
    VariantList args = { std::vector<bool>( {true, false, true, false} ) };

    return args;
}
#endif

VariantList getVariantListUnixFdVec(const std::vector<std::string>& fileNames, const std::string& fileContent)
{
    VariantList args;

    for ( auto fileName : fileNames )
    {
        int fd = open(fileName.c_str(), O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
        EXPECT_TRUE(fd > -1);
        ssize_t count = write(fd, fileContent.c_str(), fileContent.size() );
        EXPECT_EQ(count, (ssize_t)fileContent.size());
        close(fd);

        UnixFd unixFd;
        unixFd.fd = open(fileName.c_str(), O_RDONLY);
        EXPECT_TRUE(fd > -1);

        args.push_back(unixFd);
    }

    return args;
}

VariantList getVariantListStringVec()
{
    VariantList args = {"One", "Tow", "Three", "Four"};

    return args;
}

struct DictVariantValue
{
    DictVariantValue()
        : vDbusObject("/test/ipc/ai")
    {
        vUint8 = std::numeric_limits<uint8_t>::max();
        vBool = true;
        vInt16 = std::numeric_limits<int16_t>::max();
        vUint16 = std::numeric_limits<uint16_t>::max();
        vInt32 = std::numeric_limits<int32_t>::max();
        vUint32 = std::numeric_limits<uint32_t>::max();
        vInt64 = std::numeric_limits<int64_t>::max();
        vUint64 = std::numeric_limits<uint64_t>::max();
        vUnixFd.fd = 11;
        vString = "string text";
    }

    uint8_t vUint8;
    bool vBool;
    int16_t vInt16;
    uint16_t vUint16;
    int32_t vInt32;
    uint32_t vUint32;
    int64_t vInt64;
    uint64_t vUint64;
    UnixFd vUnixFd;
    std::string vString;
    DbusObjectPath vDbusObject;
};

VariantList getVariantListDict()
{
    std::map<std::string, DictDataType> dict;

    DictVariantValue values;

    dict["key01"]   = values.vUint8;
    dict["key02"]   = values.vBool;
    dict["key03"]   = values.vInt16;
    dict["key04"]   = values.vUint16;
    dict["key05"]   = values.vInt32;
    dict["key06"]   = values.vUint32;
    dict["key07"]   = values.vInt64;
    dict["key08"]   = values.vUint64;
    dict["key09"]   = values.vUnixFd;
    dict["key10"]   = values.vString;
    dict["key11"]   = values.vDbusObject;


    VariantList args = { dict };

    return args;
}

}

class IIpcServiceTest : public ::testing::Test
{
public:
    void SetUp()
    {
        AICommon::initLogging();

        AI_LOG_FN_ENTRY();

        mIpcServerService = createIpcService(true);
        mIpcClientService = createIpcService(false);

        AI_LOG_FN_EXIT();
    }

    void TearDown()
    {
        if ( mIpcClientService )
        {
            mIpcClientService->stop();
            mIpcClientService.reset();
        }

        if ( mIpcServerService )
        {
            for ( auto regId : mRegIds )
            {
                EXPECT_TRUE(mIpcServerService->unregisterHandler(regId) );
            }

            mIpcServerService->stop();
            mIpcServerService.reset();
        }

        // added for valgrind
        dbus_shutdown();
    }

    void methodHandler(std::shared_ptr<IAsyncReplySender> replySender)
    {
        AI_LOG_FN_ENTRY();

        AI_LOG_INFO( "Method handler is invoked" );

        std::unique_lock<std::mutex> lock(mMutex);

        if( replySender )
        {
            VariantList methodArgs = replySender->getMethodCallArguments();
            mReceivedMethodArgs.push_back(methodArgs);

            AI_LOG_INFO( "Received method arg size %zu", methodArgs.size() );
            if( !replySender->sendReply(methodArgs) )
            {
                AI_LOG_ERROR( "Unable to send reply" );
            }
        }
        else
        {
            AI_LOG_ERROR( "No reply sender" );
        }

        mCondVar.notify_all();
        lock.unlock();

        AI_LOG_FN_EXIT();
    }

    void methodHandlerNoResponse(std::shared_ptr<IAsyncReplySender> replySender)
    {
        AI_LOG_INFO( "Method handler for 'no response' is invoked" );
        if( !replySender )
        {
            AI_LOG_ERROR( "No reply sender" );
        }

        // Don't send any reply
    }

    void methodHandlerDelayedResponse(std::shared_ptr<IAsyncReplySender> replySender)
    {
        AI_LOG_INFO( "Method handler for 'delayed response' is invoked" );
        if( !replySender )
        {
            AI_LOG_ERROR( "No reply sender" );
        }
        else
        {
            VariantList methodArgs = replySender->getMethodCallArguments();

            uint32_t delayMs = boost::get<uint32_t>(methodArgs[0]);
            if( delayMs > 0 )
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
            }

            if( !replySender->sendReply({ true }) )
            {
                AI_LOG_ERROR( "Unable to send reply" );
            }
        }
    }

    void signalHandler(const Signal& signal, const VariantList& args)
    {
        AI_LOG_FN_ENTRY();

        AI_LOG_INFO("received signal %s", signal.name.c_str());

        std::unique_lock<std::mutex> lock(mMutex);

        mReceivedSignalArgs.push_back(args);

        mCondVar.notify_all();
        lock.unlock();

        AI_LOG_FN_EXIT();
    }

    bool registerServerHanders()
    {
        AI_LOG_FN_ENTRY();

        bool res = false;

        if( mIpcServerService )
        {
            Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);
            std::string regId = mIpcServerService->registerMethodHandler(method, std::bind(&IIpcServiceTest::methodHandler, this, std::placeholders::_1));
            if( !regId.empty() )
            {
                mRegIds.push_back(regId);
            }

            Method method2(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NO_RESPONSE_NAME);
            regId = mIpcServerService->registerMethodHandler(method2, std::bind(&IIpcServiceTest::methodHandlerNoResponse, this, std::placeholders::_1));
            if( !regId.empty() )
            {
                mRegIds.push_back(regId);
            }

            Method method3(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_DELAYED_RESPONSE_NAME);
            regId = mIpcServerService->registerMethodHandler(method3, std::bind(&IIpcServiceTest::methodHandlerDelayedResponse, this, std::placeholders::_1));
            if( !regId.empty() )
            {
                mRegIds.push_back(regId);
            }

            Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);
            regId = mIpcServerService->registerSignalHandler(signal, std::bind(&IIpcServiceTest::signalHandler, this, signal, std::placeholders::_1));
            if( !regId.empty() )
            {
                mRegIds.push_back(regId);
            }

            if( mRegIds.size() == 4 )
            {
                res = true;
            }
        }

        AI_LOG_FN_EXIT();

        return res;
    }

    std::shared_ptr<AI_IPC::IIpcService> createIpcService(bool isServer)
    {
        AI_LOG_FN_ENTRY();

        std::shared_ptr<AI_IPC::IIpcService> ipcService;

        std::string address;

        const char* addStr = getenv("DBUS_SESSION_BUS_ADDRESS");
        if( !addStr )
        {
            AI_LOG_ERROR( "IPC service forced to set" );
            //Hardcoded address to run tests on STB
            //FIXME: How to get address if we want to run tests on STB
            address = "unix:abstract=/tmp/dbus-JDiX8cfbls,guid=201ea88171e2680e9ac8361300000147";
        }
        else
        {
            address = std::string(addStr);
        }

        AI_LOG_INFO( "Session  bus address: %s", address.c_str()  );

        {
            try
            {
                std::shared_ptr<MockDbusServer> dbusServer = std::make_shared<MockDbusServer>(address);

                if ( isServer )
                {
                    ipcService = AI_IPC::createIpcService(dbusServer, std::string(IPCTEST_SERVICE_PROCESS_SERVER));
                }
                else
                {
                    ipcService = AI_IPC::createIpcService(dbusServer, std::string(IPCTEST_SERVICE_PROCESS_CLIENT));
                }

                if ( ipcService )
                {
                    ipcService->start();
                }
            }
            catch(const std::exception& e)
            {
                AI_LOG_ERROR( "Unable to create IPC service: %s.", e.what() );
            }
        }

        AI_LOG_FN_EXIT();

        return ipcService;
    }

    bool waitForReceivedSignalCount(unsigned int receivedSignalCount, unsigned int timeoutSec = 2)
    {
        auto timeout = std::chrono::system_clock::now() + std::chrono::seconds(timeoutSec);

        std::unique_lock<std::mutex> lock(mMutex);

        while (mReceivedSignalArgs.size() < receivedSignalCount)
        {
            if (mCondVar.wait_until(lock, timeout) == std::cv_status::timeout)
                break;
        }

        return (mReceivedSignalArgs.size() == receivedSignalCount);
    }

    bool waitForReceivedMethodCallCount(unsigned int receivedMethodCount, unsigned int timeoutSec = 2)
    {
        auto timeout = std::chrono::system_clock::now() + std::chrono::seconds(timeoutSec);

        std::unique_lock<std::mutex> lock(mMutex);

        while (mReceivedMethodArgs.size() < receivedMethodCount)
        {
            if (mCondVar.wait_until(lock, timeout) == std::cv_status::timeout)
                break;
        }

        return (receivedMethodCount == mReceivedMethodArgs.size());
    }

protected: //accessible in the test cases

    std::shared_ptr<AI_IPC::IIpcService> mIpcClientService;
    std::shared_ptr<AI_IPC::IIpcService> mIpcServerService;
    std::vector<VariantList> mReceivedSignalArgs;
    std::vector<VariantList> mReceivedMethodArgs;
    std::vector<std::string> mRegIds;
    std::mutex mMutex;
    std::condition_variable mCondVar;
    unsigned int mMethodCallCount = 0;

private: //not accessible in test cases

};

TEST_F( IIpcServiceTest, testIpcServiceCtorDtor )
{
    AI_LOG_FN_ENTRY();

    ASSERT_TRUE( mIpcClientService != NULL );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testRegisterUnregisterMethodHandlers )
{
    AI_LOG_FN_ENTRY();

    Method methodOne(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, "exampleMethodOne");
    std::string regIdOne = mIpcServerService->registerMethodHandler(methodOne, std::bind(&IIpcServiceTest::methodHandler, this, std::placeholders::_1));
    ASSERT_TRUE( !regIdOne.empty() );

    Method methodTwo(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, "exampleMethodTwo");
    std::string regIdTwo = mIpcServerService->registerMethodHandler(methodTwo, std::bind(&IIpcServiceTest::methodHandler, this, std::placeholders::_1));
    ASSERT_TRUE( !regIdTwo.empty() );

    EXPECT_TRUE(mIpcServerService->unregisterHandler(regIdOne) );
    EXPECT_TRUE(mIpcServerService->unregisterHandler(regIdTwo) );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testRegisterUnregisterSignalHandlers )
{
    AI_LOG_FN_ENTRY();

    Signal signalOne(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, "exampleSignalOne");
    std::string regIdOne = mIpcClientService->registerSignalHandler(signalOne, std::bind(&IIpcServiceTest::signalHandler, this, signalOne, std::placeholders::_1));
    ASSERT_TRUE( !regIdOne.empty() );

    Signal signalTwo(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, "exampleSignalTwo");
    std::string regIdTwo = mIpcClientService->registerSignalHandler(signalTwo, std::bind(&IIpcServiceTest::signalHandler, this, signalTwo, std::placeholders::_1));
    ASSERT_TRUE( !regIdTwo.empty() );

    std::string regIdTwoAnother = mIpcClientService->registerSignalHandler(signalTwo, std::bind(&IIpcServiceTest::signalHandler, this, signalTwo, std::placeholders::_1));
    ASSERT_TRUE( !regIdTwoAnother.empty() );

    EXPECT_TRUE(mIpcClientService->unregisterHandler(regIdOne) );
    EXPECT_TRUE(mIpcClientService->unregisterHandler(regIdTwo) );
    EXPECT_TRUE(mIpcClientService->unregisterHandler(regIdTwoAnother) );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testRegisterMultipleHandlersForSameMethod )
{
    AI_LOG_FN_ENTRY();

    Method methodOne(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, "exampleMethodOne");
    std::string regIdOne = mIpcServerService->registerMethodHandler(methodOne, std::bind(&IIpcServiceTest::methodHandler, this, std::placeholders::_1));
    ASSERT_TRUE( !regIdOne.empty() );

    std::string regIdTwo = mIpcServerService->registerMethodHandler(methodOne, std::bind(&IIpcServiceTest::methodHandler, this, std::placeholders::_1));
    ASSERT_TRUE( regIdTwo.empty() );

    EXPECT_TRUE(mIpcServerService->unregisterHandler(regIdOne) );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalVoid )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs;
    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == 0 );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalUint8 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListUint8();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == 1 );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalUint16 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListUint16();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == 1 );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalInt32 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListInt32();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == 1 );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalUint32 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListUint32();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == 1 );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalUint64 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListUint64();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == 1 );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalBool )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListBool();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == 1 );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalUnixFd )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    std::string fileName("/tmp/txt-dbus-xxx1.txt");
    std::string fileContent("Hello World");
    VariantList signalArgs = getVariantListUnixFd(fileName, fileContent);

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == 1 );

    VariantList receivedArgs = mReceivedSignalArgs[0];
    UnixFd unixFd = boost::get<UnixFd>(receivedArgs[0]);

    char buf[512] = {0};
    ssize_t count = read(unixFd.fd, buf, 512);
    EXPECT_EQ( count, (ssize_t)fileContent.size() );
    EXPECT_EQ( fileContent, std::string(buf) );

    EXPECT_TRUE( unlink(fileName.c_str()) == 0 );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalString )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListString();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == 1 );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalUint8Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListUint8Vec();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == signalArgs.size() );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalUint16Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListUint16Vec();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == signalArgs.size() );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalInt32Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListInt32Vec();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == signalArgs.size() );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalUint32Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListUint32Vec();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == signalArgs.size() );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalUint64Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListUint64Vec();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == signalArgs.size() );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

#if 0 // boolean vectors no longer supported
TEST_F( IIpcServiceTest, testEmitSignalBoolVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListBoolVec();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == signalArgs.size() );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}
#endif

TEST_F( IIpcServiceTest, testEmitSignalUnixFdVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    std::string fileContent("Hello World");
    std::vector<std::string> fileNames = {"/tmp/txt-dbus-xxx1.txt", "/tmp/txt-dbus-xxx2.txt"};

    VariantList signalArgs = getVariantListUnixFdVec(fileNames, fileContent);

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == 2 );

    VariantList receivedArgs = mReceivedSignalArgs[0];
    std::vector<UnixFd> unixFds;
    unixFds.push_back(boost::get<UnixFd>(receivedArgs[0]));
    unixFds.push_back(boost::get<UnixFd>(receivedArgs[1]));

    for ( auto unixFd : unixFds )
    {
        char buf[512] = {0};
        ssize_t count = read(unixFd.fd, buf, 512);
        EXPECT_EQ( count, (ssize_t)fileContent.size() );
        EXPECT_EQ( fileContent, std::string(buf) );
    }

    for ( auto fileName : fileNames )
    {
        EXPECT_TRUE( unlink(fileName.c_str()) == 0 );
    }

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalStringVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListStringVec();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == signalArgs.size() );
    EXPECT_TRUE( signalArgs == mReceivedSignalArgs[0] );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitSignalDict )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Signal signal(IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_SIGNAL_NAME);

    VariantList signalArgs = getVariantListDict();

    EXPECT_TRUE( mIpcClientService->emitSignal(signal, signalArgs ) );
    ASSERT_TRUE( waitForReceivedSignalCount(1) );
    ASSERT_TRUE( mReceivedSignalArgs.size() == 1 );
    ASSERT_TRUE( mReceivedSignalArgs[0].size() == signalArgs.size() );

    try
    {
        std::map<std::string, DictDataType> dictSent = boost::get<std::map<std::string, DictDataType>>(signalArgs[0]);
        std::map<std::string, DictDataType> dictReceived = boost::get<std::map<std::string, DictDataType>>(mReceivedSignalArgs[0][0]);
        ASSERT_TRUE( dictSent.size() == dictReceived.size());

        auto dictSentIter = dictSent.begin();
        auto dictReceivedIter = dictReceived.begin();
        for( ;
            (dictSentIter != dictSent.end()) || (dictReceivedIter != dictReceived.end());
            ++dictSentIter, ++dictReceivedIter)
        {
            ASSERT_TRUE( dictSentIter->first == dictReceivedIter->first );
        }


        uint8_t vUint8Sent = boost::get<uint8_t>(dictSent.find("key01")->second);
        bool vBoolSent = boost::get<bool>(dictSent.find("key02")->second);
        int16_t vInt16Sent = boost::get<int16_t>(dictSent.find("key03")->second);
        uint16_t vUint16Sent = boost::get<uint16_t>(dictSent.find("key04")->second);
        int32_t vInt32Sent = boost::get<int32_t>(dictSent.find("key05")->second);
        uint32_t vUint32Sent = boost::get<uint32_t>(dictSent.find("key06")->second);
        int64_t vInt64Sent = boost::get<int64_t>(dictSent.find("key07")->second);
        uint64_t vUint64Sent = boost::get<uint64_t>(dictSent.find("key08")->second);
        UnixFd vUnixFdSent = boost::get<UnixFd>(dictSent.find("key09")->second);
        std::string vStringSent = boost::get<std::string>(dictSent.find("key10")->second);
        DbusObjectPath vDbusObjectSent = boost::get<DbusObjectPath>(dictSent.find("key11")->second);

        uint8_t vUint8Received = boost::get<uint8_t>(dictReceived.find("key01")->second);
        bool vBoolReceived = boost::get<bool>(dictReceived.find("key02")->second);
        int16_t vInt16Received = boost::get<int16_t>(dictReceived.find("key03")->second);
        uint16_t vUint16Received = boost::get<uint16_t>(dictReceived.find("key04")->second);
        int32_t vInt32Received = boost::get<int32_t>(dictReceived.find("key05")->second);
        uint32_t vUint32Received = boost::get<uint32_t>(dictReceived.find("key06")->second);
        int64_t vInt64Received = boost::get<int64_t>(dictReceived.find("key07")->second);
        uint64_t vUint64Received = boost::get<uint64_t>(dictReceived.find("key08")->second);
        UnixFd vUnixFdReceived = boost::get<UnixFd>(dictReceived.find("key09")->second);
        std::string vStringReceived = boost::get<std::string>(dictReceived.find("key10")->second);
        DbusObjectPath vDbusObjectReceived = boost::get<DbusObjectPath>(dictReceived.find("key11")->second);

        EXPECT_TRUE( vUint8Sent == vUint8Received );
        EXPECT_TRUE( vBoolSent == vBoolReceived );
        EXPECT_TRUE( vInt16Sent == vInt16Received );
        EXPECT_TRUE( vUint16Sent == vUint16Received );
        EXPECT_TRUE( vInt32Sent == vInt32Received );
        EXPECT_TRUE( vUint32Sent == vUint32Received );
        EXPECT_TRUE( vInt64Sent == vInt64Received );
        EXPECT_TRUE( vUint64Sent == vUint64Received );
        EXPECT_TRUE( vUnixFdSent.fd != -1 );
        EXPECT_TRUE( vUnixFdReceived.fd != -1 );
        EXPECT_TRUE( vStringSent == vStringReceived );
        EXPECT_TRUE( vDbusObjectSent == vDbusObjectReceived );
    }
    catch(const std::exception& e)
    {
        AI_LOG_ERROR( "Exception caught %s", e.what() );
    }

    AI_LOG_FN_EXIT();
}


TEST_F( IIpcServiceTest, testInvokeMethodAsyncUint8 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint8();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;

    ASSERT_TRUE( replyGetter->getReply(replyArgs) );

    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncUint16 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint16();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;

    ASSERT_TRUE( replyGetter->getReply(replyArgs) );

    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncInt32 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListInt32();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    ASSERT_TRUE( replyGetter->getReply(replyArgs) );
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncUint32 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint32();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    ASSERT_TRUE( replyGetter->getReply(replyArgs) );
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncUint64 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint64();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    ASSERT_TRUE( replyGetter->getReply(replyArgs) );
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncBool )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListBool();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    ASSERT_TRUE( replyGetter->getReply(replyArgs) );
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncUnixFd )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    std::string fileName("/tmp/txt-dbus-xxx1.txt");
    std::string fileContent("Hello World");
    VariantList methodArgs = getVariantListUnixFd(fileName, fileContent);

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    ASSERT_TRUE( waitForReceivedMethodCallCount(1) );
    ASSERT_TRUE( mReceivedMethodArgs.size() == 1 );
    ASSERT_TRUE( mReceivedMethodArgs[0].size() == 1 );

    VariantList receivedArgs = mReceivedMethodArgs[0];
    UnixFd unixFd = boost::get<UnixFd>(receivedArgs[0]);

    char buf[512] = {0};
    ssize_t count = read(unixFd.fd, buf, 512);
    EXPECT_EQ( count, (ssize_t)fileContent.size() );
    EXPECT_EQ( fileContent, std::string(buf) );

    VariantList replyArgs;
    ASSERT_TRUE( replyGetter->getReply(replyArgs) );
    ASSERT_TRUE( replyArgs.size() == 1 );

    unixFd = boost::get<UnixFd>(replyArgs[0]);

    memset( buf, 0, sizeof(buf) );

    EXPECT_TRUE( lseek(unixFd.fd, 0, SEEK_SET) == 0);

    count = read(unixFd.fd, buf, 512);
    EXPECT_EQ( count, (ssize_t)fileContent.size() );
    EXPECT_EQ( fileContent, std::string(buf) );

    EXPECT_TRUE( unlink(fileName.c_str()) == 0 );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncString )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListString();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    ASSERT_TRUE( replyGetter->getReply(replyArgs) );
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncUint8Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint8Vec();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    EXPECT_TRUE( replyGetter->getReply(replyArgs) );
    EXPECT_EQ( replyArgs.size(), methodArgs.size() );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncUint16Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint16Vec();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    EXPECT_TRUE( replyGetter->getReply(replyArgs) );
    EXPECT_EQ( replyArgs.size(), methodArgs.size() );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncInt32Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListInt32Vec();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    ASSERT_TRUE( replyGetter->getReply(replyArgs) );
    ASSERT_EQ( replyArgs.size(), methodArgs.size() );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncUint32Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint32Vec();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    EXPECT_TRUE( replyGetter->getReply(replyArgs) );
    EXPECT_EQ( replyArgs.size(), methodArgs.size() );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncUint64Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint64Vec();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    EXPECT_TRUE( replyGetter->getReply(replyArgs) );
    EXPECT_EQ( replyArgs.size(), methodArgs.size() );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

#if 0 // boolean vectors no longer supported
TEST_F( IIpcServiceTest, testInvokeMethodAsyncBoolVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListBoolVec();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    EXPECT_TRUE( replyGetter->getReply(replyArgs) );
    EXPECT_EQ( replyArgs.size(), methodArgs.size() );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}
#endif

TEST_F( IIpcServiceTest, testInvokeMethodAsyncUnixFdVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    std::string fileContent("Hello World");
    std::vector<std::string> fileNames = {"/tmp/txt-dbus-xxx1.txt", "/tmp/txt-dbus-xxx2.txt"};

    VariantList methodArgs = getVariantListUnixFdVec(fileNames, fileContent);

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    ASSERT_TRUE( waitForReceivedMethodCallCount(1) );
    ASSERT_TRUE( mReceivedMethodArgs.size() == 1 );
    ASSERT_TRUE( mReceivedMethodArgs[0].size() == 2 );

    VariantList receivedArgs = mReceivedMethodArgs[0];
    std::vector<UnixFd> unixFds;
    unixFds.push_back(boost::get<UnixFd>(receivedArgs[0]));
    unixFds.push_back(boost::get<UnixFd>(receivedArgs[1]));

    for ( auto unixFd : unixFds )
    {
        char buf[512] = {0};
        ssize_t count = read(unixFd.fd, buf, 512);
        EXPECT_EQ( count, (ssize_t)fileContent.size() );
        EXPECT_EQ( fileContent, std::string(buf) );
    }

    VariantList replyArgs;
    ASSERT_TRUE( replyGetter->getReply(replyArgs) );
    ASSERT_TRUE( replyArgs.size() == 2 );

    unixFds.clear();
    unixFds.push_back(boost::get<UnixFd>(replyArgs[0]));
    unixFds.push_back(boost::get<UnixFd>(replyArgs[1]));

    for ( auto unixFd : unixFds )
    {
        EXPECT_TRUE( lseek(unixFd.fd, 0, SEEK_SET) == 0);
        char buf[512] = {0};
        ssize_t count = read(unixFd.fd, buf, 512);
        EXPECT_EQ( count, (ssize_t)fileContent.size() );
        EXPECT_EQ( fileContent, std::string(buf) );
    }

    for ( auto fileName : fileNames )
    {
        EXPECT_TRUE( unlink(fileName.c_str()) == 0 );
    }

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncStringVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListStringVec();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    EXPECT_TRUE( replyGetter->getReply(replyArgs) );
    EXPECT_EQ( replyArgs.size(), methodArgs.size() );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncEmptyVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = { std::vector<int32_t>() };

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    EXPECT_TRUE( replyGetter->getReply(replyArgs) );
    EXPECT_EQ( replyArgs.size(), methodArgs.size() );
    EXPECT_TRUE( replyArgs == methodArgs );
    EXPECT_TRUE( boost::get<std::vector<int32_t>>(replyArgs[0]).empty() );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodAsyncWrongMethodCall )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, "WrongMethodName");

    VariantList methodArgs = getVariantListBool();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    EXPECT_FALSE( replyGetter->getReply(replyArgs) );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncUint8 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint8();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncUint16 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint16();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncInt32 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListInt32();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncUint32 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint32();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncUint64 )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint64();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncBool )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListBool();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncUnixFd )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    std::string fileName("/tmp/txt-dbus-xxx1.txt");
    std::string fileContent("Hello World");
    VariantList methodArgs = getVariantListUnixFd(fileName, fileContent);

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    ASSERT_TRUE( replyArgs.size() == 1 );

    VariantList receivedArgs = mReceivedMethodArgs[0];
    UnixFd unixFd = boost::get<UnixFd>(receivedArgs[0]);

    char buf[512] = {0};
    ssize_t count = read(unixFd.fd, buf, 512);
    EXPECT_EQ( count, (ssize_t)fileContent.size() );
    EXPECT_EQ( fileContent, std::string(buf) );

    unixFd = boost::get<UnixFd>(replyArgs[0]);

    memset( buf, 0, sizeof(buf) );

    EXPECT_TRUE( lseek(unixFd.fd, 0, SEEK_SET) == 0);

    count = read(unixFd.fd, buf, 512);
    EXPECT_EQ( count, (ssize_t)fileContent.size() );
    EXPECT_EQ( fileContent, std::string(buf) );

    EXPECT_TRUE( unlink(fileName.c_str()) == 0 );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncString )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListString();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    ASSERT_TRUE( replyArgs.size() == 1 );
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncUint8Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint8Vec();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncUint16Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint16Vec();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncInt32Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListInt32Vec();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncUint32Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint32Vec();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncUint64Vec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListUint64Vec();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

#if 0 // boolean vectors no longer supported
TEST_F( IIpcServiceTest, testInvokeMethodSyncBoolVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListBoolVec();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}
#endif

TEST_F( IIpcServiceTest, testInvokeMethodSyncUnixFdVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    std::string fileContent("Hello World");
    std::vector<std::string> fileNames = {"/tmp/txt-dbus-xxx1.txt", "/tmp/txt-dbus-xxx2.txt"};

    VariantList methodArgs = getVariantListUnixFdVec(fileNames, fileContent);

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    ASSERT_TRUE( replyArgs.size() == 2 );

    VariantList receivedArgs = mReceivedMethodArgs[0];
    std::vector<UnixFd> unixFds;
    unixFds.push_back(boost::get<UnixFd>(receivedArgs[0]));
    unixFds.push_back(boost::get<UnixFd>(receivedArgs[1]));

    for ( auto unixFd : unixFds )
    {
        char buf[512] = {0};
        ssize_t count = read(unixFd.fd, buf, 512);
        EXPECT_EQ( count, (ssize_t)fileContent.size() );
        EXPECT_EQ( fileContent, std::string(buf) );
    }

    unixFds.clear();
    unixFds.push_back(boost::get<UnixFd>(replyArgs[0]));
    unixFds.push_back(boost::get<UnixFd>(replyArgs[1]));

    for ( auto unixFd : unixFds )
    {
        EXPECT_TRUE( lseek(unixFd.fd, 0, SEEK_SET) == 0);
        char buf[512] = {0};
        ssize_t count = read(unixFd.fd, buf, 512);
        EXPECT_EQ( count, (ssize_t)fileContent.size() );
        EXPECT_EQ( fileContent, std::string(buf) );
    }

    for ( auto fileName : fileNames )
    {
        EXPECT_TRUE( unlink(fileName.c_str()) == 0 );
    }

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncStringVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListStringVec();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    EXPECT_TRUE( replyArgs == methodArgs );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodDict )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListDict();

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));

    ASSERT_TRUE( methodArgs.size() == replyArgs.size() );

    try
    {
        std::map<std::string, DictDataType> dictSent = boost::get<std::map<std::string, DictDataType>>(methodArgs[0]);
        std::map<std::string, DictDataType> dictReceived = boost::get<std::map<std::string, DictDataType>>(replyArgs[0]);
        ASSERT_TRUE( dictSent.size() == dictReceived.size());

        auto dictSentIter = dictSent.begin();
        auto dictReceivedIter = dictReceived.begin();
        for( ;
            (dictSentIter != dictSent.end()) || (dictReceivedIter != dictReceived.end());
            ++dictSentIter, ++dictReceivedIter)
        {
            ASSERT_TRUE( dictSentIter->first == dictReceivedIter->first );
        }


        uint8_t vUint8Sent = boost::get<uint8_t>(dictSent.find("key01")->second);
        bool vBoolSent = boost::get<bool>(dictSent.find("key02")->second);
        int16_t vInt16Sent = boost::get<int16_t>(dictSent.find("key03")->second);
        uint16_t vUint16Sent = boost::get<uint16_t>(dictSent.find("key04")->second);
        int32_t vInt32Sent = boost::get<int32_t>(dictSent.find("key05")->second);
        uint32_t vUint32Sent = boost::get<uint32_t>(dictSent.find("key06")->second);
        int64_t vInt64Sent = boost::get<int64_t>(dictSent.find("key07")->second);
        uint64_t vUint64Sent = boost::get<uint64_t>(dictSent.find("key08")->second);
        UnixFd vUnixFdSent = boost::get<UnixFd>(dictSent.find("key09")->second);
        std::string vStringSent = boost::get<std::string>(dictSent.find("key10")->second);
        DbusObjectPath vDbusObjectSent = boost::get<DbusObjectPath>(dictSent.find("key11")->second);

        uint8_t vUint8Received = boost::get<uint8_t>(dictReceived.find("key01")->second);
        bool vBoolReceived = boost::get<bool>(dictReceived.find("key02")->second);
        int16_t vInt16Received = boost::get<int16_t>(dictReceived.find("key03")->second);
        uint16_t vUint16Received = boost::get<uint16_t>(dictReceived.find("key04")->second);
        int32_t vInt32Received = boost::get<int32_t>(dictReceived.find("key05")->second);
        uint32_t vUint32Received = boost::get<uint32_t>(dictReceived.find("key06")->second);
        int64_t vInt64Received = boost::get<int64_t>(dictReceived.find("key07")->second);
        uint64_t vUint64Received = boost::get<uint64_t>(dictReceived.find("key08")->second);
        UnixFd vUnixFdReceived = boost::get<UnixFd>(dictReceived.find("key09")->second);
        std::string vStringReceived = boost::get<std::string>(dictReceived.find("key10")->second);
        DbusObjectPath vDbusObjectReceived = boost::get<DbusObjectPath>(dictReceived.find("key11")->second);

        EXPECT_TRUE( vUint8Sent == vUint8Received );
        EXPECT_TRUE( vBoolSent == vBoolReceived );
        EXPECT_TRUE( vInt16Sent == vInt16Received );
        EXPECT_TRUE( vUint16Sent == vUint16Received );
        EXPECT_TRUE( vInt32Sent == vInt32Received );
        EXPECT_TRUE( vUint32Sent == vUint32Received );
        EXPECT_TRUE( vInt64Sent == vInt64Received );
        EXPECT_TRUE( vUint64Sent == vUint64Received );
        EXPECT_TRUE( vUnixFdSent.fd != -1 );
        EXPECT_TRUE( vUnixFdReceived.fd != -1 );
        EXPECT_TRUE( vStringSent == vStringReceived );
        EXPECT_TRUE( vDbusObjectSent == vDbusObjectReceived );
    }
    catch(const std::exception& e)
    {
        AI_LOG_ERROR( "Exception caught %s", e.what() );
    }

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodSyncEmptyVec )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = { std::vector<int32_t>() };

    VariantList replyArgs;
    ASSERT_TRUE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));
    EXPECT_TRUE( replyArgs == methodArgs );
    EXPECT_TRUE( boost::get<std::vector<int32_t>>(replyArgs[0]).empty() );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testEmitInvalidSignal )
{
    AI_LOG_FN_ENTRY();

    Signal signal;
    EXPECT_FALSE( mIpcClientService->emitSignal(signal, VariantList()));

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeInvalidMethodAsync )
{
    AI_LOG_FN_ENTRY();

    Method method;
    VariantList methodArgs = getVariantListStringVec();
    ASSERT_FALSE(mIpcClientService->invokeMethod(method, methodArgs));

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeInvalidMethodSync )
{
    AI_LOG_FN_ENTRY();

    Method method;
    VariantList methodArgs = getVariantListStringVec();
    VariantList replyArgs;
    ASSERT_FALSE(mIpcClientService->invokeMethod(method, methodArgs, replyArgs));

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodWrongService )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method("test.ipc.unknown", IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NAME);

    VariantList methodArgs = getVariantListInt32();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, methodArgs);
    ASSERT_TRUE( replyGetter != NULL );

    VariantList replyArgs;
    ASSERT_FALSE( replyGetter->getReply(replyArgs) );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodNoReply )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NO_RESPONSE_NAME);

    unsigned int timeoutMs = 100;
    auto startTime = std::chrono::steady_clock::now();

    std::shared_ptr<IAsyncReplyGetter> replyGetter = mIpcClientService->invokeMethod(method, { }, timeoutMs);
    ASSERT_TRUE( replyGetter != nullptr );

    VariantList replyArgs;
    ASSERT_FALSE( replyGetter->getReply(replyArgs) );

    // Check that we only blocked for the timeout time
    std::chrono::milliseconds timeTook = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime);
    ASSERT_NEAR(timeTook.count(), timeoutMs, 50);

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMethodDelayedReply )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    VariantList replyArgs;
    std::shared_ptr<IAsyncReplyGetter> replyGetter;
    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_DELAYED_RESPONSE_NAME);

    // Invoke a method that sends a reply in 200ms, but only wait for 100ms for the reply
    replyGetter = mIpcClientService->invokeMethod(method, { uint32_t(200) }, 100);
    ASSERT_TRUE( replyGetter != nullptr );
    ASSERT_FALSE( replyGetter->getReply(replyArgs) );

    // Now call the method with no delayed reply and ensure we get the response
    replyGetter = mIpcClientService->invokeMethod(method, { uint32_t(0) }, 500);
    ASSERT_TRUE( replyGetter != nullptr );
    ASSERT_TRUE( replyGetter->getReply(replyArgs) );

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testMultipleAsyncReplies )
{
    EXPECT_TRUE( registerServerHanders() );

    std::vector< std::shared_ptr<IAsyncReplyGetter> > replyGetters;
    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_DELAYED_RESPONSE_NAME);

    // Invoke a bunch of method calls
    for (unsigned int i = 0; i < 128; i++)
    {
        std::shared_ptr<IAsyncReplyGetter> replyGetter =
            mIpcClientService->invokeMethod(method, { uint32_t(0) } );
        ASSERT_NE( replyGetter, nullptr );

        replyGetters.push_back(replyGetter);
    }

    // Shuffle the replies
    std::random_shuffle(replyGetters.begin(), replyGetters.end());

    // And then go get 'em
    for (size_t i = 0; i < replyGetters.size(); i++)
    {
        VariantList replyArgs;
        ASSERT_TRUE( replyGetters[i]->getReply(replyArgs) );
        ASSERT_EQ( boost::get<bool>(replyArgs[0]), true );

        replyGetters[i].reset();
    }
}

TEST_F( IIpcServiceTest, testMultipleAsyncAndNoReplies )
{
    EXPECT_TRUE( registerServerHanders() );

    std::vector< std::shared_ptr<IAsyncReplyGetter> > replyGetters;
    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_DELAYED_RESPONSE_NAME);

    // Invoke a bunch of method calls
    for (unsigned int i = 0; i < 128; i++)
    {
        std::shared_ptr<IAsyncReplyGetter> replyGetter =
            mIpcClientService->invokeMethod(method, { uint32_t(0) } );
        ASSERT_NE( replyGetter, nullptr );

        replyGetters.push_back(replyGetter);
    }

    // Shuffle the replies
    std::random_shuffle(replyGetters.begin(), replyGetters.end());

    // Trim half of the shuffled results
    replyGetters.resize(replyGetters.size() / 2);

    // And then go get 'em
    for (size_t i = 0; i < replyGetters.size(); i++)
    {
        VariantList replyArgs;
        ASSERT_TRUE( replyGetters[i]->getReply(replyArgs) );
        ASSERT_EQ( boost::get<bool>(replyArgs[0]), true );

        replyGetters[i].reset();
    }
}

TEST_F( IIpcServiceTest, testInvokeMethodMultipleNoReply )
{
    AI_LOG_FN_ENTRY();

    EXPECT_TRUE( registerServerHanders() );

    std::list< std::shared_ptr<IAsyncReplyGetter> > replyGetters;
    Method method(IPCTEST_SERVICE_PROCESS_SERVER, IPCTEST_OBJECT_PATH, IPCTEST_INTERFACE_NAME, IPCTEST_METHOD_NO_RESPONSE_NAME);

    // Invoke a bunch of method calls, the first block has low timeouts
    for (unsigned int i = 0; i < 64; i++)
    {
        std::shared_ptr<IAsyncReplyGetter> replyGetter =
            mIpcClientService->invokeMethod(method, { uint32_t(0) }, 100);
        ASSERT_NE( replyGetter, nullptr );

        replyGetters.push_back(replyGetter);
    }

    // The second bunch has a longer timeout
    for (unsigned int i = 0; i < 64; i++)
    {
        std::shared_ptr<IAsyncReplyGetter> replyGetter =
            mIpcClientService->invokeMethod(method, { uint32_t(0) }, 1000);
        ASSERT_NE( replyGetter, nullptr );

        replyGetters.push_back(replyGetter);
    }

    // Wait for the first bunch to timeout
    for (size_t i = 0; i < 64; i++)
    {
        VariantList replyArgs;
        ASSERT_FALSE( replyGetters.front()->getReply(replyArgs) );

        replyGetters.pop_front();
    }

    // Push some more in with another timeout
    for (unsigned int i = 0; i < 64; i++)
    {
        std::shared_ptr<IAsyncReplyGetter> replyGetter =
            mIpcClientService->invokeMethod(method, { uint32_t(0) }, 100);
        ASSERT_NE( replyGetter, nullptr );

        replyGetters.push_front(replyGetter);
    }

    // Wait for all of them to finish
    for (size_t i = 0; i < 128; i++)
    {
        VariantList replyArgs;
        ASSERT_FALSE( replyGetters.front()->getReply(replyArgs) );

        replyGetters.pop_front();
    }

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testInvokeMultipleStartStop )
{
    AI_LOG_FN_ENTRY();

    std::shared_ptr<AI_IPC::IIpcService> ipcService;
    std::shared_ptr<MockDbusServer> dbusServer;

    const char* address = getenv("DBUS_SESSION_BUS_ADDRESS");
    if( address )
    {
        dbusServer = std::make_shared<MockDbusServer>(address);

        try
        {
            ipcService = AI_IPC::createIpcService(dbusServer, std::string(IPCTEST_SERVICE_COMMON));
            if ( ipcService )
            {
                ASSERT_TRUE(ipcService->start());
                ASSERT_FALSE(ipcService->start());

                ASSERT_TRUE(ipcService->stop());
                ASSERT_FALSE(ipcService->stop());
            }
            else
            {
                ASSERT_TRUE(false);
            }
        }
        catch(const std::exception& e)
        {
            ASSERT_TRUE(false);
        }
    }
    else
    {
        ASSERT_TRUE(false);
    }

    AI_LOG_FN_EXIT();
}

TEST_F( IIpcServiceTest, testIsServiceAvailable )
{
    AI_LOG_FN_ENTRY();

    std::shared_ptr<AI_IPC::IIpcService> ipcService;
    std::shared_ptr<MockDbusServer> dbusServer;

    const char* address = getenv("DBUS_SESSION_BUS_ADDRESS");
    if( address )
    {
        dbusServer = std::make_shared<MockDbusServer>(address);

        try
        {
            ipcService = AI_IPC::createIpcService(dbusServer, std::string(IPCTEST_SERVICE_COMMON));
            if ( ipcService )
            {
                ASSERT_TRUE(ipcService->start());
                ASSERT_FALSE(ipcService->isServiceAvailable("some.thing.that.doesnt.exist"));
                ASSERT_TRUE(ipcService->isServiceAvailable(IPCTEST_SERVICE_COMMON));
                ASSERT_TRUE(ipcService->stop());
            }
            else
            {
                ASSERT_TRUE(false);
            }
        }
        catch(const std::exception& e)
        {
            ASSERT_TRUE(false);
        }
    }
    else
    {
        ASSERT_TRUE(false);
    }

    AI_LOG_FN_EXIT();
}

