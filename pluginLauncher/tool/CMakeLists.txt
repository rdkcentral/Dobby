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

project(DobbyPluginLauncher)

# Allow loading RDK plugins from a different location
set(PLUGIN_PATH "" CACHE PATH "Load RDK plugins from a different location")
if (NOT PLUGIN_PATH STREQUAL "")
    add_definitions(-DPLUGIN_PATH="${PLUGIN_PATH}")
endif()

add_executable(${PROJECT_NAME}
    source/Main.cpp
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
    include
    $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(${PROJECT_NAME}
    DobbyPluginLauncherLib
    AppInfraLogging

    # Export the logging symbols so plugins can use AI_LOG correctly
    -Wl,--dynamic-list=${CMAKE_CURRENT_LIST_DIR}/exported.syms

    # 3rd party libraries
    JsonCpp::JsonCpp

    # Adds dlopen support (if it's a separate library on target)
    ${CMAKE_DL_LIBS}

    # fixes an issue with getting a SIGSEGV when spawning threads
    # from within plugins using IPCService
    Threads::Threads
)

install(
    TARGETS ${PROJECT_NAME}

    RUNTIME DESTINATION bin
)
