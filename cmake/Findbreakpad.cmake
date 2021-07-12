# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2021 Sky UK
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

# - Try to find the breakpad_wrapper library.

find_path( BREAKPAD_INCLUDE_DIR NAMES breakpad_wrapper.h )
find_library( BREAKPAD_LIBRARY NAMES libbreakpadwrapper.so )

include( FindPackageHandleStandardArgs )

# Handle the QUIETLY and REQUIRED arguments and set the BREAKPAD_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args( BREAKPAD DEFAULT_MSG
        BREAKPAD_LIBRARY BREAKPAD_INCLUDE_DIR )

mark_as_advanced( BREAKPAD_INCLUDE_DIR BREAKPAD_LIBRARY )

if( BREAKPAD_FOUND )
    set( BREAKPAD_LIBRARIES ${BREAKPAD_LIBRARY} )
    set( BREAKPAD_INCLUDE_DIRS ${BREAKPAD_INCLUDE_DIR} )
endif()

if( BREAKPAD_FOUND AND NOT TARGET BREAKPAD::BREAKPAD )
    add_library( Breakpad::BreakpadWrapper SHARED IMPORTED )
    set_target_properties( Breakpad::BreakpadWrapper PROPERTIES
            IMPORTED_LOCATION "${BREAKPAD_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${BREAKPAD_INCLUDE_DIRS}" )
endif()