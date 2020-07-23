/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2014 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// File        :   Unit-tests for ISO8601 parsing - we support a small subset - should parse to chrono format anyway
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ISO8601.h>
#include <Logging.h>

#include <gtest/gtest.h>

using namespace AICommon;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ISO8601, MinMaxTest) {
    AICommon::initLogging();
    AI_LOG_FN_ENTRY();
    
    ISO8601 dtMin(std::chrono::system_clock::time_point::min());
    AI_LOG_DEBUG("min time as string is %s", dtMin.toString().c_str());

    ISO8601 dtMax(std::chrono::system_clock::time_point::max());
    AI_LOG_DEBUG("max time as string is %s", dtMax.toString().c_str());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ISO8601, ParseDateZeroLengthString) {
    ISO8601 dt("");
    ASSERT_FALSE(dt.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ISO8601, ParseDateIncorrectYear) {
    ISO8601 dt("999a-06-04");
    ASSERT_FALSE(dt.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ISO8601, ParseDateIncorrectMonth) {
    ISO8601 dt("2014-19-04");
    ASSERT_FALSE(dt.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ISO8601, ParseDateIncorrectDay) {
    ISO8601 dt("2014-06-40");
    ASSERT_FALSE(dt.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ISO8601, ParseDateIncorrectDayOfMonth) {
    ISO8601 dt("2019-04-31");
    ASSERT_FALSE(dt.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ISO8601, ParseDateIncorrectLeapYearDay) {
    ISO8601 dt("2017-02-29");
    ASSERT_FALSE(dt.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ISO8601, ParseDate) {
    ISO8601 dt("2014-06-04");
    ASSERT_EQ("2014-06-04T00:00:00Z", dt.toString());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ISO8601, ParseDateTime) {
    ISO8601 dt("2014-06-04T10:47Z");
    ASSERT_EQ("2014-06-04T10:47:00Z", dt.toString());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(ISO8601, ParseDateTimeSeconds) {
    ISO8601 dt("2014-06-04T10:47:59Z");
    ASSERT_EQ("2014-06-04T10:47:59Z", dt.toString());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
