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
#include "AdvisorProtocolResponse.h"
#include "AdvisorProtocolUtil.h"

class AdvisorProtocolToResponseJsonTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

  protected:
    uint32_t refSize = 15;
    std::string operatorDispatchNote = "Please use `torch_npu.npu.set_compile_mode(jit_compile=False)` "
                                       "to disable jit compile in dynamic shape usage.";
};

TEST_F(AdvisorProtocolToResponseJsonTest, ToAffinityOptimizerResponseTest) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();

    Dic::Protocol::AffinityOptimizerResponse response;
    response.body = {.size = 0, .data = {}};
    std::string error;
    std::optional<Dic::document_t> jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    response.body.size = refSize;
    response.body.data = {
        {{"0", "0", 1, 1, "pid1", "tid1", 1}, "Optimizer.step#SGD.step", "torch_npu.optim.NpuFusedSGD"},
        {{"1", "1", 2, 2, "pid2", "tid2", 2}, // rank = 1, depth = 2
            "Optimizer.step#Adadelta.step", "torch_npu.optim.NpuFusedAdadelta"},
        {{"2", "2", 3, 3, "pid3", "tid3", 3}, // rank = 2, depth = 3
            "Optimizer.step#Lamb.step", "torch_npu.optim.NpuFusedLamb"}};
    jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("count"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["count"].GetUint(), refSize);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("data"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].IsArray(), true);
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        EXPECT_EQ(item.HasMember("rankId"), true);
        EXPECT_EQ(item["rankId"].GetString(), response.body.data[i].baseInfo.rankId);
        EXPECT_EQ(item.HasMember("originOptimizer"), true);
        EXPECT_EQ(item["originOptimizer"].GetString(), response.body.data[i].originOptimizer);
        EXPECT_EQ(item.HasMember("replaceOptimizer"), true);
        EXPECT_EQ(item["replaceOptimizer"].GetString(), response.body.data[i++].replaceOptimizer);
    }
    EXPECT_EQ(i, response.body.data.size());
}

TEST_F(AdvisorProtocolToResponseJsonTest, ToAffinityAPIResponseTest) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    Dic::Protocol::AffinityAPIResponse response;
    response.body = {.size = 0, .data = {}};
    std::string error;
    std::optional<Dic::document_t> jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    response.body.size = refSize;
    response.body.data = {{{"0", "0", 1, 1, "pid1", "tid1", 1}, "aten::gelu", "aten::gelu", "torch_npu.fast_gelu", ""},
        {{"1", "1", 2, 2, "pid2", "tid2", 2}, "aten::linear", "aten::linear", "torch_npu.npu_linear", ""}};
    jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("count"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["count"].GetUint(), refSize);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("data"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].IsArray(), true);
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        EXPECT_EQ(item.HasMember("rankId"), true);
        EXPECT_EQ(item["rankId"].GetString(), response.body.data[i].baseInfo.rankId);
        EXPECT_EQ(item.HasMember("originAPI"), true);
        EXPECT_EQ(item["originAPI"].GetString(), response.body.data[i].originAPI);
        EXPECT_EQ(item.HasMember("replaceAPI"), true);
        EXPECT_EQ(item["replaceAPI"].GetString(), response.body.data[i++].replaceAPI);
    }
    EXPECT_EQ(i, response.body.data.size());
}

TEST_F(AdvisorProtocolToResponseJsonTest, ToAICpuOperatorResponseTest) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    Dic::Protocol::AICpuOperatorResponse response;
    response.body = {.size = 0, .data = {}};
    std::string error;
    std::optional<Dic::document_t> jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    response.body.size = refSize;
    response.body.data = {
        {{"0", "0", 1, 1, "pid1", "tid1", 1}, "Cast75", ""}, {{"1", "1", 2, 2, "pid2", "tid2", 2}, "Add77", ""}};
    jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("count"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["count"].GetUint(), refSize);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("data"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].IsArray(), true);
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        EXPECT_EQ(item.HasMember("rankId"), true);
        EXPECT_EQ(item["rankId"].GetString(), response.body.data[i].baseInfo.rankId);
        EXPECT_EQ(item.HasMember("name"), true);
        EXPECT_EQ(item["name"].GetString(), response.body.data[i].opName);
        EXPECT_EQ(item.HasMember("note"), true);
        EXPECT_EQ(item["note"].GetString(), response.body.data[i++].note);
    }
    EXPECT_EQ(i, response.body.data.size());
}

