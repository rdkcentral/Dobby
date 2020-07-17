# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
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

# - Find libnl
#
# This module defines
#  LIBNL_FOUND - whether the libnl library was found
#  LIBNL_LIBRARIES - the libnl library
#  LIBNL_INCLUDE_DIR - the include path of the libnl library

find_library( LIBNL_LIBRARY nl-3 )
find_library( LIBNL_ROUTE_LIBRARY nl-route-3 )
find_path( LIBNL_INCLUDE_DIR NAMES netlink/netlink.h PATH_SUFFIXES libnl3 )

include( FindPackageHandleStandardArgs )
find_package_handle_standard_args( LIBNL DEFAULT_MSG
        LIBNL_LIBRARY LIBNL_ROUTE_LIBRARY LIBNL_INCLUDE_DIR )

mark_as_advanced( LIBNL_INCLUDE_DIR LIBNL_LIBRARY LIBNL_ROUTE_LIBRARY )


if( LIBNL_FOUND )
    set( LIBNL_LIBRARIES ${LIBNL_LIBRARY} ${LIBNL_ROUTE_LIBRARY} )
    set( LIBNL_INCLUDE_DIRS ${LIBNL_INCLUDE_DIR} )
endif()

if( LIBNL_FOUND AND NOT TARGET LIBNL::libnl )
    add_library( LIBNL::libnl SHARED IMPORTED )
    set_target_properties( LIBNL::libnl PROPERTIES
            IMPORTED_LOCATION "${LIBNL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBNL_INCLUDE_DIRS}" )

    add_library( LIBNL::libnl-route SHARED IMPORTED )
    set_target_properties( LIBNL::libnl-route PROPERTIES
            IMPORTED_LOCATION "${LIBNL_ROUTE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBNL_INCLUDE_DIRS}" )

endif()
