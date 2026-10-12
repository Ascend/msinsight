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
#include "IECurveHandler.h"
using namespace Dic::Module::IE;
class IECurveHandlerTest : public ::testing::Test {
  protected:
    class MockRepo : public Dic::Module::IE::CurveRepo {
      public:
        std::vector<Dic::Module::ColumnAtt> QueryTableInfoByName(
            const std::string &fileId, const std::string &tableName) override {
            std::vector<Dic::Module::ColumnAtt> res;
            Dic::Module::ColumnAtt col1;
            col1.key = "lll";
            res.emplace_back(col1);
            Dic::Module::ColumnAtt col2;
            col2.key = "jjj";
            res.emplace_back(col2);
            return res;
        }

        std::vector<std::map<std::string, std::string>> QueryDataByColumn(const std::string &fileId,
            const std::string &tableName, const std::vector<Dic::Module::ColumnAtt> &columns) override {
            if (!rows.empty()) {
                return rows;
            }
            std::vector<std::map<std::string, std::string>> res;
            std::map<std::string, std::string> data1;
            data1["lll"] = "mmm1";
            data1["jjj"] = "ggg2";
            res.emplace_back(data1);
            std::map<std::string, std::string> data2;
            data2["lll"] = "mmm3";
            data2["jjj"] = "ggg4";
            res.emplace_back(data2);
            res.emplace_back(data2);
            return res;
        }

        std::vector<std::map<std::string, std::string>> rows;
    };

    class MockIECurveHandler : public Dic::Module::IE::IECurveHandler {
      public:
        void SetRepo(std::shared_ptr<CurveRepo> mockRepo) { repo = std::move(mockRepo); }

        void QueryData(const Dic::Protocol::IEUsageViewParamsRequest &request,
            Dic::Protocol::IEUsageViewResponse &response, std::vector<Dic::Module::ColumnAtt> &columns) {
            QueryDatasByCols(request, response, columns);
        }
    };
};

TEST_F(IECurveHandlerTest, TestHandler) {
    std::shared_ptr<MockRepo> mockRepo = std::make_shared<MockRepo>();
    std::shared_ptr<MockIECurveHandler> handler = std::make_shared<MockIECurveHandler>();
    std::unique_ptr<Dic::Protocol::IEUsageViewParamsRequest> request =
        std::make_unique<Dic::Protocol::IEUsageViewParamsRequest>();
    request->params.type = "llllllllll";
    handler->SetRepo(mockRepo);
    auto res = handler->HandleRequest(std::move(request));
    EXPECT_EQ(res, true);
}

TEST_F(IECurveHandlerTest, RemovesConsecutiveDuplicateRows) {
    auto mockRepo = std::make_shared<MockRepo>();
    MockIECurveHandler handler;
    handler.SetRepo(mockRepo);
    Dic::Protocol::IEUsageViewParamsRequest request;
    Dic::Protocol::IEUsageViewResponse response;
    std::vector<Dic::Module::ColumnAtt> columns(2);
    columns[0].key = "lll";
    columns[1].key = "jjj";

    handler.QueryData(request, response, columns);

    ASSERT_EQ(response.data.lines.size(), 2);
    EXPECT_EQ(response.data.lines[0], (std::vector<std::string>{"mmm1", "ggg2"}));
    EXPECT_EQ(response.data.lines[1], (std::vector<std::string>{"mmm3", "ggg4"}));
}

TEST_F(IECurveHandlerTest, SamplesLargeCurveWithNonNumericValues) {
    auto mockRepo = std::make_shared<MockRepo>();
    for (int i = 0; i < 4000; ++i) {
        mockRepo->rows.push_back({{"lll", std::to_string(i)}, {"jjj", std::to_string(i)}});
    }
    mockRepo->rows[1]["jjj"] = "NULL";
    mockRepo->rows[2]["jjj"] = "invalid";
    mockRepo->rows[3]["jjj"] = "NaN";
    mockRepo->rows[4]["jjj"] = "";
    mockRepo->rows.push_back(mockRepo->rows.back());

    MockIECurveHandler handler;
    handler.SetRepo(mockRepo);
    Dic::Protocol::IEUsageViewParamsRequest request;
    request.params.type = "curve";
    Dic::Protocol::IEUsageViewResponse response;
    std::vector<Dic::Module::ColumnAtt> columns(2);
    columns[0].key = "lll";
    columns[1].key = "jjj";

    handler.QueryData(request, response, columns);

    EXPECT_EQ(response.data.legends, (std::vector<std::string>{"lll", "jjj"}));
    ASSERT_GT(response.data.lines.size(), 1000);
    EXPECT_LT(response.data.lines.size(), 4000);
    EXPECT_EQ(response.data.lines.front()[0], "0");
    EXPECT_EQ(response.data.lines.back()[0], "3999");
}
