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

set(IPCSERVICE_SRC
        source/common/IpcVariantList.cpp
        source/common/IpcFileDescriptor.cpp
)

if (USE_SYSTEMD)
        list(APPEND IPCSERVICE_SRC
                source/sdbus/SDBusIpcFactory.cpp
                source/sdbus/SDBusIpcService.cpp
                source/sdbus/SDBusArguments.cpp
                source/sdbus/SDBusAsyncReplyGetter.cpp
                source/sdbus/SDBusAsyncReplySender.cpp
        )
else()
        list(APPEND IPCSERVICE_SRC
                source/libdbus/DbusMessageParser.cpp
                source/libdbus/AsyncReplyGetter.cpp
                source/libdbus/BaseService.cpp
                source/libdbus/IpcService.cpp
                source/libdbus/IpcFactory.cpp
                source/libdbus/DbusConnection.cpp
                source/libdbus/DbusTimeouts.cpp
                source/libdbus/DbusEventDispatcher.cpp
                source/libdbus/IpcUtilities.cpp
                source/libdbus/DbusWatches.cpp
                source/libdbus/AsyncReplySender.cpp
                source/libdbus/DbusUserIdSenderIdCache.cpp
        )
endif()

add_library( IpcService

        STATIC
        ${IPCSERVICE_SRC}
)

set_target_properties(IpcService PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories( IpcService

        PUBLIC

        # Adds the include dir to INTERFACE_INCLUDE_DIRECTORIES for exporting
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>

        PRIVATE
        ${Boost_INCLUDE_DIRS}
        $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:AppInfraCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../Public>
        ${DBUS_INCLUDE_DIRS}

        )

install( TARGETS IpcService

        EXPORT DobbyTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

# Install public headers for external use
install( DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Dobby/IpcService
        FILES_MATCHING PATTERN "*.h"
)
