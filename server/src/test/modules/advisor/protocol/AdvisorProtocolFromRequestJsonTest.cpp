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
#include "JsonUtil.h"
#include "ProtocolDefs.h"
#include "AdvisorProtocolFromRequestJson.h"
#include "AdvisorProtocolUtil.h"

using namespace Dic::Protocol;
class AdvisorProtocolFromRequestJsonTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {}

    static void TearDownTestSuite() {}

  protected:
    Dic::document_t GenerateRequestJson(const Dic::Protocol::APITypeParams &param, const std::string &command) {
        Dic::document_t json(Dic::kObjectType);
        auto &allocator = json.GetAllocator();
        Dic::json_t params(Dic::kObjectType);
        Dic::JsonUtil::AddMember(params, "rankId", param.rankId, allocator);
        Dic::JsonUtil::AddMember(params, "current", param.currentPage, allocator);
        Dic::JsonUtil::AddMember(params, "pageSize", param.pageSize, allocator);
        Dic::JsonUtil::AddMember(params, "orderBy", param.orderBy, allocator);
        Dic::JsonUtil::AddMember(params, "order", param.orderType, allocator);
        Dic::JsonUtil::AddMember(json, "params", params, allocator);
        Dic::JsonUtil::AddMember(json, "id", 1, allocator);
        Dic::JsonUtil::AddMember(json, "type", "request", allocator);
        Dic::JsonUtil::AddMember(json, "command", command, allocator);
        Dic::JsonUtil::AddMember(json, "moduleName", "advisor", allocator);
        return json;
    }
};

TEST_F(AdvisorProtocolFromRequestJsonTest, ToAffinityOptimizerRequestTestFailedWhenLackIdField) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", REQ_RES_ADVISOR_AFFINITY_OPTIMIZER, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "advisor", allocator);
    std::string error;
    auto result = advisorProtocol.FromJson(json, error);
    EXPECT_EQ(result, nullptr);
}

TEST_F(AdvisorProtocolFromRequestJsonTest, ToAffinityOptimizerRequestTestSuccess) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    APITypeParams params = {.rankId = "0",
        .currentPage = 1,
        .pageSize = 15, // 15 record per page
        .orderBy = "duration",
        .orderType = "ascend"};
    Dic::document_t json = GenerateRequestJson(params, REQ_RES_ADVISOR_AFFINITY_OPTIMIZER);
    std::string error;
    APITypeParams result = dynamic_cast<AffinityOptimizerRequest &>(*(advisorProtocol.FromJson(json, error))).params;
    EXPECT_EQ(params.rankId, result.rankId);
    EXPECT_EQ(params.pageSize, result.pageSize);
}

TEST_F(AdvisorProtocolFromRequestJsonTest, ToAffinityApiRequestTestSuccess) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    APITypeParams params = {.rankId = "1",
        .currentPage = 2,
        .pageSize = 20, // 20 record per page, and now page 2
        .orderBy = "duration",
        .orderType = "ascend"};
    Dic::document_t json = GenerateRequestJson(params, REQ_RES_ADVISOR_AFFINITY_API);
    std::string error;
    APITypeParams result = dynamic_cast<AffinityAPIRequest &>(*(advisorProtocol.FromJson(json, error))).params;
    EXPECT_EQ(params.orderType, result.orderType);
    EXPECT_EQ(params.pageSize, result.pageSize);
}

TEST_F(AdvisorProtocolFromRequestJsonTest, ToOperatorFusionRequestTestSuccess) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    APITypeParams params = {.rankId = "1",
        .currentPage = 2,
        .pageSize = 20, // 20 record per page, and now page 2
        .orderBy = "duration",
        .orderType = "ascend"};
    Dic::document_t json = GenerateRequestJson(params, REQ_RES_ADVISOR_OPERATORS_FUSION);
    std::string error;
    APITypeParams result = dynamic_cast<OperatorFusionRequest &>(*(advisorProtocol.FromJson(json, error))).params;
    EXPECT_EQ(params.orderBy, result.orderBy);
    EXPECT_EQ(params.currentPage, result.currentPage);
}

TEST_F(AdvisorProtocolFromRequestJsonTest, ToAICpuOperatorRequestTestSuccess) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    APITypeParams params = {.rankId = "1",
        .currentPage = 2,
        .pageSize = 20, // 20 record per page, and now page 2
        .orderBy = "duration",
        .orderType = "ascend"};
    Dic::document_t json = GenerateRequestJson(params, REQ_RES_ADVISOR_AICPU_OPERATORS);
    std::string error;
    APITypeParams result = dynamic_cast<AICpuOperatorRequest &>(*(advisorProtocol.FromJson(json, error))).params;
    EXPECT_EQ(params.orderBy, result.orderBy);
    EXPECT_EQ(params.orderType, result.orderType);
}

TEST_F(AdvisorProtocolFromRequestJsonTest, ToAclnnOperatorRequestTestSuccess) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    APITypeParams params = {.rankId = "1",
        .currentPage = 2,
        .pageSize = 20, // 20 record per page, and now page 2
        .orderBy = "duration",
        .orderType = "ascend"};
    Dic::document_t json = GenerateRequestJson(params, REQ_RES_ADVISOR_ACLNN_OPERATORS);
    std::string error;
    APITypeParams result = dynamic_cast<AclnnOperatorRequest &>(*(advisorProtocol.FromJson(json, error))).params;
    EXPECT_EQ(params.pageSize, result.pageSize);
    EXPECT_EQ(params.currentPage, result.currentPage);
}

TEST_F(AdvisorProtocolFromRequestJsonTest, ToOperatorDispatchRequestTestSuccess) {
    Dic::Protocol::AdvisorProtocolUtil advisorProtocol;
    advisorProtocol.Register();
    APITypeParams params = {.rankId = "1",
        .currentPage = 2,
        .pageSize = 20, // 20 record per page, and now page 2
        .orderBy = "duration",
        .orderType = "ascend"};
    Dic::document_t json = GenerateRequestJson(params, REQ_RES_ADVISOR_OPERATOR_DISPATCH);
    std::string error;
    APITypeParams result = dynamic_cast<OperatorDispatchRequest &>(*(advisorProtocol.FromJson(json, error))).params;
    EXPECT_EQ(params.pageSize, result.pageSize);
    EXPECT_EQ(params.currentPage, result.currentPage);
}
