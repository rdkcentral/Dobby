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


add_component_gtests(
        AppInfraCommon

        SOURCES
        "${CMAKE_CURRENT_LIST_DIR}/source/AppIdTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/AI_MD5_gtest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/Base64Test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/Boolean_gtest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/CallerThreadDispatcherTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/ConditionVariableTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/FileUtilitiesTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/FileWatcherTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/IDGeneratorTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/ISO8601Test.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/MutexTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/NotifierTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/ScratchSpaceTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/SignallingFifoTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/TerminateHandlerTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/ThreadedDispatcherTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/TimerTest.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/source/VersionNumberTest.cpp"

        DEFINES
        DISABLE_TERMINATE_LOG_TO_STDERR=1

        INCLUDES
        "${CMAKE_CURRENT_LIST_DIR}/../../Public"
        ${Boost_INCLUDE_DIRS}
        ${CURL_INCLUDE_DIRS}

        COMPONENTS
        AppInfraCommon

        LIBRARIES
        CURL::libcurl

)

