/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2024 Sky UK
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

// All system and third-party headers must come BEFORE #define private public
// to prevent the macro from mangling standard library internals.
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ctemplate/template.h>
#include <json/json.h>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <memory>
#include <unistd.h>

// DobbySettingsMock.h pulls in gmock which eventually includes <sstream>;
// it must be included before #define private public.
#include "DobbySettingsMock.h"

// Open up private members of DobbySpecConfig so tests can access
// mDictionary directly and call processSwapLimit.
#define private public
#include "DobbySpecConfig.h"
#include "DobbyTemplate.h"
// Undefine immediately so the macro does not leak into gtest/gmock headers
// below, which can cause hard-to-diagnose build failures on some compilers.
#undef private

using ::testing::NiceMock;
using ::testing::Return;

// ── Minimal valid Dobby spec strings ─────────────────────────────────────────

static const char* kSpecMemOnly = R"({
    "version": "1.0",
    "args": ["/bin/true"],
    "user": { "uid": 1000, "gid": 1000 },
    "memLimit": 2998272
})";

static const char* kSpecWithSwap = R"({
    "version": "1.0",
    "args": ["/bin/true"],
    "user": { "uid": 1000, "gid": 1000 },
    "memLimit": 2998272,
    "swapLimit": 5996544
})";

static const char* kSpecSwapEqualsLimit = R"({
    "version": "1.0",
    "args": ["/bin/true"],
    "user": { "uid": 1000, "gid": 1000 },
    "memLimit": 2998272,
    "swapLimit": 2998272
})";

static const char* kSpecSwapBelowLimit = R"({
    "version": "1.0",
    "args": ["/bin/true"],
    "user": { "uid": 1000, "gid": 1000 },
    "memLimit": 5996544,
    "swapLimit": 2998272
})";

// ── Inline ctemplate for reading MEM_LIMIT / MEM_SWAP back from the dict ─────
static const char* kMemTemplateName = "test_swap_memory";
static const char* kMemTemplateStr  = "LIMIT={{MEM_LIMIT}} SWAP={{MEM_SWAP}}";

// ── Fixture ───────────────────────────────────────────────────────────────────

class DobbySpecConfigTest : public ::testing::Test
{
protected:
    char mTmpDir[64];

    NiceMock<DobbySettingsMock>* p_settingsMock = nullptr;
    std::shared_ptr<IDobbySettings> mSettings;
    std::shared_ptr<DobbyBundle>    mBundle;

    void SetUp() override
    {
        // Reserve a unique path for the bundle directory.
        // mkdtemp creates the directory; we immediately remove it so that
        // DobbyBundle(path, persist) can create it itself via mkdir().
        std::strcpy(mTmpDir, "/tmp/dobby_spectest_XXXXXX");
        ASSERT_NE(mkdtemp(mTmpDir), nullptr) << "mkdtemp failed";
        ::rmdir(mTmpDir);  // let DobbyBundle create the dir

        // Settings mock – return empty/null for GPU, VPU, plugins.
        p_settingsMock = new NiceMock<DobbySettingsMock>();
        ON_CALL(*p_settingsMock, gpuAccessSettings())
            .WillByDefault(Return(nullptr));
        ON_CALL(*p_settingsMock, vpuAccessSettings())
            .WillByDefault(Return(nullptr));
        ON_CALL(*p_settingsMock, defaultPlugins())
            .WillByDefault(Return(std::vector<std::string>{}));
        ON_CALL(*p_settingsMock, rdkPluginsData())
            .WillByDefault(Return(Json::Value(Json::objectValue)));
        ON_CALL(*p_settingsMock, extraEnvVariables())
            .WillByDefault(Return(std::map<std::string, std::string>{}));

        // Use a no-op deleter; fixture owns the raw pointer.
        mSettings = std::shared_ptr<IDobbySettings>(p_settingsMock,
                        [](IDobbySettings*){});

        // Real DobbyBundle pointing at the temp directory.
        // utils is not used in the constructor body so nullptr is safe.
        mBundle = std::make_shared<DobbyBundle>(
                        std::shared_ptr<const IDobbyUtils>(nullptr),
                        std::string(mTmpDir),
                        /*persist=*/true);

        // Initialise the DobbyTemplate singleton with our settings before any
        // DobbySpecConfig is constructed (parseSpec calls applyAt internally).
        DobbyTemplate::setSettings(mSettings);

        // Register a tiny inline template so we can read MEM_LIMIT / MEM_SWAP
        // from the populated dictionary without parsing the full OCI JSON.
        ctemplate::StringToTemplateCache(
            kMemTemplateName,
            kMemTemplateStr,
            ctemplate::DO_NOT_STRIP);
    }

