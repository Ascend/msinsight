// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026 Huawei Technologies Co.,Ltd.
//  *
//  * MindStudio is licensed under Mulan PSL v2.
//  * You can use this software according to the terms and conditions of the Mulan PSL v2.
//  * You may obtain a copy of Mulan PSL v2 at:
//  *
//  *          http://license.coscl.org.cn/MulanPSL2
//  *
//  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//  * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//  * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//  * See the Mulan PSL v2 for more details.
//  * -------------------------------------------------------------------------
//  *

#include <gtest/gtest.h>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include "JsonUtil.h"
#include "TritonProtocolRequest.h"
#include "TritonProtocolResponse.h"

using namespace Dic::Protocol;
using namespace Dic::Module::Triton;
using namespace Dic;

class TritonProtocolTest : public ::testing::Test {
  protected:
    void SetUp() override {}
    void TearDown() override {}
};

/**
 * @brief 场景说明：测试 TritonMemoryBlocksRequest 从 JSON 字符串解析的功能。
 * 覆盖正常参数解析，以及 endTimestamp 缺失时的默认处理。
 */
TEST_F(TritonProtocolTest, TritonMemoryBlocksRequestFromJsonTest) {
    std::string error;
    std::string jsonStr = R"({
        "id": 1,
        "type": "request",
        "moduleName":"triton",
        "projectName": "test",
        "command": "Triton/memory/blocks",
        "params": {
            "_startTimestamp": 100,
            "_endTimestamp": 200,
            "scope": "forward"
        }
    })";

    document_t doc;
    doc.Parse(jsonStr.c_str());
    auto req = TritonMemoryBlocksRequest::FromJson(doc, error);

    ASSERT_NE(req, nullptr);
    auto *tritonReq = static_cast<TritonMemoryBlocksRequest *>(req.get());
    EXPECT_EQ(tritonReq->startTimestamp, 100);
    EXPECT_EQ(tritonReq->endTimestamp, 200);
    EXPECT_EQ(tritonReq->scopeType, "forward");
}

/**
 * @brief 场景说明：测试 TritonMemoryBlocksRequest 解析时，当 endTimestamp 为负值或缺失时的边界处理。
 */
TEST_F(TritonProtocolTest, TritonMemoryBlocksRequestDefaultEndTimestampTest) {
    std::string error;
    std::string jsonStr = R"({
        "id": 2,
        "moduleName":"Triton",
        "type": "request",
        "projectName": "test",
        "command": "Triton/memory/blocks",
        "params": {
            "_startTimestamp": 100,
            "_endTimestamp": -1
        }
    })";

    document_t doc;
    doc.Parse(jsonStr.c_str());
    auto req = TritonMemoryBlocksRequest::FromJson(doc, error);

    ASSERT_NE(req, nullptr);
    auto *tritonReq = static_cast<TritonMemoryBlocksRequest *>(req.get());
    EXPECT_EQ(tritonReq->endTimestamp, std::numeric_limits<uint64_t>::max());
}

/**
 * @brief 场景说明：测试 TritonBasicInfoRequest 从 JSON 字符串解析的功能。
 */
TEST_F(TritonProtocolTest, TritonBasicInfoRequestFromJsonTest) {
    std::string error;
    std::string jsonStr = R"({
        "id": 3,
        "moduleName":"Triton",
        "type": "request",
        "projectName": "test",
        "command": "Triton/memory/basicInfo",
        "params": {}
    })";

    document_t doc;
    doc.Parse(jsonStr.c_str());
    auto req = TritonBasicInfoRequest::FromJson(doc, error);

    ASSERT_NE(req, nullptr);
    EXPECT_EQ(req->command, REQ_RES_TRITON_MEMORY_BASIC_INFO);
}

/**
 * @brief 场景说明：测试 TritonBasicInfoResponse 序列化为 JSON 的功能。
 */
TEST_F(TritonProtocolTest, TritonBasicInfoResponseToJsonTest) {
    TritonBasicInfoResponse res;
    res.id = 3;
    res.kernelName = "test_kernel";
    res.scopeTypes = {"forward", "backward"};

    auto docOpt = res.ToJson();
    ASSERT_TRUE(docOpt.has_value());

    const auto &doc = docOpt.value();
    EXPECT_TRUE(doc.HasMember("body"));
    const auto &body = doc["body"];

    EXPECT_TRUE(body.HasMember("kernelName"));
    EXPECT_STREQ(body["kernelName"].GetString(), "test_kernel");

    EXPECT_TRUE(body.HasMember("scopeTypes"));
    const auto &scopes = body["scopeTypes"];
    EXPECT_TRUE(scopes.IsArray());
    EXPECT_EQ(scopes.Size(), 2);
    EXPECT_STREQ(scopes[0].GetString(), "forward");
    EXPECT_STREQ(scopes[1].GetString(), "backward");
}

