/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
/// File        :   Unit-tests for Boolean parsing
/// Copyright   :   Sky UK 2014
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <Boolean.h>

#include <gtest/gtest.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace std;
using namespace AICommon;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(BooleanTests, ParseNullStrings) {
    Boolean boolean_empty("");
    EXPECT_FALSE(boolean_empty.isValid());
    EXPECT_FALSE(boolean_empty);

    string empty_string;
    Boolean boolean_empty_string;
    boolean_empty_string.fromString(empty_string);
    EXPECT_FALSE(boolean_empty_string.isValid());
    EXPECT_FALSE(boolean_empty_string);
    
    Boolean boolean_null((const char *) (NULL));
    EXPECT_FALSE(boolean_null.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(BooleanTests, CheckCtors) {
    Boolean boolean_true(true);
    EXPECT_TRUE(boolean_true.isValid());
    EXPECT_TRUE(boolean_true);

    Boolean boolean_false(false);
    EXPECT_TRUE(boolean_false.isValid());
    EXPECT_FALSE(boolean_false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(BooleanTests, CheckTooMuchInput) {
    Boolean boolean_true_but_not_really("true but not really");
    EXPECT_FALSE(boolean_true_but_not_really.isValid());
    EXPECT_FALSE(boolean_true_but_not_really);

    Boolean boolean_false_but_not_really("false but not really");
    EXPECT_FALSE(boolean_false_but_not_really.isValid());
    EXPECT_FALSE(boolean_false_but_not_really);

    Boolean boolean_but_not_really_true("but not really true");
    EXPECT_FALSE(boolean_but_not_really_true.isValid());
    EXPECT_FALSE(boolean_but_not_really_true);

    Boolean boolean_but_not_really_false("but not really false");
    EXPECT_FALSE(boolean_but_not_really_false.isValid());
    EXPECT_FALSE(boolean_but_not_really_false);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(BooleanTests, CheckCorrectInputs) {
    Boolean boolean_true("true");
    EXPECT_TRUE(boolean_true.isValid());
    EXPECT_TRUE(boolean_true);
    EXPECT_EQ("true", boolean_true.toString());

    Boolean boolean_false("false");
    EXPECT_TRUE(boolean_false.isValid());
    EXPECT_FALSE(boolean_false);
    EXPECT_EQ("false", boolean_false.toString());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(BooleanTests, CheckCopy) {
    Boolean boolean_true("true");
    Boolean boolean_true_copy(boolean_true);
    EXPECT_EQ(boolean_true.isValid(), boolean_true_copy.isValid());
    EXPECT_EQ(boolean_true, boolean_true_copy);
    
    Boolean boolean_invalid("rum bunch");
    Boolean boolean_invalid_copy(boolean_invalid);
    EXPECT_EQ(boolean_invalid.isValid(), boolean_invalid_copy.isValid());
    EXPECT_EQ(boolean_invalid, boolean_invalid_copy);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
