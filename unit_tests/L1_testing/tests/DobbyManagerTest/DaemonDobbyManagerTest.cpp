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

#include <gtest/gtest.h>
#include "DobbyStreamMock.h"
#include "DobbyRdkPluginUtilsMock.h"
#include "DobbyLoggerMock.h"
#include "DobbyRunCMock.h"
#include "DobbyManager.h"
#include "DobbyContainerMock.h"
#include "DobbyRdkPluginManagerMock.h"
#include "DobbyStartStateMock.h"
#include "DobbyRootfsMock.h"
#include "DobbySpecConfigMock.h"
#include "DobbyBundleMock.h"
#include "DobbyConfigMock.h"
#include "DobbyBundleConfigMock.h"
#include "IDobbySettingsMock.h"
#include "IDobbyIPCUtilsMock.h"
#include "IDobbyUtilsMock.h"
#include "IDobbyEnvMock.h"
#include "IAsyncReplySenderMock.h"
#include "ContainerIdMock.h"
#include "DobbyFileAccessFixerMock.h"
#include "DobbyLegacyPluginManagerMock.h"
#include "DobbyStats.h"

DobbyContainerImpl* DobbyContainer::impl = nullptr;
DobbyRdkPluginManagerImpl* DobbyRdkPluginManager::impl = nullptr;
DobbyRootfsImpl* DobbyRootfs::impl = nullptr;
DobbySpecConfigImpl* DobbySpecConfig::impl = nullptr;
DobbyBundleImpl* DobbyBundle::impl = nullptr;
DobbyConfigImpl* DobbyConfig::impl = nullptr;
DobbyBundleConfigImpl* DobbyBundleConfig::impl = nullptr;
DobbyRdkPluginUtilsImpl* DobbyRdkPluginUtils::impl = nullptr;
AI_IPC::IAsyncReplySenderApiImpl* AI_IPC::IAsyncReplySender::impl = nullptr;
DobbyLegacyPluginManagerImpl* DobbyLegacyPluginManager::impl = nullptr;
ContainerIdImpl* ContainerId::impl = nullptr;
DobbyBufferStreamImpl *DobbyBufferStream::impl = nullptr;
DobbyFileAccessFixerImpl *DobbyFileAccessFixer::impl = nullptr;
DobbyRunCImpl *DobbyRunC::impl = nullptr;
DobbyStatsImpl *DobbyStats::impl = nullptr;
DobbyLoggerImpl *DobbyLogger::impl = nullptr;
AI_IPC::IpcFileDescriptorApiImpl* AI_IPC::IpcFileDescriptor::impl = nullptr;
DobbyStartStateImpl *DobbyStartState::impl = nullptr;
using ::testing::NiceMock;

class DaemonDobbyManagerTest : public ::testing::Test {

protected:

    NiceMock<DobbyContainerMock> containerMock ;
    NiceMock<DobbyRdkPluginManagerMock> rdkPluginManagerMock ;
    NiceMock<DobbyStartStateMock> startStateMock ;
    NiceMock<DobbyRootfsMock> rootfsMock ;
    NiceMock<DobbySpecConfigMock> specConfigMock ;
    NiceMock<DobbyBundleMock> bundleMock ;
    NiceMock<DobbyConfigMock> configMock ;
    NiceMock<DobbyBundleConfigMock> bundleConfigMock ;
    NiceMock<DobbyRdkPluginUtilsMock> rdkPluginUtilsMock ;
    NiceMock<AI_IPC::IAsyncReplySenderMock> asyncReplySenderMock ;

    DobbyContainer*dobbyContainer = DobbyContainer::getInstance();
    DobbyRdkPluginManager*rdkPluginManager = DobbyRdkPluginManager::getInstance();
    DobbyRootfs*rootfs = DobbyRootfs::getInstance();
    DobbySpecConfig*specConfig = DobbySpecConfig::getInstance();
    DobbyBundle*bundle = DobbyBundle::getInstance();
    DobbyConfig*config = DobbyConfig::getInstance();
    DobbyBundleConfig*bundleConfig = DobbyBundleConfig::getInstance();
    DobbyRdkPluginUtils*rdkPluginUtils = DobbyRdkPluginUtils::getInstance();

    DaemonDobbyManagerTest()
    {
        dobbyContainer->setImpl(&containerMock);
        rdkPluginManager->setImpl(&rdkPluginManagerMock);
        rootfs->setImpl(&rootfsMock);
        specConfig->setImpl(&specConfigMock);
        bundle->setImpl(&bundleMock);
        config->setImpl(&configMock);
        bundleConfig->setImpl(&bundleConfigMock);
        rdkPluginUtils->setImpl(&rdkPluginUtilsMock);
    }

    virtual ~DaemonDobbyManagerTest() override
    {
        dobbyContainer->setImpl(nullptr);
        rdkPluginManager->setImpl(nullptr);
        rootfs->setImpl(nullptr);
        specConfig->setImpl(nullptr);
        bundle->setImpl(nullptr);
        config->setImpl(nullptr);
        bundleConfig->setImpl(nullptr);
        rdkPluginUtils->setImpl(nullptr);
    }

};


