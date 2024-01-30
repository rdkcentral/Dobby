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
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <fstream>
#include <unordered_map>
#include <chrono>
#include <thread>

#include "DobbyRdkPluginUtilsMock.h"
#include "DobbyLoggerMock.h"

#include "DobbyRunCMock.h"
#include "DobbyUtilsMock.h"

#include "DobbyManager.h"
#include "DobbyContainerMock.h"
#include "DobbyRdkPluginManagerMock.h"
#include "DobbyStartStateMock.h"
#include "DobbyRootfsMock.h"

#if defined(LEGACY_COMPONENTS)
#include "DobbySpecConfigMock.h"
#endif //defined(LEGACY_COMPONENTS)

#include "DobbyBundleMock.h"
#include "DobbyConfigMock.h"
#include "DobbyBundleConfigMock.h"
#include "DobbySettingsMock.h"
#include "DobbyIPCUtilsMock.h"
#include "DobbyEnvMock.h"
#include "IAsyncReplySenderMock.h"
#include "DobbyStatsMock.h"
#include "DobbyFileAccessFixerMock.h"
#include "DobbyRunCMock.h"
#include "DobbyLegacyPluginManagerMock.h"
#include "DobbyStreamMock.h"
#include "ContainerIdMock.h"
#include "IDobbyRdkLoggingPluginMock.h"
#include "DobbyProtocol.h"

#define MAX_TIMEOUT_CONTAINER_STARTED (5000) /* 5sec */
#define LIST_CONTAINERS_HUGE_COUNT 8

DobbyContainerImpl* DobbyContainer::impl = nullptr;
DobbyRdkPluginManagerImpl* DobbyRdkPluginManager::impl = nullptr;
DobbyRootfsImpl* DobbyRootfs::impl = nullptr;

#if defined(LEGACY_COMPONENTS)
DobbySpecConfigImpl* DobbySpecConfig::impl = nullptr;
#endif //defined(LEGACY_COMPONENTS)

DobbyBundleImpl* DobbyBundle::impl = nullptr;
DobbyConfigImpl* DobbyConfig::impl = nullptr;
DobbyBundleConfigImpl* DobbyBundleConfig::impl = nullptr;
DobbyRdkPluginUtilsImpl* DobbyRdkPluginUtils::impl = nullptr;
DobbyStartStateImpl* DobbyStartState::impl = nullptr;
AI_IPC::IAsyncReplySenderApiImpl* AI_IPC::IAsyncReplySender::impl = nullptr;
ContainerIdImpl* ContainerId::impl = nullptr;
DobbyFileAccessFixerImpl* DobbyFileAccessFixer::impl = nullptr;
DobbyLoggerImpl* DobbyLogger::impl = nullptr;
DobbyRunCImpl* DobbyRunC::impl = nullptr;
DobbyBufferStreamImpl* DobbyBufferStream::impl = nullptr;
DobbyLegacyPluginManagerImpl* DobbyLegacyPluginManager::impl = nullptr;
DobbyStatsImpl* DobbyStats::impl = nullptr;
AI_IPC::IpcFileDescriptorApiImpl* AI_IPC::IpcFileDescriptor::impl = nullptr;
DobbyIPCUtilsImpl* DobbyIPCUtils::impl = nullptr;
DobbyUtilsImpl* DobbyUtils::impl = nullptr;


using ::testing::NiceMock;

class DaemonDobbyManagerTest : public ::testing::Test {

protected:

    void onContainerStarted(int32_t cd, const ContainerId& id);

    void onContainerStopped(int32_t cd, const ContainerId& id, int status);

    void onContainerHibernated(int32_t cd, const ContainerId& id);

    void onContainerAwoken(int32_t cd, const ContainerId& id);

    bool waitForContainerStarted(int32_t timeout_ms);

    bool waitForContainerStopped(int32_t timeout_ms);

    typedef std::function<void(int32_t cd, const ContainerId& id)> ContainerStartedFunc;
    typedef std::function<void(int32_t cd, const ContainerId& id, int32_t status)> ContainerStoppedFunc;
    typedef std::function<void(int32_t cd, const ContainerId& id)> ContainerHibernatedFunc;
    std::function<bool()> Test_invalidContainerCleanupTask;

    ContainerStartedFunc startcb =
        std::bind(&DaemonDobbyManagerTest::onContainerStarted, this,
                  std::placeholders::_1, std::placeholders::_2);

    ContainerStoppedFunc stopcb =
        std::bind(&DaemonDobbyManagerTest::onContainerStopped, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);

    ContainerHibernatedFunc hibernatedCb  =
        std::bind(&DaemonDobbyManagerTest::onContainerHibernated, this,
                  std::placeholders::_1, std::placeholders::_2);

    ContainerHibernatedFunc awokenCb  =
        std::bind(&DaemonDobbyManagerTest::onContainerAwoken, this,
                  std::placeholders::_1, std::placeholders::_2);

    public:
        std::mutex m_mutex;
        std::condition_variable m_condition_variable;
        bool m_containerStarted = false;
        bool m_containerStopped = false;

        //Initializing pointers to mock objects
        DobbyContainerMock*  p_containerMock = nullptr;
        DobbyRdkPluginManagerMock*  p_rdkPluginManagerMock = nullptr;
        DobbyStartStateMock*  p_startStateMock = nullptr;
        DobbyRootfsMock*  p_rootfsMock = nullptr;
        #if defined(LEGACY_COMPONENTS)
        DobbySpecConfigMock*  p_specConfigMock = nullptr;
        #endif //defined(LEGACY_COMPONENTS)
        DobbyBundleMock*  p_bundleMock = nullptr;
        DobbyConfigMock*  p_configMock = nullptr;
        DobbyBundleConfigMock*  p_bundleConfigMock = nullptr;
        DobbyRdkPluginUtilsMock*  p_rdkPluginUtilsMock = nullptr;
        AI_IPC::IAsyncReplySenderMock*  p_asyncReplySenderMock = nullptr;
        ContainerIdMock*  p_containerIdMock = nullptr;
        DobbyFileAccessFixerMock*  p_fileAccessFixerMock = nullptr;
        DobbyRunCMock*  p_runcMock = nullptr;
        DobbyStreamMock*  p_streamMock = nullptr;
        DobbyLegacyPluginManagerMock *  p_legacyPluginManagerMock = nullptr;
        DobbyStatsMock*  p_statsMock = nullptr;
        DobbyLoggerMock*  p_loggerMock = nullptr;
        DobbyEnvMock*  p_envMock = nullptr;
        DobbyIPCUtilsMock*  p_ipcutilsMock = nullptr;
        DobbyUtilsMock*  p_utilsMock = nullptr;

        DobbyBundle *p_bundle = nullptr;
        DobbyBundleConfig *p_bundleConfig = nullptr;
        DobbyFileAccessFixer *p_fileAccessFixer = nullptr;
        DobbyBufferStream *p_stream = nullptr;
        DobbyLegacyPluginManager *p_legacyPluginManager = nullptr;
        DobbyStats *p_stats = nullptr;
        DobbyLogger *p_logger = nullptr;
        std::shared_ptr<const IDobbySettings>  p_dobbysettingsMock =  nullptr;

        std::shared_ptr<DobbyManager> dobbyManager_test;

        virtual void SetUp()
        {
            //setting up the mocks
            p_containerMock = new NiceMock <DobbyContainerMock>;
            p_rdkPluginManagerMock = new NiceMock <DobbyRdkPluginManagerMock>;
            p_startStateMock = new NiceMock <DobbyStartStateMock>;
            p_rootfsMock = new NiceMock <DobbyRootfsMock>;

            #if defined(LEGACY_COMPONENTS)
            p_specConfigMock = new NiceMock <DobbySpecConfigMock>;
            #endif //defined(LEGACY_COMPONENTS)

            p_bundleMock = new NiceMock <DobbyBundleMock>;
            p_configMock = new NiceMock <DobbyConfigMock>;
            p_bundleConfigMock = new NiceMock <DobbyBundleConfigMock>;
            p_rdkPluginUtilsMock = new NiceMock <DobbyRdkPluginUtilsMock>;
            p_asyncReplySenderMock = new NiceMock <AI_IPC::IAsyncReplySenderMock>;
            p_containerIdMock= new NiceMock <ContainerIdMock>;
            p_fileAccessFixerMock = new NiceMock <DobbyFileAccessFixerMock>;
            p_runcMock = new NiceMock <DobbyRunCMock>;
            p_streamMock = new NiceMock <DobbyStreamMock>;
            p_legacyPluginManagerMock = new NiceMock <DobbyLegacyPluginManagerMock>;
            p_statsMock = new NiceMock <DobbyStatsMock>;
            p_loggerMock = new NiceMock <DobbyLoggerMock>;
            p_envMock = new NiceMock <DobbyEnvMock>;
            p_ipcutilsMock = new NiceMock <DobbyIPCUtilsMock>;
            p_utilsMock = new NiceMock <DobbyUtilsMock>;

            DobbyContainer::setImpl(p_containerMock);
            DobbyRdkPluginManager::setImpl(p_rdkPluginManagerMock);
            DobbyRootfs::setImpl(p_rootfsMock);
            DobbyStartState::setImpl(p_startStateMock);

            #if defined(LEGACY_COMPONENTS)
            DobbySpecConfig::setImpl(p_specConfigMock);
            #endif //defined(LEGACY_COMPONENTS)

            DobbyBundle::setImpl(p_bundleMock);
            DobbyConfig::setImpl(p_configMock);
            DobbyBundleConfig::setImpl(p_bundleConfigMock);
            DobbyRdkPluginUtils::setImpl(p_rdkPluginUtilsMock);
            AI_IPC::IAsyncReplySender::setImpl(p_asyncReplySenderMock);
            ContainerId::setImpl(p_containerIdMock);
            DobbyFileAccessFixer::setImpl(p_fileAccessFixerMock);
            DobbyLogger::setImpl(p_loggerMock);
            DobbyRunC::setImpl(p_runcMock);
            DobbyBufferStream::setImpl(p_streamMock);
            DobbyLegacyPluginManager::setImpl(p_legacyPluginManagerMock);
            DobbyStats::setImpl(p_statsMock);
            DobbyEnv::setImpl(p_envMock);
            DobbyIPCUtils::setImpl(p_ipcutilsMock);
            DobbyUtils::setImpl(p_utilsMock);
            p_dobbysettingsMock =  std::make_shared<NiceMock<DobbySettingsMock>>();

           const std::shared_ptr<DobbyEnv> p_env = std::make_shared<DobbyEnv>(p_dobbysettingsMock);
           const std::shared_ptr<DobbyUtils> p_utils = std::make_shared<DobbyUtils>();
           const std::shared_ptr<DobbyIPCUtils> p_ipcutils = std::make_shared<DobbyIPCUtils>("dobbymanager",nullptr);

           EXPECT_CALL(*p_utilsMock,writeTextFile(::testing::_, ::testing::_, ::testing::_, ::testing::_))
               .Times(1)
               .WillOnce(::testing::Return(true));

           const std::string expectedWorkDir = "tests/L1_testing/tests";
           EXPECT_CALL(*p_runcMock, getWorkingDir())
               .Times(2)
               .WillRepeatedly(::testing::Return(expectedWorkDir));

           int32_t cd = 4444;
           ContainerId id = ContainerId::create("UnknownContainer");
           DobbyRunC::ContainerListItem container = { id, 1234, "/path/to/bundle",DobbyRunC::ContainerStatus::Unknown };
           std::list<DobbyRunC::ContainerListItem> containers;

           containers.emplace_back(container);

           EXPECT_CALL(*p_runcMock, list())
               .Times(1)
               .WillOnce(::testing::Return(containers));

           EXPECT_CALL(*p_containerMock, allocDescriptor())
               .Times(1)
               .WillOnce(::testing::Return(cd));

           ON_CALL(*p_utilsMock, startTimerImpl(::testing::_,::testing::_,::testing::_))
               .WillByDefault(::testing::Invoke(
                   [this](const std::chrono::microseconds& timeout,
                       bool oneShot,
                      const std::function<bool()>& handler) {
                      Test_invalidContainerCleanupTask = handler;
           return 123456;
           }));

           dobbyManager_test = std::make_shared<NiceMock<DobbyManager>>(p_env,p_utils,p_ipcutils,p_dobbysettingsMock,startcb,stopcb,hibernatedCb,awokenCb);
        }