TEST_F(AdvisorProtocolToResponseJsonTest, ToAclnnOperatorResponseTest) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    Dic::Protocol::AclnnOperatorResponse response;
    response.body = {.size = 0, .data = {}};
    std::string error;
    std::optional<Dic::document_t> jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    response.body.size = refSize;
    response.body.data = {{{"0", "0", 1, 1, "pid1", "tid1", 1}, "Ascend@aclnnCast", ""},
        {{"1", "1", 2, 2, "pid2", "tid2", 2}, "Ascend@aclnnAdd", ""}};
    jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("count"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["count"].GetUint(), refSize);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("data"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].IsArray(), true);
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        EXPECT_EQ(item.HasMember("rankId"), true);
        EXPECT_EQ(item["rankId"].GetString(), response.body.data[i].baseInfo.rankId);
        EXPECT_EQ(item.HasMember("name"), true);
        EXPECT_EQ(item["name"].GetString(), response.body.data[i].opName);
        EXPECT_EQ(item.HasMember("note"), true);
        EXPECT_EQ(item["note"].GetString(), response.body.data[i++].note);
    }
    EXPECT_EQ(i, response.body.data.size());
}

TEST_F(AdvisorProtocolToResponseJsonTest, ToOperatorFusionResponseTest) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    Dic::Protocol::OperatorFusionResponse response;
    response.body = {.size = 0, .data = {}};
    std::string error;
    std::optional<Dic::document_t> jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    response.body.size = refSize;
    response.body.data = {{{"0", "0", 1, 1, "pid1", "tid1", 1}, "Cast", "Cast, LayerNorm, Cast", "LayerNorm", ""},
        {{"1", "1", 2, 2, "pid2", "tid2", 2}, "Transpose", "Transpose, Transpose, GatherElement, Transpose",
            "GatherElements", ""}};
    jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("count"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["count"].GetUint(), refSize);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("data"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].IsArray(), true);
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        EXPECT_EQ(item.HasMember("rankId"), true);
        EXPECT_EQ(item["rankId"].GetString(), response.body.data[i].baseInfo.rankId);
        EXPECT_EQ(item.HasMember("originOpList"), true);
        EXPECT_EQ(item["originOpList"].GetString(), response.body.data[i].originOpList);
        EXPECT_EQ(item.HasMember("fusedOp"), true);
        EXPECT_EQ(item["fusedOp"].GetString(), response.body.data[i++].fusedOp);
    }
    EXPECT_EQ(i, response.body.data.size());
}

TEST_F(AdvisorProtocolToResponseJsonTest, ToOperatorDispatchResponseTest) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    Dic::Protocol::OperatorDispatchResponse response;
    response.body = {.size = 0, .data = {}};
    std::string error;
    std::optional<Dic::document_t> jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    response.body.size = refSize;
    response.body.data = {
        {{"0", "0", 1, 1, "pid1", "tid1", 1}, "AscendCL@aclopCompileAndExecute", operatorDispatchNote},
        {{"1", "1", 2, 2, "pid2", "tid2", 2}, "AscendCL@aclopCompileAndExecute", operatorDispatchNote}};
    jsonOptional = advisorProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("count"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["count"].GetUint(), refSize);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("data"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].IsArray(), true);
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        EXPECT_EQ(item.HasMember("rankId"), true);
        EXPECT_EQ(item["rankId"].GetString(), response.body.data[i].baseInfo.rankId);
        EXPECT_EQ(item.HasMember("name"), true);
        EXPECT_EQ(item["name"].GetString(), response.body.data[i].opName);
        EXPECT_EQ(item.HasMember("note"), true);
        EXPECT_EQ(item["note"].GetString(), response.body.data[i++].note);
    }
    EXPECT_EQ(i, response.body.data.size());
}
