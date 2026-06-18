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
#include "MemoryProtocolRequest.h"
#include "DataBaseManager.h"
#include "DbClusterDataBase.h"
#include "ClusterDomainObject.h"
#include "ParamsParser.h"
#include "PacketAnalyzer.h"
#include "ByteAlignmentAnalyzer.h"
#include "TestSuit.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;
using namespace Dic;
using namespace Dic::Module::Communication;

class DbCommunicationTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        const ParamsOption &option = ParamsParser::Instance().GetOption();
        ServerLog::Initialize(option.logPath, option.logSize, option.logLevel, to_string(option.wsPort));
        std::string dataDir = TestSuit::GetTestDataFile("full_db");
        std::string dbFilePath = FileUtil::SplicePath(dataDir, "cluster_analysis.db");
        // FIX: 防止其他测试夹具完成后没有清理缓存导致这里的测试失败，这里手动清理一次
        DataBaseManager::Instance().Clear();
        DataBaseManager::Instance().SetDataType(DataType::DB, dbFilePath);
        DataBaseManager::Instance().SetFileType(FileType::MS_PROF, dbFilePath);
        std::string clusterDbPath = FileUtil::SplicePath(dataDir, "cluster_analysis.db");
        Dic::Module::FullDb::DataBaseManager::Instance().CreateClusterConnectionPool(
            COMPARE, clusterDbPath, Dic::Module::Timeline::DataType::DB);
        auto clusterDatabase = Dic::Module::FullDb::DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    }
    static void TearDownTestSuite() {}
};

TEST_F(DbCommunicationTest, QueryIterationsData) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::vector<Dic::Protocol::IterationsOrRanksObject> responseBody;
    database->QueryIterations(responseBody);
    int expectSize = 2;
    EXPECT_EQ(responseBody.size(), expectSize);
    EXPECT_EQ(responseBody[0].iterationOrRankId, "1");
}

TEST_F(DbCommunicationTest, QueryOperatorNameData) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::OperatorNamesParams requestParams;
    requestParams.iterationId = "1";
    requestParams.groupIdHash = "7959537521297666018";
    requestParams.rankList = {};
    std::vector<Dic::Protocol::OperatorNamesObject> responseBody;
    database->QueryOperatorNames(requestParams, responseBody);
    int expectSize = 4;
    EXPECT_EQ(responseBody.size(), expectSize);
    EXPECT_EQ(responseBody[0].operatorName, "hcom_allGather__018_3_1");
    EXPECT_EQ(responseBody[1].operatorName, "hcom_allReduce__018_0_1");
}

TEST_F(DbCommunicationTest, QueryRanksData) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::vector<Dic::Protocol::IterationsOrRanksObject> responseBody;
    database->QueryRanksHandler(responseBody);
    int expectSize = 8;
    EXPECT_EQ(responseBody.size(), expectSize);
    EXPECT_EQ(responseBody[0].iterationOrRankId, "0");
    EXPECT_EQ(responseBody[1].iterationOrRankId, "1");
}

TEST_F(DbCommunicationTest, QueryDurationData) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::DurationListParams requestParams;
    std::vector<Dic::Module::DurationDo> durationDoList;
    requestParams.iterationId = "1";
    requestParams.groupIdHash = "7959537521297666018";
    requestParams.operatorName = "hcom_allReduce__018_0_1";
    database->QueryDurationList(requestParams, durationDoList);
    int expectSize = 8;
    EXPECT_EQ(durationDoList.size(), expectSize);
}

TEST_F(DbCommunicationTest, QueryDurationDataWithRank) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::DurationListParams requestParams;
    std::vector<Dic::Module::DurationDo> durationDoList;
    requestParams.iterationId = "1";
    requestParams.groupIdHash = "7959537521297666018";
    requestParams.operatorName = "hcom_allReduce__018_0_1";
    requestParams.rankList = {"0"};
    database->QueryDurationList(requestParams, durationDoList);
    int expectSize = 1;
    EXPECT_EQ(durationDoList.size(), expectSize);
}

TEST_F(DbCommunicationTest, QueryBandwidthDistributionData) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::DistributionDataParam requestParams;
    Dic::Protocol::DistributionResBody responseBody;
    requestParams.iterationId = "1";
    requestParams.groupIdHash = "7959537521297666018";
    requestParams.operatorName = "hcom_allGather__018_3_1";
    requestParams.transportType = "HCCS";
    requestParams.rankId = "1";
    database->QueryDistributionData(requestParams, responseBody);
    std::string expectResult = "{\"0.0001\":[7,0.0100401953125]}";
    EXPECT_EQ(responseBody.distributionData, expectResult);
}

