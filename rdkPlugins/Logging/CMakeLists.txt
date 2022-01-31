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

# Project name should match the name of the plugin
# CMake will prefix this with "lib"
project(LoggingPlugin)

set(LOGGING_PLUGIN_SRC
        source/LoggingPlugin.cpp
        source/FileSink.cpp
        source/NullSink.cpp
)

if (USE_SYSTEMD)
        list(APPEND LOGGING_PLUGIN_SRC
                source/JournaldSink.cpp
        )
endif()

add_library( ${PROJECT_NAME}
        SHARED
        ${LOGGING_PLUGIN_SRC}
        )

target_include_directories(${PROJECT_NAME}
        PRIVATE
        include
        $<TARGET_PROPERTY:DobbyDaemonLib,INTERFACE_INCLUDE_DIRECTORIES>
)

install(
        TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib/plugins/dobby
        NAMELINK_SKIP
        )

target_link_libraries(${PROJECT_NAME}
        DobbyRdkPluginCommonLib
)

if (USE_SYSTEMD)
        target_link_libraries(${PROJECT_NAME}
                systemd
        )
endif()

set_target_properties( ${PROJECT_NAME} PROPERTIES SOVERSION 1 )