        virtual void TearDown()
        {
            dobbyManager_test.reset();

            DobbyContainer::setImpl(nullptr);
            DobbyRdkPluginManager::setImpl(nullptr);
            DobbyRootfs::setImpl(nullptr);
            DobbyStartState::setImpl(nullptr);

            #if defined(LEGACY_COMPONENTS)
            DobbySpecConfig::setImpl(nullptr);
            #endif //defined(LEGACY_COMPONENTS)

            DobbyBundle::setImpl(nullptr);
            DobbyConfig::setImpl(nullptr);
            DobbyBundleConfig::setImpl(nullptr);
            DobbyRdkPluginUtils::setImpl(nullptr);
            AI_IPC::IAsyncReplySender::setImpl(nullptr);
            ContainerId::setImpl(nullptr);
            DobbyFileAccessFixer::setImpl(nullptr);
            DobbyLogger::setImpl(nullptr);
            DobbyRunC::setImpl(nullptr);
            DobbyBufferStream::setImpl(nullptr);
            DobbyLegacyPluginManager::setImpl(nullptr);
            DobbyStats::setImpl(nullptr);
            DobbyEnv::setImpl(nullptr);
            DobbyIPCUtils::setImpl(nullptr);
            DobbyUtils::setImpl(nullptr);

            p_dobbysettingsMock.reset();

            if( p_rdkPluginManagerMock != nullptr)
            {
                delete  p_rdkPluginManagerMock;
                p_rdkPluginManagerMock = nullptr;
            }

            if( p_startStateMock != nullptr)
            {
                delete  p_startStateMock;
                p_startStateMock = nullptr;
            }

            if( p_rootfsMock != nullptr)
            {
                delete  p_rootfsMock;
                p_rootfsMock = nullptr;
            }

        #if defined(LEGACY_COMPONENTS)
            if( p_specConfigMock != nullptr)
            {
                delete  p_specConfigMock;
                p_specConfigMock = nullptr;
            }
        #endif //defined(LEGACY_COMPONENTS)

            if( p_bundleMock != nullptr)
            {
                delete  p_bundleMock;
                p_bundleMock = nullptr;
            }

            if( p_configMock != nullptr)
            {
                delete  p_configMock;
                p_configMock = nullptr;
            }

            if( p_bundleConfigMock != nullptr)
            {
                delete  p_bundleConfigMock;
                p_bundleConfigMock = nullptr;
            }

            if( p_rdkPluginUtilsMock != nullptr)
            {
                delete  p_rdkPluginUtilsMock;
                p_rdkPluginUtilsMock = nullptr;
            }

            if( p_asyncReplySenderMock != nullptr)
            {
                delete  p_asyncReplySenderMock;
                p_asyncReplySenderMock = nullptr;
            }

            if( p_containerIdMock != nullptr)
            {
                delete  p_containerIdMock;
                p_containerIdMock = nullptr;
            }

            if( p_fileAccessFixerMock != nullptr)
            {
                delete  p_fileAccessFixerMock;
                p_fileAccessFixerMock = nullptr;
            }

            if( p_loggerMock != nullptr)
            {
                delete  p_loggerMock;
                p_loggerMock = nullptr;
            }

            if( p_runcMock != nullptr)
            {
                delete  p_runcMock;
                p_runcMock = nullptr;
            }

            if( p_streamMock != nullptr)
            {
                delete  p_streamMock;
                p_streamMock = nullptr;
            }

            if( p_legacyPluginManagerMock != nullptr)
            {
                delete  p_legacyPluginManagerMock;
                p_legacyPluginManagerMock = nullptr;
            }

            if( p_statsMock != nullptr)
            {
                delete  p_statsMock;
                p_statsMock = nullptr;
            }

            if(p_containerMock != nullptr)
            {
                delete  p_containerMock;
                p_containerMock = nullptr;
            }

            if(p_envMock != nullptr)
            {
                delete  p_envMock;
                p_envMock = nullptr;
            }

            if(p_ipcutilsMock != nullptr)
            {
                delete  p_ipcutilsMock;
                p_ipcutilsMock = nullptr;
            }

            if(p_utilsMock != nullptr)
            {
                delete  p_utilsMock;
                p_utilsMock = nullptr;
            }


        }

        void expect_startContainerFromBundle(int32_t cd, ContainerId &id)
        {
            EXPECT_CALL(*p_bundleConfigMock, isValid())
                .Times(1)
                    .WillOnce(::testing::Return(true));

            EXPECT_CALL(*p_bundleMock, isValid())
                .Times(1)
                    .WillOnce(::testing::Return(true));

            EXPECT_CALL(*p_rootfsMock, isValid())
                .Times(1)
                   .WillOnce(::testing::Return(true));

            EXPECT_CALL(*p_startStateMock, isValid())
                .Times(1)
                    .WillOnce(::testing::Return(true));

            std::map<std::string, Json::Value> sampleData;
            sampleData["plugin1"] = Json::Value("value1");
            sampleData["plugin2"] = Json::Value("value2");

        // Set the expectation to return the sample data
            EXPECT_CALL(*p_bundleConfigMock, rdkPlugins())
                .Times(1)
                    .WillOnce(::testing::ReturnRef(sampleData));

            EXPECT_CALL(*p_containerMock, allocDescriptor())
                .Times(1)
                    .WillOnce(::testing::Return(cd));

            const std::string validPath = "/tests/L1_testing/tests/";

        // Set the expectation to return the valid path
            EXPECT_CALL(*p_rootfsMock, path())
                 .Times(4)
                    .WillRepeatedly(::testing::ReturnRef(validPath));

            std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest";

            EXPECT_CALL(*p_bundleMock, path())
                .Times(4)
                    .WillOnce(::testing::ReturnRef(valid_path))
                    .WillOnce(::testing::ReturnRef(valid_path))

                    .WillOnce(::testing::ReturnRef(valid_path))
                    .WillOnce(::testing::ReturnRef(valid_path));

            EXPECT_CALL(*p_bundleConfigMock, config())
                .Times(2)
                    .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()))
                    .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()));

            const std::vector<std::string> expectedStrings = {"plugin1", "plugin2", "plugin3"};

            EXPECT_CALL(*p_rdkPluginManagerMock, listLoadedPlugins())
                .Times(1)
                    .WillOnce(::testing::Return(expectedStrings));

            std::map<std::string, Json::Value> data;
            data["key1"] = Json::Value("value1");
            data["key2"] = Json::Value("value2");

            EXPECT_CALL(*p_bundleConfigMock, legacyPlugins())
                .Times(3)
                    .WillRepeatedly(testing::ReturnRef(data));

            EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
                        return true;
            }));

            EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_))
                .Times(2)
                    .WillOnce(::testing::Invoke(
                    [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                        return true;
            }))
                    .WillOnce(::testing::Invoke(
                    [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                        return true;
            }));

            EXPECT_CALL(*p_configMock, writeConfigJson(::testing::_))
                .Times(2)
                    .WillOnce(::testing::Invoke(
                    [](const std::string& filePath) {
                        return true;
            }))

                    .WillOnce(::testing::Invoke(
                    [](const std::string& filePath) {
                        return true;
            }));

            EXPECT_CALL(*p_startStateMock, files())
                .Times(1)
                    .WillOnce(::testing::Return(std::list<int>{1, 2, 3}));

            EXPECT_CALL(*p_rdkPluginManagerMock, getContainerLogger())
                .Times(1)
                    .WillOnce(::testing::Return(std::make_shared<IDobbyRdkLoggingPluginMock>()));

            pid_t pid1 = 1234;
            pid_t pid2 = 5678;

            EXPECT_CALL(*p_legacyPluginManagerMock, executePreStartHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
                        return true;
             }));

            EXPECT_CALL(*p_legacyPluginManagerMock, executePostStartHooks(::testing::_, ::testing::_,::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
                        return true;
            }));

            EXPECT_CALL(*p_runcMock, create(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [pid1, pid2](const ContainerId &id,const std::shared_ptr<const DobbyBundle> &bundle,const std::shared_ptr<const IDobbyStream> &console,const std::list<int> &files = std::list<int>(),const std::string& customConfigPath) {
                        return std::make_pair(pid1, pid2);
            }));

            EXPECT_CALL(*p_loggerMock, DumpBuffer(::testing::_,::testing::_,::testing::_))
                .Times(2)
                .WillRepeatedly(::testing::Invoke(
                [](int bufferMemFd,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin){
                    return true;
            }));

            EXPECT_CALL(*p_runcMock, start(::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) {
                        return true;
            }));

            EXPECT_CALL(*p_loggerMock, StartContainerLogging(::testing::_,::testing::_,::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](std::string containerId,pid_t runtimePid,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin){
                        return true;
            }));

            const std::string bundlePath = "/path/to/bundle";
            const std::list<int> files = {1, 2, 3};
            const std::string command = "ls -l";
            const std::string displaySocket = "/tmp/display";
            std::vector<std::string> envVars;
            envVars = {"PATH=/usr/bin", "HOME=/home/user"};

            int result = dobbyManager_test->startContainerFromBundle(id,bundlePath,files,command,displaySocket,envVars);

            EXPECT_EQ(result, cd);

            EXPECT_TRUE(waitForContainerStarted(MAX_TIMEOUT_CONTAINER_STARTED));

        }

#ifdef LEGACY_COMPONENTS
        void expect_startContainerFromSpec(int32_t cd)
        {
            EXPECT_CALL(*p_bundleMock, isValid())
                .Times(1)
                    .WillOnce(::testing::Return(true));

            EXPECT_CALL(*p_specConfigMock, isValid())
                .Times(1)
                    .WillOnce(::testing::Return(true));

            EXPECT_CALL(*p_rootfsMock, isValid())
                .Times(1)
                    .WillOnce(::testing::Return(true));

            EXPECT_CALL(*p_startStateMock, isValid())
                .Times(1)
                    .WillOnce(::testing::Return(true));

            std::map<std::string, Json::Value> sampleData;
            sampleData["plugin1"] = Json::Value("value1");
            sampleData["plugin2"] = Json::Value("value2");

            // Set the expectation to return the sample data
            EXPECT_CALL(*p_specConfigMock, rdkPlugins())
                .Times(2)
                    .WillOnce(::testing::ReturnRef(sampleData))
                    .WillOnce(::testing::ReturnRef(sampleData));

            EXPECT_CALL(*p_containerMock, allocDescriptor())
                .Times(1)
                    .WillOnce(::testing::Return(cd));

            const std::string validPath = "/tests/L1_testing/tests/";

        // Set the expectation to return the valid path
            EXPECT_CALL(*p_rootfsMock, path())
                .Times(6)
                    .WillRepeatedly(::testing::ReturnRef(validPath));

            std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest";
            EXPECT_CALL(*p_bundleMock, path())
                .Times(2)
                    .WillRepeatedly(::testing::ReturnRef(valid_path));

            EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_))
                .Times(2)
                    .WillRepeatedly(::testing::Invoke(
                     [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                         return true;
            }));

            EXPECT_CALL(*p_specConfigMock, config())
                .Times(2)
                    .WillRepeatedly(::testing::Return(std::make_shared<rt_dobby_schema>()));

            const std::vector<std::string> expectedStrings = {"plugin1", "plugin2", "plugin3"};

            EXPECT_CALL(*p_rdkPluginManagerMock, listLoadedPlugins())
                .Times(1)
                    .WillOnce(::testing::Return(expectedStrings));

            EXPECT_CALL(*p_configMock, writeConfigJson(::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const std::string& filePath) {
                        return true;
            }));

            EXPECT_CALL(*p_specConfigMock, restartOnCrash()).WillOnce(::testing::Return(true));

            EXPECT_CALL(*p_containerMock, setRestartOnCrash(::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                        [](const std::list<int>& files) {
            }));

            EXPECT_CALL(*p_startStateMock, files())
                .Times(2)
                    .WillRepeatedly(::testing::Return(std::list<int>{1, 2, 3}));

            std::map<std::string, Json::Value> data;
            data["key1"] = Json::Value("value1");
            data["key2"] = Json::Value("value2");
            EXPECT_CALL(*p_specConfigMock, legacyPlugins())
                .Times(5)
                    .WillRepeatedly(testing::ReturnRef(data));

            EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
                        return true;
            }));

            EXPECT_CALL(*p_legacyPluginManagerMock, executePreStartHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
               .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
                        return true;
            }));

            EXPECT_CALL(*p_legacyPluginManagerMock, executePostStartHooks(::testing::_, ::testing::_,::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
                        return true;
            }));

            EXPECT_CALL(*p_legacyPluginManagerMock, executePostStopHooks(::testing::_, ::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                        return true;
             }));

            EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_, ::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                        return true;
            }));

            EXPECT_CALL(*p_rdkPluginManagerMock, getContainerLogger())
                .Times(2)
                    .WillOnce(::testing::Return(std::make_shared<IDobbyRdkLoggingPluginMock>()))
                    .WillOnce(::testing::Return(std::make_shared<IDobbyRdkLoggingPluginMock>()));

            pid_t pid1 = 1234;
            pid_t pid2 = 5678;
            EXPECT_CALL(*p_runcMock, create(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [pid1, pid2](const ContainerId &id,const std::shared_ptr<const DobbyBundle> &bundle,const std::shared_ptr<const IDobbyStream> &console,const std::list<int> &files = std::list<int>(),const std::string& customConfigPath) {
                        return std::make_pair(pid1, pid2);
            }));

            EXPECT_CALL(*p_runcMock, start(::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) {
                        return true;
            }));

            EXPECT_CALL(*p_streamMock, getMemFd())
                .Times(3)
                    .WillRepeatedly(::testing::Return(123));

            EXPECT_CALL(*p_loggerMock, DumpBuffer(::testing::_,::testing::_,::testing::_))
                .Times(3)
                    .WillRepeatedly(::testing::Invoke(
                    [](int bufferMemFd,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin){
                        return true;
            }));

            EXPECT_CALL(*p_loggerMock, StartContainerLogging(::testing::_,::testing::_,::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](std::string containerId,pid_t runtimePid,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin){
                        return true;
            }));

            EXPECT_CALL(*p_runcMock, killCont(::testing::_,::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                    [](const ContainerId &id, int signal, bool all) {
                         return true;
            }));

            EXPECT_CALL(*p_runcMock, destroy(::testing::_,::testing::_,::testing::_))
                .Times(1)
                    .WillOnce(::testing::Invoke(
                     [](const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console, bool force) {
                         return true;
            }));

            ContainerId id = ContainerId::create("container_123");
            std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
            std::list<int> files = {1, 2, 3};//file descriptors
            std::string command ="ls -l";
            std::string displaySocket = "/tmp/display";
            std::vector<std::string> envVars;
            envVars = {"PATH=/usr/bin", "HOME=/home/user"};

            int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);
            EXPECT_EQ(result, cd);
            EXPECT_TRUE(waitForContainerStarted(MAX_TIMEOUT_CONTAINER_STARTED));

        }