TEST_F(DbCommunicationTest, QueryBandwidthData) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::BandwidthDataParam requestParams;
    Dic::Protocol::BandwidthDataResBody responseBody;
    requestParams.iterationId = "1";
    requestParams.groupIdHash = "7959537521297666018";
    requestParams.operatorName = "hcom_allGather__018_3_1";
    requestParams.rankId = "1";
    database->QueryBandwidthData(requestParams, responseBody);
    int expectSize = 2;
    EXPECT_EQ(responseBody.items.size(), expectSize);
    EXPECT_EQ(responseBody.items[0].transportType, "HCCS");
}

TEST_F(DbCommunicationTest, QueryIterationAndCommunicationGroupDBSuccess) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::CommunicationKernelParams requestParams;
    requestParams.name = "hcom_broadcast__293_0_1";
    requestParams.rankId = "0";
    Dic::Protocol::CommunicationKernelBody responseBody;
    database->QueryIterationAndCommunicationGroup(requestParams, responseBody);
    EXPECT_EQ(responseBody.step, "1");
    EXPECT_EQ(responseBody.group, "(0,1,2,3,4,5,6,7)");
}

TEST_F(DbCommunicationTest, QueryBandwidthDataWithErrorParamReturnExpectSize0) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::BandwidthDataParam requestParams;
    Dic::Protocol::BandwidthDataResBody responseBody;
    requestParams.iterationId = "2";
    requestParams.groupIdHash = "test";
    requestParams.operatorName = "Total Op Info";
    requestParams.rankId = "1";
    database->QueryBandwidthData(requestParams, responseBody);
    const int expectSize = 0;
    EXPECT_EQ(responseBody.items.size(), expectSize);
}

TEST_F(DbCommunicationTest, QueryOperatorsCount) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::OperatorDetailsParam requestParams;
    Dic::Protocol::OperatorDetailsResBody responseBody;
    requestParams.iterationId = "1";
    requestParams.groupIdHash = "492248724195588293";
    requestParams.rankId = "1";
    database->QueryOperatorsCount(requestParams, responseBody);
    const int expectSize = 1320;
    EXPECT_EQ(responseBody.count, expectSize);
    requestParams.iterationId = "2";
    requestParams.groupIdHash = "test";
    requestParams.rankId = "0";
    database->QueryOperatorsCount(requestParams, responseBody);
    const int stageExpectSize = 0;
    EXPECT_EQ(responseBody.count, stageExpectSize);
}

TEST_F(DbCommunicationTest, GetCommunicationGroups) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::vector<GroupInfoDo> groupList;
    database->GetGroups(groupList);
    int expectSize = 3;
    EXPECT_EQ(groupList.size(), expectSize);
    EXPECT_EQ(groupList[0].rankSet, "(0, 1, 2, 3, 4, 5, 6, 7)");
}

TEST_F(DbCommunicationTest, GetAllRankFromStepStatisticInfoSuccess) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::vector<std::string> res = database->GetAllRankFromStepStatisticInfo();
    int expectSize = 8;
    EXPECT_EQ(res.size(), expectSize);
}

TEST_F(DbCommunicationTest, QueryMatrixData) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::MatrixBandwidthParam requestParams;
    std::vector<Dic::Module::MatrixInfoDo> matrixList;
    requestParams.iterationId = "1";
    requestParams.groupIdHash = "7959537521297666018";
    requestParams.operatorName = "allgather-bottom1";
    database->QueryMatrixList(requestParams, matrixList);
    int expectSize = 64;
    EXPECT_EQ(matrixList.size(), expectSize);
}

TEST_F(DbCommunicationTest, QueryAllCommunicationOperatorsDetails) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::OperatorDetailsParam requestParams;
    Dic::Protocol::OperatorDetailsResBody responseBody;
    requestParams.iterationId = "step1";
    requestParams.groupIdHash = "492248724195588293";
    requestParams.rankId = "0";
    requestParams.currentPage = 1;
    requestParams.pageSize = 100; // pageSize = 100
    database->QueryAllOperators(requestParams, responseBody);
    int expectSize = 100;
    EXPECT_EQ(responseBody.allOperators.size(), expectSize);
    EXPECT_EQ(responseBody.allOperators[0].operatorName, "hcom_allReduce__293_647_1");
}

