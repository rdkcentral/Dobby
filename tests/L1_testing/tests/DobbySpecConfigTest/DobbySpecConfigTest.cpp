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

// Open private members so processSwapLimit / mSpec / mDictionary are accessible
#define private public
#include "DobbySpecConfig.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <ctemplate/template.h>
#include <json/json.h>

#include "DobbySettingsMock.h"
#include "DobbyBundleMock.h"
#include "DobbyUtilsMock.h"
#include "ContainerIdMock.h"

using ::testing::NiceMock;
using ::testing::Return;

// ── Static impl pointers required by the mock delegate pattern ────────────────
DobbyBundleImpl* DobbyBundle::impl  = nullptr;
DobbyUtilsImpl*  DobbyUtils::impl   = nullptr;
ContainerIdImpl* ContainerId::impl  = nullptr;

// ── Minimal valid Dobby spec strings ─────────────────────────────────────────
//   (mandatory fields: version, args, user, memLimit)

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

// ── Inline ctemplate used to read MEM_LIMIT / MEM_SWAP out of the dictionary ──
static const char* kMemTemplateName = "test_swap_memory";
static const char* kMemTemplateStr  = "LIMIT={{MEM_LIMIT}} SWAP={{MEM_SWAP}}";

// ── Test fixture ──────────────────────────────────────────────────────────────

class DobbySpecConfigTest : public ::testing::Test
{
protected:
    NiceMock<DobbySettingsMock>* p_settingsMock = nullptr;
    NiceMock<DobbyBundleMock>*   p_bundleMock   = nullptr;
    NiceMock<DobbyUtilsMock>*    p_utilsMock    = nullptr;

    std::shared_ptr<IDobbySettings> mSettings;
    std::shared_ptr<DobbyBundle>    mBundle;
    std::shared_ptr<DobbyUtils>     mUtils;

    void SetUp() override
    {
        p_settingsMock = new NiceMock<DobbySettingsMock>();
        p_bundleMock   = new NiceMock<DobbyBundleMock>();
        p_utilsMock    = new NiceMock<DobbyUtilsMock>();

        DobbyBundle::setImpl(p_bundleMock);
        DobbyUtils::setImpl(p_utilsMock);

        // GPU/VPU settings not needed for these tests
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

        // dirFd() returns -1 so the template-write at step 8 of parseSpec
        // fails, but all dictionary values are populated by the preceding
        // steps. isValid() will be false for all configs constructed here.
        ON_CALL(*p_bundleMock, dirFd())
            .WillByDefault(Return(-1));

        // Use a no-op deleter so mock lifetime is managed by this fixture
        mSettings = std::shared_ptr<IDobbySettings>(p_settingsMock,
                        [](IDobbySettings*){});
        mBundle   = std::make_shared<DobbyBundle>();
        mUtils    = std::make_shared<DobbyUtils>();

        // Register a tiny inline template so we can read MEM_LIMIT / MEM_SWAP
        // back out of the dictionary without expanding the full OCI template.
        ctemplate::StringToTemplateCache(
            kMemTemplateName,
            kMemTemplateStr,
            ctemplate::DO_NOT_STRIP);
    }

    void TearDown() override
    {
        DobbyBundle::setImpl(nullptr);
        DobbyUtils::setImpl(nullptr);

        delete p_bundleMock;
        delete p_utilsMock;
        delete p_settingsMock;
    }

    // Construct a DobbySpecConfig from the given JSON string.
    std::unique_ptr<DobbySpecConfig> makeConfig(const std::string& specJson)
    {
        return std::make_unique<DobbySpecConfig>(mUtils, mSettings, mBundle, specJson);
    }

    // Expand the test template against the config's ctemplate dictionary.
    // Returns a string of the form "LIMIT=<N> SWAP=<M>".
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

// ── Test cases ────────────────────────────────────────────────────────────────

/**
 * When 'swapLimit' is absent, MEM_SWAP must default to MEM_LIMIT
 * (no extra swap beyond the memory limit).
 */
TEST_F(DobbySpecConfigTest, SwapLimit_DefaultsToMemLimit)
{
    auto cfg = makeConfig(kSpecMemOnly);
    EXPECT_EQ(expandMemTemplate(*cfg), "LIMIT=2998272 SWAP=2998272");
}

/**
 * When 'swapLimit' is greater than 'memLimit', MEM_SWAP must be set to the
 * supplied swap limit independently of MEM_LIMIT.
 */
TEST_F(DobbySpecConfigTest, SwapLimit_SetIndependently)
{
    auto cfg = makeConfig(kSpecWithSwap);
    EXPECT_EQ(expandMemTemplate(*cfg), "LIMIT=2998272 SWAP=5996544");
}

/**
 * When 'swapLimit' equals 'memLimit' (minimum valid value), parsing must
 * succeed and MEM_SWAP must equal the shared value.
 */
TEST_F(DobbySpecConfigTest, SwapLimit_EqualToMemLimit_Succeeds)
{
    auto cfg = makeConfig(kSpecSwapEqualsLimit);
    EXPECT_EQ(expandMemTemplate(*cfg), "LIMIT=2998272 SWAP=2998272");
}

/**
 * When 'swapLimit' < 'memLimit', processSwapLimit must return false because
 * the kernel rejects memory.memsw.limit_in_bytes < memory.limit_in_bytes.
 */
TEST_F(DobbySpecConfigTest, SwapLimit_LessThanMemLimit_Fails)
{
    // Construct a config so mSpec is populated with memLimit=5996544
    DobbySpecConfig cfg(mUtils, mSettings, mBundle, kSpecSwapBelowLimit);

    // Directly invoke processSwapLimit with a value that is below memLimit
    ctemplate::TemplateDictionary dict("test_below");
    Json::Value badSwap(2998272);   // 2998272 < 5996544
    EXPECT_FALSE(cfg.processSwapLimit(badSwap, &dict));
}

/**
 * When 'swapLimit' is not an integer, processSwapLimit must return false.
 */
TEST_F(DobbySpecConfigTest, SwapLimit_NonIntegral_Fails)
{
    DobbySpecConfig cfg(mUtils, mSettings, mBundle, kSpecMemOnly);

    ctemplate::TemplateDictionary dict("test_nonint");
    Json::Value badSwap("not-a-number");
    EXPECT_FALSE(cfg.processSwapLimit(badSwap, &dict));
}
