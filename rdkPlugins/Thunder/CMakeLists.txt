# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2020 Sky UK
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
# limitations under the License.

cmake_minimum_required( VERSION 3.7.0 )

project(ThunderPlugin)

find_package(securityagent CONFIG)

add_library(${PROJECT_NAME}
        SHARED
        source/ThunderPlugin.cpp

        ../Networking/source/Netfilter.cpp
        ../Networking/source/StdStreamPipe.cpp
        ../Networking/source/NetworkingHelper.cpp
        ../Networking/source/PortForwarding.cpp
        ../Networking/source/IPAllocator.cpp
        )

if(securityagent_FOUND)
        add_definitions("-DHAS_SECURITY_AGENT")
else()
        message("Could not find ThunderClientLibraries SecurityAgent - building without token support")
endif()

if (EXTERNAL_THUNDER_PLUGIN_BUILD)
        # TODO:: Improve this. All plugins should be able to be built standalone
        # This allows the Thunder plugin to be built in a standalone recipe to break
        # the circular dependency of Dobby <- Thunder ProcessContainers <- Dobby Thunder Plugin
        find_package(Dobby)

        # TODO:: We shouldn't have to duplicate this here
        string( TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE )

        # Set the 'AI' build type defines for removing some of the code on release builds
        add_definitions( -DAI_DEBUG=1 -DAI_RELEASE=2 )
        if( BUILD_TYPE STREQUAL "RELEASE" )
                add_definitions( -DAI_BUILD_TYPE=AI_RELEASE )
        elseif( BUILD_TYPE STREQUAL "DEBUG" )
                add_definitions( -DAI_BUILD_TYPE=AI_DEBUG )
        else()
                message( FATAL_ERROR "BUILD_TYPE '${BUILD_TYPE}' not supported" )
        endif()

        target_include_directories(${PROJECT_NAME}
                PRIVATE
                ../Networking/include
        )
else()
        target_include_directories(${PROJECT_NAME}
                PRIVATE
                ../Networking/include
                $<TARGET_PROPERTY:DobbyDaemonLib,INTERFACE_INCLUDE_DIRECTORIES>
        )
endif()

install(
        TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib/plugins/dobby
        NAMELINK_SKIP
        )

target_link_libraries(${PROJECT_NAME}
        DobbyRdkPluginCommonLib
        )

if (securityagent_FOUND)
        target_link_libraries(${PROJECT_NAME}
                securityagent::securityagent
        )
endif()

set_target_properties( ${PROJECT_NAME} PROPERTIES SOVERSION 1 )