#endif //defined(LEGACY_COMPONENTS)

        /*Unknown container is added in SetUp(), so every test should call this function to remove unknown container */
        void expect_invalidContainerCleanupTask()
        {
            ASSERT_NE(Test_invalidContainerCleanupTask,nullptr);
            EXPECT_CALL(*p_runcMock, destroy(::testing::_,::testing::_,::testing::_))
                .Times(1)
                .WillOnce(::testing::Return(true));

            Test_invalidContainerCleanupTask();
        }

        void expect_stopContainerOnCleanup()
        {
            EXPECT_CALL(*p_runcMock, killCont(::testing::_,::testing::_,::testing::_))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(::testing::Invoke(
                    [](const ContainerId &id, int signal, bool all) {
                        return true;
                    }));
        }

        void expect_handleContainerTerminate()
        {
            std::map<std::string, Json::Value> data;
            data["key1"] = Json::Value("value1");
            data["key2"] = Json::Value("value2");

            EXPECT_CALL(*p_legacyPluginManagerMock, executePostStopHooks(::testing::_,::testing::_,::testing::_))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(::testing::Return(true));

            EXPECT_CALL(*p_bundleConfigMock, legacyPlugins())
                .Times(testing::AtLeast(2))
                .WillRepeatedly(testing::ReturnRef(data));

            std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest/DaemonDobbyManagerTest.cpp";
            EXPECT_CALL(*p_rootfsMock, path())
                .Times(testing::AtLeast(2))
                .WillRepeatedly(::testing::ReturnRef(valid_path));

            EXPECT_CALL(*p_containerMock, shouldRestart(::testing::_))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(::testing::Return(false));

            EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_,::testing::_,::testing::_))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(::testing::Return(true));

            std::map<std::string, Json::Value> sampleData;
            sampleData["plugin1"] = Json::Value("value1");
            sampleData["plugin2"] = Json::Value("value2");

            /* Set the expectation to return the sample data */
            EXPECT_CALL(*p_bundleConfigMock, rdkPlugins())
                .Times(testing::AtLeast(1))
                .WillRepeatedly(::testing::ReturnRef(sampleData));

            EXPECT_CALL(*p_rdkPluginManagerMock, setExitStatus(::testing::_)).Times(testing::AtLeast(1));

            EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_,::testing::_))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(::testing::Return(true));

            EXPECT_CALL(*p_runcMock, destroy(::testing::_,::testing::_,::testing::_))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(::testing::Return(true));

            EXPECT_CALL(*p_rdkPluginManagerMock, getContainerLogger())
                .Times(testing::AtLeast(1))
                .WillRepeatedly(::testing::Return(std::make_shared<IDobbyRdkLoggingPluginMock>()));

            EXPECT_CALL(*p_loggerMock, DumpBuffer(::testing::_,::testing::_,::testing::_))
                .Times(testing::AtLeast(1))
                .WillRepeatedly(::testing::Return(true));

            EXPECT_CALL(*p_streamMock, getMemFd())
                .Times(testing::AtLeast(1))
                .WillRepeatedly(::testing::Return(123));

        }

        void expect_cleanupContainersShutdown()
        {
            expect_stopContainerOnCleanup();
            expect_handleContainerTerminate();
        }

        void expect_stopContainerSuccess(int32_t containerState)
        {
            if (containerState == CONTAINER_STATE_PAUSED)
            {
                EXPECT_CALL(*p_runcMock,resume(::testing::_))
                    .Times(1)
                    .WillOnce(::testing::Invoke(
                        [](const ContainerId &id){
                            return true;
                    }));

                EXPECT_CALL(*p_runcMock, killCont(::testing::_,::testing::_,::testing::_))
                    .Times(1)
                    .WillOnce(::testing::Invoke(
                        [](const ContainerId &id, int signal, bool all) {
                            return true;
                    }));
            }
            else if (containerState == CONTAINER_STATE_RUNNING)
            {
                EXPECT_CALL(*p_runcMock, killCont(::testing::_,::testing::_,::testing::_))
                    .Times(1)
                    .WillOnce(::testing::Invoke(
                        [](const ContainerId &id, int signal, bool all) {
                            return true;
                    }));
            }
            else if (containerState == CONTAINER_STATE_INVALID)
            {
                return;
            }
        }

        void expect_stopContainerFailedToResumeFromPausedState()
        {
            EXPECT_CALL(*p_runcMock,resume(::testing::_))
                .Times(1)
                .WillOnce(::testing::Invoke(
                    [](const ContainerId &id){
                        return false;
                }));
        }

        void expect_stopContainerFailedToKillContainer()
        {
            EXPECT_CALL(*p_runcMock, killCont(::testing::_,::testing::_,::testing::_))
                .Times(1)
                .WillOnce(::testing::Invoke(
                    [](const ContainerId &id, int signal, bool all) {
                        return false;
                }));
        }

        void expect_pauseContainerSuccess()
        {
            ContainerId id = ContainerId::create("container1");
            EXPECT_CALL(*p_runcMock,pause(id))
                .Times(1)
                .WillOnce(::testing::Invoke(
                [](const ContainerId &id){
                    return true;
                }));
        }

        void expect_pauseContainerFailed()
        {
            ContainerId id = ContainerId::create("container1");
            EXPECT_CALL(*p_runcMock,pause(id))
                .Times(1)
                .WillOnce(::testing::Invoke(
                [](const ContainerId &id){
                    return false;
                }));
        }

        void expect_resumeContainer_sucess(ContainerId &id)
        {
            EXPECT_CALL(*p_runcMock,resume(id))
                .Times(1)
                .WillOnce(::testing::Invoke(
                [](const ContainerId &id){
                     return true;
                }));
        }

        void expect_resumeContainer_failed(ContainerId &id)
        {
            EXPECT_CALL(*p_runcMock,resume(id))
                .Times(1)
                .WillOnce(::testing::Invoke(
                [](const ContainerId &id){
                    return false;
                }));
        }
};

        void DaemonDobbyManagerTest::onContainerStarted(int32_t cd, const ContainerId& id) {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_containerStarted = true;
            m_condition_variable.notify_one();

        }

        bool DaemonDobbyManagerTest::waitForContainerStarted(int32_t timeout_ms) {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::system_clock::now();
            std::chrono::milliseconds timeout(timeout_ms);

            if (!m_containerStarted) {
                if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
                    std::cout << "Timeout waiting for container start." << std::endl;
                    return false;
                }
            }
            return true;
        }

        void DaemonDobbyManagerTest::onContainerStopped(int32_t cd, const ContainerId& id, int status) {

            std::unique_lock<std::mutex> lock(m_mutex);
            m_containerStopped = true;
            m_condition_variable.notify_one();

        }

        bool DaemonDobbyManagerTest::waitForContainerStopped(int32_t timeout_ms) {
            std::unique_lock<std::mutex> lock(m_mutex);
            auto now = std::chrono::system_clock::now();
            std::chrono::milliseconds timeout(timeout_ms);

            if (!m_containerStopped) {
                if (m_condition_variable.wait_until(lock, now + timeout) == std::cv_status::timeout) {
                    std::cout << "Timeout waiting for container stop." << std::endl;
                    return false;
                }
            }
            return true;
        }

        void DaemonDobbyManagerTest::onContainerHibernated(int32_t cd, const ContainerId& id) {
        }

        void DaemonDobbyManagerTest::onContainerAwoken(int32_t cd, const ContainerId& id) {
        }

#if defined(LEGACY_COMPONENTS)

/****************************************************************************************************
 *  @brief Where the magic begins .... attempts to create a container
 *  from a Dobby spec file.
 *
 *  @param[in]  id          The id string for the container
 *  @param[in]  jsonSpec    The sky json spec with the container details
 *  @param[in]  files       A list of file descriptors to pass into the
 *                          container, can be empty.
 *  @param[in]  command     The custom command to run instead of the args in the
 *                          config file (optional)
 *
 *  @return a container descriptor, which is just a unique number that
 *  identifies the container.

 * Use case coverage:
 *                @Success :2
 *                @Failure :7
 ***************************************************************************************************/

/**
 * @brief Test startContainerFromSpec with valid inputs and with Rdk plugins
 * returns a container descriptor, which is just a unique number that
 *  identifies the container.
*/

