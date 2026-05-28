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
#include "SummaryProtocolResponse.h"
#include "DataBaseManager.h"
#include "DbSummaryDataBase.h"
#include "ParamsParser.h"
#include "FileUtil.h"
#include "TestSuit.h"
#include "TraceTime.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;

class DbSummaryTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        std::string currPath = Dic::FileUtil::GetCurrPath();
        const ParamsOption &option = ParamsParser::Instance().GetOption();
        ServerLog::Initialize(option.logPath, option.logSize, option.logLevel, to_string(option.wsPort));
        std::string dbPath = TestSuit::GetTestDataFile("full_db", "msprof_0.db");
        DataBaseManager::Instance().SetDataType(DataType::DB, dbPath);
        DataBaseManager::Instance().SetFileType(FileType::MS_PROF, dbPath);
        DataBaseManager::Instance().CreateTraceConnectionPool("0", dbPath);
        auto database = std::dynamic_pointer_cast<DbTraceDataBase, VirtualTraceDatabase>(
            DataBaseManager::Instance().GetTraceDatabaseByRankId("0"));
        database->UpdateStartTime("0");
        auto summaryDatabase =
            std::dynamic_pointer_cast<DbSummaryDataBase, Dic::Module::Summary::VirtualSummaryDataBase>(
                DataBaseManager::Instance().CreateSummaryDatabase("0", dbPath));
        summaryDatabase->OpenDb(dbPath, false);
    }
    static void TearDownTestSuite() {}
};

TEST_F(DbSummaryTest, QueryComputeStatisticsData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SummaryStatisticParams requestParams;
    requestParams.rankId = "2";
    Dic::Protocol::SummaryStatisticsBody responseBody;
    database->QueryComputeStatisticsData(requestParams, responseBody);
    int expectSize = 1;
    EXPECT_EQ(responseBody.summaryStatisticsItemList.size(), expectSize);
}

TEST_F(DbSummaryTest, QueryComputeStatisticsDataWithEmptyParamReturnExpectSize) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SummaryStatisticParams requestParams;
    requestParams.stepId = "";
    Dic::Protocol::SummaryStatisticsBody responseBody;
    auto res = database->QueryComputeStatisticsData(requestParams, responseBody);
    EXPECT_EQ(res, true);
    const int expectSize = 1;
    EXPECT_EQ(responseBody.summaryStatisticsItemList.size(), expectSize);
}

TEST_F(DbSummaryTest, QueryComputeStatisticsData2) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    Dic::Protocol::SummaryStatisticParams requestParams;
    requestParams.rankId = "2";
    requestParams.stepId = "16";
    Dic::Protocol::SummaryStatisticsBody responseBody;
    database->QueryComputeStatisticsData(requestParams, responseBody);
    int expectSize = 1;
    EXPECT_EQ(responseBody.summaryStatisticsItemList.size(), expectSize);
}

TEST_F(DbSummaryTest, QueryCommunicationDetailData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::CommunicationDetailParams requestParams;
    requestParams.rankId = "2";
    requestParams.currentPage = 0;
    requestParams.pageSize = 10; // page size = 10
    requestParams.timeFlag = "AI_VECTOR_CORE";
    Dic::Protocol::CommunicationDetailResponse responseBody;
    database->QueryCommunicationOpDetail(requestParams, responseBody.commDetails);
    int expectSize = 10;
    EXPECT_EQ(responseBody.commDetails.size(), expectSize);
}

TEST_F(DbSummaryTest, QueryGetTotalNumData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::CommunicationDetailParams requestParams;
    requestParams.rankId = "2";
    requestParams.currentPage = 0;
    requestParams.pageSize = 10; // page size = 10
    requestParams.timeFlag = "AI_VECTOR_CORE";
    Dic::Protocol::CommunicationDetailResponse responseBody;
    database->QueryTotalNumByAcceleratorCore(requestParams.timeFlag, responseBody.totalNum);
    int expectSize = 11;
    EXPECT_EQ(responseBody.totalNum, expectSize);
}

TEST_F(DbSummaryTest, QueryComputeDetailData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId("0");
    Dic::Protocol::ComputeDetailParams requestParams;
    requestParams.rankId = "2";
    requestParams.currentPage = 0;
    requestParams.pageSize = 10; // page size = 10
    requestParams.timeFlag = "AI_VECTOR_CORE";
    std::vector<Dic::Protocol::ComputeDetail> responseBody;
    database->QueryComputeOpDetail(requestParams, responseBody);
    int expectSize = 10;
    EXPECT_EQ(responseBody.size(), expectSize);
}
