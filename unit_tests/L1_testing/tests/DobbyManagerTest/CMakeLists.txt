# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
#limitations under the License.

cmake_minimum_required(VERSION 3.7)
project(DobbyManagerL1Test)

set(CMAKE_CXX_STANDARD 14)

find_package(GTest REQUIRED)
find_package(ctemplate REQUIRED)
find_package(jsoncpp REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

add_library(DaemonDobbyManagerTest SHARED STATIC
            ../../../../daemon/lib/source/DobbyManager.cpp
            ../../../../AppInfrastructure/Logging/source/Logging.cpp
            ../../mocks/DobbyBundleConfigMock.cpp
            ../../mocks/DobbyRunCMock.cpp
            ../../mocks/ContainerIdMock.cpp
            ../../mocks/DobbyBundleMock.cpp
            ../../mocks/DobbyConfigMock.cpp
            ../../mocks/DobbyContainerMock.cpp
            ../../mocks/DobbyEnvMock.cpp
            ../../mocks/DobbyFileAccessFixerMock.cpp
            ../../mocks/DobbyIPCUtilsMock.cpp
            ../../mocks/DobbyLegacyPluginManagerMock.cpp
            ../../mocks/DobbyRdkPluginManagerMock.cpp
            ../../mocks/DobbyRdkPluginUtilsMock.cpp
            ../../mocks/DobbyRootfsMock.cpp
            ../../mocks/DobbySpecConfigMock.cpp
            ../../mocks/DobbyStreamMock.cpp
            ../../mocks/DobbyLoggerMock.cpp
            ../../mocks/DobbyStatsMock.cpp
            ../../mocks/rt_dobby_schema.c
            ../../mocks/IpcFileDescriptorMock.cpp
            ../../mocks/DobbyStartStateMock.cpp
            )

target_include_directories(DaemonDobbyManagerTest
                PUBLIC
                ../../mocks
                ../../../../utils/include
                ../../../../utils/source
                ../../../../AppInfrastructure/Logging/include
                ../../../../AppInfrastructure/Common/include
                ../../../../bundle/lib/include
                ../../../../ipcUtils/include
                ../../../../settings/include
                ../../../../daemon/lib/include
                ../../../../daemon/lib/source/include
                ../../../../libocispec/generated_output
                ../../../../AppInfrastructure/IpcService/include
                ../../../../libocispec/generated_output
                ../../../../pluginLauncher/lib/include
                ../../../../daemon/lib/include
                ../../../../protocol/include
                ../../../../build/AppInfrastructure/Tracing
                ../../../../AppInfrastructure/ReadLine/include
                ../../../../AppInfrastructure/IpcService/source/libdbus
                ../../../../rdkPlugins/Networking/include
                ../../../../AppInfrastructure/ReadLine/include
                ../../../../AppInfrastructure/ReadLine/source
                ../../../../unit_tests/L1_testing/tests
                /usr/lib/plugins/dobby
                /usr/include/jsoncpp
                /usr/include/dbus-1.0
                /usr/lib/x86_64-linux-gnu/dbus-1.0/include
                )

file(GLOB TESTS *.cpp)

add_executable(${PROJECT_NAME} ${TESTS})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    gmock_main
    DaemonDobbyManagerTest
    ${GTEST_LIBRARIES}
    GTest::GTest
    GTest::Main
    gmock
    ctemplate
    pthread
    jsoncpp
    systemd
    dl
    dbus-1
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