/**
 * @brief 场景说明：测试 TritonMemoryUsageRequest 从 JSON 字符串解析的功能。
 */
TEST_F(TritonProtocolTest, TritonMemoryUsageRequestFromJsonTest) {
    std::string error;
    std::string jsonStr = R"({
        "id": 4,
        "command": "Triton/memory/usage",
        "moduleName":"Triton",
        "type": "request",
        "projectName": "test",
        "params": {
            "timestamp": 500,
            "scope": "backward"
        }
    })";

    document_t doc;
    doc.Parse(jsonStr.c_str());
    auto req = TritonMemoryUsageRequest::FromJson(doc, error);

    ASSERT_NE(req, nullptr);
    auto *tritonReq = static_cast<TritonMemoryUsageRequest *>(req.get());
    EXPECT_EQ(tritonReq->timestamp, 500);
    EXPECT_EQ(tritonReq->scopeType, "backward");
}

/**
 * @brief 场景说明：测试 TritonMemoryBlocksResponse 序列化为 JSON 的功能。
 * 覆盖多个 block 的正确组合。
 */
TEST_F(TritonProtocolTest, TritonMemoryBlocksResponseToJsonTest) {
    TritonMemoryBlocksResponse res;
    res.id = 1;

    TritonTensorBlock b1;
    b1.id = "101";
    b1.offset = 0x1000;
    b1.size = 1024;
    b1.start = 10;
    b1.end = 20;

    TritonTensorBlock b2;
    b2.id = "102";
    b2.offset = 0x2000;
    b2.size = 2048;
    b2.start = 30;
    b2.end = 40;

    res.blocks.push_back(b1);
    res.blocks.push_back(b2);

    res.status = "Success";
    res.errMsg = "";

    auto docOpt = res.ToJson();
    ASSERT_TRUE(docOpt.has_value());

    const auto &doc = docOpt.value();
    EXPECT_TRUE(doc.HasMember("body"));
    const auto &body = doc["body"];
    EXPECT_TRUE(body.HasMember("blocks"));
    const auto &blocks = body["blocks"];
    EXPECT_TRUE(blocks.IsArray());
    EXPECT_EQ(blocks.Size(), 2);

    EXPECT_STREQ(blocks[0]["id"].GetString(), "101");
    EXPECT_TRUE(body.HasMember("status"));
    EXPECT_STREQ(body["status"].GetString(), "Success");
    EXPECT_TRUE(body.HasMember("errMsg"));
    EXPECT_STREQ(body["errMsg"].GetString(), "");
}

/**
 * @brief 场景说明：测试 TritonMemoryUsageResponse 序列化为 JSON 的功能。
 * 特别验证 segments 和嵌套的 blocks 是否正确序列化（包含之前修复的 bug 验证）。
 */
TEST_F(TritonProtocolTest, TritonMemoryUsageResponseToJsonTest) {
    TritonMemoryUsageResponse res;
    res.id = 2;

    TritonTensorSegment s;
    s.allocate = 0x5000;
    s.size = 4096;
    s.start = 100;
    s.end = 200;
    s.sourceLocation = "test.py:10";

    TritonTensorBlock b;
    b.id = "201";
    b.offset = 0x5100;
    b.size = 256;
    s.blocks.push_back(b);

    res.segments.push_back(s);

    auto docOpt = res.ToJson();
    ASSERT_TRUE(docOpt.has_value());

    const auto &doc = docOpt.value();
    const auto &body = doc["body"];
    const auto &segments = body["segments"];
    EXPECT_TRUE(segments.IsArray());
    EXPECT_EQ(segments.Size(), 1);

    const auto &seg0 = segments[0];
    EXPECT_TRUE(seg0.HasMember("blocks"));
    EXPECT_TRUE(seg0["blocks"].IsArray());
    EXPECT_EQ(seg0["blocks"].Size(), 1);
    EXPECT_STREQ(seg0["blocks"][0]["id"].GetString(), "201");
}
