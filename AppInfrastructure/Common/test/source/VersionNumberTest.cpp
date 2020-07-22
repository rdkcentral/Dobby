/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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
/// Function: Unit tests for VersionNumber parsing and comparison
/// Copyright: (C) Sky UK 2014+
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <VersionNumber.h>

#include <gtest/gtest.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

using namespace AICommon;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, Parse1) {
    VersionNumber vn("999");
    EXPECT_EQ(VersionNumber::VNE_OK, vn.state);
    EXPECT_TRUE(vn.isValid());
    EXPECT_EQ(1U, vn.fields);
    EXPECT_EQ(999U, vn.field[0]);
    EXPECT_EQ(0U, vn.field[1]);
    EXPECT_EQ(0U, vn.field[2]);
    EXPECT_EQ(0U, vn.field[3]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, Parse2) {
    VersionNumber vn("12.34");
    EXPECT_EQ(VersionNumber::VNE_OK, vn.state);
    EXPECT_TRUE(vn.isValid());
    EXPECT_EQ(2U, vn.fields);
    EXPECT_EQ(12U, vn.field[0]);
    EXPECT_EQ(34U, vn.field[1]);
    EXPECT_EQ(0U, vn.field[2]);
    EXPECT_EQ(0U, vn.field[3]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, Parse3) {
    VersionNumber vn("10.21.33");
    EXPECT_EQ(VersionNumber::VNE_OK, vn.state);
    EXPECT_TRUE(vn.isValid());
    EXPECT_EQ(3U, vn.fields);
    EXPECT_EQ(10U, vn.field[0]);
    EXPECT_EQ(21U, vn.field[1]);
    EXPECT_EQ(33U, vn.field[2]);
    EXPECT_EQ(0U, vn.field[3]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, Parse4) {
    VersionNumber vn("71.45.13.4");
    EXPECT_EQ(VersionNumber::VNE_OK, vn.state);
    EXPECT_TRUE(vn.isValid());
    EXPECT_EQ(4U, vn.fields);
    EXPECT_EQ(71U, vn.field[0]);
    EXPECT_EQ(45U, vn.field[1]);
    EXPECT_EQ(13U, vn.field[2]);
    EXPECT_EQ(4U, vn.field[3]);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, Parse5) {
    VersionNumber vn("45.12.892.13.12");
    EXPECT_EQ(VersionNumber::VNE_TooManyFields, vn.state);
    EXPECT_FALSE(vn.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, ParseUnknown1) {
    VersionNumber vn("abcdef");
    EXPECT_EQ(VersionNumber::VNE_IllegalCharacter, vn.state);
    EXPECT_FALSE(vn.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, ParseUnknown2) {
    VersionNumber vn("12.a");
    EXPECT_EQ(VersionNumber::VNE_IllegalCharacter, vn.state);
    EXPECT_FALSE(vn.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, ParseEmptyString) {
    VersionNumber vn("");
    EXPECT_EQ(VersionNumber::VNE_Nonsense, vn.state);
    EXPECT_FALSE(vn.isValid());
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareEQa) {
    VersionNumber vn1("1");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("1");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 == vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareEQb) {
    VersionNumber vn1("2.9");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("2.9");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 == vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareEQc) {
    VersionNumber vn1("9999.9999.9999.9999");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("9999.9999.9999.9999");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 == vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareEQd) {
    VersionNumber vn1("4294967295.4294967295.4294967295.4294967295");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("4294967295.4294967295.4294967295.4294967295");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 == vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareNE) {
    VersionNumber vn1("1.2");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("1.3");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 == vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLT1) {
    VersionNumber vn1("1.3");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("1.2");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 < vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLT2) {
    VersionNumber vn1("1.2");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("1.3");
    EXPECT_TRUE(vn1 < vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLT3) {
    VersionNumber vn1("10.2");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("1.1");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 < vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLT4a) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("2.4.3.7");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 < vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLT4b) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("2.4.4.6");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 < vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLT4c) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("2.5.3.6");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 < vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLT4d) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("3.4.3.6");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 < vn2);
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLTE1) {
    VersionNumber vn1("1.3");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("1.2");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 <= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLTE2) {
    VersionNumber vn1("1.2");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("1.2");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 <= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLTE3) {
    VersionNumber vn1("10.2");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("1.1");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 <= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLTE4a) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("2.4.3.6");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 <= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLTE4b) {
    VersionNumber vn1("1.0.5");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("1.0.4");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 <= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLTE4c) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("2.4.3.7");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 <= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLTE4d) {
    VersionNumber vn1("6.1");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("7.0");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 <= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareLTE4de) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("3.4.3.6");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 <= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareGTa) {
    VersionNumber vn1("3.1.4.1");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("3.1.4.1");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 > vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareGTb) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("3.4.3.7");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 > vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareGTc) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("3.4.4.6");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 > vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareGTd) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("3.5.3.6");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 > vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareGTe) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("4.4.3.6");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 > vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareGTf) {
    VersionNumber vn1("7.2.1");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("7.0.1");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 > vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareGTEa) {
    VersionNumber vn1("3.1.4.1");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("3.1.4.1");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 >= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareGTEb) {
    VersionNumber vn1("2.4.3.6");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("2.4.3.7");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_FALSE(vn1 >= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST(VersionNumber, CompareGTEc) {
    VersionNumber vn1("4");
    EXPECT_TRUE(vn1.isValid());
    VersionNumber vn2("3");
    EXPECT_TRUE(vn2.isValid());
    EXPECT_TRUE(vn1 >= vn2);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