TEST_F(DaemonDobbyManagerTest, startContainerFromSpec_ValidInputs)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
        .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
        .WillOnce(::testing::Return(true));

    std::map<std::string, Json::Value> sampleData;
    sampleData["plugin1"] = Json::Value("value1");
    sampleData["plugin2"] = Json::Value("value2");

    // Set the expectation to return the sample data
    EXPECT_CALL(*p_specConfigMock, rdkPlugins())
        .Times(2)
        .WillOnce(::testing::ReturnRef(sampleData))
        .WillOnce(::testing::ReturnRef(sampleData));

    int32_t cd = 123;
    EXPECT_CALL(*p_containerMock, allocDescriptor())
        .Times(1)
        .WillOnce(::testing::Return(cd));

    const std::string validPath = "/tests/L1_testing/tests/";

    // Set the expectation to return the valid path
    EXPECT_CALL(*p_rootfsMock, path())
        .Times(6)
        .WillRepeatedly(::testing::ReturnRef(validPath));

    std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest";
    EXPECT_CALL(*p_bundleMock, path())
        .Times(2)
        .WillRepeatedly(::testing::ReturnRef(valid_path));

    EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_))
        .Times(2)
        .WillRepeatedly(::testing::Invoke(
         [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
             return true;
    }));

    EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_,::testing::_))
        .Times(1)
        .WillRepeatedly(::testing::Invoke(
         [](const IDobbyRdkPlugin::HintFlags &hookPoint,const uint timeoutMs) {
             return true;
    }));

    EXPECT_CALL(*p_specConfigMock, config())
        .Times(2)
        .WillRepeatedly(::testing::Return(std::make_shared<rt_dobby_schema>()));

    const std::vector<std::string> expectedStrings = {"plugin1", "plugin2", "plugin3"};

    EXPECT_CALL(*p_rdkPluginManagerMock, listLoadedPlugins())
        .Times(1)
        .WillOnce(::testing::Return(expectedStrings));

    EXPECT_CALL(*p_configMock, writeConfigJson(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [](const std::string& filePath) {
            return true;
    }));

    EXPECT_CALL(*p_specConfigMock, restartOnCrash()).WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_containerMock, setRestartOnCrash(::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [](const std::list<int>& files) {
    }));

    EXPECT_CALL(*p_startStateMock, files())
        .Times(2)
        .WillRepeatedly(::testing::Return(std::list<int>{1, 2, 3}));

    std::map<std::string, Json::Value> data;
    data["key1"] = Json::Value("value1");
    data["key2"] = Json::Value("value2");
    EXPECT_CALL(*p_specConfigMock, legacyPlugins())
        .Times(5)
        .WillRepeatedly(testing::ReturnRef(data));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
            return true;
    }));


    EXPECT_CALL(*p_legacyPluginManagerMock, executePreStartHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
            return true;
    }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostStartHooks(::testing::_, ::testing::_,::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
            return true;
    }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostStopHooks(::testing::_, ::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
            return true;
    }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_, ::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
            return true;
    }));

    EXPECT_CALL(*p_rdkPluginManagerMock, getContainerLogger())
        .Times(2)
        .WillOnce(::testing::Return(std::make_shared<IDobbyRdkLoggingPluginMock>()))
        .WillOnce(::testing::Return(std::make_shared<IDobbyRdkLoggingPluginMock>()));

    pid_t pid1 = 1234;
    pid_t pid2 = 5678;
    EXPECT_CALL(*p_runcMock, create(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [pid1, pid2](const ContainerId &id,const std::shared_ptr<const DobbyBundle> &bundle,const std::shared_ptr<const IDobbyStream> &console,const std::list<int> &files = std::list<int>(),const std::string& customConfigPath) {
            return std::make_pair(pid1, pid2);
    }));

    EXPECT_CALL(*p_runcMock, start(::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [](const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) {
            return true;
    }));

    EXPECT_CALL(*p_streamMock, getMemFd())
        .Times(3)
        .WillRepeatedly(::testing::Return(123));

    EXPECT_CALL(*p_loggerMock, DumpBuffer(::testing::_,::testing::_,::testing::_))
        .Times(3)
        .WillRepeatedly(::testing::Invoke(
        [](int bufferMemFd,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin){
            return true;
    }));

    EXPECT_CALL(*p_loggerMock, StartContainerLogging(::testing::_,::testing::_,::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [](std::string containerId,pid_t runtimePid,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin){
            return true;
    }));

    EXPECT_CALL(*p_runcMock, killCont(::testing::_,::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Invoke(
        [](const ContainerId &id, int signal, bool all) {
             return true;
    }));

    EXPECT_CALL(*p_runcMock, destroy(::testing::_,::testing::_,::testing::_))
        .Times(1)
         .WillOnce(::testing::Invoke(
         [](const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console, bool force) {
             return true;
    }));

    ContainerId id = ContainerId::create("container_123");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
    std::list<int> files = {1, 2, 3};//file descriptors
    std::string command ="ls -l";
    std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);
    EXPECT_EQ(result, cd);
    EXPECT_TRUE(waitForContainerStarted(MAX_TIMEOUT_CONTAINER_STARTED));

}


/**
 * @brief Test startContainerFromSpec with valid inputs and without Rdk plugins
 * returns a container descriptor, which is just a unique number that
 *  identifies the container.
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromSpec_SuccessWithoutRdkPlugins)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    std::map<std::string, Json::Value> emptyMap;

// Set up the mock behavior for rdkPlugins() to return the empty map
    EXPECT_CALL(*p_specConfigMock, rdkPlugins())
        .Times(2)
            .WillOnce(::testing::ReturnRef(emptyMap))
            .WillOnce(::testing::ReturnRef(emptyMap));

    int32_t cd = 123;
    EXPECT_CALL(*p_containerMock, allocDescriptor())
        .Times(1)
        .WillOnce(::testing::Return(cd));

    const std::string validPath = "/tests/L1_testing/tests/";

// Set the expectation to return the valid path
    EXPECT_CALL(*p_rootfsMock, path())
        .Times(5)
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath));

    std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest";
    EXPECT_CALL(*p_bundleMock, path())
        .Times(1)
            .WillOnce(::testing::ReturnRef(valid_path));

    EXPECT_CALL(*p_configMock, writeConfigJson(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::string& filePath) {
                return true;
            }));

    EXPECT_CALL(*p_startStateMock, files())
        .Times(1)
            .WillOnce(::testing::Return(std::list<int>{1, 2, 3}));

    std::map<std::string, Json::Value> data;
    data["key1"] = Json::Value("value1");
    data["key2"] = Json::Value("value2");
    EXPECT_CALL(*p_specConfigMock, legacyPlugins())
        .Times(5)
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
                return true;
            }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreStartHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
                return true;
            }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostStartHooks(::testing::_, ::testing::_,::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
                return true;
            }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostStopHooks(::testing::_, ::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
        }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_, ::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
        }));

    pid_t pid1 = 1234;
    pid_t pid2 = 5678;
    EXPECT_CALL(*p_runcMock, create(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [pid1, pid2](const ContainerId &id,const std::shared_ptr<const DobbyBundle> &bundle,const std::shared_ptr<const IDobbyStream> &console,const std::list<int> &files = std::list<int>(),const std::string& customConfigPath) {
                return std::make_pair(pid1, pid2);
            }));

    EXPECT_CALL(*p_runcMock, start(::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) {
                return true;
            }));

        EXPECT_CALL(*p_runcMock, killCont(::testing::_,::testing::_,::testing::_))
            .Times(1)
             .WillOnce(::testing::Invoke(
             [](const ContainerId &id, int signal, bool all) {
                 return true;
        }));

        EXPECT_CALL(*p_runcMock, destroy(::testing::_,::testing::_,::testing::_))
            .Times(1)
             .WillOnce(::testing::Invoke(
             [](const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console, bool force) {
                 return true;
        }));

    ContainerId id = ContainerId::create("container_123");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
    std::list<int> files = {1, 2, 3};//file descriptors
    std::string command ="ls -l";
    std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);

    EXPECT_EQ(result, cd);

    EXPECT_TRUE(waitForContainerStarted(MAX_TIMEOUT_CONTAINER_STARTED));

}


/**
 * @brief Test startContainerFromSpec where bundle is not created
 * returns  (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromSpec_InvalidBundleCreation)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(false));

    ContainerId id = ContainerId::create("container_123");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
    std::list<int> files = {1, 2, 3};//file descriptors
    std::string command ="ls -l";
    std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromSpec config object is not created from OCI bundle config
 * returns  (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromSpec_InvalidConfigObject)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(false));

    ContainerId id = ContainerId::create("container_123");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
    std::list<int> files = {1, 2, 3};//file descriptors
    std::string command ="ls -l";
    std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromSpec where rootfs is not created
 * returns  (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromSpec_InvalidRootfsCreation)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(false));

    ContainerId id = ContainerId::create("container_123");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
    std::list<int> files = {1, 2, 3};
    std::string command ="ls -l";
    std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromSpec where startstate object is not created
 * returns  (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromSpec_InvalidStartStateObject)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(false));

    ContainerId id = ContainerId::create("container_123");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
    std::list<int> files = {1, 2, 3};//file descriptors
    std::string command ="ls -l";
    std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromSpec with onPostConstructionHookFailure
 * returns  (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromSpec_onPostConstructionHookFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    std::map<std::string, Json::Value> sampleData;
    sampleData["plugin1"] = Json::Value("value1");
    sampleData["plugin2"] = Json::Value("value2");

// Set the expectation to return the sample data
    EXPECT_CALL(*p_specConfigMock, rdkPlugins())
        .Times(1)
            .WillOnce(::testing::ReturnRef(sampleData));

    int32_t cd = 123;
    EXPECT_CALL(*p_containerMock, allocDescriptor())
        .Times(1)
            .WillOnce(::testing::Return(cd));

    const std::string validPath = "/tests/L1_testing/tests/";

// Set the expectation to return the valid path
    EXPECT_CALL(*p_rootfsMock, path())
        .Times(3)
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath));

    EXPECT_CALL(*p_specConfigMock, config())
        .Times(2)
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()))
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()));

    const std::vector<std::string> expectedStrings = {"plugin", "plugin", "plugin"};

    EXPECT_CALL(*p_rdkPluginManagerMock, listLoadedPlugins())
        .Times(1)
            .WillOnce(::testing::Return(expectedStrings));

    std::map<std::string, Json::Value> data;
    data["key1"] = Json::Value("value1");
    data["key2"] = Json::Value("value2");

    EXPECT_CALL(*p_specConfigMock, legacyPlugins())
        .Times(2)
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
                return false;
            }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
            }));

    ContainerId id = ContainerId::create("container_123");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
    std::list<int> files = {1, 2, 3};//file descriptors
    std::string command ="ls -l";
    std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}

/**
 * @brief Test startContainerFromSpec with config json failure
 * returns  (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromSpec_WriteConfigJsonFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    std::map<std::string, Json::Value> sampleData;
    sampleData["plugin1"] = Json::Value("value1");
    sampleData["plugin2"] = Json::Value("value2");

// Set the expectation to return the sample data
    EXPECT_CALL(*p_specConfigMock, rdkPlugins())
        .Times(1)
            .WillOnce(::testing::ReturnRef(sampleData));

    const std::string validPath = "/tests/L1_testing/tests/";

    int32_t cd = 123;
    EXPECT_CALL(*p_containerMock, allocDescriptor())
        .Times(1)
            .WillOnce(::testing::Return(cd));

// Set the expectation to return the valid path
    EXPECT_CALL(*p_rootfsMock, path())
        .Times(3)
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath));

    std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest";

    EXPECT_CALL(*p_bundleMock, path())
        .Times(2)
            .WillOnce(::testing::ReturnRef(valid_path))
            .WillOnce(::testing::ReturnRef(valid_path));


    EXPECT_CALL(*p_specConfigMock, config())
        .Times(2)
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()))
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()));

    const std::vector<std::string> expectedStrings = {"plugin1", "plugin2", "plugin3"};

    EXPECT_CALL(*p_rdkPluginManagerMock, listLoadedPlugins())
        .Times(1)
            .WillOnce(::testing::Return(expectedStrings));

    std::map<std::string, Json::Value> data;
    data["key1"] = Json::Value("value1");
    data["key2"] = Json::Value("value2");

    EXPECT_CALL(*p_specConfigMock, legacyPlugins())
        .Times(2)
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
                return true;
            }));

    EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_))
        .Times(2)
            .WillOnce(::testing::Invoke(
            [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                return true;
    }))
            .WillOnce(::testing::Invoke(
            [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                return true;
    }));

    EXPECT_CALL(*p_configMock, writeConfigJson(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::string& filePath) {
                return false;
            }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
            }));

    ContainerId id = ContainerId::create("container_123");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
    std::list<int> files = {1, 2, 3};//file descriptors
    std::string command ="ls -l";
    std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromSpec with container that is already running.
 * returns  (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromSpec_FailedAsContainerAlreadyRunning)
{
    expect_invalidContainerCleanupTask();

    ContainerId id1 = ContainerId::create("container_123");
    expect_startContainerFromBundle(123,id1);

    ContainerId id = ContainerId::create("container_123");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
    std::list<int> files = {1, 2, 3};//file descriptors
    std::string command ="ls -l";
    std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}

/**
 * @brief Test startContainerFromSpec with failure in createAndStart contianer
 * returns  (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromSpec_CreateAndStartContainerFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    std::map<std::string, Json::Value> sampleData;
    sampleData["plugin1"] = Json::Value("value1");
    sampleData["plugin2"] = Json::Value("value2");

// Set the expectation to return the sample data
    EXPECT_CALL(*p_specConfigMock, rdkPlugins())
        .Times(2)
            .WillOnce(::testing::ReturnRef(sampleData))

            .WillOnce(::testing::ReturnRef(sampleData));

    int32_t cd = 123;
    EXPECT_CALL(*p_containerMock, allocDescriptor())
        .Times(1)
            .WillOnce(::testing::Return(cd));

    const std::string validPath = "/tests/L1_testing/tests/";

    EXPECT_CALL(*p_rootfsMock, path())
        .Times(5)
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))

            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath));

    std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest";

    EXPECT_CALL(*p_bundleMock, path())
        .Times(2)
            .WillOnce(::testing::ReturnRef(valid_path))
            .WillOnce(::testing::ReturnRef(valid_path));

    EXPECT_CALL(*p_specConfigMock, config())
        .Times(2)
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()))
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()));

    const std::vector<std::string> expectedStrings = {"plugin1", "plugin2", "plugin3"};

    EXPECT_CALL(*p_rdkPluginManagerMock, listLoadedPlugins())
        .Times(1)
            .WillOnce(::testing::Return(expectedStrings));

    std::map<std::string, Json::Value> data;
    data["key1"] = Json::Value("value1");
    data["key2"] = Json::Value("value2");

    EXPECT_CALL(*p_specConfigMock, legacyPlugins())
        .Times(4)
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
                return true;
            }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostStopHooks(::testing::_, ::testing::_,::testing::_))
            .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
        }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_, ::testing::_,::testing::_))
            .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
        }));


    EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_))
        .Times(2)
            .WillOnce(::testing::Invoke(
            [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                return true;
    }))
            .WillOnce(::testing::Invoke(
            [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                return true;
    }));

    EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_,::testing::_))
            .Times(1)
             .WillRepeatedly(::testing::Invoke(
             [](const IDobbyRdkPlugin::HintFlags &hookPoint,const uint timeoutMs) {
                 return true;
        }));


    EXPECT_CALL(*p_configMock, writeConfigJson(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::string& filePath) {
                return true;
    }));

    EXPECT_CALL(*p_rdkPluginManagerMock, getContainerLogger())
        .Times(2)
            .WillOnce(::testing::Return(nullptr))
            .WillOnce(::testing::Return(nullptr));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreStartHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
                return true;
    }));

    EXPECT_CALL(*p_runcMock, start(::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) {
                return false;
    }));

    EXPECT_CALL(*p_runcMock, destroy(::testing::_,::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(true));

    ContainerId id = ContainerId::create("container_123");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 42}";
    std::list<int> files = {1, 2, 3};//file descriptors
    std::string command ="ls -l";
    std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromSpec(id,jsonSpec,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);
}

/*Test cases for startContainerFromSpec ends here*/

