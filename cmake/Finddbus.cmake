# If not stated otherwise in this file or this component's license file the
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

# - Try to find the dbus library.
#
# The following are set after configuration is done:
#  DBUS_FOUND
#  DBUS_INCLUDE_DIRS
#  DBUS_LIBRARY_DIRS
#  DBUS_LIBRARIES

find_path( DBUS_INCLUDE_DIR NAMES dbus/dbus.h PATH_SUFFIXES dbus-1.0 )
find_path( DBUS_CFG_INCLUDE_DIR NAMES dbus/dbus-arch-deps.h PATH_SUFFIXES lib/dbus-1.0/include )
find_library( DBUS_LIBRARY NAMES libdbus-1.so dbus-1 )

# message( "DBUS_INCLUDE_DIR include dir = ${DBUS_INCLUDE_DIR}" )
# message( "DBUS_CFG_INCLUDE_DIR include dir = ${DBUS_CFG_INCLUDE_DIR}" )
# message( "DBUS_LIBRARY lib = ${DBUS_LIBRARY}" )

include(FindPackageHandleStandardArgs)

# Handle the QUIETLY and REQUIRED arguments and set the LM_SENSORS_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(DBUS DEFAULT_MSG
        DBUS_LIBRARY DBUS_INCLUDE_DIR DBUS_CFG_INCLUDE_DIR)

mark_as_advanced( DBUS_INCLUDE_DIR DBUS_CFG_INCLUDE_DIR DBUS_LIBRARY )

if( DBUS_FOUND )
    set( DBUS_LIBRARIES ${DBUS_LIBRARY} )
    set( DBUS_INCLUDE_DIRS ${DBUS_INCLUDE_DIR} ${DBUS_CFG_INCLUDE_DIR} )
endif()

if( DBUS_FOUND AND NOT TARGET DBUS::libdbus)
    add_library( DBUS::libdbus SHARED IMPORTED )
    set_target_properties( DBUS::libdbus PROPERTIES
            IMPORTED_LOCATION "${DBUS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${DBUS_INCLUDE_DIRS}"
            )
endif()