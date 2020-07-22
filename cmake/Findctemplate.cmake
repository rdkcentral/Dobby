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

# - Try to find the ctemplate library.
#
# The following are set after configuration is done:
#  CTEMPLATE_FOUND
#  CTEMPLATE_INCLUDE_DIRS
#  CTEMPLATE_LIBRARY_DIRS
#  CTEMPLATE_LIBRARIES

find_path( CTEMPLATE_INCLUDE_DIR NAMES ctemplate/template.h )
find_library( CTEMPLATE_LIBRARY NAMES libctemplate.so ctemplate )

#message( "CTEMPLATE_INCLUDE_DIR include dir = ${CTEMPLATE_INCLUDE_DIR}" )
#message( "CTEMPLATE_LIBRARY lib = ${CTEMPLATE_LIBRARY}" )

set( CTEMPLATE_LIBRARIES ${CTEMPLATE_LIBRARY} )
set( CTEMPLATE_INCLUDE_DIRS ${CTEMPLATE_INCLUDE_DIR} )

include( FindPackageHandleStandardArgs )

# Handle the QUIETLY and REQUIRED arguments and set the CTEMPLATE_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args( CTEMPLATE DEFAULT_MSG
        CTEMPLATE_LIBRARY CTEMPLATE_INCLUDE_DIR )

mark_as_advanced( CTEMPLATE_INCLUDE_DIR CTEMPLATE_LIBRARY )


if( CTEMPLATE_FOUND )
    set( CTEMPLATE_LIBRARIES ${DBUS_LIBRARY} )
    set( CTEMPLATE_INCLUDE_DIRS ${CTEMPLATE_INCLUDE_DIR} )
endif()

if( CTEMPLATE_FOUND AND NOT TARGET CTemplate::libctemplate )
    add_library( CTemplate::libctemplate SHARED IMPORTED )
    set_target_properties( CTemplate::libctemplate PROPERTIES
            IMPORTED_LOCATION "${CTEMPLATE_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CTEMPLATE_INCLUDE_DIRS}"
            )
endif()