/****************************************************************************************************
 * Test functions for :createBundle
 *  @brief Debugging method to allow you to create a bundle with rootfs and
 *  config.json without actually running runc on it.
 *
 *
 *  @param[in]  id          The id of the new bundle to create.
 *  @param[in]  jsonSpec    The spec file to use to generate the rootfs and
 *                          config.json within the bundle.
 *
 *  @return true on success, false on failure.

  * Use case coverage:
 *                @Success :1
 *                @Failure :3

 ***************************************************************************************************/

/**
 * @brief Test createBundle with valid inputs
 * returns true if the bundle is create is created successfully
 * otherwise returns false.
 */

TEST_F(DaemonDobbyManagerTest, createBundle_Success)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest";
    EXPECT_CALL(*p_bundleMock, path())
        .Times(1)
            .WillOnce(::testing::ReturnRef(valid_path));

    EXPECT_CALL(*p_configMock, writeConfigJson(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::string& filePath) {
                return true;
            }));

    EXPECT_CALL(*p_rootfsMock, setPersistence(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](bool persist) {
            }));

    EXPECT_CALL(*p_bundleMock, setPersistence(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](bool persist) {
            }));

    ContainerId id = ContainerId::create("container_111");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 44}";

    bool result = dobbyManager_test->createBundle(id,jsonSpec);

    EXPECT_EQ(result, true);

}

/**
 * @brief Test createBundle with failure in bundle
 * returns false
 */

TEST_F(DaemonDobbyManagerTest, createBundle_BundleFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(false));

    ContainerId id = ContainerId::create("container_111");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 44}";

    bool result = dobbyManager_test->createBundle(id,jsonSpec);

    EXPECT_EQ(result, false);

}

/**
 * @brief Test createBundle with failure in config object creation.
 * returns false
 */

TEST_F(DaemonDobbyManagerTest, createBundle_CreateConfigObjectFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(false));

    ContainerId id = ContainerId::create("container_111");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 44}";

    bool result = dobbyManager_test->createBundle(id,jsonSpec);

    EXPECT_EQ(result, false);

}

/**
 * @brief Test createBundle with failure in rootfs creation
 * returns false
 */

TEST_F(DaemonDobbyManagerTest, createBundle_RootfsCreationFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_specConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(false));

    ContainerId id = ContainerId::create("container_111");
    std::string jsonSpec = "{\"key\": \"value\", \"number\": 44}";

    bool result = dobbyManager_test->createBundle(id,jsonSpec);

    EXPECT_EQ(result, false);

}

#endif //defined(LEGACY_COMPONENTS)

/*Test cases for createBundle ends here*/

/****************************************************************************************************
 * Test functions for :startContainerFromBundle
 *  @brief Where the magic begins ... attempts to create a container from
 *         an OCI bundle*
 *
 *
 *  @param[in]  id          The id string for the container
 *  @param[in]  bundlePath  The absolute path to the OCI bundle*
 *  @param[in]  files       A list of file descriptors to pass into the
 *                          container, can be empty.
 * @param[in]   command     The custom command to run instead of the args in the
 *                          config file (optional)
 *
 *  @return a container descriptor, which is just a unique number that
 *  identifies the container.

  * Use case coverage:
 *                @Success :2
 *                @Failure :8

 ***************************************************************************************************/

/**
 * @brief Test startContainerFromBundle with failure in config object creation
 * returns (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromBundle_CreateConfigObjectFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(false));

    const ContainerId id = ContainerId::create("test_container_123");
    const std::string bundlePath = "/path/to/bundle";
    const std::list<int> files = {1, 2, 3};//file descriptors
    const std::string command = "ls -l";
    const std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromBundle(id,bundlePath,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromBundle with failure in bundle creation
 * returns (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromBundle_DobbyBundleFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(false));

    const ContainerId id = ContainerId::create("test_container_123");
    const std::string bundlePath = "/path/to/bundle";
    const std::list<int> files = {1, 2, 3};//file descriptors
    const std::string command = "ls -l";
    const std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromBundle(id,bundlePath,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromBundle with failure in rootfs creation
 * returns (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromBundle_RootfsCreationFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(false));

    const ContainerId id = ContainerId::create("test_container_123");
    const std::string bundlePath = "/path/to/bundle";
    const std::list<int> files = {1, 2, 3};//file descriptors
    const std::string command = "ls -l";
    const std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromBundle(id,bundlePath,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromBundle with failure in startstate object creation
 * returns (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromBundle_StartStateObjectFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(false));

    const ContainerId id = ContainerId::create("test_container_123");
    const std::string bundlePath = "/path/to/bundle";
    const std::list<int> files = {1, 2, 3};//file descriptors
    const std::string command = "ls -l";
    const std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromBundle(id,bundlePath,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromBundle with failure in postConstructionHook
 * returns (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromBundle_onPostConstructionHookFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    std::map<std::string, Json::Value> sampleData;
    sampleData["plugin1"] = Json::Value("value1");
    sampleData["plugin2"] = Json::Value("value2");

// Set the expectation to return the sample data
    EXPECT_CALL(*p_bundleConfigMock, rdkPlugins())
        .Times(1)
            .WillOnce(::testing::ReturnRef(sampleData));

    int32_t cd = 123;
    EXPECT_CALL(*p_containerMock, allocDescriptor())
        .Times(1)
            .WillOnce(::testing::Return(cd));

    const std::string validPath = "/tests/L1_testing/tests/";

// Set the expectation to return the valid path
    EXPECT_CALL(*p_rootfsMock, path())
        .Times(3)
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath));

    EXPECT_CALL(*p_bundleConfigMock, config())
        .Times(2)
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()))
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()));

    const std::vector<std::string> expectedStrings = {"plugin1", "plugin2", "plugin3"};

    EXPECT_CALL(*p_rdkPluginManagerMock, listLoadedPlugins())
        .Times(1)
            .WillOnce(::testing::Return(expectedStrings));

    std::map<std::string, Json::Value> data;
    data["key1"] = Json::Value("value1");
    data["key2"] = Json::Value("value2");

    EXPECT_CALL(*p_bundleConfigMock, legacyPlugins())
        .Times(2)
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
                return false;
            }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
            }));

    const ContainerId id = ContainerId::create("test_container_123");
    const std::string bundlePath = "/path/to/bundle";
    const std::list<int> files = {1, 2, 3};//file descriptors
    const std::string command = "ls -l";
    const std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromBundle(id,bundlePath,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromBundle with failure in configJsonFile creation
 * returns (-1)
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromBundle_ConfigJsonFileCreationFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    std::map<std::string, Json::Value> sampleData;
    sampleData["plugin1"] = Json::Value("value1");
    sampleData["plugin2"] = Json::Value("value2");

// Set the expectation to return the sample data
    EXPECT_CALL(*p_bundleConfigMock, rdkPlugins())
        .Times(1)
            .WillOnce(::testing::ReturnRef(sampleData));

    int32_t cd = 123;
    EXPECT_CALL(*p_containerMock, allocDescriptor())
        .Times(1)
            .WillOnce(::testing::Return(cd));

    const std::string validPath = "/tests/L1_testing/tests/";

// Set the expectation to return the valid path
    EXPECT_CALL(*p_rootfsMock, path())
        .Times(3)
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath));

    std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest";

    EXPECT_CALL(*p_bundleMock, path())
        .Times(2)
            .WillOnce(::testing::ReturnRef(valid_path))
            .WillOnce(::testing::ReturnRef(valid_path));

    EXPECT_CALL(*p_bundleConfigMock, config())
        .Times(2)
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()))
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()));

    const std::vector<std::string> expectedStrings = {"plugin1", "plugin2", "plugin3"};

    EXPECT_CALL(*p_rdkPluginManagerMock, listLoadedPlugins())
        .Times(1)
            .WillOnce(::testing::Return(expectedStrings));

    std::map<std::string, Json::Value> data;
    data["key1"] = Json::Value("value1");
    data["key2"] = Json::Value("value2");

    EXPECT_CALL(*p_bundleConfigMock, legacyPlugins())
        .Times(2)
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
                return true;
            }));

    EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_))
        .Times(2)
            .WillOnce(::testing::Invoke(
            [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                return true;
    }))
            .WillOnce(::testing::Invoke(
            [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                return true;
    }));

    EXPECT_CALL(*p_configMock, writeConfigJson(::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::string& filePath) {
                return false;
            }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
            }));

    const ContainerId id = ContainerId::create("test_container_123");
    const std::string bundlePath = "/path/to/bundle";
    const std::list<int> files = {1, 2, 3};//file descriptors
    const std::string command = "ls -l";
    const std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromBundle(id,bundlePath,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromBundle with valid inputs and Rdk plugins
 * returns descriptor(cd) which is an integer.
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromBundle_ValidInputs)
{
    expect_invalidContainerCleanupTask();

    ContainerId id = ContainerId::create("container1");
    expect_startContainerFromBundle(123,id);

    expect_cleanupContainersShutdown();
}


/**
 * @brief Test startContainerFromBundle using containerId that is already running
 * returns (-1)
 */


