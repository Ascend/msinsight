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
#include "ProtocolDefs.h"
#include "RLProtocolRequest.h"
#include "RLProtocolResponse.h"
#include "RLProtocol.h"

using namespace Dic::Protocol;
class RLProtocolUtilTest : public ::testing::Test {
  protected:
    void SetUp() override { protocol.Register(); }

    void TearDown() override { protocol.UnRegister(); }

    Dic::Protocol::RLProtocol protocol;
};

// 强化学习流水图响应序列化验证
TEST_F(RLProtocolUtilTest, ToRLPipelineResponseTest) {
    RLPipelineResponse response;
    response.body.framework = "fsdp";
    response.body.minTime = 0;
    response.body.maxTime = 100;
    response.body.taskData.push_back({{{"file", "type", 0, 100, "name", "stage"}}, "1", "host"});
    response.body.stageTypeList = {"type1", "type2"};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body.HasMember("taskData"), true);
    EXPECT_EQ(body["taskData"].GetArray().Size(), response.body.taskData.size());
    size_t index = 0;
    for (const auto &item : body["taskData"].GetArray()) {
        EXPECT_EQ(item.HasMember("lists"), true);
        EXPECT_EQ(item["lists"].GetArray().Size(), response.body.taskData[index].lists.size());
        size_t j = 0;
        for (const auto &node : item["lists"].GetArray()) {
            EXPECT_EQ(node["fileId"].GetString(), response.body.taskData[index].lists[j].fileId);
            EXPECT_EQ(node["nodeType"].GetString(), response.body.taskData[index].lists[j].nodeType);
            EXPECT_EQ(node["startTime"].GetUint64(), response.body.taskData[index].lists[j].startTime);
            EXPECT_EQ(node["duration"].GetUint64(), response.body.taskData[index].lists[j].duration);
            EXPECT_EQ(node["name"].GetString(), response.body.taskData[index].lists[j].name);
            EXPECT_EQ(node["stageType"].GetString(), response.body.taskData[index].lists[j].stageType);
        }
        EXPECT_EQ(item["rankId"].GetString(), response.body.taskData[index].rankId);
        EXPECT_EQ(item["hostName"].GetString(), response.body.taskData[index].hostName);
    }
    EXPECT_EQ(body["backendType"].GetString(), response.body.backendType);
    EXPECT_EQ(body["framework"].GetString(), response.body.framework);
    EXPECT_EQ(body.HasMember("stageTypeList"), true);
    EXPECT_EQ(body["stageTypeList"].GetArray().Size(), response.body.stageTypeList.size());
    EXPECT_EQ(body["minTime"].GetDouble(), response.body.minTime);
    EXPECT_EQ(body["maxTime"].GetDouble(), response.body.maxTime);
}

// 强化学习流水图请求反序列化验证
TEST_F(RLProtocolUtilTest, ToRLPipelineRequestTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "rl", "type": "request", "resultCallbackId": 0,
        "command": "RL/pipeline", "params": {}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->command, "RL/pipeline");
    EXPECT_EQ(result->type, ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(result->moduleName, MODULE_RL);
}
