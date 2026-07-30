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
#include <gtest/gtest.h>
#include "ExportAffinityAPIAdvice.h"
#include "AdvisorProtocolRequest.h"
#include "AdvisorProtocolResponse.h"
#include "AdvisorErrorManager.h"

namespace Dic::Module::Advisor {
using namespace Dic::Protocol;

class ExportAffinityAPIAdviceTest : public ::testing::Test {
  public:
    static int Main(int argc, char **argv) {
        ::testing::InitGoogleTest(&argc, argv);
        return RUN_ALL_TESTS();
    }
};

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenRankIdEmpty) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenOrderByIllegal) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "test";
    request.orderBy = "duration--";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenStartTimeGreaterThanEndTime) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "test";
    request.startTime = 100;
    request.endTime = 50;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

TEST_F(ExportAffinityAPIAdviceTest, HandleRequestReturnsFalseWhenNoDatabase) {
    ExportAffinityAPIAdvice handler;
    std::unique_ptr<Request> requestPtr = std::make_unique<ExportAffinityAPIRequest>();
    auto &request = dynamic_cast<ExportAffinityAPIRequest &>(*requestPtr);
    request.rankId = "nonexistent_rank";
    request.orderBy = "duration";
    request.orderType = "descend";
    request.startTime = 0;
    request.endTime = 0;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_EQ(result, false);
}

} // Dic::Module::Advisor