TEST_F(DaemonDobbyManagerTest, startContainerFromBundle_FailedAsContainerAlreadyRunning)
{
    expect_invalidContainerCleanupTask();

    ContainerId id1 = ContainerId::create("container_123");
    expect_startContainerFromBundle(123,id1);

    const ContainerId id = ContainerId::create("container_123");
    const std::string bundlePath = "/path/to/bundle";
    const std::list<int> files = {1, 2, 3};//file descriptors
    const std::string command = "ls -l";
    const std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromBundle(id,bundlePath,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}


/**
 * @brief Test startContainerFromBundle of valid inputs and without Rdk plugins
 * returns decsriptor (cd) an integer value
 */

TEST_F(DaemonDobbyManagerTest, startContainerFromBundle_SuccessWithoutRdkPlugins)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    std::map<std::string, Json::Value> emptyMap;
    EXPECT_CALL(*p_bundleConfigMock, rdkPlugins())
        .Times(2)
            .WillOnce(::testing::ReturnRef(emptyMap))
            .WillOnce(::testing::ReturnRef(emptyMap));

    int32_t cd = 123;
    EXPECT_CALL(*p_containerMock, allocDescriptor())
        .Times(1)
            .WillOnce(::testing::Return(cd));

    const std::string validPath = "/tests/L1_testing/tests/";

// Set the expectation to return the valid path
    EXPECT_CALL(*p_rootfsMock, path())
        .Times(5)
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath));

    std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest";
    EXPECT_CALL(*p_bundleMock, path())
        .Times(2)
            .WillOnce(::testing::ReturnRef(valid_path))
            .WillOnce(::testing::ReturnRef(valid_path));

    EXPECT_CALL(*p_configMock, writeConfigJson(::testing::_))
        .Times(2)
            .WillOnce(::testing::Invoke(
            [](const std::string& filePath) {
                return true;
            }))

            .WillOnce(::testing::Invoke(
            [](const std::string& filePath) {
                return true;
    }));

    std::map<std::string, Json::Value> data;
    data["key1"] = Json::Value("value1");
    data["key2"] = Json::Value("value2");

    EXPECT_CALL(*p_bundleConfigMock, legacyPlugins())
        .Times(5)
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
                return true;
    }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreStartHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
                return true;
    }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostStartHooks(::testing::_, ::testing::_,::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
                return true;
    }));

        EXPECT_CALL(*p_legacyPluginManagerMock, executePostStopHooks(::testing::_, ::testing::_,::testing::_))
            .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
        }));

        EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_, ::testing::_,::testing::_))
            .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
        }));


    pid_t pid1 = 1234;
    pid_t pid2 = 5678;
    EXPECT_CALL(*p_runcMock, create(::testing::_,::testing::_,::testing::_,::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [pid1, pid2](const ContainerId &id,const std::shared_ptr<const DobbyBundle> &bundle,const std::shared_ptr<const IDobbyStream> &console,const std::list<int> &files = std::list<int>(),const std::string& customConfigPath) {
                return std::make_pair(pid1, pid2);
    }));

    EXPECT_CALL(*p_runcMock, start(::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) {
                return true;
    }));

        EXPECT_CALL(*p_runcMock, killCont(::testing::_,::testing::_,::testing::_))
            .Times(1)
             .WillOnce(::testing::Invoke(
             [](const ContainerId &id, int signal, bool all) {
                 return true;
        }));

        EXPECT_CALL(*p_runcMock, destroy(::testing::_,::testing::_,::testing::_))
            .Times(1)
             .WillOnce(::testing::Invoke(
             [](const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console, bool force) {
                 return true;
        }));

    const ContainerId id = ContainerId::create("container_123");
    const std::string bundlePath = "/path/to/bundle";
    const std::list<int> files = {1, 2, 3};//file descriptors
    const std::string command = "ls -l";
    const std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    int result = dobbyManager_test->startContainerFromBundle(id,bundlePath,files,command,displaySocket,envVars);

    EXPECT_EQ(result, cd);

    EXPECT_TRUE(waitForContainerStarted(MAX_TIMEOUT_CONTAINER_STARTED));

}

TEST_F(DaemonDobbyManagerTest, startContainerFromBundle_CreateAndStartContainerFailure)
{
    expect_invalidContainerCleanupTask();

    EXPECT_CALL(*p_bundleConfigMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_bundleMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_rootfsMock, isValid())
        .Times(1)
           .WillOnce(::testing::Return(true));

    EXPECT_CALL(*p_startStateMock, isValid())
        .Times(1)
            .WillOnce(::testing::Return(true));

    std::map<std::string, Json::Value> sampleData;
    sampleData["plugin1"] = Json::Value("value1");
    sampleData["plugin2"] = Json::Value("value2");

// Set the expectation to return the sample data
    EXPECT_CALL(*p_bundleConfigMock, rdkPlugins())
        .Times(2)
            .WillOnce(::testing::ReturnRef(sampleData))
            .WillOnce(::testing::ReturnRef(sampleData));

    int32_t cd = 123;
    EXPECT_CALL(*p_containerMock, allocDescriptor())
        .Times(1)
            .WillOnce(::testing::Return(cd));

    const std::string validPath = "/tests/L1_testing/tests/";

    EXPECT_CALL(*p_rootfsMock, path())
        .Times(5)
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))
            .WillOnce(::testing::ReturnRef(validPath))

            .WillOnce(::testing::ReturnRef(validPath));

    std::string valid_path = "/tests/L1_testing/tests/DobbyManagerTest";

    EXPECT_CALL(*p_bundleMock, path())
        .Times(4)
            .WillOnce(::testing::ReturnRef(valid_path))
            .WillOnce(::testing::ReturnRef(valid_path))
            .WillOnce(::testing::ReturnRef(valid_path))

            .WillOnce(::testing::ReturnRef(valid_path));

    EXPECT_CALL(*p_bundleConfigMock, config())
        .Times(2)
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()))
            .WillOnce(::testing::Return(std::make_shared<rt_dobby_schema>()));

    const std::vector<std::string> expectedStrings = {"plugin1", "plugin2", "plugin3"};

    EXPECT_CALL(*p_rdkPluginManagerMock, listLoadedPlugins())
        .Times(1)
            .WillOnce(::testing::Return(expectedStrings));

    std::map<std::string, Json::Value> data;
    data["key1"] = Json::Value("value1");
    data["key2"] = Json::Value("value2");

    EXPECT_CALL(*p_bundleConfigMock, legacyPlugins())
        .Times(4)
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data))
            .WillOnce(testing::ReturnRef(data))

            .WillOnce(testing::ReturnRef(data));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostConstructionHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::shared_ptr<IDobbyStartState>& startupState,const std::string& rootfsPath) {
                return true;
            }));

    EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_))
        .Times(2)
            .WillOnce(::testing::Invoke(
            [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                return true;
    }))
            .WillOnce(::testing::Invoke(
            [](const IDobbyRdkPlugin::HintFlags &hookPoint) {
                return true;
    }));

    EXPECT_CALL(*p_configMock, writeConfigJson(::testing::_))
        .Times(2)
            .WillOnce(::testing::Invoke(
            [](const std::string& filePath) {
                return true;
            }))
            .WillOnce(::testing::Invoke(
            [](const std::string& filePath) {
                return true;
    }));

        EXPECT_CALL(*p_rdkPluginManagerMock, getContainerLogger())
            .Times(2)
                .WillOnce(::testing::Return(nullptr))
                .WillOnce(::testing::Return(nullptr));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreStartHooks(::testing::_, ::testing::_, ::testing::_, ::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,pid_t pid,const std::string& rootfsPath) {
                return true;
    }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePostStopHooks(::testing::_, ::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
        }));

    EXPECT_CALL(*p_legacyPluginManagerMock, executePreDestructionHooks(::testing::_, ::testing::_,::testing::_))
        .Times(1)
            .WillOnce(::testing::Invoke(
            [](const std::map<std::string, Json::Value>& plugins,const ContainerId& id,const std::string& rootfsPath) {
                return true;
        }));

    EXPECT_CALL(*p_rdkPluginManagerMock, runPlugins(::testing::_,::testing::_))
        .Times(1)
             .WillRepeatedly(::testing::Invoke(
             [](const IDobbyRdkPlugin::HintFlags &hookPoint,const uint timeoutMs) {
                 return true;
        }));

    EXPECT_CALL(*p_runcMock, destroy(::testing::_,::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(true));

    const ContainerId id = ContainerId::create("container_123");
    const std::string bundlePath = "/path/to/bundle";
    const std::list<int> files = {1, 2, 3};//file descriptors
    const std::string command = "ls -l";
    const std::string displaySocket = "/tmp/display";
    std::vector<std::string> envVars;
    envVars = {"PATH=/usr/bin", "HOME=/home/user"};

    const int DobbyErrorValue = -1;

    int result = dobbyManager_test->startContainerFromBundle(id,bundlePath,files,command,displaySocket,envVars);

    EXPECT_EQ(result, DobbyErrorValue);

}

/* -----------------------------------------------------------------------------
 *  @brief Stops a running container
 *
 *  If withPrejudice is not specified (the default) then we send the init
 *  process within the container a SIGTERM.
 *
 *  If the withPrejudice is true then we use the SIGKILL signal.
 *
 *  This call is asynchronous, i.e. it is a request to stop rather than a
 *  blocking call that ensures the container is stopped before returning.
 *
 *  The @a mContainerStoppedCb callback will be called when the container
 *  has actually been torn down.
 *
 *  @param[in]  cd              The descriptor of the container to stop.
 *  @param[in]  withPrejudice   If true the container process is killed with
 *                              SIGKILL, otherwise SIGTERM is used.
 *
 *  @return true if a container with a matching id was found and a signal
 *  sent successfully to it.
 *
 * Use case coverage:
 *                @Success :3
 *                @Failure :4
 *  -----------------------------------------------------------------------------
*/

/**
 * @brief Test stopContainer.
 * Check the stopContainer method failed to find the invalid descriptor Id
 *
 * @return false.
 */
TEST_F(DaemonDobbyManagerTest, stopContainer_FailedToFindTheContainer)
{
    expect_invalidContainerCleanupTask();

    ContainerId id = ContainerId::create("container1");
    expect_startContainerFromBundle(3456,id);

    /* set stop container with unknow descriptor value */
    int return_value = dobbyManager_test->stopContainer(1234,true);
    EXPECT_EQ(return_value,false);
    expect_cleanupContainersShutdown();
}

/**
 * @brief Test stopContainer.
 * Check the stopContainer method find the valid descriptor Id from containers list and stop the container.
 *
 * @return true.
 */
TEST_F(DaemonDobbyManagerTest, stopContainer_SuccessWithMultipleContainers)
{
    ContainerId id = ContainerId::create("container1");
    ContainerId id1 = ContainerId::create("container2");
    ContainerId id2 = ContainerId::create("container3");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(1234,id);
    expect_startContainerFromBundle(2345,id1);
    expect_startContainerFromBundle(3456,id2);

    expect_stopContainerSuccess(dobbyManager_test->stateOfContainer(2345));
    int return_value = dobbyManager_test->stopContainer(2345,true);
    EXPECT_EQ(return_value,true);
    expect_cleanupContainersShutdown();
}

/**
 * @brief Test stopContainer.
 * Check the stopContainer method find the valid descriptor Id and stop the container.
 *
 * @return true.
 */
TEST_F(DaemonDobbyManagerTest, stopContainer_SuccessWithOneContainer)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");
    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    /* stopContainer is called from the cleanup shutdown */
    expect_stopContainerSuccess(dobbyManager_test->stateOfContainer(cd));
    int return_value = dobbyManager_test->stopContainer(cd,true);
    EXPECT_EQ(return_value,true);

    expect_cleanupContainersShutdown();
}

/**
 * @brief Test stopContainer.
 * Check the stopContainer method find the valid descriptor Id and stop the unknown state container.
 *
 * @return false.
 */
TEST_F(DaemonDobbyManagerTest, stopContainer_UnknownContainerState)
{
    int32_t cd = 1234;
    int32_t stop_cd = 4444;
    ContainerId id = ContainerId::create("container1");

    expect_startContainerFromBundle(cd,id);

    /* stopContainer is called from the cleanup shutdown */
    expect_stopContainerSuccess(dobbyManager_test->stateOfContainer(stop_cd));
    int return_value = dobbyManager_test->stopContainer(stop_cd,true);
    EXPECT_EQ(return_value,false);

    expect_invalidContainerCleanupTask();

    expect_cleanupContainersShutdown();
}

/**
 * @brief Test stopContainer.
 * Check the stopContainer method find the valid descriptor Id and stop the paused container .
 *
 * @return true.
 */
