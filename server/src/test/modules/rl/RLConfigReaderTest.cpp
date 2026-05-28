/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "RLMstxConfigReader.h"
#include "FileUtil.h"
#include "gtest/gtest.h"
#include "framework/DtFramework.h"
#include <fstream>

using namespace Dic::Module::RL;
using namespace Dic;
using namespace DT::Framework;
class RLConfigReaderTest : public testing::Test {
  public:
    static std::string tempConfigPath;

  protected:
    static void SetUpTestSuite() {
        auto testDataPath = DT::Framework::DtFramework::GetTestDataDirPath(0);
        tempConfigPath = FileUtil::SplicePath(testDataPath, "rl", "RLConfig_tmp_test.json");
    }

    void WriteJsonIntoTestFile(const std::string &jsonStr) const {
        auto file = std::ofstream(RLConfigReaderTest::tempConfigPath, std::ios::out);
        file.write(jsonStr.c_str(), jsonStr.size());
        file.close();
    }

    static void TearDownTestSuite() { FileUtil::RemoveFile(tempConfigPath); }
};

std::string RLConfigReaderTest::tempConfigPath = "";

TEST_F(RLConfigReaderTest, emptyConfigPath) {
    RLMstxConfigReader reader;
    reader.SetConfigPath("");
    EXPECT_TRUE(reader.ReadConfigFile().empty());
}

TEST_F(RLConfigReaderTest, InvalidPath) {
    RLMstxConfigReader reader;
    reader.SetConfigPath("/home/xx/test_no_exist_dir");
    EXPECT_TRUE(reader.ReadConfigFile().empty());
}

TEST_F(RLConfigReaderTest, Normal) {
    std::string configPath = FileUtil::SplicePath(DtFramework::GetTestDataDirPath(0), "rl", "RLConfig.json");
    RLMstxConfigReader reader;
    reader.SetConfigPath(configPath);
    auto res = reader.ReadConfigFile();
    EXPECT_EQ(res.size(), 2); // expect 2 config
    auto &config1 = res[0];
    EXPECT_EQ(config1.algorithm, "grpo");
    EXPECT_EQ(config1.framework, "verl");
    EXPECT_EQ(config1.taskConfigs.size(), 2); // expect contains 2 stage
    EXPECT_EQ(config1.taskConfigs[0].roleName, "actor");
    EXPECT_EQ(config1.taskConfigs[0].taskName, "generator_sequence");
    EXPECT_EQ(config1.taskConfigs[0].microBatchConfigs.size(), 2); // stage contains 2 microbatch
    EXPECT_EQ(config1.taskConfigs[0].microBatchConfigs[0].batchName, "actor_micro_batch1");
    EXPECT_EQ(config1.taskConfigs[0].microBatchConfigs[0].type, "FP");
}

TEST_F(RLConfigReaderTest, read_config_null) {
    RLMstxConfigReader reader;
    WriteJsonIntoTestFile("");
    reader.SetConfigPath(RLConfigReaderTest::tempConfigPath);
    auto res = reader.ReadConfigFile();
    EXPECT_EQ(res.size(), 0);
}

TEST_F(RLConfigReaderTest, read_config_array) {
    RLMstxConfigReader reader;
    WriteJsonIntoTestFile("[]");
    reader.SetConfigPath(RLConfigReaderTest::tempConfigPath);
    auto res = reader.ReadConfigFile();
    EXPECT_EQ(res.size(), 0);
}

TEST_F(RLConfigReaderTest, read_config_no_config_key) {
    RLMstxConfigReader reader;
    WriteJsonIntoTestFile("{}");
    reader.SetConfigPath(RLConfigReaderTest::tempConfigPath);
    auto res = reader.ReadConfigFile();
    EXPECT_EQ(res.size(), 0);
}

TEST_F(RLConfigReaderTest, read_config_no_role_key) {
    RLMstxConfigReader reader;
    WriteJsonIntoTestFile(R"({
  "config":[
    {
      "framework":"verl",
      "algorithm":"grpo",
      "tasks":[
        {
          "roleName":"actor",
          "taskName":"generator_sequence",
          "microBatches":[
            {
              "name":  "actor_micro_batch1",
              "type": "FP"
            },
            {
              "name": "actor_micro_batch2",
              "type": "BP"
            }
          ]
        },
      ]
    },
  ]
})");
    reader.SetConfigPath(RLConfigReaderTest::tempConfigPath);
    auto res = reader.ReadConfigFile();
    EXPECT_EQ(res.size(), 0);
}
