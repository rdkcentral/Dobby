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
project(DobbyL1Test)

set(CMAKE_CXX_STANDARD 14)

find_package(GTest REQUIRED)
find_package(ctemplate REQUIRED)
find_package(jsoncpp REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})


add_library(DaemonDobbyTests SHARED STATIC
            ../../../../daemon/lib/source/Dobby.cpp
            ../../../../AppInfrastructure/Logging/source/Logging.cpp
            ../../mocks/ContainerIdMock.cpp
            ../../mocks/DobbyContainerMock.cpp
            ../../mocks/DobbyEnvMock.cpp
            ../../mocks/DobbyIPCUtilsMock.cpp
            ../../mocks/DobbyManagerMock.cpp
            ../../mocks/DobbyTemplateMock.cpp
            ../../mocks/DobbyUtilsMock.cpp
            ../../mocks/DobbyWorkQueueMock.cpp
            ../../mocks/IIpcServiceMock.cpp
            ../../mocks/IpcFileDescriptorMock.cpp
            )


target_include_directories(DaemonDobbyTests
                PUBLIC
                ../../mocks
                        ../../mocks/dobbymanager
                ../../../../utils/include
                ../../../../utils/source
                ../../../../AppInfrastructure/Logging/include
                ../../../../AppInfrastructure/Common/include
                ../../../../bundle/lib/include
                ../../../../AppInfrastructure/IpcService/include
                ../../../../ipcUtils/include
                ../../../../settings/include
                ../../../../AppInfrastructure/IpcService/source/libdbus
                ../../../../libocispec/generated_output
                ../../../../pluginLauncher/lib/include
                ../../../../protocol/include
	        ../../../../daemon/lib/include
	        ../../../../daemon/lib/source/include
                ../../../../build/AppInfrastructure/Tracing                
                ../../../../AppInfrastructure/ReadLine/include
                ../../../../AppInfrastructure/ReadLine/source
                ../../../../rdkPlugins/Networking/include
                /usr/lib/plugins/dobby
                /usr/include/jsoncpp
                )

file(GLOB TESTS *.cpp 
     ../../mocks/IpcVariantList.cpp
     ../../mocks/ContainerIdMock.cpp 
     )

add_executable(${PROJECT_NAME} ${TESTS})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    gmock_main
    DaemonDobbyTests
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
