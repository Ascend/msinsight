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
#include "QueryAclnnOpAdvisorHandler.h"
#include "QueryAffinityAPIAdvice.h"
#include "QueryAffinityOptimizerAdvice.h"
#include "QueryAiCpuOpAdviceHandler.h"
#include "QueryFusedOpAdviceHandler.h"
#include "QueryOperatorDispatchHandler.h"
#include "AdvisorProtocolRequest.h"
#include "../../TestSuit.h"

using namespace Dic::Server;
using namespace Dic::Protocol;
using namespace Dic::Module::Advisor;

class AdvisorHandleTest : public ::testing::Test {
  public:
    static int Main(int argc, char **argv) {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
};

TEST_F(AdvisorHandleTest, QueryAclnnOpAdvisorHandlerTestReturnFalseWhenInvalidPageSize) {
    QueryAclnnOpAdvisorHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<AclnnOperatorRequest>();
    auto &request = dynamic_cast<AclnnOperatorRequest &>(*requestPtr);
    request.params.pageSize = MAX_PAGESIZE + 1;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(AdvisorHandleTest, QueryAclnnOpAdvisorHandlerTestReturnFalseWhenInvalidCurrentPage) {
    QueryAclnnOpAdvisorHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<AclnnOperatorRequest>();
    auto &request = dynamic_cast<AclnnOperatorRequest &>(*requestPtr);
    request.params.pageSize = MAX_PAGESIZE + 1;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(AdvisorHandleTest, QueryAclnnOpAdvisorHandlerTestReturnFalseWhenEmptyRankId) {
    QueryAclnnOpAdvisorHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<AclnnOperatorRequest>();
    auto &request = dynamic_cast<AclnnOperatorRequest &>(*requestPtr);
    request.params.pageSize = MAX_PAGESIZE;
    request.params.currentPage = 1;
    request.params.rankId = "";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(AdvisorHandleTest, QueryAclnnOpAdvisorHandlerTestReturnFalseWhenRankIdContainsIllegalChars) {
    QueryAclnnOpAdvisorHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<AclnnOperatorRequest>();
    auto &request = dynamic_cast<AclnnOperatorRequest &>(*requestPtr);
    request.params.pageSize = MAX_PAGESIZE - 1;
    request.params.currentPage = 1;
    request.params.rankId = "0&";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(AdvisorHandleTest, QueryAclnnOpAdvisorHandlerTestReturnFalseWhenOrderByContainsIllegalChars) {
    QueryAclnnOpAdvisorHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<AclnnOperatorRequest>();
    auto &request = dynamic_cast<AclnnOperatorRequest &>(*requestPtr);
    request.params.pageSize = MAX_PAGESIZE - 1;
    request.params.currentPage = 1;
    request.params.rankId = "0";
    request.params.orderBy = "&";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(AdvisorHandleTest, QueryAclnnOpAdvisorHandlerTestReturnFalseWhenOrderTypeIsTooLong) {
    QueryAclnnOpAdvisorHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<AclnnOperatorRequest>();
    auto &request = dynamic_cast<AclnnOperatorRequest &>(*requestPtr);
    request.params.pageSize = MAX_PAGESIZE - 1;
    request.params.currentPage = 1;
    request.params.rankId = "0";
    request.params.orderBy = "step";
    std::string str(MAX_PAGESIZE, 'a'); // ref 500
    request.params.orderBy = str;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(AdvisorHandleTest, QueryAffinityAPIAdvice) {
    QueryAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<AffinityAPIRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(AdvisorHandleTest, QueryAffinityOptimizerAdvice) {
    QueryAffinityOptimizerAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<AffinityOptimizerRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(AdvisorHandleTest, QueryAiCpuOpAdviceHandler) {
    QueryAiCpuOpAdviceHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<AICpuOperatorRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(AdvisorHandleTest, QueryFusedOpAdviceHandler) {
    QueryFusedOpAdviceHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<OperatorFusionRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(AdvisorHandleTest, QueryOperatorDispatchHandler) {
    QueryOperatorDispatchHandler handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<OperatorDispatchRequest>();
    handler.HandleRequest(std::move(requestPtr));
}
