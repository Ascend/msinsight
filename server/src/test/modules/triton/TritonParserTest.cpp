/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include "JsonUtil.h"
#include "TritonParser.h"
#include "TritonService.h"

using namespace Dic::Module::Triton;

class TestTritonParser : public TritonParser {
  public:
    Dic::Module::Triton::ParseResult TestParseOneTriton(const std::string &memFile) { return ParseOneTriton(memFile); }
};

class TritonParserTest : public ::testing::Test {
  protected:
    void SetUp() override { TritonService::Instance().Reset(); }

    void TearDown() override { TritonService::Instance().Reset(); }

    void CreateTempJsonFile(const std::string &filename, const std::string &content) {
        std::ofstream ofs(filename);
        ofs << content;
        ofs.close();
    }
};

/**
 * @brief 场景说明：测试 TritonParser 对正常 Triton JSON 文件的解析功能。
 * 覆盖 Header 和 Record 的解析，并验证数据是否正确存入 TritonService。
 */
TEST_F(TritonParserTest, ParseOneTritonSuccessTest) {
    std::string tempFile = "temp_triton_info.json";
    std::string content = R"({
        "Header": {
            "KernelName": "mock_kernel"
        },
        "Record": [
            {
                "scope": "cc",
                "status": "success",
                "err_msg": "none",
                "memory_info_array": [
                    {
                        "alloc_time_in_ir": 1000,
                        "buffer": "buf1",
                        "source_location": "test.py:10",
                        "life_time_in_ir": [1000, 2000],
                        "extent": 1024,
                        "offset": [0, 1024],
                        "is_tmpbuf":false
                    }
                ]
            }
        ]
    })";

    CreateTempJsonFile(tempFile, content);

    TestTritonParser parser;
    auto result = parser.TestParseOneTriton(tempFile);

    EXPECT_TRUE(result.IsSuccess());
    EXPECT_EQ(TritonService::Instance().GetHeader().kernelName, "mock_kernel");

    auto segments = TritonService::Instance().QuerySegmentsContainRange("cc", 1000);
    ASSERT_EQ(segments.size(), 1);
    EXPECT_EQ(segments[0].buffer, "buf1");
    EXPECT_EQ(segments[0].size, 256); // extend(1024) * offset.size(2)

    std::remove(tempFile.c_str());
}

/**
 * @brief 场景说明：测试 TritonParser 对异常 JSON 格式（缺失 Header）的错误处理。
 */
TEST_F(TritonParserTest, ParseOneTritonFailTest) {
    std::string tempFile = "temp_triton_fail.json";
    std::string content = R"({
        "Record": []
    })"; // 缺失 Header

    CreateTempJsonFile(tempFile, content);

    TestTritonParser parser;
    auto result = parser.TestParseOneTriton(tempFile);

    EXPECT_FALSE(result.IsSuccess());

    std::remove(tempFile.c_str());
}

/**
 * @brief 场景说明：测试 TritonParser 对异常 Record 结构的错误处理。
 */
TEST_F(TritonParserTest, ParseOneTritonRecordFailTest) {
    std::string tempFile = "temp_triton_record_fail.json";
    std::string content = R"({
        "Header": {
            "KernelName": "mock_kernel"
        },
        "Record": [
            {
                "scope": "cc",
                "status": "success"
                // 缺失 memory_info_array
            }
        ]
    })";

    CreateTempJsonFile(tempFile, content);

    TestTritonParser parser;
    auto result = parser.TestParseOneTriton(tempFile);

    EXPECT_FALSE(result.IsSuccess());

    std::remove(tempFile.c_str());
}

/**
 * @brief 场景说明：测试 TritonParser 的 IsParsed 功能。
 * 验证是否能正确识别 Triton 相关的内存信息文件。
 */
TEST_F(TritonParserTest, IsParsedTest) {
    auto &parser = TritonParser::Instance();
    EXPECT_FALSE(parser.IsParsed("some/path/memory_info.json"));
    EXPECT_FALSE(parser.IsParsed("some/path/other_file.json"));
    EXPECT_FALSE(parser.IsParsed(""));
}
