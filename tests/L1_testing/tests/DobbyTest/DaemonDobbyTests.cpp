/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Synamedia
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


#include "IAsyncReplySenderMock.h"
#define private public
#include "Dobby.h"
#include <gtest/gtest.h>

#include "DobbyWorkQueueMock.h"
#include "IpcFileDescriptorMock.h"
#include "DobbySettingsMock.h"
#include "IIpcServiceMock.h"
#if defined(LEGACY_COMPONENTS)
#include "DobbyTemplateMock.h"
#endif /* LEGACY_COMPONENTS */
#include "DobbyEnvMock.h"
#include "DobbyManagerMock.h"
#include "DobbyIPCUtilsMock.h"
#include "DobbyUtilsMock.h"
#include "ContainerIdMock.h"
#include "DobbyProtocol.h"

#include <cstdlib>
#include <fcntl.h>

DobbyWorkQueueImpl* DobbyWorkQueue::impl = nullptr;
DobbyManagerImpl*   DobbyManager::impl = nullptr;
DobbyIPCUtilsImpl*  DobbyIPCUtils::impl = nullptr;
#if defined(LEGACY_COMPONENTS)
DobbyTemplateImpl*  DobbyTemplate::impl = nullptr;
#endif /* LEGACY_COMPONENTS */
DobbyUtilsImpl*  DobbyUtils::impl = nullptr;
ContainerIdImpl*  ContainerId::impl = nullptr;
AI_IPC::IIpcServiceImpl* AI_IPC::IIpcService::impl = nullptr;
AI_IPC::IAsyncReplySenderApiImpl* AI_IPC::IAsyncReplySender::impl = nullptr;
AI_IPC::IpcFileDescriptorApiImpl* AI_IPC::IpcFileDescriptor::impl = nullptr;

using ::testing::NiceMock;
class DaemonDobbyTest : public ::testing::Test{

protected:
    AI_IPC::IAsyncReplySenderMock* p_asyncReplySenderMock = nullptr;
    AI_IPC::IpcFileDescriptorMock*  p_ipcFileDescriptorMock = nullptr;
    DobbyWorkQueueMock*  p_workQueueMock = nullptr;
#if defined(LEGACY_COMPONENTS)
    DobbyTemplateMock*  p_templateMock = nullptr;
#endif /* LEGACY_COMPONENTS */
    DobbyUtilsMock*  p_utilsMock = nullptr;
    DobbyIPCUtilsMock*  p_IPCUtilsMock = nullptr;
    DobbyManagerMock*  p_dobbyManagerMock = nullptr ;
    AI_IPC::IpcServiceMock*  p_ipcServiceMock = nullptr ;
    ContainerIdMock*  p_containerIdMock  = nullptr;
#if defined(LEGACY_COMPONENTS)
    DobbyTemplate *p_dobbyTemplate = nullptr;
#endif //defined(LEGACY_COMPONENTS)

    DobbyManager *p_dobbyManager = nullptr;
    AI_IPC::IIpcService *p_ipcService = nullptr;
    DobbyWorkQueue *p_workQueue = nullptr;
    DobbyUtils *p_dobbyUtils = nullptr;
    DobbyIPCUtils *p_dobbyIPCUtils = nullptr;
    ContainerId *p_containerId = nullptr;
    AI_IPC::IAsyncReplySender *p_iasyncReplySender = nullptr;
    AI_IPC::IpcFileDescriptor *p_iIpcFileDescriptor = nullptr;
    std::shared_ptr<const IDobbySettings>  p_settingsMock =  nullptr;

    std::shared_ptr<Dobby> dobby_test ;

    virtual void SetUp()
    {
        p_asyncReplySenderMock = new NiceMock<AI_IPC::IAsyncReplySenderMock>;
        p_ipcFileDescriptorMock = new NiceMock<AI_IPC::IpcFileDescriptorMock>;
        p_workQueueMock =new NiceMock <DobbyWorkQueueMock>;
#if defined(LEGACY_COMPONENTS)
        p_templateMock = new NiceMock <DobbyTemplateMock>;
#endif /* LEGACY_COMPONENTS */
        p_utilsMock =  new NiceMock <DobbyUtilsMock>;
        p_IPCUtilsMock = new NiceMock <DobbyIPCUtilsMock>;
        p_dobbyManagerMock = new NiceMock <DobbyManagerMock>;
        p_ipcServiceMock  = new NiceMock <AI_IPC::IpcServiceMock>;
        p_containerIdMock = new NiceMock  <ContainerIdMock>;

        AI_IPC::IAsyncReplySender::setImpl(p_asyncReplySenderMock);
        AI_IPC::IpcFileDescriptor::setImpl(p_ipcFileDescriptorMock);
        AI_IPC::IIpcService::setImpl(p_ipcServiceMock);
        DobbyWorkQueue::setImpl(p_workQueueMock);
#if defined(LEGACY_COMPONENTS)
        DobbyTemplate::setImpl(p_templateMock);
#endif /* LEGACY_COMPONENTS */
        DobbyUtils::setImpl(p_utilsMock);
        DobbyIPCUtils::setImpl(p_IPCUtilsMock);
        DobbyManager::setImpl(p_dobbyManagerMock);
        ContainerId::setImpl(p_containerIdMock);

        p_settingsMock =  std::make_shared<NiceMock<DobbySettingsMock>>();
        std::string dbusAddress = "unix:path=/some/socket";

        ON_CALL(*p_ipcServiceMock, registerMethodHandler(::testing::_,::testing::_))
           .WillByDefault(::testing::Return("some_method_id"));

#if defined(LEGACY_COMPONENTS)
        EXPECT_CALL(*p_templateMock, setSettings(::testing::_))
           .Times(1);
#endif //defined(LEGACY_COMPONENTS)

        dobby_test = std::make_shared<NiceMock<Dobby>>(dbusAddress, (std::shared_ptr<AI_IPC::IIpcService>)p_ipcService, p_settingsMock);
        EXPECT_NE(dobby_test, nullptr);
    }

    virtual void TearDown()
    {
        ON_CALL(*p_ipcServiceMock, unregisterHandler(::testing::_))
        .WillByDefault(::testing::Return(true));

        dobby_test.reset();
        dobby_test = nullptr;
        DobbyWorkQueue::setImpl(nullptr);
        AI_IPC::IAsyncReplySender::setImpl(nullptr);
        AI_IPC::IpcFileDescriptor::setImpl(nullptr);
#if defined(LEGACY_COMPONENTS)
        DobbyTemplate::setImpl(nullptr);
#endif //defined(LEGACY_COMPONENTS)
        AI_IPC::IIpcService::setImpl(nullptr);
        DobbyManager::setImpl(nullptr);
        DobbyIPCUtils::setImpl(nullptr);
        ContainerId::setImpl(nullptr);
        DobbyUtils::setImpl(nullptr);

        if( p_asyncReplySenderMock != nullptr)
        {
            delete  p_asyncReplySenderMock;
            p_asyncReplySenderMock = nullptr;
        }
        if ( p_ipcFileDescriptorMock != nullptr)
        {
            delete  p_ipcFileDescriptorMock;
            p_ipcFileDescriptorMock = nullptr;
        }

        if ( p_workQueueMock != nullptr)
        {
            delete  p_workQueueMock;
            p_workQueueMock = nullptr;
        }

#if defined(LEGACY_COMPONENTS)
        if ( p_templateMock != nullptr)
        {
            delete  p_templateMock;
            p_templateMock = nullptr;
        }
#endif //defined(LEGACY_COMPONENTS)

        if (p_utilsMock != nullptr)
        {
            delete p_utilsMock;
            p_utilsMock = nullptr;
        }

        if ( p_IPCUtilsMock != nullptr)
        {
            delete  p_IPCUtilsMock;
            p_IPCUtilsMock = nullptr;
        }

        if ( p_dobbyManagerMock != nullptr)
        {
            delete  p_dobbyManagerMock;
            p_dobbyManagerMock = nullptr;
        }

        if ( p_ipcServiceMock != nullptr)
        {
            delete  p_ipcServiceMock;
            p_ipcServiceMock = nullptr;
        }

        if ( p_containerIdMock != nullptr)
        {
            delete  p_containerIdMock;
            p_containerIdMock = nullptr;
        }

        p_settingsMock.reset();
        p_settingsMock = nullptr;
    }
};