TEST_F(DbCommunicationTest, QueryBaseInfoSuccess) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::SummaryTopRankResBody responseBody;
    database->QueryBaseInfo(responseBody.baseInfo.compare);
    const unsigned int expectRankCount = 8;
    const unsigned int expectStepNum = 2;
    EXPECT_EQ(responseBody.baseInfo.compare.rankCount, expectRankCount);
    EXPECT_EQ(responseBody.baseInfo.compare.stepNum, expectStepNum);
}

TEST_F(DbCommunicationTest, QueryExtremumTimestampSuccess) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    uint64_t min = 0;
    uint64_t max = 0;
    bool res = database->QueryExtremumTimestamp(min, max);
    const uint64_t expectMin = 1718682999267391232;
    const uint64_t expectMax = 1718683020669117696;
    EXPECT_EQ(res, true);
    EXPECT_EQ(min, expectMin);
    EXPECT_EQ(max, expectMax);
}

TEST_F(DbCommunicationTest, UpdateCollectTimeInfoSuccess) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    SummaryBaseInfo baseInfo;
    baseInfo.collectStartTime = 1718682999267391232;
    baseInfo.collectDuration = 123.45;
    bool res = database->UpdateCollectTimeInfo(baseInfo);
    EXPECT_EQ(res, true);
    SummaryBaseInfo ans;
    database->QueryBaseInfo(ans);
    EXPECT_EQ(baseInfo.collectStartTime, ans.collectStartTime);
    EXPECT_EQ(baseInfo.collectDuration, ans.collectDuration);
}

TEST_F(DbCommunicationTest, QueryOperatorListSuccess) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::DurationListParams requestParams;
    requestParams.rankList.emplace_back("0");
    requestParams.operatorName = "hcom_broadcast__293_0_1";
    requestParams.iterationId = "1";
    requestParams.groupIdHash = "492248724195588293";
    std::vector<Dic::Module::OperatorTimeDo> opLists;
    database->QueryOperatorList(requestParams, opLists);
    int expectSize = 1;
    EXPECT_EQ(opLists.size(), expectSize);
}

TEST_F(DbCommunicationTest, QueryMatrixSortOpNamesSuccess) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Protocol::OperatorNamesParams requestParams;
    requestParams.rankList.emplace_back("0");
    requestParams.iterationId = "1";
    requestParams.groupIdHash = "492248724195588293";
    std::vector<Protocol::OperatorNamesObject> responseBody;
    bool res = database->QueryMatrixSortOpNames(requestParams, responseBody);
    const int expectSize = 12;
    EXPECT_EQ(res, true);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(DbCommunicationTest, GetParallelConfigFromStepTraceSuccess) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    Dic::Module::ParallelStrategyConfig config;
    std::string level;
    bool res = database->GetParallelConfigFromStepTrace(config, level);
    const int exceptTpSize = 1;
    EXPECT_EQ(res, true);
    EXPECT_EQ(config.tpSize, exceptTpSize);
    EXPECT_EQ(level, "undefined");
}

TEST_F(DbCommunicationTest, QueryAllPerformanceDataByStepWhenSingleStep) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::string step = "1";
    std::unordered_map<uint32_t, Dic::Module::StepStatistic> data{};
    auto result = database->QueryAllPerformanceDataByStep(step, data);
    EXPECT_EQ(result, true);
    EXPECT_EQ(data.size(), 8); // 8
    EXPECT_EQ(data.at(0).prepareTime, 473.646); // 473.646 for result
    step = "3";
    data.clear();
    result = database->QueryAllPerformanceDataByStep(step, data);
    EXPECT_EQ(result, true);
    EXPECT_EQ(data.size(), 0);
}

TEST_F(DbCommunicationTest, QueryAllPerformanceDataByStepWhenAllStep) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::string step = "";
    std::unordered_map<uint32_t, Dic::Module::StepStatistic> data{};
    auto result = database->QueryAllPerformanceDataByStep(step, data);
    EXPECT_EQ(result, true);
    EXPECT_EQ(data.size(), 8); // 8
    EXPECT_EQ(data.at(0).prepareTime, 1075.410); // 1075.410 for result
    step = "All";
    data.clear();
    result = database->QueryAllPerformanceDataByStep(step, data);
    EXPECT_EQ(result, true);
    EXPECT_EQ(data.size(), 8); // 8
    EXPECT_EQ(data.at(0).prepareTime, 1075.410); // 1075.410 for result
}

