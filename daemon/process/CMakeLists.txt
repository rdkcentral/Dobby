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


add_executable( # Sets the name of the exec.
        DobbyDaemon

        # The entry point
        source/Main.cpp

        )

# Specifies libraries CMake should link to your target binary. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_include_directories(
        DobbyDaemon

        PRIVATE
        ../../protocol/include
        $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:AppInfraCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:DobbyDaemonLib,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:IpcService,INTERFACE_INCLUDE_DIRECTORIES>

        )

target_link_libraries(DobbyDaemon
        DobbyDaemonLib
        DobbyBundleGenLib
        DobbySettings
        DobbyUtils
        DobbyIpcUtils
        IpcService
        AppInfraLogging
        AppInfraCommon

)

target_compile_definitions(
        DobbyDaemon

        PRIVATE
        -DDOBBY_VERSION="${DOBBY_MAJOR_VERSION}.${DOBBY_MINOR_VERSION}"

        )

# The following is used to export the AI_LOG functions and the JsonCpp API so
# that plugin hooks can use them rather than having their own version of the
# logging / JsonCpp library
# (However RDK is on old cmake that doesn't support target_link_options(),
#  so instead put the link command in the target_link_libraries() below)

# target_link_options(
#        DobbyDaemon
#        PUBLIC
#        -Wl,--dynamic-list=${CMAKE_CURRENT_LIST_DIR}/exported.syms )


# Specifies libraries CMake should link to your target binary. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target exec
        DobbyDaemon

        # Add the export symbol list (see above)
        -Wl,--dynamic-list=${CMAKE_CURRENT_LIST_DIR}/exported.syms

        # Import the 3rd party libraries
        systemd
        JsonCpp::JsonCpp
        CTemplate::libctemplate
        DBUS::libdbus
        LIBNL::libnl
        LIBNL::libnl-route

        # iptc
        # ip4tc

        # Adds pthread support (if it's a separate library on target)
        Threads::Threads

        # Adds dlopen support (if it's a separate library on target)
        ${CMAKE_DL_LIBS}

        )


install( # Specifies the target to install
        TARGETS DobbyDaemon

        RUNTIME DESTINATION sbin

        )


# Install the config file for the target platform
if( RDK_PLATFORM STREQUAL "XI6" )
        set( DOBBY_SETTINGS_FILE "settings/dobby.xi6.json" )
elseif( RDK_PLATFORM STREQUAL "XI1" )
        set( DOBBY_SETTINGS_FILE "settings/dobby.xi1.json" )
elseif( RDK_PLATFORM STREQUAL "LLAMA" )
        set( DOBBY_SETTINGS_FILE "settings/dobby.llama.json" )
elseif( RDK_PLATFORM STREQUAL "DEV_VM" )
        set( DOBBY_SETTINGS_FILE "settings/dobby.dev_vm.json" )
else()
        message( FATAL_ERROR "Unknown RDK_PLATFORM '${RDK_PLATFORM}'" )
endif()

install(
        FILES       "${DOBBY_SETTINGS_FILE}"
        RENAME      dobby.json
        DESTINATION /etc/sky/ )



# Install a systemd launch service config and enables it by default
install(
        FILES       sky-dobby.service
        DESTINATION /lib/systemd/system/ )

# The following doesn't seem to work in RDK, disabling for now
# install(
#        CODE "execute_process( \
#            COMMAND ${CMAKE_COMMAND} -E create_symlink \
#            /lib/systemd/system/sky-dobby.service \
#            $DESTDIR/${CMAKE_INSTALL_PREFIX}/etc/systemd/system/multi-user.target.wants/sky-dobby.service \
#            )" )