/****************************************************************************************************
 * Test functions for :shutdown
 * @brief Method called from admin client requesting the daemon to shutdown

 * Use case coverage:
 *                @Success :2
 *                @Failure :0
 ***************************************************************************************************/

/**
 * @brief Test shutdown with successful sendReply.
 * Check if shutdown method is successfully completed  and sending back an empty Reply after a successful sendReply.
 *
 * @param[in] shared pointer to iasyncReplySender.
 * @return None.
 */
TEST_F(DaemonDobbyTest, shutdownSuccess_sendReplySuccess)
{
    EXPECT_CALL(*p_workQueueMock, exit())
        .Times(1);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));
    dobby_test->shutdown((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test shutdown with failed sendReply.
 * Check if shutdown method is successfully completed after a failed sendReply.
 *
 * @param[in] shared pointer to iasyncReplySender.
 * @return None.
 */
 TEST_F(DaemonDobbyTest, shutdownSuccess_sendReplyFailed)
{
    EXPECT_CALL(*p_workQueueMock, exit())
        .Times(1);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                return false;
            }));
    dobby_test->shutdown((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/*Test cases for shutdown ends here*/

/****************************************************************************************************
 * Test functions for :ping
 * @brief Simple ping dbus method call

 * Use case coverage:
 *                @Success :1
 *                @Failure :1
 ***************************************************************************************************/
/**
 * @brief Test ping with failed postWork.
 * Check if ping method sending back the reply as false after a failed postWork.
 *
 * @param[in] replySender Shared pointer to IAsyncReplySender.
 * @return None.
 */

#if defined(RDK) && defined(USE_SYSTEMD)
/*
 * @brief Test ping with successful postWork.
 * Check if ping method successfully completes and sending back the Reply as true after a successful  postWork.
 *
 * @param[in] replySender Shared pointer to IAsyncReplySender.
 * @return None.
 */
TEST_F(DaemonDobbyTest, pingSuccess_postWorkSuccess)
{
    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->ping((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/*
 * @brief Test ping with postWork and send replyfailed.
 * Check if ping method failure for send Reply and failed postWork.
 *
 * @param[in] replySender Shared pointer to IAsyncReplySender.
 * @return None.
 */
TEST_F(DaemonDobbyTest, pingFailed_postWorkFailedSendReplyFailed)
{
    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));

    dobby_test->ping((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for ping ends here*/
#endif //defined(RDK) && defined(USE_SYSTEMD)

/****************************************************************************************************
 * Test functions for :setLogMethod
 * @brief Method called from APP_Process telling which method to use for logging
 *
 *  This method is provided with a single of mandatory fields; logMethod. An
 *  optional second parameter containing the logging pipe fd should be supplied
 *  if the log method is 'ethanlog'
 *
 * Use case coverage:
 *                @Success :4
 *                @Failure :4
 ***************************************************************************************************/


/**
 * @brief Test setLogMethod with invalid argument size.
 * Check if setLogMethod handles a case where getMethodCallArguments returns an invalid argument size
 * by sending back the Reply as false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogMethodFailled_invalidArgSize)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{2}));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->setLogMethod((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test setLogMethod with invalid argument list.
 * Check if setLogMethod handles a case where getMethodCallArguments returns an invalid arguments,
 * by sending back the Reply as false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogMethodFailled_invalidArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{12456,3}));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->setLogMethod((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test setting log method with valid argument size and Invalid LogMethod.
 * Check if setLogMethod handles a case where getMethodCallArguments returns a valid argument size (4) and an invalid LogMethod is provided.
 * by setting log target value = 0 and  sending back the Reply as fasle,
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogMethodFailed_validArgSize_InvalidLogMethod)
{
    uint32_t dobby_logType = 5;
    AI_IPC::UnixFd validUnixFd(1);
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{dobby_logType, validUnixFd, 3, 4}));

    EXPECT_CALL(*p_ipcFileDescriptorMock, isValid())
        .Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->setLogMethod((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/**
 * @brief Test setting log method with method = DOBBY_LOG_ETHANLOG and invalid log pipe fd
 * Checks if setLogMethod handles a case with valid arguments (DOBBY_LOG_ETHANLOG and an invalid pipe fd),
 * by sending back the Reply as true, and ensures that it does not replace the existing logging pipe fd with the new one.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogMethodFailed_DOBBY_LOG_ETHANLOG_invalidLogPipe)
{
    int32_t newFd = 1;
    AI_IPC::VariantList validArgs = {
    uint32_t(DOBBY_LOG_ETHANLOG),
    AI_IPC::UnixFd(newFd),
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_ipcFileDescriptorMock, isValid())
        .Times(1)
        .WillOnce(::testing::Return(false));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->setLogMethod((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
    // Ensure that the logging pipe fd is not replaced
    int currentlogPipeFd = Dobby::mEthanLogPipeFd;
    ASSERT_NE(currentlogPipeFd, newFd);
    // Check the log targets value
    unsigned logTargetsValue = Dobby::mLogTargets.load();
    ASSERT_EQ(logTargetsValue,Dobby::LogTarget::EthanLog);
}

/**
 * @brief Test setting log method with valid arguments and DOBBY_LOG_ETHANLOG with valid log pipe.
 * Checks if setLogMethod handles a case with valid arguments (DOBBY_LOG_ETHANLOG and a valid pipe fd),
 * by sending back the Reply as true, and ensures that it replaces the existing logging pipe fd with the new one.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogMethodSuccess_DOBBY_LOG_ETHANLOG_validLogPipe)
{
    int32_t newFd = 1;
    AI_IPC::VariantList validArgs = {
    uint32_t(DOBBY_LOG_ETHANLOG),
    AI_IPC::UnixFd(newFd),
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_ipcFileDescriptorMock, isValid())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_ipcFileDescriptorMock, fd())
    .WillOnce(::testing::Return(123));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->setLogMethod((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
    // Ensure that the logging pipe fd is  replaced with new Fd
    int currentlogPipeFd = Dobby::mEthanLogPipeFd;
    ASSERT_NE(currentlogPipeFd, newFd);
    unsigned logTargetsValue = Dobby::mLogTargets.load();
    ASSERT_EQ(logTargetsValue,Dobby::LogTarget::EthanLog);
}

/**
 * @brief Test setting log method with valid argument size and DOBBY_LOG_SYSLOG.
 * Check if setLogMethod handles a case where getMethodCallArguments returns a valid argument size (4) and DOBBY_LOG_SYSLOG is provided.
 * by setting log target value = 2 and  sending back the Reply as true,
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogMethodSuccess_DOBBY_LOG_SYSLOG)
{
     uint32_t dobby_logType = DOBBY_LOG_SYSLOG;
     AI_IPC::UnixFd validUnixFd(1);
     EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
                .WillOnce(::testing::Return(AI_IPC::VariantList{dobby_logType, validUnixFd, 3, 4}));

     EXPECT_CALL(*p_ipcFileDescriptorMock, isValid())
        .Times(0);

     EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

     dobby_test->setLogMethod((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
     unsigned logTargetsValue = Dobby::mLogTargets.load();
     ASSERT_EQ(logTargetsValue,Dobby::LogTarget::SysLog);
}

/**
 * @brief Test setting log method with valid argument size and DOBBY_LOG_CONSOLE
 * Check if setLogMethod handles a case where getMethodCallArguments returns a valid argument size (4) and DOBBY_LOG_CONSOLE is provided.
 * by setting log target value = 1 and  sending back the Reply as true,
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogMethodSuccess_DOBBY_LOG_CONSOLE)
{
    uint32_t dobby_logType = DOBBY_LOG_CONSOLE;
    AI_IPC::UnixFd validUnixFd(1);
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{dobby_logType, validUnixFd, 3, 4}));

    EXPECT_CALL(*p_ipcFileDescriptorMock, isValid())
        .Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

     dobby_test->setLogMethod((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
     unsigned logTargetsValue = Dobby::mLogTargets.load();
     ASSERT_EQ(logTargetsValue,Dobby::LogTarget::Console);
}

/**
 * @brief Test setting log method with valid argument size and LogMethod NULL.
 * Check if setLogMethod handles a case where getMethodCallArguments returns a valid argument size (4) and LogMethod NULL is provided.
 * by setting log target value = 0 and  sending back the Reply as true,
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogMethodSuccess_LogMethod_NULL)
{
    uint32_t dobby_logType = 0;
    AI_IPC::UnixFd validUnixFd(1);
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{dobby_logType, validUnixFd, 3, 4}));

    EXPECT_CALL(*p_ipcFileDescriptorMock, isValid())
        .Times(0);

     EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

     dobby_test->setLogMethod((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
     unsigned logTargetsValue = Dobby::mLogTargets.load();
     ASSERT_EQ(logTargetsValue, 0);
}
/*Test cases for setLogMethod ends here*/

/****************************************************************************************************
 * Test functions for :setLogLevel
 * @brief Method called from APP_Process telling the log level to use.
 * The log level can only be dynamically changed on non-production builds.
 * @param[in]  replySender     Contains the arguments and the reply object.
 * Use case coverage:
 *                @Success :1
 *                @Failure :2
 ***************************************************************************************************/


/**
 * @brief Test setting log level with invalid argument.
 * Check if setLogLevel handles a case where getMethodCallArguments returns an invalid argument.
 * by sending back the result as false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogLevelFailed_invalidArg)
{
    AI_IPC::VariantList invalidArgs = {
    uint32_t(2),
    true,
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(invalidArgs));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->setLogLevel((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test setting log level with valid argument.
 * Check if setLogLevel handles a case where getMethodCallArguments returns a valid argument
 * by sending back the result as true
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogLevelSuccess_validArg)
{
    // Create a valid log level argument
    int32_t logLevel = AI_DEBUG_LEVEL_MILESTONE;
    AI_IPC::VariantList validArgs = {
    int32_t(logLevel)
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->setLogLevel((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
    ASSERT_EQ(__ai_debug_log_level, static_cast<int>(logLevel));
}

/**
 * @brief Test setting log level with invalid argument.
 * Check if setLogLevel handles a case where getMethodCallArguments returns an invalid argument and send reply failed.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setLogLevelFailedSendReplyFailed_invalidArg)
{
    AI_IPC::VariantList invalidArgs = {
    uint32_t(2),
    true,
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(invalidArgs));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));

    dobby_test->setLogLevel((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for setLogLevel ends here*/

/****************************************************************************************************
 * Test functions for :getState
 * @brief Gets the state of a container
 *
 * Use case coverage:
 *                @Success :1
 *                @Failure :3
 ***************************************************************************************************/

/**
 * @brief Test getState with invalid arguments.
 * Check if getState method handles the case with invalid arguments;
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getStateFailed_invalidArg)
{
    AI_IPC::VariantList invalidArgs = {
    uint32_t(2),
    true,
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(invalidArgs));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                                int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getState((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test getState with a valid argument and a failure post work.
 * Check if getState method successfully handles a valid argument and failed post work
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getStateFailed_postWorkFailure)
{
    int32_t validDescriptor = 1;
    AI_IPC::VariantList validArgs = {
    int32_t(validDescriptor)
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, stateOfContainer(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
               int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getState((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test getState with a valid argument.
 * Check if getState method successfully handles a valid argument.
 * by sending back a reply value returned by stateOfContainer.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getStateSuccess_postWorkSuccess)
{
    int32_t validDescriptor = 1;
    AI_IPC::VariantList validArgs = {
    int32_t(validDescriptor)
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, stateOfContainer(::testing::_))
        .WillOnce(::testing::Return(CONTAINER_STATE_RUNNING));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = CONTAINER_STATE_RUNNING;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getState((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test getState with a valid argument.
 * Check if getState method failed postWork and sendReplay.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getStateFailed_postWorkFailedSendReplyFailed)
{
    int32_t validDescriptor = 1;
    AI_IPC::VariantList validArgs = {
    int32_t(validDescriptor)
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, stateOfContainer(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
               int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));

    dobby_test->getState((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for getState ends here*/

/****************************************************************************************************
 * Test functions for :getInfo
 * @brief Gets some info about a container
 *
 * Use case coverage:
 *                @Success :1
 *                @Failure :2
 ***************************************************************************************************/

/**
 * @brief Test getInfo with invalid arguments.
 * Check if getInfo method handles the case with invalid arguments;
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getInfoFailed_invalidArg)
{
    AI_IPC::VariantList invalidArgs = {
    uint32_t(2),
    true,
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(invalidArgs));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
              std::string expectedResult = "";
              std::string actualResult = "";
              if (AI_IPC::parseVariantList <std::string>
                       (replyArgs, &actualResult))
              {
                  EXPECT_EQ(actualResult, expectedResult);
              }
              return true;
            }));

    dobby_test->getInfo((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/**
 * @brief Test getInfo with valid argument and failed postWork.
 * Check if getInfo method handles the case with a valid argument and failed postWork;
 * by sending back reply = empty
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getInfoFailed_validArg_postWorkFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123),
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, statsOfContainer(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getInfo((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test getInfo with valid argument and successful postWork.
 * Check if getInfo method handles the case with a valid argument and successful postWork.
 * by sending back a reply value returned by statsOfContainer.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getInfoSuccess_validArg_postWorkSuccess)
{
    //Simulates a valid argument 'descriptor' with a value of 123
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{123}));

    EXPECT_CALL(*p_dobbyManagerMock, statsOfContainer(::testing::_))
        .WillOnce(::testing::Return("DobbyContainer::State::Starting"));


    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "DobbyContainer::State::Starting";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));


    dobby_test->getInfo((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/*Test cases for getInfo ends here*/

#if (AI_BUILD_TYPE == AI_DEBUG) && defined(LEGACY_COMPONENTS)
/****************************************************************************************************
 * Test functions for :createBundle
 * @brief Debugging utility that can be used to create a bundle based on
 * a dobby spec file

 * Use case coverage:
 *                @Success :1
 *                @Failure :4
 ***************************************************************************************************/

/**
 * @brief Test createBundle with invalid arguments.
 * Check if createBundle method handles the case with invalid arguments.
 * by sending back the reply = false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, createBundleFailed_invalidArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{1, 2, 3}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_dobbyManagerMock, createBundle(::testing::_, ::testing::_)).Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->createBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test createBundle with empty arguments.
 * Check if createBundle method handles the case with empty arguments.
 * by sending back the reply = false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, createBundleFailed_emptyArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{}));

    // Since there are no arguments, postWork and createBundle should not be called.
    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_dobbyManagerMock, createBundle(::testing::_, ::testing::_)).Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->createBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test createBundle with invalid containerId.
 * Check if createBundle method handles the case invalid ocntainer id.
 * by sending back the reply = false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, createBundleFailed_invalidContainerId)
{
    AI_IPC::VariantList validArgs = {
    std::string("1"),
    std::string("2"),
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{validArgs}));

    // Since there are no arguments, postWork and createBundle should not be called.
    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_dobbyManagerMock, createBundle(::testing::_, ::testing::_)).Times(0);

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(false));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->createBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test createBundle with valid arguments and failed postWork.
 * Check if createBundle method handles the case with valid arguments and failed postWork.
 * by sending back the reply = false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, createBundleFailed_validArg_postWorkFailed)
{
    AI_IPC::VariantList validArgs = {
    std::string("1"),
    std::string("2"),
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, createBundle(::testing::_, ::testing::_))
        .Times(0);

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->createBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test createBundle with valid arguments and failed postWork.
 * Check if createBundle method handles the case with valid arguments and failed postWork and failed sendReplay.
 * by sending back the reply = false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, createBundleFailed_validArg_postWorkFailedSendReplyFailed)
{
    AI_IPC::VariantList validArgs = {
    std::string("1"),
    std::string("2"),
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, createBundle(::testing::_, ::testing::_))
        .Times(0);

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));

    dobby_test->createBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/**
 * @brief Test createBundle with valid arguments and successful postWork.
 * Check if createBundle method handles the case with valid arguments and successful postWork.
 * by sending back the reply = true
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest,createBundleSuccess_validArg_postWorkSuccess)
{
    AI_IPC::VariantList validArgs = {
    std::string("arg1"),
    std::string("arg2"),
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, createBundle(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));


    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->createBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}


/****************************************************************************************************
 * Test functions for :getSpec
 *  @brief Debugging utility to retrieve the original spec file for a running
 *  container (i.e. like the 'virsh dumpxml' command).
 * Use case coverage:
 *                @Success :1
 *                @Failure :4
 ***************************************************************************************************/

/**
 * @brief Test getSpec with invalid arguments.
 * Check if getSpec method handles the case with invalid arguments.
 * by sending back empty reply
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getSpecFailed_invalidArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{1, 2, 3}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_dobbyManagerMock, specOfContainer(::testing::_)).Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

 /* @brief Test getSpec with empty arguments.
 * Check if getSpec method handles the case with empty arguments.
 * by sending back empty reply
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getSpecFailed_emptyArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_dobbyManagerMock, specOfContainer(::testing::_)).Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/**
 * @brief Test getSpec with valid arguments and failed posting specOfContainer to the  work queue
 * Check if getSpec method handles the case with valid arguments and failed postWork.
 * by sending back empty reply
 * @return None.
 */
TEST_F(DaemonDobbyTest, getSpecFailed_validArg_postWorkFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123), /*Simulates a valid argument 'descriptor' with a value of 123*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, specOfContainer(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test getSpec with valid arguments and successful postWork.
 * Check if getSpec method handles the case with valid arguments and successful postWork.
 * by sending back a reply value returned by specOfContainer.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getSpecSuccess_validArg_postWorkSuccess)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123), /*Simulates a valid argument 'descriptor' with a value of 123*/
    };
    std::string specContainerString = "ContainerSpec123";

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, specOfContainer(::testing::_))
        .WillOnce(::testing::Return(specContainerString));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "ContainerSpec123";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test getSpec with valid arguments and successful postWork.
 * Check if getSpec method handles the case with valid arguments and failed postWork, send reply.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getSpecFailed_validArg_postWorkFailedSendReplyFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123), /*Simulates a valid argument 'descriptor' with a value of 123*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, specOfContainer(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));

    dobby_test->getSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for getSpec ends here*/
#endif //(AI_BUILD_TYPE == AI_DEBUG) && defined(LEGACY_COMPONENTS)

#if (AI_BUILD_TYPE == AI_DEBUG)
/****************************************************************************************************
 * Test functions for :getOCIConfig
 * @brief Debugging utility to retrieve the OCI config.json file for a running
 * container (i.e. like the 'virsh dumpxml' command)
 *
 * Use case coverage:
 *                @Success :1
 *                @Failure :4
 ***************************************************************************************************/

/**
 * @brief Test getOCIConfig with empty arguments.
 * Check if getOCIConfig method handles the case with empty arguments.
 * by sending back a reply value empty
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getOCIConfigFailed_emptyArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_dobbyManagerMock, ociConfigOfContainer(::testing::_)).Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getOCIConfig((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test getOCIConfig with invalid arguments.
 * Check if getOCIConfig method handles the case with invalid arguments.
 * by sending back a reply value empty
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getOCIConfigFailed_invalidArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{1, 2, 3}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getOCIConfig((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test getOCIConfig with valid arguments and failed postWork.
 * Check if getOCIConfig method handles the case with valid arguments and failed postWork.
 * by sending back a reply value empty
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getOCIConfigFailed_validArg_postWorkFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123),/*Simulates a valid argument 'descriptor' with a value of 123*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, ociConfigOfContainer(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getOCIConfig((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/**
 * @brief Test getOCIConfig with valid arguments and successful postWork.
 * Check if getOCIConfig method handles the case with valid arguments and successful postWork.
 * by sending back a reply value returned by ociConfigOfContainer.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getOCIConfigSuccess_validArg_postWorkSuccess)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123),/*Simulates a valid argument 'descriptor' with a value of 123*/
    };
    std::string ociConfigString = "OCIConfig123";

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, ociConfigOfContainer(::testing::_))
        .WillOnce(::testing::Return(ociConfigString));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "OCIConfig123";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->getOCIConfig((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test getOCIConfig with valid arguments and failed postWork and sendReply.
 * Check if getOCIConfig method handles the case with valid arguments and failed postWork and sendReply.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, getOCIConfigFailed_validArg_postWorkFailedSendReplyFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123),/*Simulates a valid argument 'descriptor' with a value of 123*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, ociConfigOfContainer(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::string expectedResult = "";
                std::string actualResult = "";
                if (AI_IPC::parseVariantList <std::string>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));

    dobby_test->getOCIConfig((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for getOciConfig ends here*/
#endif // (AI_BUILD_TYPE == AI_DEBUG)

/****************************************************************************************************
 * Test functions for :stop
 * @brief Stops a running container
 *
 * Use case coverage:
 *                @Success :1
 *                @Failure :4
 ***************************************************************************************************/

/**
 * @brief Test stop with invalid arguments.
 * Check if stop method handles the case with invalid arguments.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, stopFailed_invalidArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{1, 2, 3}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->stop((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test stop with empty arguments.
 * Check if stop method handles the case with empty arguments.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, stopFailed_emptyArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_dobbyManagerMock, stopContainer(::testing::_,::testing::_,::testing::_)).Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->stop((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}


/**
 * @brief Test stop with valid arguments and failed postWork.
 * Check if stop method handles the case with valid arguments and failed postWork.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, stopFailed_validArg_postWorkFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123),/*Simulates a valid argument 'descriptor' with a value of 123*/
    true, /*Simulates a valid argument 'force'*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, stopContainer(::testing::_,::testing::_,::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));


    dobby_test->stop((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test stop with valid arguments and successful postWork.
 * Check if stop method handles the case with valid arguments and successful postWork.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, stopSuccess_validArg_postWorkSuccess)
{
    AI_IPC::VariantList validArgs = {
    int32_t(1),
    true,
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, stopContainer(::testing::_,::testing::_,::testing::_))
        .WillOnce(::testing::Invoke(
                 [&](int32_t cd, bool withPrejudice, const std::function<void(int32_t cd, const ContainerId& id,  int32_t status)> containnerStopCb) {
                     ContainerId containerId;
                     containnerStopCb(cd, containerId, 2/*DobbyContainer::State:Stopping*/);
                     return cd;
                 }));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->stop((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test stop with valid arguments and failed postWork.
 * Check if stop method handles the case with valid arguments and failed postWork, send reply.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, stopFailed_validArg_postWorkFailedSendReplyFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123),/*Simulates a valid argument 'descriptor' with a value of 123*/
    true, /*Simulates a valid argument 'force'*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, stopContainer(::testing::_,::testing::_,::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));


    dobby_test->stop((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for stop ends here*/

/****************************************************************************************************
 * Test functions for :pause
 * @brief Pauses (freezes) a running container
 *
 * Use case coverage:
 *                @Success :1
 *                @Failure :3
 ***************************************************************************************************/

/**
 * @brief Test pause with invalid arguments.
 * Check if pause method handles the case with invalid arguments.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, pauseFailed_invalidArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{1, 2, 3}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->pause((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test pause with empty arguments.
 * Check if pause method handles the case with empty arguments.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, pauseFailed_emptyArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_dobbyManagerMock, pauseContainer(::testing::_)).Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->pause((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test pause with valid arguments and failed postWork.
 * Check if pause method handles the case with valid arguments and failed postWork.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, pauseFailed_validArg_postWorkFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123), /*Simulates a valid argument 'descriptor' with a value of 123*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{validArgs}));

    EXPECT_CALL(*p_dobbyManagerMock, pauseContainer(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));


    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));
    dobby_test->pause((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test pause with valid arguments and failed postWork.
 * Check if pause method handles the case with valid arguments and failed postWork, send reply failed.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, pauseFailed_validArg_postWorkFailedSendReplyFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123), /*Simulates a valid argument 'descriptor' with a value of 123*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{validArgs}));

    EXPECT_CALL(*p_dobbyManagerMock, pauseContainer(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));
    dobby_test->pause((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test pause with valid arguments and successful postWork.
 * Check if pause method handles the case with valid arguments and successful postWork.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, pauseSuccess_validArg_postWorkSuccess)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123),/*Simulates a valid argument 'descriptor' with a value of 123*/
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, pauseContainer(::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->pause((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for pause ends here*/

/****************************************************************************************************
 * Test functions for :resume
 * @brief Resumes a paused (frozen) container
 * Use case coverage:
 *                @Success :1
 *                @Failure :4
 ***************************************************************************************************/
/**
 * @brief Test resume with invalid arguments.
 * Check if resume method handles the case with invalid arguments.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, resumeFailed_invalidArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{1, 2, 3}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->resume((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/**
 * @brief Test resume with empty arguments.
 * Check if resume method handles the case with empty arguments.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, resumeFailed_emptyArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_dobbyManagerMock, resumeContainer(::testing::_)).Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->resume((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}


/**
 * @brief Test resume with valid arguments and failed postWork.
 * Check if resume method handles the case with valid arguments and failed postWork.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, resumeFailed_validArg_postWorkFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123), /*Simulates a valid argument 'descriptor' with a value of 123*/
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{validArgs}));

    EXPECT_CALL(*p_dobbyManagerMock, resumeContainer(::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->resume((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test resume with valid arguments and successful postWork.
 * Check if resume method handles the case with valid arguments and successful postWork.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, resumeSuccess_validArg_postWorkSuccess)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123), /*Simulates a valid argument 'descriptor' with a value of 123*/
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{validArgs}));

    EXPECT_CALL(*p_dobbyManagerMock, resumeContainer(::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->resume((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test resume with valid arguments and failed sendReply and postWork.
 * Check if resume method handles the case with valid arguments failed sendReply and failed postWork.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, resumeSuccess_validArg_SendReplyFailedpostWorkFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(123), /*Simulates a valid argument 'descriptor' with a value of 123*/
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{validArgs}));

    EXPECT_CALL(*p_dobbyManagerMock, resumeContainer(::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }))
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));


    dobby_test->resume((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for resume ends here*/

/****************************************************************************************************
 * Test functions for :exec
 * @brief Executes a command in a container
 *
 * Use case coverage:
 *                @Success :1
 *                @Failure :4
 ***************************************************************************************************/
/**
 * @brief Test exec with invalid arguments.
 * Check if exec method handles the case with invalid arguments.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, execFailed_invalidArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{1}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->exec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test exec with empty arguments.
 * Check if exec method handles the case with empty arguments.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, execFailed_emptyArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{}));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_)).Times(0);
    EXPECT_CALL(*p_dobbyManagerMock, execInContainer(::testing::_,::testing::_,::testing::_)).Times(0);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->exec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}



/**
 * @brief Test exec with valid arguments and failed postWork.
 * Check if exec method handles the case with valid arguments and failed postWork.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, execFailed_validArg_postWorkFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(1),
    std::string("2"),
    std::string("3"),
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, execInContainer(::testing::_,::testing::_,::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));


    dobby_test->exec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test exec with valid arguments and successful postWork.
 * Check if exec method handles the case with valid arguments and successful postWork.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, execSuccess_validArg_postWorkSuccess)
{
    AI_IPC::VariantList validArgs = {
    int32_t(1),
    std::string("2"),
    std::string("3"),
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, execInContainer(::testing::_,::testing::_,::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->exec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test exec with valid arguments and successful postWork.
 * Check if exec method handles the case with valid arguments and failed postWork, failed send reply.
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, execFailed_validArg_postWorkFailedSendReplyFailed)
{
    AI_IPC::VariantList validArgs = {
    int32_t(1),
    std::string("2"),
    std::string("3"),
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_dobbyManagerMock, execInContainer(::testing::_,::testing::_,::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }))
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->exec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/*Test cases for exec ends here*/

/****************************************************************************************************
 * Test functions for :setAIDbusAddress
 * @brief Method called from APP_Process telling us the AI dbus addresses
 *
 * Use case coverage:
 *                @Success :1
 *                @Failure :3
 ***************************************************************************************************/
/**
 * @brief Test setAIDbusAddress with invalid arguments.
 * Check if setAIDbusAddress method handles the case with invalid arguments.
 * by sending back reply = false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setAIDbusAddressFailed_invalidArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(AI_IPC::VariantList{1, 2, 3}));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->setAIDbusAddress((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/**
 * @brief Test setAIDbusAddress with valid arguments and failed setAIDbusAddress
 * Check if setAIDbusAddress method handles the case when setAIDbusAddress failed
 * by sending back reply = false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setAIDbusAddressFailed_setAIDbusAddressSuccess)
{
    AI_IPC::VariantList validArgs = {
    true, /*Simulates a valid argument 'privateBus' with a value of true*/
    std::string("2"),/*Simulate a valis argument  DbusAddress with a valude of 2*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_IPCUtilsMock, setAIDbusAddress(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(false));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->setAIDbusAddress((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test setAIDbusAddress with valid arguments and setAIDbusAddress success.
 * Check if setAIDbusAddress method handles the case when setAIDbusAddress success.
 * by sending back reply = true
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setAIDbusAddressSuccess_setAIDbusAddressSuccess)
{
    AI_IPC::VariantList validArgs = {
    true, /*Simulates a valid argument 'privateBus' with a value of true*/
    std::string("2"), /*Simulate a valis argument  DbusAddress with a valude of 2*/
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_IPCUtilsMock, setAIDbusAddress(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->setAIDbusAddress((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test setAIDbusAddress with valid arguments and setAIDbusAddress success and sendReply failed.
 * Check if setAIDbusAddress method handles the case when setAIDbusAddress success  and sendReply failed.
 * by sending back reply = false
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, setAIDbusAddressSuccess_sendReplyFailed)
{
    AI_IPC::VariantList validArgs = {
    true, /*Simulates a valid argument 'privateBus' with a value of true*/
    std::string("2"), /*Simulate a valis argument  DbusAddress with a valude of 2*/
    };
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .WillOnce(::testing::Return(validArgs));

    EXPECT_CALL(*p_IPCUtilsMock, setAIDbusAddress(::testing::_, ::testing::_))
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = true;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));

    dobby_test->setAIDbusAddress((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for setAIDbusAddress ends here*/
#if defined(LEGACY_COMPONENTS)

/****************************************************************************************************
 * Test functions for :startFromSpec
 * @brief Starts a new container from the supplied json spec document.
 * Use case coverage:
 *                @Success :2
 *                @Failure :4
 ***************************************************************************************************/
/**
 * @brief Test starting a container from a spec with invalid arguments and parse parameter failed.
 * Check if startFromSpec method handles the case with invalid arguments and parsing arguments failed;
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromSpecFailed_parseParamFailure)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .Times(2)
        .WillOnce(::testing::Return(AI_IPC::VariantList{1}))
        .WillOnce(::testing::Return(AI_IPC::VariantList{1,2}));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->startFromSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test starting a container from a spec with argument size 3 and postWork failure.
 * Check if startFromSpec method handles the case when argument size is 3 and postWork fails.
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromSpecFailed_argSize_3_postWorkFail)
{
    AI_IPC::UnixFd fd1 = 123; // Assuming 123 is a valid file descriptor
    AI_IPC::VariantList validArgs_set1 = {
    std::string("1"),/*Simulate a string representing the identifier of the container*/
    std::string("2"),/*Simulate a string representing a JSON specification.*/
    std::vector<AI_IPC::UnixFd>{fd1},/*Simulate valid file descriptors*/
    };


        // Create a UnixFd object
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
            .Times(2)
        .WillOnce(::testing::Return(validArgs_set1))
        .WillOnce(::testing::Return(validArgs_set1));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_dobbyManagerMock, startContainerFromSpec(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->startFromSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test starting a container from a spec with argument size 3 and postWork success.
 * Check if startFromSpec method handles the case when argument size is 3 and postWork succeeds.
 * by sending back reply returned by startContainerFromSpec
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromSpecSuccess_argSize_3_postWorkSuccess)
{
        // Create a UnixFd object
    AI_IPC::UnixFd fd1 = 123; // Assuming 123 is a valid file descriptor
    AI_IPC::VariantList validArgs_set1 = {
    std::string("1"),/*Simulate a string representing the identifier of the container*/
    std::string("2"),/*Simulate a string representing a JSON specification.*/
    std::vector<AI_IPC::UnixFd>{fd1},/*Simulate valid file descriptors*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .Times(2)
        .WillOnce(::testing::Return(validArgs_set1))
        .WillOnce(::testing::Return(validArgs_set1));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_dobbyManagerMock, startContainerFromSpec(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const ContainerId& id, const std::string& jsonSpec,
                  const std::list<int>& files, const std::string& command,
                  const std::string& displaySocket, const std::vector<std::string>& envVars,
                  const std::function<void(int32_t cd, const ContainerId& id)> containnerStartCb) {
                containnerStartCb(123, id);
                return 123;
            }));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = 123;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->startFromSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test starting a container from a spec with argument size 6 and postWork success.
 * Check if startFromSpec method handles the case when argument size is 6 and postWork succeeds.
 * by sending back reply returned by startContainerFromSpec
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromSpecSuccess_argSize_6_postWorkSuccess)
{
        // Create a UnixFd object
    AI_IPC::UnixFd fd1 = 123; // Assuming 123 is a valid file descriptor
    AI_IPC::VariantList validArgs_set2 = {
    std::string("1"),/*Simulate a string representing the identifier of the container*/
    std::string("2"),/*Simulate a string representing a JSON specification.*/
    std::vector<AI_IPC::UnixFd>{fd1},/*Simulate valid file descriptors*/
    std::string("abc"), /*Simulate a command string*/
    std::string("def"),/*Simulate a string representing a display socket.*/
    std::vector<std::string>{ "ghi" }, /*Simulate a  vector of strings representing environment variables.*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .Times(3)
        .WillOnce(::testing::Return(validArgs_set2))
        .WillOnce(::testing::Return(validArgs_set2))
        .WillOnce(::testing::Return(validArgs_set2));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    /* Simulates a successful start, returning a container descriptor ,which is a unique number that identifies the container */
    EXPECT_CALL(*p_dobbyManagerMock, startContainerFromSpec(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const ContainerId& id, const std::string& jsonSpec,
                  const std::list<int>& files, const std::string& command,
                  const std::string& displaySocket, const std::vector<std::string>& envVars,
                  const std::function<void(int32_t cd, const ContainerId& id)> containnerStartCb) {
                containnerStartCb(123, id);
                return 123;
            }));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = 123;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->startFromSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test starting a container from a spec with invalid containerId.
 * Check if startFromSpec method handles the case when containerId is invalid.
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromSpecFailed_ContainerIdIsValidFail)
{
    // Create a UnixFd object
    AI_IPC::UnixFd fd1 = 123; // Assuming 123 is a valid file descriptor
    AI_IPC::VariantList validArgs_set1 = {
    std::string("1"),/*Simulate a string representing the identifier of the container*/
    std::string("2"),/*Simulate a string representing a JSON specification.*/
    std::vector<AI_IPC::UnixFd>{fd1},/*Simulate valid file descriptors*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .Times(2)
        .WillOnce(::testing::Return(validArgs_set1))
        .WillOnce(::testing::Return(validArgs_set1));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(false));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->startFromSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test starting a container from a spec with argument size 3 and postWork failure, send reply fail.
 * Check if startFromSpec method handles the case when argument size is 3 and postWork fails, send reply fail.
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromSpecFailed_postWorkFailSendReplyFailed)
{
        // Create a UnixFd object
    AI_IPC::UnixFd fd1 = 123; // Assuming 123 is a valid file descriptor
    AI_IPC::VariantList validArgs_set1 = {
    std::string("1"),/*Simulate a string representing the identifier of the container*/
    std::string("2"),/*Simulate a string representing a JSON specification.*/
    std::vector<AI_IPC::UnixFd>{fd1},/*Simulate valid file descriptors*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .Times(2)
        .WillOnce(::testing::Return(validArgs_set1))
        .WillOnce(::testing::Return(validArgs_set1));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_dobbyManagerMock, startContainerFromSpec(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const ContainerId& id, const std::string& jsonSpec,
                  const std::list<int>& files, const std::string& command,
                  const std::string& displaySocket, const std::vector<std::string>& envVars,
                  const std::function<void(int32_t cd, const ContainerId& id)> containnerStartCb) {
                containnerStartCb(123, id);
                return 123;
            }));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = 123;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }))
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));

    dobby_test->startFromSpec((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for startFromSpec ends here*/
#endif //defined(LEGACY_COMPONENTS)

/****************************************************************************************************
 * Test functions for :startFromBundle
 *@brief Starts a new container from the supplied bundle path.
 * Use case coverage:
 *                @Success :2
 *                @Failure :4
 ***************************************************************************************************/
/**
 * @brief Test starting a container from a bundle with invalid arguments.
 * Check if startFromBundle method handles the case with invalid arguments.
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromBundleFailed_invalidArg)
{
    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .Times(2)
        .WillOnce(::testing::Return(AI_IPC::VariantList{1}))
                .WillOnce(::testing::Return(AI_IPC::VariantList{1,2}));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->startFromBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test starting a container from a bundle with argument size 3 and postWork failure.
 * Check if startFromBundle method handles the case when argument size is 3 and postWork fails.
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromBundleFailed_argSize_3_postWorkFail)
{
        // Create a UnixFd object
    AI_IPC::UnixFd fd1 = 123; // Assuming 123 is a valid file descriptor
    AI_IPC::VariantList validArgs_set1 = {
    std::string("1"), /*Simulate a string representing the identifier of the container*/
    std::string("2"), /*Simulate a string  representing the bundle Path */
    std::vector<AI_IPC::UnixFd>{fd1}, /*Simulate valid file descriptors*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .Times(2)
        .WillOnce(::testing::Return(validArgs_set1))
                .WillOnce(::testing::Return(validArgs_set1));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_dobbyManagerMock, startContainerFromBundle(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .Times(0);

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->startFromBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test starting a container from a bundle with argument size 3 and postWork success.
 * Check if startFromBundle method handles the case when argument size is 3 and postWork succeeds.
 * by sending back reply returned by startContainerFromBundle
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromBundleSuccess_argSize_3_postWorkSuccess)
{
        // Create a UnixFd object
    AI_IPC::UnixFd fd1 = 123; // Assuming 123 is a valid file descriptor
    AI_IPC::VariantList validArgs_set1 = {
    std::string("1"), /*Simulate a string representing the identifier of the container*/
    std::string("2"), /*Simulate a string representing the bundle path */
    std::vector<AI_IPC::UnixFd>{fd1},/*Simulate valid file descriptors*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
            .Times(2)
        .WillOnce(::testing::Return(validArgs_set1))
        .WillOnce(::testing::Return(validArgs_set1));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    /* Simulates a successful start, returning a container descriptor ,which is a unique number that identifies the container */
    EXPECT_CALL(*p_dobbyManagerMock, startContainerFromBundle(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const ContainerId& id,
            const std::string& bundlePath,
            const std::list<int>& files,
            const std::string& command,
            const std::string& displaySocket,
            const std::vector<std::string>& envVars,
            const std::function<void(int32_t cd, const ContainerId& id)> containnerStartCb) {
                containnerStartCb(12, id);
                return 12;
            }));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = 12;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->startFromBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test starting a container from a bundle with argument size 6 and postWork success.
 * Check if startFromBundle method handles the case when argument size is 6 and postWork succeeds.
 * by sending back reply returned by startContainerFromBundle
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromBundleSuccess_argSize_6_postWorkSuccess)
{
        // Create a UnixFd object
    AI_IPC::UnixFd fd1 = 123; // Assuming 123 is a valid file descriptor
    AI_IPC::VariantList validArgs_set2 = {
    std::string("1"),/*Simulate a string representing the identifier of the container*/
    std::string("2"), /*Simulate a string representing the bundle path */
    std::vector<AI_IPC::UnixFd>{fd1},/*Simulate valid file descriptors*/
    std::string("abc"),/*Simulate a string representing a command*/
    std::string("def"), /*Simulate a string representing display socket*/
    std::vector<std::string>{ "ghi" }, /*simulate a vector of strings representing env variable*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .Times(3)
        .WillOnce(::testing::Return(validArgs_set2))
        .WillOnce(::testing::Return(validArgs_set2))
                .WillOnce(::testing::Return(validArgs_set2));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_dobbyManagerMock, startContainerFromBundle(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const ContainerId& id,
                const std::string& bundlePath,
                const std::list<int>& files,
                const std::string& command,
                const std::string& displaySocket,
                const std::vector<std::string>& envVars,
                const std::function<void(int32_t cd, const ContainerId& id)> containnerStartCb) {
                containnerStartCb(12, id);
                return 12;
            }));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = 12;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->startFromBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test starting a container from a bundle with invalid containerId.
 * Check if startFromBundle method handles the case when containerId is invalid.
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromBundleFailed_ContainerIdIsValidFail)
{
    // Create a UnixFd object
    AI_IPC::UnixFd fd1 = 123; // Assuming 123 is a valid file descriptor
    AI_IPC::VariantList validArgs_set1 = {
    std::string("1"),/*Simulate a string representing the identifier of the container*/
    std::string("2"),/*Simulate a string representing a JSON specification.*/
    std::vector<AI_IPC::UnixFd>{fd1},/*Simulate valid file descriptors*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .Times(2)
        .WillOnce(::testing::Return(validArgs_set1))
        .WillOnce(::testing::Return(validArgs_set1));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(false));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->startFromBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}

/**
 * @brief Test starting a container from a bundle with argument size 3 and postWork failure, failed send reply.
 * Check if startFromBundle method handles the case when argument size is 3 and postWork fails, failed send reply.
 * by sending back reply = -1
 *
 * @return None.
 */
TEST_F(DaemonDobbyTest, startFromBundleFailed_postWorkFailSendReplyFailed)
{
        // Create a UnixFd object
    AI_IPC::UnixFd fd1 = 123; // Assuming 123 is a valid file descriptor
    AI_IPC::VariantList validArgs_set1 = {
    std::string("1"),/*Simulate a string representing the identifier of the container*/
    std::string("2"),/*Simulate a string representing a JSON specification.*/
    std::vector<AI_IPC::UnixFd>{fd1},/*Simulate valid file descriptors*/
    };

    EXPECT_CALL(*p_asyncReplySenderMock, getMethodCallArguments())
        .Times(2)
        .WillOnce(::testing::Return(validArgs_set1))
        .WillOnce(::testing::Return(validArgs_set1));

    EXPECT_CALL(*p_containerIdMock, isValid())
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_dobbyManagerMock, startContainerFromBundle(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .WillOnce(::testing::Invoke(
            [&](const ContainerId& id,
                const std::string& bundlePath,
                const std::list<int>& files,
                const std::string& command,
                const std::string& displaySocket,
                const std::vector<std::string>& envVars,
                const std::function<void(int32_t cd, const ContainerId& id)> containnerStartCb) {
                containnerStartCb(12, id);
                return 12;
            }));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = 12;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }))
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                int32_t expectedResult = -1;
                int32_t actualResult;
                if (AI_IPC::parseVariantList <int32_t>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return false;
            }));

    dobby_test->startFromBundle((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for startFromBundle ends here*/

/****************************************************************************************************
 * Test functions for :list
 * @brief Lists all the running containers
 * Use case coverage:
 *                @Success :2
 *                @Failure :2
 ***************************************************************************************************/

/**
 * @brief Test list with normal list data arguments and verify the success post work.
 * Check if exec method handles the case with valid arguments, postWork and sendReply both are success.
 *
 * @return descriptor id and container name.
 */
TEST_F(DaemonDobbyTest, list_postWorkSuccess_SendReplySuccess)
{
    std::list<std::pair<int32_t, ContainerId>> containers;
    std::string s[3] = {"container1","container2","container3"};

    containers.emplace_back (1,ContainerId::create (s[0]));
    containers.emplace_back (2,ContainerId::create (s[1]));
    containers.emplace_back (3,ContainerId::create (s[2]));

    ON_CALL(*p_dobbyManagerMock, listContainers())
        .WillByDefault(::testing::Return(containers));

    EXPECT_CALL(*p_dobbyManagerMock, listContainers())
        .Times(1)
        .WillOnce(::testing::Return(containers));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs){
                std::vector<int32_t> descriptors;
                std::vector<std::string> ids;

                if (AI_IPC::parseVariantList
                        <std::vector<int32_t>, std::vector<std::string>>
                        (replyArgs, &descriptors, &ids))
                {
                    std::vector<std::string> expected_str{"container1","container2","container3"};

                    EXPECT_EQ(descriptors.size(),3);
                    EXPECT_EQ(expected_str==ids,true);

                    for (size_t i = 0; i < descriptors.size(); i++)
                    {
                        EXPECT_EQ(descriptors[i],i+1);
                    }
                }
                return true;
            }));

    dobby_test->list((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
    containers.clear();
}

/*Test cases for startFromBundle ends here*/

/**
 * @brief Test list with empty data arguments and failed postWork.
 * Check if exec method handles the case with empty data list, and failed postWork and sendReply success.
 *
 * @return, send reply will get empty reply as post work failed
 */
TEST_F(DaemonDobbyTest, list_postWorkFailed_SendReplySuccess)
{
    std::list<std::pair<int32_t, ContainerId>> containers;
    std::string s[3];

    containers.emplace_back (1,ContainerId::create (s[0]));
    containers.emplace_back (2,ContainerId::create (s[1]));
    containers.emplace_back (3,ContainerId::create (s[2]));

    ON_CALL(*p_dobbyManagerMock, listContainers())
        .WillByDefault(::testing::Return(containers));

    EXPECT_CALL(*p_dobbyManagerMock, listContainers())
        .Times(1)
        .WillOnce(::testing::Return(containers));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::vector<int32_t> descriptors;
                std::vector<std::string> ids;

                if (AI_IPC::parseVariantList
                    <std::vector<int32_t>, std::vector<std::string>>
                    (replyArgs, &descriptors, &ids))
                {
                    EXPECT_EQ(descriptors.size(),3);
                    for (size_t i = 0; i < descriptors.size(); i++)
                    {
                        EXPECT_EQ(descriptors[i],i+1);
                        EXPECT_EQ(ids[i].empty(),true); // Verify the empty string
                    }
                }
                return true;
            }))
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                return true;
            }));

    dobby_test->list((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);

    containers.clear();
}

