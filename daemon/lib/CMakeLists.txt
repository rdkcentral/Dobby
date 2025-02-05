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

option(DOBBY_HIBERNATE_MEMCR_IMPL "Hibernate containers using Memcr tool" OFF)
option(DOBBY_HIBERNATE_MEMCR_PARAMS_ENABLED "Enable optional parameters in Memcr protocol" OFF)

if (LEGACY_COMPONENTS)
        list(APPEND ADDITIONAL_SOURCES
                "source/DobbyLegacyPluginManager.cpp"
        )
        list(APPEND ADDITIONAL_INCLUDES ${CTEMPLATE_INCLUDE_DIRS})
endif()

add_library( DobbyDaemonLib

        STATIC
        source/Dobby.cpp
        source/DobbyContainer.cpp
        source/DobbyManager.cpp
        source/DobbyEnv.cpp
        source/DobbyRunC.cpp
        source/DobbyStream.cpp
        source/DobbyStartState.cpp
        source/DobbyStats.cpp
        source/DobbyAsync.cpp
        source/DobbyWorkQueue.cpp
        source/DobbyLogger.cpp
        source/DobbyLogRelay.cpp
        source/DobbyHibernate.cpp

        ${ADDITIONAL_SOURCES}
        )

target_include_directories( DobbyDaemonLib

        PUBLIC
        include
        source/include
        $<TARGET_PROPERTY:IpcService,INTERFACE_INCLUDE_DIRECTORIES>

        PRIVATE
        ${JSONCPP_INCLUDE_DIRS}
        ${DBUS_INCLUDE_DIRS}
        ${ADDITIONAL_INCLUDES}

        $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:AppInfraCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:IpcService,INTERFACE_INCLUDE_DIRECTORIES>

        ../../protocol/include
)

target_link_libraries(DobbyDaemonLib
        DobbyBundleGenLib
        DobbySettings
        DobbyUtils
        DobbyIpcUtils
        DobbyPluginLauncherLib
        AppInfraTracing
)

if (ENABLE_PERFETTO_TRACING)
        target_link_libraries(DobbyDaemonLib
                DobbyTracing
        )
endif()

# Configure breakpad if we find it
if(BREAKPAD_FOUND)
        message("Enabling breakpad support")
        add_definitions( -DUSE_BREAKPAD )

        target_link_libraries(DobbyDaemonLib
                Breakpad::BreakpadWrapper
        )
endif()


if( BUILDING_WITH_RDK_SDK )

    add_dependencies( DobbyDaemonLib

            JsonCpp::JsonCpp
            CTemplate::libctemplate )

endif()

if(DOBBY_HIBERNATE_MEMCR_IMPL)
    add_definitions( -DDOBBY_HIBERNATE_MEMCR_IMPL=1 )

    if (DOBBY_HIBERNATE_MEMCR_PARAMS_ENABLED)
        add_definitions( -DDOBBY_HIBERNATE_MEMCR_PARAMS_ENABLED=1 )
    endif()
endif()

# Install public headers for external use
install( DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Dobby/rdkPlugins/
        FILES_MATCHING PATTERN "IDobbyStartState.h"
)
