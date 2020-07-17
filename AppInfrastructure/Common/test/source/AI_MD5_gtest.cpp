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

#include <fstream>
#include <stdio.h>
#include <unistd.h>
#include <vector>

#include <FileUtilities.h>
#include <ScratchSpace.h>
#include <Logging.h>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace std;
using namespace testing;
using namespace AICommon;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct MD5TestConfig {
    unsigned length, start, base;
};

static vector<MD5TestConfig> md5Tests {
    {   0x0000,     0x0000,      0x0000 },
    {   0x1000,     0x0000,      0x0000 },
    {   0x1001,     0x0000,      0x0001 },
    {   0x1001,     0x007f,      0x0001 },
    {   0x8888,     0x0043,      0x0080 },
    {   0x07ff,     0x007f,      0x0001 },
    {   0x0100,     0x0000,      0x0000 },
    {   0x0100,     0x0000,      0x0001 },
    {   0x0100,     0x0000,      0x0010 },
    {   0x0100,     0x0000,      0x0011 },
    {   0x0100,     0x0000,      0x00ff },
    {   0x0001,     0x00aa,      0x0001 },
    {   0x0002,     0x0088,      0x0001 },
    {   0x0003,     0x001f,      0x0001 },
    {   0x0007,     0x007a,      0x0001 },
    {   0x0008,     0x00aa,      0x0001 },
    {   0x0009,     0x00ab,      0x0001 },
    {   0x000f,     0x00aa,      0x0001 },
    {   0x0010,     0x00ac,      0x0001 },
    {   0x0011,     0x001a,      0x0001 },
    {   0x1000000,     0x91543812,  0x0000 },
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void generateJunk(const std::string & filePath, unsigned length, unsigned start, unsigned base) {
    ofstream out(filePath);
    for (unsigned c = 0; c < length; c++) {
        start--;
        start = start * ~start;
        out.put(((start + base) & 0xff));
    }
    out.close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static std::string runCommandGetOutput(const std::string & command) {
    char data[1024];
    FILE * fp = popen(command.c_str(), "r");
    if (fp) {
        char * dataBack = fgets(data, sizeof(data), fp);
        pclose(fp);
        if (dataBack) {
            return string(dataBack);
        }
        pclose(fp);
    }
    return string();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct MD5TestParams : ::TestWithParam<int> {
    void SetUp() {
        AICommon::initLogging();
        config = &(md5Tests[GetParam()]);
    }
    MD5TestConfig * config;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

TEST_P(MD5TestParams, testHash) {
    ScratchSpace scratch("/tmp");
    std::string folder = scratch.path();

    string filePathTemp = folder + "/test_md5.bin";
    generateJunk(filePathTemp, config->length, config->start, config->base);
    string aiMD5 = AICommon::fileMD5(filePathTemp);

#if defined(__linux__)
    std::string commandOutput = runCommandGetOutput("md5sum " + filePathTemp);
#else
    std::string commandOutput = runCommandGetOutput("md5 -r " + filePathTemp);
#endif
    ASSERT_GE(commandOutput.length(), 32U);
    std::string cliMD5 = commandOutput.substr(0, 32);
    ASSERT_EQ(cliMD5, aiMD5);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TEST_CASE_P(MD5Tests,
                        MD5TestParams,
                        Range(0, int(md5Tests.size())));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct MD5ConstTests {
    const char * input, * output;
};

// Tests based on test suite defined in section A.5 of RFC1321
static std::vector<MD5ConstTests> md5ConstTests {
    {
        "",
        "d41d8cd98f00b204e9800998ecf8427e"
    }, {
        "a",
        "0cc175b9c0f1b6a831c399e269772661"
    }, {
        "abc",
        "900150983cd24fb0d6963f7d28e17f72"
    }, {
        "message digest",
        "f96b697d7cb7938d525a2f31aaf161d0"
    }, {
        "abcdefghijklmnopqrstuvwxyz",
        "c3fcd3d76192e4007dfb496cca67e13b"
    }, {
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
        "d174ab98d277d9f5a5611c2c9f419d9f"
    }, {
        "12345678901234567890123456789012345678901234567890123456789012345678901234567890",
        "57edf4a22be3c955ac49da2e2107b67a"
    },
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct MD5ConstTestParams : ::TestWithParam<int> {
    void SetUp() {
        AICommon::initLogging();
        config = &(md5ConstTests[GetParam()]);
    }
    MD5ConstTests * config;
};

TEST_P(MD5ConstTestParams, testHash) {
    ScratchSpace scratch("/tmp");
    std::string folder = scratch.path();

    string filePathTemp = folder + "/test_md5.bin";
    FILE * testFile = fopen(filePathTemp.c_str(), "w");
    ASSERT_NE(testFile, (FILE *) (0));
    ASSERT_EQ(fwrite(config->input, 1, strlen(config->input), testFile), strlen(config->input));
    fclose(testFile);
    string aiMD5 = AICommon::fileMD5(filePathTemp);
    ASSERT_EQ(aiMD5, config->output);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TEST_CASE_P(MD5ConstTests,
                        MD5ConstTestParams,
                        Range(0, int(md5ConstTests.size())));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