/**
 * @brief Test list with huge list data arguments and failed send reply.
 * Check if exec method handles the case with valid arguments, postWork success and sendReply failed.
 *
 * @return descriptor id and container name.
 */
TEST_F(DaemonDobbyTest, list_postWorkSuccess_sendReplyFailed)
{
    std::list<std::pair<int32_t, ContainerId>> containers;
    std::string s[10];

    for (size_t i = 0; i < 10; ++i)
    {
        s[i] = "container" + std::to_string(i + 1);
        containers.emplace_back(i + 1, ContainerId::create (s[i]));
    }

    ON_CALL(*p_dobbyManagerMock, listContainers())
        .WillByDefault(::testing::Return(containers));

    EXPECT_CALL(*p_dobbyManagerMock, listContainers())
        .Times(1)
        .WillOnce(::testing::Return(containers));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return true;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::vector<int32_t> descriptors;
                std::vector<std::string> ids;

                if (AI_IPC::parseVariantList
                        <std::vector<int32_t>, std::vector<std::string>>
                        (replyArgs, &descriptors, &ids))
                {
                    std::vector<std::string> expected_str(10);
                    for (size_t i = 0;i<10;i++)
                    {
                        expected_str[i] = "container" + std::to_string(i + 1);
                    }
                    EXPECT_EQ(descriptors.size(),10);
                    EXPECT_EQ(expected_str==ids,true);

                    for (size_t i = 0; i < descriptors.size(); i++)
                    {
                        EXPECT_EQ(descriptors[i],i+1);
                    }
                }
                return false;
            }));

    dobby_test->list((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
    containers.clear();
}

/**
 * @brief Test list with empty list data arguments and failed send reply.
 * Check if exec method handles the case with empty list, postWork failed and sendReply failed.
 *
 * @return, send reply will get empty reply as post work failed
 */
TEST_F(DaemonDobbyTest, list_postWorkFailed_sendReplyFailed)
{
    std::list<std::pair<int32_t, ContainerId>> containers;
    std::string s[10];

    ON_CALL(*p_dobbyManagerMock, listContainers())
        .WillByDefault(::testing::Return(containers));

    EXPECT_CALL(*p_dobbyManagerMock, listContainers())
        .Times(1)
        .WillOnce(::testing::Return(containers));

    EXPECT_CALL(*p_workQueueMock, postWork(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const WorkFunc &work) {
                work();
                return false;
            }));

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                std::vector<int32_t> descriptors;
                std::vector<std::string> ids;

                if (AI_IPC::parseVariantList
                    <std::vector<int32_t>, std::vector<std::string>>
                    (replyArgs, &descriptors, &ids))
                {
                    EXPECT_EQ(descriptors.size(),0); // list is empty
                }
                return false;
                }))
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                return false;
        }));

    dobby_test->list((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);
}
/*Test cases for list ends here*/