TEST_F(DaemonDobbyManagerTest, stopContainer_ContainerStatePaused)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");
    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    expect_pauseContainerSuccess();

    int return_value = dobbyManager_test->pauseContainer(cd);
    EXPECT_EQ(return_value,true);

    expect_stopContainerSuccess(dobbyManager_test->stateOfContainer(cd));
    return_value = dobbyManager_test->stopContainer(cd,true);
    EXPECT_EQ(return_value,true);
}

/**
 * @brief Test stopContainer.
 * Check the stopContainer method find the valid descriptor Id and failed to resume and stop the paused container.
 *
 * @return false.
 */
TEST_F(DaemonDobbyManagerTest, stopContainer_FailedToResumeFromPausedState)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");
    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    expect_pauseContainerSuccess();

    int return_value = dobbyManager_test->pauseContainer(cd);
    EXPECT_EQ(return_value,true);

    expect_stopContainerFailedToResumeFromPausedState();
    return_value = dobbyManager_test->stopContainer(cd,true);
    EXPECT_EQ(return_value,false);

    expect_resumeContainer_sucess(id);
    return_value = dobbyManager_test->resumeContainer(cd);
    EXPECT_EQ(return_value,true);
    expect_cleanupContainersShutdown();
}

/**
 * @brief Test stopContainer.
 * Check the stopContainer method find the valid descriptor Id and failed to stop on killContainer call.
 *
 * @return false.
 */
TEST_F(DaemonDobbyManagerTest, stopContainer_FailedToSendSignal)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");
    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    expect_stopContainerFailedToKillContainer();
    int return_value = dobbyManager_test->stopContainer(cd,false);
    EXPECT_EQ(return_value,false);

    expect_cleanupContainersShutdown();
}

/* -----------------------------------------------------------------------------
 *  @brief Gets the stats for the container
 *
 *  This is primarily a debugging method, used to get statistics on the
 *  container and roughly correlates to the 'runc events --stats <id>' call.
 *
 *  The reply is a json formatted string containing some info, it's form may
 *  change over time.
 *
 *      {
 *          "id": "blah",
 *          "state": "running",
 *          "timestamp": 348134887768,
 *          "pids": [ 1234, 1245 ],
 *          "cpu": {
 *              "usage": {
 *                  "total":734236982,
 *                  "percpu":[348134887,386102095]
 *              }
 *          },
 *          "memory":{
 *              "user": {
 *                  "limit":41943040,
 *                  "usage":356352,
 *                  "max":524288,
 *                  "failcnt":0
 *              }
 *          }
 *          "gpu":{
 *              "memory": {
 *                  "limit":41943040,
 *                  "usage":356352,
 *                  "max":524288,
 *                  "failcnt":0
 *              }
 *          }
 *          ...
 *      }
 *
 *  @param[in]  cd      The container descriptor
 *
 *  @return Json formatted string with the info for the container, on failure an
 *  empty string.
 *
 * Use case coverage:
 *                @Success :2
 *                @Failure :1
 * -----------------------------------------------------------------------------
 */

/**
 * @brief Test statsOfContainer.
 * Check the statsOfContainer method find state after startContainer without failure.
 *
 * @return DobbyContainer state.
 */
TEST_F(DaemonDobbyManagerTest, statsOfContainer_Success)
{
    std::string expected_string("{\n \"id\" : \"container1\",\n \"state\" : \"running\"\n}");
    int32_t cd = 1234;
    Json::Value jsonStats;

    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();
    expect_startContainerFromBundle(cd,id);

    EXPECT_CALL(*p_statsMock,stats())
        .Times(1)
        .WillOnce(::testing::ReturnRef(jsonStats));

    std::string actual_string= dobbyManager_test->statsOfContainer(cd);
    EXPECT_EQ(actual_string,expected_string);
    expect_cleanupContainersShutdown();
}

/**
 * @brief Test statsOfContainer.
 * Check the statsOfContainer method find state of unknown container without failure.
 *
 * @return DobbyContainer state.
 */
TEST_F(DaemonDobbyManagerTest, statsOfContainer_EmptyString)
{
    std::string expected_string("{\n \"id\" : \"UnknownContainer\",\n \"state\" : \"unknown\"\n}");
    int32_t cd = 4444;
    Json::Value jsonStats;

    EXPECT_CALL(*p_statsMock,stats())
        .Times(1)
        .WillOnce(::testing::ReturnRef(jsonStats));

    std::string actual_string= dobbyManager_test->statsOfContainer(cd);
    EXPECT_EQ(actual_string,expected_string);

    expect_invalidContainerCleanupTask();

}

/**
 * @brief Test statsOfContainer.
 * Check the statsOfContainer method find state after Paused Container without failure.
 *
 * @return DobbyContainer state.
 */
TEST_F(DaemonDobbyManagerTest, statsOfContainer_FailedToFindContainer)
{
    std::string expected_string;
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");
    Json::Value jsonStats;

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    std::string actual_string= dobbyManager_test->statsOfContainer(2345);
    EXPECT_EQ(actual_string,expected_string);
    expect_cleanupContainersShutdown();
}

/* -----------------------------------------------------------------------------
 *  @brief Returns the state of a given container
 *
 *
 *
 *  @param[in]  cd      The descriptor of the container to get the state of.
 *
 *  @return one of the possible state values.
 * Use case coverage:
 *                @Success :2
 *                @Failure :1
 * -----------------------------------------------------------------------------
 */

/**
 * @brief Test stateOfContainer.
 * Check the stateOfContainer method find state after startContainer without failure.
 *
 * @return DobbyContainer state.
 */
TEST_F(DaemonDobbyManagerTest, stateOfContainer_SuccessWhenContainerRunning)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    int return_value = dobbyManager_test->stateOfContainer(cd);
    EXPECT_EQ(return_value,CONTAINER_STATE_RUNNING);
    expect_cleanupContainersShutdown();
}

/**
 * @brief Test stateOfContainer.
 * Check the stateOfContainer method find state after pausedContainer without failure.
 *
 * @return DobbyContainer state.
 */
TEST_F(DaemonDobbyManagerTest, stateOfContainer_SuccessWhenContainerPaused)
{
    int32_t cd = 1234;
    int return_value;

    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    expect_pauseContainerSuccess();
    return_value = dobbyManager_test->pauseContainer(cd);
    EXPECT_EQ(return_value,true);

    return_value = dobbyManager_test->stateOfContainer(cd);
    EXPECT_EQ(return_value,CONTAINER_STATE_PAUSED);
}

/**
 * @brief Test statsOfContainer.
 * Check the statsOfContainer method failed to find container Id.
 *
 * @return DobbyContainer state.
 */
TEST_F(DaemonDobbyManagerTest, stateOfContainer_FailedToFindContainer)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    int return_value = dobbyManager_test->stateOfContainer(2345);

    EXPECT_EQ(return_value,CONTAINER_STATE_INVALID);
    expect_cleanupContainersShutdown();
}

/* -----------------------------------------------------------------------------
 *  @brief Freezes a running container
 *
 *  Currently we have no use case for pause/resume containers so the method
 *  hasn't been implemented, however when testing manually I've discovered it
 *  actually works quite well.
 *
 *  If wanting to have a play you can run the following on the command line
 *
 *      runc --root /var/run/runc pause <id>
 *
 *  @param[in]  cd      The descriptor of the container to pause.
 *
 *  @return true if a container with a matching descriptor was found and it was
 *  frozen.
 * Use case coverage:
 *                @Success :1
 *                @Failure :3
 * -----------------------------------------------------------------------------
 */

/**
 * @brief Test pauseContainer with valid arguments.
 * Check if pauseContainer method handles the case with valid,
 *
 * @return true.
 */
TEST_F(DaemonDobbyManagerTest, pauseContainer_ValidInput)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    expect_pauseContainerSuccess();
    int return_value = dobbyManager_test->pauseContainer(cd);
    EXPECT_EQ(return_value,true);

    expect_resumeContainer_sucess(id);
    return_value = dobbyManager_test->resumeContainer(cd);
    EXPECT_EQ(return_value,true);
    expect_cleanupContainersShutdown();
}

/**
 * @brief Test pauseContainer with invalid container Id.
 * Check if pauseContainer method failed to find the containter Id, then It will return false,
 *
 * @return false.
 */
TEST_F(DaemonDobbyManagerTest, pauseContainer_FailedToFindContainer)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    int return_value = dobbyManager_test->pauseContainer(2345);
    EXPECT_EQ(return_value,false);
    expect_cleanupContainersShutdown();
}

/**
 * @brief Test pauseContainer with valid arguments.
 * Check if pauseContainer method handles the case with valid arguments and failed to pause the container.
 *
 * @return false.
 */
TEST_F(DaemonDobbyManagerTest, pauseContainer_FailedToPauseContainer)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    expect_pauseContainerFailed();
    int return_value = dobbyManager_test->pauseContainer(cd);
    EXPECT_EQ(return_value,false);
    expect_cleanupContainersShutdown();
}

/**
 * @brief Test pauseContainer with valid arguments.
 * Check if pauseContainer method verify the container already paused, then It will avoid the pause call.
 *
 * @return false.
 */
TEST_F(DaemonDobbyManagerTest, pauseContainer_FailedAsAlreadyPaused)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    /* Freezes a running container and set the container state to puased */
    expect_pauseContainerSuccess();
    int return_value = dobbyManager_test->pauseContainer(cd);
    EXPECT_EQ(return_value,true);

    /* Freezes a puased container, and it will failed */
    return_value = dobbyManager_test->pauseContainer(cd);
    EXPECT_EQ(return_value,false);

    /* Resume a puased container, before stop container we should resume the container */
    expect_resumeContainer_sucess(id);
    return_value = dobbyManager_test->resumeContainer(cd);
    EXPECT_EQ(return_value,true);
    expect_cleanupContainersShutdown();
}


/* -----------------------------------------------------------------------------
 *  @brief Thaws a frozen container
 *
 *  @param[in]  cd      The descriptor of the container to resume.
 *
 *  @return true if a container with a matching descriptor was found and it was
 *  resumed.
 * Use case coverage:
 *                @Success :1
 *                @Failure :3
 *-----------------------------------------------------------------------------
 */

/**
 * @brief Test resumeContainer with valid arguments.
 * Check if resumeContainer method resume the paused container for descriptor Id from input argument,
 *
 * @return true.
 */
TEST_F(DaemonDobbyManagerTest, resumeContainer_Success)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    expect_pauseContainerSuccess();
    int return_value = dobbyManager_test->pauseContainer(cd);
    EXPECT_EQ(return_value,true);

    expect_resumeContainer_sucess(id);
    return_value = dobbyManager_test->resumeContainer(cd);
    EXPECT_EQ(return_value,true);
    expect_cleanupContainersShutdown();
}

/**
 * @brief Test resumeContainer with invalid container Id.
 * Check if resumeContainer method failed to find the containter Id, then It will return false,
 *
 * @return false.
 */
TEST_F(DaemonDobbyManagerTest, resumeContainer_FailedToFindContainer)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    int return_value = dobbyManager_test->resumeContainer(2345);
    EXPECT_EQ(return_value,false);
    expect_cleanupContainersShutdown();
}


/**
 * @brief Test resumeContainer with valid arguments.
 * Check if resumeContainer method resume the paused container and failed to resume the container.
 *
 * @return false.
 */
TEST_F(DaemonDobbyManagerTest, resumeContainer_FailedToResume)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    expect_pauseContainerSuccess();
    int return_value = dobbyManager_test->pauseContainer(cd);
    EXPECT_EQ(return_value,true);

    expect_resumeContainer_failed(id);
    return_value = dobbyManager_test->resumeContainer(cd);
    EXPECT_EQ(return_value,false);
}

/**
 * @brief Test resumeContainer with valid arguments.
 * Check the resumeContainer method not resume, if the container is not paused
 *
 * @return false.
 */
TEST_F(DaemonDobbyManagerTest, resumeContainer_FailureAsNotInPausedState)
{
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    /* We can only resume a container that's currently paused */
    int return_value = dobbyManager_test->resumeContainer(cd);
    EXPECT_EQ(return_value,false);
    expect_cleanupContainersShutdown();
}

/*Test cases for startContainerFromBundle ends here*/

