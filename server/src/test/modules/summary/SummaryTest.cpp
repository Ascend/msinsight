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
#include "SummaryProtocolRequest.h"
#include "DataBaseManager.h"
#include "../../TestSuit.h"

class SummaryTest : TestSuit {};

TEST_F(TestSuit, QueryComputeStatisticsData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SummaryStatisticParams requestParams;
    requestParams.rankId = "0";
    Dic::Protocol::SummaryStatisticsBody responseBody;
    database->QueryComputeStatisticsData(requestParams, responseBody);
    int expectSize = 5;
    EXPECT_EQ(responseBody.summaryStatisticsItemList.size(), expectSize);
}

TEST_F(TestSuit, QueryComputeStatisticsDataWithEmptyParamReturnExpectSize) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SummaryStatisticParams requestParams;
    requestParams.stepId = "";
    Dic::Protocol::SummaryStatisticsBody responseBody;
    auto res = database->QueryComputeStatisticsData(requestParams, responseBody);
    EXPECT_EQ(res, true);
    const int expectSize = 5;
    EXPECT_EQ(responseBody.summaryStatisticsItemList.size(), expectSize);
}

TEST_F(TestSuit, QueryComputeStatisticsData2) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SummaryStatisticParams requestParams;
    requestParams.rankId = "0";
    requestParams.stepId = "2";
    Dic::Protocol::SummaryStatisticsBody responseBody;
    database->QueryComputeStatisticsData(requestParams, responseBody);
    int expectSize = 5;
    EXPECT_EQ(responseBody.summaryStatisticsItemList.size(), expectSize);
}

TEST_F(TestSuit, QueryCommunicationDetailData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::CommunicationDetailParams requestParams;
    requestParams.rankId = "0";
    requestParams.currentPage = 1;
    requestParams.pageSize = 10; // page size = 10
    Dic::Protocol::CommunicationDetailResponse responseBody;
    database->QueryCommunicationOpDetail(requestParams, responseBody.commDetails);
    int expectSize = 4;
    EXPECT_EQ(responseBody.commDetails.size(), expectSize);
}

TEST_F(TestSuit, QueryGetTotalNumData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::CommunicationDetailParams requestParams;
    requestParams.rankId = "0";
    requestParams.currentPage = 0;
    requestParams.pageSize = 10; // page size = 10
    requestParams.timeFlag = "AI_CORE";
    Dic::Protocol::CommunicationDetailResponse responseBody;
    database->QueryTotalNumByAcceleratorCore(requestParams.timeFlag, responseBody.totalNum);
    int expectSize = 4;
    EXPECT_EQ(responseBody.totalNum, expectSize);
}

TEST_F(TestSuit, QueryComputeDetailData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::ComputeDetailParams requestParams;
    requestParams.rankId = "0";
    requestParams.currentPage = 1;
    requestParams.pageSize = 10; // page size = 10
    requestParams.timeFlag = "AI_CORE";
    std::vector<Dic::Protocol::ComputeDetail> responseBody;
    database->QueryComputeOpDetail(requestParams, responseBody);
    int expectSize = 4;
    EXPECT_EQ(responseBody.size(), expectSize);
}