// -----------------------------------------------------------------------------
/**
 *  @brief Issues a 'ready' signal over dbus and then blocks until either
 *  a shutdown request is received or SIGTERM
 *
 * @brief Test run with separate thread to verify the runfor() and shutdonw().
 * Check if run method handles the separate thread and exit thread after shutdown()
 *
 * @return None
 */
TEST_F(DaemonDobbyTest, run_success)
{
    EXPECT_CALL(*p_ipcServiceMock, emitSignal(::testing::_,::testing::_))
        .Times(1);

    EXPECT_CALL(*p_workQueueMock, runFor(::testing::_))
        .Times(::testing::AtLeast(1));

    std::thread runWorkQueueThread([&] {
        // Start the run() method in a separate thread
        dobby_test->run();
    });

    // Allow some time for the runFor() to execute
    std::this_thread::sleep_for(std::chrono::seconds(1));

    EXPECT_CALL(*p_workQueueMock, exit())
        .Times(1);

    EXPECT_CALL(*p_asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                bool expectedResult = false;
                bool actualResult;
                if (AI_IPC::parseVariantList <bool>
                         (replyArgs, &actualResult))
                {
                    EXPECT_EQ(actualResult, expectedResult);
                }
                return true;
            }));

    dobby_test->shutdown((std::shared_ptr<AI_IPC::IAsyncReplySender>)p_iasyncReplySender);

    // Wait for the thread to finish
    runWorkQueueThread.join();

}