TEST_F(DbCommunicationTest, GetCommTimeForRankDimByStepWhenAllStep) {
    auto database = DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::string step = "1";
    std::vector<Dic::Module::CommInfoUnderRank> result1 = database->GetCommTimeForRankDim(step);
    const int expectSize = 0;
    EXPECT_EQ(result1.size(), expectSize);
    step = "All";
    std::vector<Dic::Module::CommInfoUnderRank> result2 = database->GetCommTimeForRankDim(step);
    EXPECT_EQ(result2.size(), expectSize);
}

TEST_F(DbCommunicationTest, QueryParseClusterStatusSuccess) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::string status = database->QueryParseClusterStatus();
    EXPECT_EQ(status, "UN_FINISH");

    database->UpdateClusterParseStatus("UNKNOWN");
    std::string status1 = database->QueryParseClusterStatus();
    EXPECT_EQ(status1, "UNKNOWN");
}

TEST_F(DbCommunicationTest, QueryPacketAnalyzerDataTest) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::vector<Dic::Module::PacketAnalyzerData> data;
    bool result = database->QueryPacketAnalyzerData(data);
    int expectSize = 20986;
    ASSERT_TRUE(result);
    ASSERT_EQ(data.size(), expectSize);
    EXPECT_EQ(data[0].type, "SDMA");
    EXPECT_EQ(data[1].type, "SDMA");
}

TEST_F(DbCommunicationTest, QueryBandwidthContentionAnalyzerDataTest) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::vector<Dic::Module::BandwidthContentionSDMAInfo> res;
    std::string rankId = "0";
    bool result = database->QueryBandwidthContentionAnalyzerData(res, rankId);
    ASSERT_TRUE(result);
    ASSERT_EQ(res.size(), 2452); // 2452
    EXPECT_EQ(res[0].name, "Total Op Info");
    EXPECT_EQ(res[1].name, "Total Op Info");
    rankId = "1";
    res.clear();
    result = database->QueryBandwidthContentionAnalyzerData(res, rankId);
    ASSERT_TRUE(result);
    ASSERT_EQ(res.size(), 2650); // 2650
    EXPECT_EQ(res[0].name, "Total Op Info");
    EXPECT_EQ(res[1].name, "Total Op Info");
}
TEST_F(DbCommunicationTest, PacketAnalyzerTest) {
    PacketAnalyzer analyzer;
    analyzer.QueryAdvisorData("compare");
    analyzer.ComputeStatistics();
    CommunicationAdvisorInfo info;
    analyzer.GenerateAdvisor(info, "compare");
    EXPECT_EQ(info.name, "Packet Analysis");
    EXPECT_EQ(info.statistics.size(), 6); // expect size 6
    analyzer.AssembleAdvisor(info);
    EXPECT_EQ(info.name, "Packet Analysis");
    EXPECT_EQ(info.statistics.size(), 6); // expect size 6
}

TEST_F(DbCommunicationTest, ByteAlignmentAnalyzerTest) {
    ByteAlignmentAnalyzer analyzer;
    analyzer.QueryAdvisorData("compare");
    analyzer.ComputeStatistics();
    CommunicationAdvisorInfo info;
    analyzer.AssembleAdvisor(info);
    EXPECT_EQ(info.name, "Byte Alignment Analysis");
    EXPECT_EQ(info.statistics.size(), 2); // expect size 2
    analyzer.GenerateAdvisor(info, "compare");
    EXPECT_EQ(info.name, "Byte Alignment Analysis");
    EXPECT_EQ(info.statistics.size(), 2); // expect size 2
}

TEST_F(DbCommunicationTest, QueryRetransmissionAnalyzerClassificationData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::vector<Dic::Module::RetransmissionClassificationInfo> data;
    bool result = database->QueryRetransmissionAnalyzerData(data);
    int expectSize = 0;
    ASSERT_TRUE(result);
    ASSERT_EQ(data.size(), expectSize);
}

TEST_F(DbCommunicationTest, GetOpStatByStepIdSuccess) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(COMPARE);
    std::string stepId = "1";
    database->ClearCache();
    std::vector<Dic::Module::OpTypeStatistics> res = database->GetOpStatByStepId(stepId);
    const int expectSize = 4;
    ASSERT_EQ(res.size(), expectSize);
    ASSERT_EQ(res[0].opType, "hcomallGather");
    ASSERT_EQ(res[1].opType, "hcomallReduce");
}
