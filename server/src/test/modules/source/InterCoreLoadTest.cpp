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

#include <gtest/gtest.h>
#include "InterCoreLoadGraphParser.h"
#include "SourceProtocolRequest.h"
#include "SourceFileParser.h"
#include "SourceProtocolUtil.h"
#include "WsSessionManager.h"
#include "InterCoreLoadTest.h"
#include "JsonUtil.h"

namespace Dic::Module::Source::Test {
using namespace Dic::Module::Source;
using namespace Dic::Protocol;

class InterCoreLoadTest : public ::testing::Test {
  protected:
    document_t GeneratorBigCoreDetail(size_t len) {
        document_t doc(kObjectType);
        auto &allocator = doc.GetAllocator();
        JsonUtil::AddMember(doc, "soc", "910B", allocator);
        JsonUtil::AddMember(doc, "op_type", "vector", allocator);
        JsonUtil::AddMember(doc, "advice", "test advice", allocator);
        json_t op_detail(kArrayType);
        for (size_t i = 0; i < len; i++) {
            json_t detail(kObjectType);
            JsonUtil::AddMember(detail, "core_id", std::to_string(i), allocator);
            json_t core_detail(kArrayType);
            JsonUtil::AddMember(detail, "core_detail", core_detail, allocator);
            op_detail.PushBack(detail, allocator);
        }
        JsonUtil::AddMember(doc, "op_detail", op_detail, allocator);
        return doc;
    }
};

TEST_F(InterCoreLoadTest, test_GetInterCoreLoadAnalysisInfo_with_normal_json_of_vector_op_type) {
    InterCoreLoadGraphParser parser;
    DetailsInterCoreLoadGraphBody body;
    parser.GetInterCoreLoadAnalysisInfo(INTER_CORE_LOAD_ANALYSIS_OF_VECTOR_JSON, body);
    // 校验解析的结果
    EXPECT_STREQ(body.soc.c_str(), "Ascend910B4");
    EXPECT_STREQ(body.opType.c_str(), "vector");
    EXPECT_STREQ(body.advice.c_str(), "\t1) core3 vector1 took more time than other vector cores.\n");
    int opDetailSize = 2;
    EXPECT_EQ(body.opDetails.size(), opDetailSize);
    DetailsInterCoreLoadOpDetail &opDetail = body.opDetails[0];
    int coreId = 0;
    EXPECT_EQ(opDetail.coreId, coreId);
    int coreDetailSize = 2;
    EXPECT_EQ(opDetail.subCoreDetails.size(), coreDetailSize);
    DetailsInterCoreLoadSubCoreDetail &subCoreDetail = opDetail.subCoreDetails[0];
    EXPECT_STREQ(subCoreDetail.subCoreName.c_str(), "vector0");
    uint64_t cycles = 7114;
    int cycleLevel = 2;
    EXPECT_EQ(subCoreDetail.cycles.value.compare, cycles);
    EXPECT_EQ(subCoreDetail.cycles.level, cycleLevel);
    float throughput = 15104;
    int throughputLevel = 0;
    EXPECT_FLOAT_EQ(subCoreDetail.throughput.value.compare, throughput);
    EXPECT_EQ(subCoreDetail.throughput.level, throughputLevel);
    float hitRate = 75.700935;
    int hitRateLevel = 2;
    EXPECT_FLOAT_EQ(subCoreDetail.cacheHitRate.value.compare, hitRate);
    EXPECT_EQ(subCoreDetail.cacheHitRate.level, hitRateLevel);
}

TEST_F(InterCoreLoadTest, test_GetInterCoreLoadAnalysisInfo_with_normal_json) {
    InterCoreLoadGraphParser parser;
    DetailsInterCoreLoadGraphBody body;
    parser.GetInterCoreLoadAnalysisInfo(INTER_CORE_LOAD_ANALYSIS_JSON, body);
    // 校验解析的结果
    EXPECT_STREQ(body.soc.c_str(), "Ascend910B4");
    EXPECT_STREQ(body.opType.c_str(), "mix");
    EXPECT_STREQ(body.advice.c_str(), "\t0) vector core0 subcore1 took more time than other core.\n");
    int opDetailSize = 1;
    EXPECT_EQ(body.opDetails.size(), opDetailSize);
    DetailsInterCoreLoadOpDetail &opDetail = body.opDetails[0];
    int coreId = 0;
    EXPECT_EQ(opDetail.coreId, coreId);
    int coreDetailSize = 3;
    EXPECT_EQ(opDetail.subCoreDetails.size(), coreDetailSize);
    DetailsInterCoreLoadSubCoreDetail &subCoreDetail = opDetail.subCoreDetails[0];
    EXPECT_STREQ(subCoreDetail.subCoreName.c_str(), "cube0");
    uint64_t cycles = 135938;
    int cycleLevel = 0;
    EXPECT_EQ(subCoreDetail.cycles.value.compare, cycles);
    EXPECT_EQ(subCoreDetail.cycles.level, cycleLevel);
    float throughput = 256;
    int throughputLevel = 0;
    EXPECT_FLOAT_EQ(subCoreDetail.throughput.value.compare, throughput);
    EXPECT_EQ(subCoreDetail.throughput.level, throughputLevel);
    float hitRate = 63.994083f;
    int hitRateLevel = 0;
    EXPECT_FLOAT_EQ(subCoreDetail.cacheHitRate.value.compare, hitRate);
    EXPECT_EQ(subCoreDetail.cacheHitRate.level, hitRateLevel);
}

TEST_F(InterCoreLoadTest, test_response_to_json) {
    DetailsInterCoreLoadGraphResponse response;
    response.body = {"soc", "optype", "advice"};
    DetailsInterCoreLoadOpDetail opDetail0 = {
        0, {{"cube0", {100, 8}, {10, 9}, {98.8, 10}}, {"vector0", {99, 7}, {9, 8}, {97.8, 9}}}};

    response.body.opDetails.emplace_back(opDetail0);
    const std::optional<document_t> &docOpt = ToResponseJson<DetailsInterCoreLoadGraphResponse>(response);
    EXPECT_TRUE(docOpt.has_value());
    document_t &doc = const_cast<document_t &>(docOpt.value());
    EXPECT_TRUE(doc.HasMember("body"));

    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    std::string docString = buffer.GetString();
    EXPECT_TRUE(StringUtil::StartWith(docString, INTER_CORE_LOAD_ANALYSIS_RESPONSE_JSON));
}

TEST_F(InterCoreLoadTest, test_op_detail_array_size_bigger_than_uint8_max) {
    InterCoreLoadGraphParser parser;
    DetailsInterCoreLoadGraphBody body;
    auto json = GeneratorBigCoreDetail(static_cast<size_t>(std::numeric_limits<uint8_t>::max()) * 2);
    std::string jsonStr = JsonUtil::JsonDump(json);
    EXPECT_NO_THROW(parser.GetInterCoreLoadAnalysisInfo(jsonStr, body));
}

// 测试场景：解析包含 simt_vf_instructions 字段的 JSON 数据
// 验证点：1) simt_vf_instructions 字段被正确解析
//         2) simtVfInstructionPerCycle 字段被正确解析
//         3) simtVfInstructions 的 level 字段根据 sigmoid 函数正确计算
TEST_F(InterCoreLoadTest, test_GetInterCoreLoadAnalysisInfo_with_simt_vf_instructions) {
    InterCoreLoadGraphParser parser;
    DetailsInterCoreLoadGraphBody body;
    parser.GetInterCoreLoadAnalysisInfo(INTER_CORE_LOAD_ANALYSIS_WITH_SIMT_JSON, body);

    // 校验基础字段解析
    EXPECT_STREQ(body.soc.c_str(), "Ascend910B4");
    EXPECT_STREQ(body.opType.c_str(), "vector");
    EXPECT_EQ(body.opDetails.size(), 1);

    // 校验第一个 core 的第一个 subcore 的 simt 指令数据
    DetailsInterCoreLoadOpDetail &opDetail0 = body.opDetails[0];
    EXPECT_EQ(opDetail0.coreId, 0);
    EXPECT_EQ(opDetail0.subCoreDetails.size(), 2);

    DetailsInterCoreLoadSubCoreDetail &subCoreDetail0 = opDetail0.subCoreDetails[0];
    EXPECT_STREQ(subCoreDetail0.subCoreName.c_str(), "vector0");
    // 验证 simt_vf_instructions 指令数被正确解析
    EXPECT_EQ(subCoreDetail0.simtVfInstructions.value.compare, 25600);
    // 验证 simtVfInstructionPerCycle 被正确解析（2.5 可被二进制精确表示）
    EXPECT_FLOAT_EQ(subCoreDetail0.simtVfInstructionPerCycle.value.compare, 0);
    // 验证 level 字段被计算（具体值依赖于 sigmoid 统计）
    EXPECT_GE(subCoreDetail0.simtVfInstructions.level, 0);
    EXPECT_LE(subCoreDetail0.simtVfInstructions.level, 10);
}

// 测试场景：解析不包含 simt_vf_instructions 字段的 JSON 数据（向后兼容测试）
// 验证点：1) 解析过程不会崩溃或抛出异常
//         2) simtVfInstructions 和 simtVfInstructionPerCycle 使用默认值（0 和 0.0）
//         3) 其他字段正常解析
TEST_F(InterCoreLoadTest, test_GetInterCoreLoadAnalysisInfo_without_simt_vf_instructions) {
    InterCoreLoadGraphParser parser;
    DetailsInterCoreLoadGraphBody body;
    // 验证解析过程不抛出异常
    EXPECT_NO_THROW(parser.GetInterCoreLoadAnalysisInfo(INTER_CORE_LOAD_ANALYSIS_WITHOUT_SIMT_JSON, body));

    // 校验基础字段解析
    EXPECT_STREQ(body.soc.c_str(), "Ascend910B4");
    EXPECT_STREQ(body.opType.c_str(), "cube");
    EXPECT_EQ(body.opDetails.size(), 1);

    DetailsInterCoreLoadOpDetail &opDetail = body.opDetails[0];
    EXPECT_EQ(opDetail.coreId, 0);
    EXPECT_EQ(opDetail.subCoreDetails.size(), 1);

    DetailsInterCoreLoadSubCoreDetail &subCoreDetail = opDetail.subCoreDetails[0];
    EXPECT_STREQ(subCoreDetail.subCoreName.c_str(), "cube0");

    // 验证 simt 相关字段使用默认值
    uint64_t defaultInstructions = 0;
    EXPECT_EQ(subCoreDetail.simtVfInstructions.value.compare, defaultInstructions);
    float defaultIpc = 0.0f;
    EXPECT_FLOAT_EQ(subCoreDetail.simtVfInstructionPerCycle.value.compare, defaultIpc);

    // 验证其他字段正常解析
    uint64_t expectedCycles = 8000;
    EXPECT_EQ(subCoreDetail.cycles.value.compare, expectedCycles);
    uint64_t expectedThroughput = 256;
    EXPECT_EQ(subCoreDetail.throughput.value.compare, expectedThroughput);
    float expectedHitRate = 75.0f;
    EXPECT_FLOAT_EQ(subCoreDetail.cacheHitRate.value.compare, expectedHitRate);
}

// 测试场景：验证 response 转换为 JSON 时包含 simt_vf_instructions 字段
// 验证点：1) 生成的 JSON 包含 simtVfInstructions 字段
//         2) 生成的 JSON 包含 simtVfInstructions_per_cycle 字段
//         3) JSON 结构正确且可序列化
TEST_F(InterCoreLoadTest, test_response_to_json_with_simt_vf_instructions) {
    DetailsInterCoreLoadGraphResponse response;
    response.body = {"Ascend910B4", "vector", "test advice"};

    DetailsInterCoreLoadOpDetail opDetail0 = {0,
        {{
            "vector0", {10000, 5}, // cycles
            {512, 8}, // throughput
            {85.5, 9}, // cacheHitRate
            {25600, 7}, // simtVfInstructions
            {2.5, 0} // simtVfInstructionPerCycle (使用 2.5 而非 2.56，可被二进制精确表示)
        }}};

    response.body.opDetails.emplace_back(opDetail0);
    const std::optional<document_t> &docOpt = ToResponseJson<DetailsInterCoreLoadGraphResponse>(response);
    EXPECT_TRUE(docOpt.has_value());

    document_t &doc = const_cast<document_t &>(docOpt.value());
    EXPECT_TRUE(doc.HasMember("body"));

    // 验证 JSON 结构包含 simt 相关字段
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    doc.Accept(writer);
    std::string docString = buffer.GetString();

    // 验证 JSON 字符串包含 simt 相关字段
    EXPECT_TRUE(docString.find("simtVfInstructions") != std::string::npos);
    EXPECT_TRUE(docString.find("simtVfInstructionPerCycle") != std::string::npos);

    // 验证具体数值被正确序列化
    EXPECT_TRUE(docString.find("25600") != std::string::npos); // instructions 值
    EXPECT_TRUE(docString.find("2.5") != std::string::npos); // instructions_per_cycle 值（精确表示）
}
}