// -----------------------------------------------------------------------------
/**
 *  @brief Debugging function for manually setting the AI dbus addresses
 *
 *
 *  @param[in]  aiPrivateBusAddress     The AI private dbus address
 *  @param[in]  aiPublicBusAddress      The AI public dbus address
 *
 * @brief Test setDefaultAIDbusAddresses with valid AI bus address.
 * Check if setDefaultAIDbusAddresses method set valid AI bus address.
 */
TEST_F(DaemonDobbyTest, setDefaultAIDbusAddresses_success)
{
    std::string aiPrivateBusAddress("/mnt/nds/tmpfs/APPLICATIONS_WORKSPACE/dbus/socket/private/serverfd");
    std::string aiPublicBusAddress("/mnt/nds/tmpfs/APPLICATIONS_WORKSPACE/dbus/socket/public/serverfd");

    EXPECT_CALL(*p_IPCUtilsMock, setAIDbusAddress(::testing::_,::testing::_))
        .Times(2)
        .WillOnce(::testing::Invoke(
            [](bool privateBus, const std::string &address) {
                return true;
            }))
        .WillOnce(::testing::Invoke(
            [](bool privateBus, const std::string &address) {
                return true;
            }));

    dobby_test->setDefaultAIDbusAddresses(aiPrivateBusAddress,aiPublicBusAddress);
}
