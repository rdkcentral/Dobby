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

if (LEGACY_COMPONENTS)
        list(APPEND ADDITIONAL_SOURCES
                "source/DobbyPluginManager.cpp"
                "source/syshooks/BaseHook.cpp"
                "source/syshooks/GpuMemoryHook.cpp"
                "source/syshooks/RtSchedulingHook.cpp"
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
        source/DobbyState.cpp
        ${ADDITIONAL_SOURCES}
)

target_include_directories( DobbyDaemonLib

        PUBLIC
        include
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
)

if( BUILDING_WITH_RDK_SDK )

    add_dependencies( DobbyDaemonLib

            JsonCpp::JsonCpp
            CTemplate::libctemplate )

endif()