    void TearDown() override
    {
        // Remove any config.json written during the test.
        std::string cfg = std::string(mTmpDir) + "/config.json";
        ::remove(cfg.c_str());
        ::rmdir(mTmpDir);
        delete p_settingsMock;
    }

    std::unique_ptr<DobbySpecConfig> makeConfig(const std::string& specJson)
    {
        return std::make_unique<DobbySpecConfig>(
                    std::shared_ptr<IDobbyUtils>(nullptr),
                    mSettings,
                    mBundle,
                    specJson);
    }

    // Expand the mini template against the config's ctemplate dictionary.
    // Returns e.g. "LIMIT=2998272 SWAP=2998272".
    std::string expandMemTemplate(DobbySpecConfig& cfg)
    {
        std::string out;
        ctemplate::ExpandTemplate(
            kMemTemplateName,
            ctemplate::DO_NOT_STRIP,
            cfg.mDictionary,
            &out);
        return out;
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

/**
 * When 'swapLimit' is absent, MEM_SWAP must default to the same value as
 * MEM_LIMIT (no extra swap beyond the memory limit).
 */
TEST_F(DobbySpecConfigTest, SwapLimit_DefaultsToMemLimit)
{
    auto cfg = makeConfig(kSpecMemOnly);
    EXPECT_TRUE(cfg->isValid());
    EXPECT_EQ(expandMemTemplate(*cfg), "LIMIT=2998272 SWAP=2998272");
}

/**
 * When 'swapLimit' is greater than 'memLimit', MEM_SWAP must be set to the
 * supplied swap limit independently of MEM_LIMIT.
 */
TEST_F(DobbySpecConfigTest, SwapLimit_SetIndependently)
{
    auto cfg = makeConfig(kSpecWithSwap);
    EXPECT_TRUE(cfg->isValid());
    EXPECT_EQ(expandMemTemplate(*cfg), "LIMIT=2998272 SWAP=5996544");
}

/**
 * When 'swapLimit' equals 'memLimit' (minimum valid value), parsing must
 * succeed and MEM_SWAP must equal the shared value.
 */
TEST_F(DobbySpecConfigTest, SwapLimit_EqualToMemLimit_Succeeds)
{
    auto cfg = makeConfig(kSpecSwapEqualsLimit);
    EXPECT_TRUE(cfg->isValid());
    EXPECT_EQ(expandMemTemplate(*cfg), "LIMIT=2998272 SWAP=2998272");
}

/**
 * When 'swapLimit' < 'memLimit', processSwapLimit must reject the value
 * and parsing must fail (kernel requires memsw >= mem).
 */
TEST_F(DobbySpecConfigTest, SwapLimit_LessThanMemLimit_Fails)
{
    auto cfg = makeConfig(kSpecSwapBelowLimit);
    EXPECT_FALSE(cfg->isValid());
}

/**
 * When 'swapLimit' is not an integer, processSwapLimit must return false.
 * Verify by calling the private method directly.
 */
TEST_F(DobbySpecConfigTest, SwapLimit_NonIntegral_Fails)
{
    auto cfg = makeConfig(kSpecMemOnly);

    ctemplate::TemplateDictionary dict("test_nonint");
    Json::Value badSwap("not-a-number");
    EXPECT_FALSE(cfg->processSwapLimit(badSwap, &dict));
}
