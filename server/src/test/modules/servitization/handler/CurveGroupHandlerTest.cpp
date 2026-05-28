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
#include <utility>
#include "CurveGroupHandler.h"
using namespace Dic::Module::IE;
class CurveGroupHandlerTest : public ::testing::Test {
  protected:
    class MockRepo : public Dic::Module::IE::CurveRepo {
      public:
        std::vector<std::string> QueryAllViews(const std::string &fileId) {
            std::vector<std::string> res;
            res.emplace_back("llllllllllllllll");
            return res;
        }
    };

    class MockCurveGroupHandler : public Dic::Module::IE::CurveGroupHandler {
      public:
        void SetRepo(std::shared_ptr<CurveRepo> mockRepo) { repo = std::move(mockRepo); }
    };
};

TEST_F(CurveGroupHandlerTest, TestQueryAllViews) {
    std::shared_ptr<MockRepo> mockRepo = std::make_shared<MockRepo>();
    std::shared_ptr<MockCurveGroupHandler> handler = std::make_shared<MockCurveGroupHandler>();
    std::unique_ptr<Dic::Protocol::IEGroupRequest> request = std::make_unique<Dic::Protocol::IEGroupRequest>();
    handler->SetRepo(mockRepo);
    auto res = handler->HandleRequest(std::move(request));
    EXPECT_EQ(res, true);
}
