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
#include "DobbyTemplateMock.h"
#include "DobbyEnvMock.h"
#include "DobbyManagerMock.h"
#include "DobbyIPCUtilsMock.h"
#include "DobbyUtilsMock.h"

DobbyWorkQueueImpl* DobbyWorkQueue::impl = nullptr;
DobbyManagerImpl*   DobbyManager::impl = nullptr;
DobbyIPCUtilsImpl*  DobbyIPCUtils::impl = nullptr;
DobbyTemplateImpl*  DobbyTemplate::impl = nullptr;
DobbyUtilsImpl*  DobbyUtils::impl = nullptr;
AI_IPC::IIpcServiceImpl* AI_IPC::IIpcService::impl = nullptr;
AI_IPC::IAsyncReplySenderApiImpl* AI_IPC::IAsyncReplySender::impl = nullptr;
AI_IPC::IpcFileDescriptorApiImpl* AI_IPC::IpcFileDescriptor::impl = nullptr;

using ::testing::NiceMock;
class DaemonDobbyTest : public ::testing::Test{

protected:
    NiceMock<AI_IPC::IAsyncReplySenderMock> asyncReplySenderMock ;
    NiceMock<AI_IPC::IpcFileDescriptorMock> ipcFileDescriptorMock ;
    NiceMock<DobbyWorkQueueMock> workQueueMock ;
    NiceMock<DobbyTemplateMock> templateMock ;
    NiceMock<AI_IPC::IpcServiceMock> ipcServiceMock ;
    DobbyTemplate*dobbyTemplate = DobbyTemplate::getInstance();
    AI_IPC::IIpcService *ipcService = AI_IPC::IIpcService::getInstance();
    DobbyWorkQueue *workQueue = DobbyWorkQueue::getInstance();
    AI_IPC::IAsyncReplySender *iasyncReplySender = AI_IPC::IAsyncReplySender::getInstance();
    std::shared_ptr<AI_IPC::IpcFileDescriptor> iIpcFileDescriptor = std::make_shared<AI_IPC::IpcFileDescriptor>();
    std::shared_ptr<const IDobbySettings> settingsMock = std::make_shared<NiceMock<DobbySettingsMock>>();
    std::string dbusAddress = "unix:path=/some/socket";

    std::shared_ptr<Dobby> test ;
    DaemonDobbyTest()
    {
        iasyncReplySender->setImpl(&asyncReplySenderMock);
        iIpcFileDescriptor->setImpl(&ipcFileDescriptorMock);
        ipcService->setImpl(&ipcServiceMock);
        workQueue->setImpl(&workQueueMock);
        dobbyTemplate->setImpl(&templateMock);

        test = std::make_shared<NiceMock<Dobby>>(dbusAddress, (std::shared_ptr<AI_IPC::IIpcService>)ipcService, settingsMock);
    }
    virtual ~DaemonDobbyTest() override
    {

        ON_CALL(ipcServiceMock, unregisterHandler(::testing::_))
        .WillByDefault(::testing::Return(true));

        test.reset();
        DobbyWorkQueue::setImpl(nullptr);
        iasyncReplySender->setImpl(nullptr);
        iIpcFileDescriptor->setImpl(nullptr);
        dobbyTemplate->setImpl(nullptr);
        ipcService->setImpl(nullptr);
    }
};

/**
 * @brief Test shutdown on successful reply sending.
 * Check if shutdown method is successfully completed after a successful sendReply.
 * 
 * @param[in] shared pointer to iasyncReplySender.
 * @return None.
 */
TEST_F(DaemonDobbyTest, shutdownSuccess_sendReplySuccess)
{
    EXPECT_CALL(workQueueMock, exit())
    .Times(1);

    EXPECT_CALL(asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                return true;
            }));
    test->shutdown((std::shared_ptr<AI_IPC::IAsyncReplySender>)iasyncReplySender);
}

/**
 * @brief Test shutdown on failed reply sending.
 * Check if shutdown method is successfully completed after a failed sendReply.
 *
 * @param[in] shared pointer to iasyncReplySender.
 * @return None.
 */
 TEST_F(DaemonDobbyTest, shutdownSuccess_sendReplyFailed)
{
    EXPECT_CALL(workQueueMock, exit())
    .Times(1);

    EXPECT_CALL(asyncReplySenderMock, sendReply(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const AI_IPC::VariantList& replyArgs) {
                return false;
            }));
    test->shutdown((std::shared_ptr<AI_IPC::IAsyncReplySender>)iasyncReplySender);
}