/****************************************************************************************************
 * Test functions for :ociConfigOfContainer
 *  @brief Check if ociConfigOfContainer method is successfully returned the config.json string
 *
 *  @param[in]  cd          The descriptor of the container to get the config.json of.
 *  @return the config.json string.
 *
  * Use case coverage:
 *                @Success :1
 *                @Failure :2
 ***************************************************************************************************/

/**
 * @brief Test ociConfigOfContainer Success case
 * Check if ociConfigOfContainer method is successfully returned the config.json string
 *
 *  @param[in]  cd      The descriptor of the container to get the config.json of.
 *  @return the config.json string.
 */
TEST_F(DaemonDobbyManagerTest, ociConfigOfContainer_Success)
{
    std::string expect_string("{\n \"id\" : \"container1\",\n \"state\" : \"running\"\n}");
    int32_t cd = 1234;

    expect_invalidContainerCleanupTask();

#ifdef LEGACY_COMPONENTS
    expect_startContainerFromSpec(cd);
#else
    ContainerId id = ContainerId::create("container1");
    expect_startContainerFromBundle(cd,id);
#endif /* LEGACY_COMPONENTS */

    EXPECT_CALL(*p_configMock, configJson())
        .Times(1)
        .WillOnce(::testing::Return(expect_string));

    std::string result = dobbyManager_test->ociConfigOfContainer(cd);

    EXPECT_EQ(result, expect_string);
}

/**
 * @brief Test ociConfigOfContainer failure case
 * Check if ociConfigOfContainer method is failing when no containers are added to the list.
 *
 *  @param[in]  cd      The descriptor of the container to get the config.json of.
 *  @return the empty config.json string as there are no containers.
 */
TEST_F(DaemonDobbyManagerTest, ociConfigOfContainer_FailedToFindContainer)
{
    std::string expect_string("");
    int32_t cd = 1234;

    expect_invalidContainerCleanupTask();

#ifdef LEGACY_COMPONENTS
    expect_startContainerFromSpec(cd);
#else
    ContainerId id = ContainerId::create("container1");
    expect_startContainerFromBundle(cd,id);
#endif /* LEGACY_COMPONENTS */

    std::string result = dobbyManager_test->ociConfigOfContainer(2345);
    EXPECT_EQ(result, expect_string);

}

/**
 * @brief Test ociConfigOfContainer Success case
 * Check if ociConfigOfContainer method is successfully returned empty config.json string
 *
 *  @param[in]  cd      The descriptor of the container to get the config.json of.
 *  @return the config.json string.
 */
TEST_F(DaemonDobbyManagerTest, ociConfigOfContainer_EmptyOCIConfigJsonSpec)
{
    std::string empty_string("{}");
    int32_t cd = 123;

    expect_invalidContainerCleanupTask();

#ifdef LEGACY_COMPONENTS
    expect_startContainerFromSpec(cd);
#else
    ContainerId id = ContainerId::create("container1");
    expect_startContainerFromBundle(cd,id);
#endif /* LEGACY_COMPONENTS */

    EXPECT_CALL(*p_configMock, configJson())
        .Times(1)
        .WillOnce(::testing::Return(empty_string));

    std::string result = dobbyManager_test->ociConfigOfContainer(cd);

    EXPECT_EQ(result, empty_string);

}
/*Test cases for ociConfigOfContainer ends here*/

#if defined(LEGACY_COMPONENTS)

/****************************************************************************************************
 * Test functions for :specOfContainer
 *  @brief allow you to retrieve the json spec used to create the container
 *
 *  @param[in]  cd          The descriptor of the container to get the spec of.
 *  @return the json spec string.
 *
  * Use case coverage:
 *                @Success :1
 *                @Failure :2
 ***************************************************************************************************/
/**
 *  @fail to find the container
 *
 *  @param[in]  cd      The descriptor of the container to get the spec of.
 *
 *  @return the json spec string.
 */

TEST_F(DaemonDobbyManagerTest, specOfContainer_FailedToFindContainer)
{

    std::string expected_string("");
    int32_t cd = 1234;
    expect_invalidContainerCleanupTask();

    expect_startContainerFromSpec(cd);

    std::string result = dobbyManager_test->specOfContainer(2345);
    EXPECT_EQ(result, expected_string);

}

/**
 *  @brief Debugging method to allow you to retrieve the json spec used to
 *  create the container
 *success case
 *
 *  @param[in]  cd      The descriptor of the container to get the spec of.
 *
 *  @return the json spec string.
 */

TEST_F(DaemonDobbyManagerTest, specOfContainer_SuccessWhenStarting)
{

    std::string expected_string("{\n \"id\" : \"container1\",\n \"state\" : \"running\"\n}");
    int32_t cd = 1234;
    expect_invalidContainerCleanupTask();

    expect_startContainerFromSpec(1234);

    EXPECT_CALL(*p_specConfigMock, spec())
        .Times(1)
        .WillOnce(::testing::Return(expected_string));

    std::string result = dobbyManager_test->specOfContainer(cd);

    EXPECT_EQ(result, expected_string);
}

/*when passed empty string*/

TEST_F(DaemonDobbyManagerTest, specOfContainer_EmptyJsonSpec)
{

    std::string empty_string("{}");
    int32_t cd = 123;
    expect_invalidContainerCleanupTask();

    expect_startContainerFromSpec(123);

    EXPECT_CALL(*p_specConfigMock, spec())
        .Times(1)
        .WillOnce(::testing::Return(empty_string));

    std::string result = dobbyManager_test->specOfContainer(cd);

    EXPECT_EQ(result, empty_string);
}
#endif //defined(LEGACY_COMPONENTS)

/*specOfContainer usecases ends here*/

// -----------------------------------------------------------------------------
/**
 *  @brief Executes a command in a running container
 *
 *  @param[in]  cd          The descriptor of the container to execute the command in.
 *  @param[in]  command     Command to be executed.
 *  @param[in]  options     Options to execute the command with.
 *
 *  @return true if a container with a matching descriptor was found and the command was run
 *
 * Use case coverage:
 *                @Success :1
 *                @Failure :3
 *-----------------------------------------------------------------------------
 */
/* Test the exec running container success and return non zero pid */
TEST_F(DaemonDobbyManagerTest, execInContainer_Success)
{
    pid_t pid1 = 1234;
    pid_t pid2 = 5678;
    std::string options = "--tty";
    std::string command = "fork exec";
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();
    expect_startContainerFromBundle(cd,id);

    EXPECT_CALL(*p_runcMock,exec(::testing::_,::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(std::make_pair(pid1, pid2)));

    EXPECT_CALL(*p_rdkPluginManagerMock, getContainerLogger())
        .Times(1)
        .WillOnce(::testing::Return(std::make_shared<IDobbyRdkLoggingPluginMock>()));

    EXPECT_CALL(*p_loggerMock, StartContainerLogging(::testing::_,::testing::_,::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(true));

    bool return_value = dobbyManager_test->execInContainer(cd, options, command);
    EXPECT_EQ(return_value,true);

}

/* Test the exec failed to get valid container */
TEST_F(DaemonDobbyManagerTest, execInContainer_FailedToFindContainer)
{
    std::string options = "--tty";
    std::string command = "fork exec";
    int32_t cd = 1234;
    int32_t expect_cd = 2345;

    ContainerId id = ContainerId::create("container1");
    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    bool return_value = dobbyManager_test->execInContainer(expect_cd, options, command);
    EXPECT_EQ(return_value,false);

}

/* Test the exec command and return a pid is zero */
TEST_F(DaemonDobbyManagerTest, execInContainer_FailedToExecuteCommand)
{
    pid_t pid1 = 1234;
    pid_t pid2 = 0;
    std::string options = "--tty";
    std::string command = "fork exec";
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    EXPECT_CALL(*p_runcMock,exec(::testing::_,::testing::_,::testing::_))
        .Times(1)
        .WillOnce(::testing::Return(std::make_pair(pid1, pid2)));

    bool return_value = dobbyManager_test->execInContainer(cd, options, command);
    EXPECT_EQ(return_value,false);

}

/* Test the exec command failed to excecute paused container, exec command process the running container only */
TEST_F(DaemonDobbyManagerTest, execInContainer_FailureAsContainerNotRunning)
{
    bool return_value;
    pid_t pid1 = 1234;
    pid_t pid2 = 0;
    std::string options = "--tty";
    std::string command = "fork exec";
    int32_t cd = 1234;
    ContainerId id = ContainerId::create("container1");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cd,id);

    expect_pauseContainerSuccess();

    /* Container move to paused state */
    return_value = dobbyManager_test->pauseContainer(cd);
    EXPECT_EQ(return_value,true);

    /*No expect call, if the container not in running state */
    return_value = dobbyManager_test->execInContainer(cd, options, command);
    EXPECT_EQ(return_value,false);

}
/*execInContainer usecases ends here*/


// -----------------------------------------------------------------------------
/**
 *  @brief Returns a list of all the containers
 *
 *  The returned list contains the id of all the containers we know about in
 *  their various states.  Just because a container id is in the list it
 *  doesn't necessarily mean it's actually running, it could be in either
 *  the starting or stopping phase.
 *
 *  @see DobbyManager::stateOfContainer for a way to retrieve the
 *  status of the container.
 *
 *  @return a list of all the containers.
 *
 * Use case coverage:
 *                @Success :3
 *                @Failure :0
 *-----------------------------------------------------------------------------

 */
/* Test the listContainers success, returns the valid containers list */
TEST_F(DaemonDobbyManagerTest, listContainers)
{
    std::vector<int32_t> cds = { 1234, 2345,3456};
    std::vector<ContainerId>ids(3);
    ids[0] = ContainerId::create("container1");
    ids[1] = ContainerId::create("container2");
    ids[2] = ContainerId::create("container3");

    expect_invalidContainerCleanupTask();

    expect_startContainerFromBundle(cds[0],ids[0]);
    expect_startContainerFromBundle(cds[1],ids[1]);
    expect_startContainerFromBundle(cds[2],ids[2]);

    std::list<std::pair<int32_t, ContainerId>> containers = dobbyManager_test->listContainers();

    size_t n = 0;
    for (const std::pair<int32_t, ContainerId>& details : containers)
    {
        EXPECT_EQ(details.first,cds[n]);
        EXPECT_EQ(details.second.mId,ids[n].mId);
        n++;
    }
    expect_cleanupContainersShutdown();
}

/* Test the listContainers without start container, returns the empty containers list */
TEST_F(DaemonDobbyManagerTest, listContainers_WhenListIsEmpty)
{
    EXPECT_CALL(*p_runcMock, destroy(::testing::_,::testing::_,::testing::_))
        .Times(testing::AtLeast(1))
        .WillRepeatedly(::testing::Return(true));

    /* Removed UnknownContainer and no containers added */
    Test_invalidContainerCleanupTask();
    Test_invalidContainerCleanupTask = nullptr;

    std::list<std::pair<int32_t, ContainerId>> containers = dobbyManager_test->listContainers();

    /* expect the containers list is count */
    EXPECT_EQ(containers.size(),0);
}

/* Test the listContainers to get huge start containers, returns the valid containers list. Verifies the all containers data */
TEST_F(DaemonDobbyManagerTest, listContainers_WhenListIsHuge)
{
    std::vector<int32_t> cds(LIST_CONTAINERS_HUGE_COUNT);
    std::vector<ContainerId>ids(LIST_CONTAINERS_HUGE_COUNT);
    std::string s[LIST_CONTAINERS_HUGE_COUNT];

    expect_invalidContainerCleanupTask();

    for (size_t i = 0; i < LIST_CONTAINERS_HUGE_COUNT; i++)
    {
        s[i] = "container" + std::to_string(i + 1);
        ids[i] = ContainerId::create(s[i]);
        cds[i] = i+1;

        expect_startContainerFromBundle(cds[i],ids[i]);
    }

    std::list<std::pair<int32_t, ContainerId>> containers = dobbyManager_test->listContainers();

    EXPECT_EQ(containers.size(),LIST_CONTAINERS_HUGE_COUNT);

    size_t n = 0;
    for (const std::pair<int32_t, ContainerId>& details : containers)
    {
        EXPECT_EQ(details.first,cds[n]);
        EXPECT_EQ(details.second.mId,ids[n].mId);
        n++;
    }
    expect_cleanupContainersShutdown();
}
/* listContainers usecases ends here*/
