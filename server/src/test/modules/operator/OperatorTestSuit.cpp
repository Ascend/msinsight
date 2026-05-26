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
#include "vector"
#include "../../TestSuit.h"
#include "DataBaseManager.h"
#include "OperatorProtocolRequest.h"
#include "OperatorProtocolResponse.h"

class OperatorTestSuit : public TestSuit {};

const std::string GROUP_OPERATOR = "Operator";
const std::string GROUP_OPERATOR_TYPE = "Operator Type";
const std::string GROUP_INPUT_SHAPE = "Input Shape";

TEST_F(TestSuit, QueryOperatorDurationInfoByOpType) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::OperatorDurationReqParams params = {"0", "0", GROUP_OPERATOR_TYPE, 15};
    std::vector<Dic::Protocol::OperatorDurationRes> data = {};
    bool result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::CATEGORY, data);
    EXPECT_EQ(result, true);
    int size = 8;
    EXPECT_EQ(data.size(), size);
    data.clear();
    result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::COMPUTE_UNIT, data);
    EXPECT_EQ(result, true);
    int unitSize = 6;
    EXPECT_EQ(data.size(), unitSize);
}

TEST_F(TestSuit, QueryOperatorDurationInfoByOpTypeAndInputShape) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::OperatorDurationReqParams params = {"0", "0", GROUP_INPUT_SHAPE, 15};
    std::vector<Dic::Protocol::OperatorDurationRes> data = {};
    bool result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::CATEGORY, data);
    EXPECT_EQ(result, true);
    int size = 9;
    EXPECT_EQ(data.size(), size);
    data.clear();
    result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::COMPUTE_UNIT, data);
    EXPECT_EQ(result, true);
    int unitSize = 6;
    EXPECT_EQ(data.size(), unitSize);
}

TEST_F(TestSuit, QueryOperatorDurationInfoByOperator) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::OperatorDurationReqParams params = {"0", "0", GROUP_OPERATOR, 15};
    std::vector<Dic::Protocol::OperatorDurationRes> data = {};
    bool result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::CATEGORY, data);
    EXPECT_EQ(result, true);
    int size = 15;
    EXPECT_EQ(data.size(), size);
    data.clear();
    result = db->QueryOperatorDurationInfo(params, Dic::Protocol::QueryType::COMPUTE_UNIT, data);
    EXPECT_EQ(result, true);
    int cnt = 6;
    EXPECT_EQ(data.size(), cnt);
}

TEST_F(TestSuit, QueryOperatorStatisticInfoByOpType) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::OperatorStatisticReqParams reqParams = {false, "0", "0", GROUP_OPERATOR_TYPE, 15, 1, 10, "", ""};
    Dic::Protocol::OperatorStatisticInfoResponse response = {};
    bool result = db->QueryOperatorStatisticInfo(reqParams, response);
    EXPECT_EQ(result, true);
    int total = 8;
    EXPECT_EQ(response.total, total);
    EXPECT_EQ(response.data.size(), total);
}

TEST_F(TestSuit, QueryAllOperatorStatisticInfoByOpType) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::OperatorStatisticReqParams reqParams = {true, "0", "0", GROUP_OPERATOR_TYPE, 15, 1, 10, "", ""};
    Dic::Protocol::OperatorStatisticInfoResponse response = {};
    std::vector<Protocol::OperatorStatisticInfoRes> compareRes;
    bool result = db->QueryAllOperatorStatisticInfo(reqParams, compareRes);
    EXPECT_EQ(result, true);
    int total = 8;
    EXPECT_EQ(compareRes.size(), total);
}

TEST_F(TestSuit, QueryAllOperatorStatisticInfoByOpTypeAndInputShape) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::OperatorStatisticReqParams reqParams = {false, "0", "0", GROUP_INPUT_SHAPE, 15, 1, 5, "", ""};
    Dic::Protocol::OperatorStatisticInfoResponse response = {};
    bool result = db->QueryOperatorStatisticInfo(reqParams, response);
    EXPECT_EQ(result, true);
    int total = 9;
    EXPECT_EQ(response.total, total);
    int size = 5;
    EXPECT_EQ(response.data.size(), size);
}

TEST_F(TestSuit, QueryOperatorDetailInfoByOperator) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::OperatorStatisticReqParams reqParams = {false, "0", "0", GROUP_OPERATOR, 15, 1, 10, "", ""};
    Dic::Protocol::OperatorDetailInfoResponse response = {};
    bool result = db->QueryOperatorDetailInfo(reqParams, response);
    EXPECT_EQ(result, true);
    int total = 15;
    EXPECT_EQ(response.total, total);
    EXPECT_EQ(response.level, "l1");
    int size = 10;
    EXPECT_EQ(response.data.size(), size);
}

TEST_F(TestSuit, QueryAllOperatorDetailInfoByOperator) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::OperatorStatisticReqParams reqParams = {false, "0", "0", GROUP_OPERATOR, 15, 1, 10, "", ""};
    Dic::Protocol::OperatorDetailInfoResponse response = {};
    std::vector<Protocol::OperatorDetailInfoRes> baselineRes;
    bool result = db->QueryAllOperatorDetailInfo(reqParams, baselineRes, response.level);
    EXPECT_EQ(result, true);
    int total = 16;
    EXPECT_EQ(response.level, "l1");
    EXPECT_EQ(baselineRes.size(), total);
}

TEST_F(TestSuit, QueryOperatorMoreInfoByOpType) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::OperatorMoreInfoReqParams reqParams = {
        "0", "0", GROUP_OPERATOR_TYPE, 15, "Cast", "", "", "AI_CORE", 1, 10, "", ""};
    Dic::Protocol::OperatorMoreInfoResponse response = {};
    bool result = db->QueryOperatorMoreInfo(reqParams, response);
    EXPECT_EQ(result, true);
    int64_t total = 1;
    EXPECT_EQ(response.total, total);
    EXPECT_EQ(response.level, "l1");
    EXPECT_EQ(response.data.size(), total);
}

TEST_F(TestSuit, QueryOperatorMoreInfoByInputShape) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::OperatorMoreInfoReqParams reqParams = {
        "0", "0", GROUP_INPUT_SHAPE, 15, "", "NonZero", R"("""16""")", "MIX_AIV", 1, 10, "", ""};
    Dic::Protocol::OperatorMoreInfoResponse response = {};
    bool result = db->QueryOperatorMoreInfo(reqParams, response);
    EXPECT_EQ(result, true);
    int total = 0;
    EXPECT_EQ(response.total, total);
    EXPECT_EQ(response.level, "l1");
    EXPECT_EQ(response.data.size(), total);
}

TEST_F(TestSuit, QueryBandwidthContentionMatMulDataTest) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    std::vector<Dic::Module::BandwidthContentionMatMulInfo> res;
    bool result = db->QueryBandwidthContentionMatMulData(res);
    ASSERT_EQ(result, true);
    ASSERT_EQ(res.size(), 0);
}
