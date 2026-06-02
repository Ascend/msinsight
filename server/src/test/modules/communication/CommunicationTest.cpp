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
#include "CommunicationProtocolRequest.h"
#include "DataBaseManager.h"
#include "ClusterDomainObject.h"
#include "ClusterDef.h"
#include "../../TestSuit.h"

class CommunicationTest : TestSuit {};

TEST_F(TestSuit, QueryIterationsData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::vector<Dic::Protocol::IterationsOrRanksObject> responseBody;
    database->QueryIterations(responseBody);
    int expectSize = 1;
    EXPECT_EQ(responseBody.size(), expectSize);
    EXPECT_EQ(responseBody[0].iterationOrRankId, "2");
}

TEST_F(TestSuit, QueryOperatorNameData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    Dic::Protocol::OperatorNamesParams requestParams;
    requestParams.iterationId = "2";
    requestParams.groupIdHash = "13337953553061386822";
    std::vector<Dic::Protocol::OperatorNamesObject> responseBody;
    database->QueryOperatorNames(requestParams, responseBody);
    int expectSize = 2;
    EXPECT_EQ(responseBody.size(), expectSize);
    EXPECT_EQ(responseBody[0].operatorName, "Total Op Info");
    EXPECT_EQ(responseBody[1].operatorName, "hcom_send__822_0");
}

TEST_F(TestSuit, QueryIterationAndCommunicationGroupSuccess) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    Dic::Protocol::CommunicationKernelParams requestParams;
    requestParams.name = "hcom_broadcast__483_0";
    requestParams.rankId = "0";
    Dic::Protocol::CommunicationKernelBody responseBody;
    database->QueryIterationAndCommunicationGroup(requestParams, responseBody);
    EXPECT_EQ(responseBody.step, "2");
    EXPECT_EQ(responseBody.group, "(0, 1, 2, 3, 4, 5, 6, 7)");
}

TEST_F(TestSuit, QueryOperatorNamesFailWithErrorGroupIdHash) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    Dic::Protocol::OperatorNamesParams requestParams;
    requestParams.iterationId = "2";
    requestParams.groupIdHash = "test";
    std::vector<Dic::Protocol::OperatorNamesObject> responseBody;
    database->QueryOperatorNames(requestParams, responseBody);
    int expectSize = 0;
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TestSuit, QueryParseClusterStatusSuccess) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::string status = database->QueryParseClusterStatus();
    EXPECT_EQ(status, "FINISH");
}

TEST_F(TestSuit, QueryRanksData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::vector<Dic::Protocol::IterationsOrRanksObject> responseBody;
    database->QueryRanksHandler(responseBody);
    int expectSize = 16;
    EXPECT_EQ(responseBody.size(), expectSize);
    EXPECT_EQ(responseBody[0].iterationOrRankId, "0");
    EXPECT_EQ(responseBody[1].iterationOrRankId, "1");
}

TEST_F(TestSuit, QueryDurationData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    Dic::Protocol::DurationListParams requestParams;
    std::vector<Dic::Module::DurationDo> durationList;
    requestParams.iterationId = "2";
    requestParams.operatorName = "hcom_send__822_0";
    requestParams.groupIdHash = "13337953553061386822";
    database->QueryDurationList(requestParams, durationList);
    int expectSize = 2;
    EXPECT_EQ(durationList.size(), expectSize);
}

TEST_F(TestSuit, QueryBandwidthDistributionData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    Dic::Protocol::DistributionDataParam requestParams;
    Dic::Protocol::DistributionResBody responseBody;
    requestParams.iterationId = "2";
    requestParams.stage = "(0, 1, 2, 3, 4, 5, 6, 7)";
    requestParams.operatorName = "hcom_broadcast__483_1";
    requestParams.transportType = "HCCS";
    requestParams.rankId = "1";
    requestParams.groupIdHash = "9350434047717501483";
    database->QueryDistributionData(requestParams, responseBody);
    std::string expectResult = "{\"0.016512\":[7,0.01114],\"0.015504\":[1,0.00166]}";
    EXPECT_EQ(responseBody.distributionData, expectResult);
}

TEST_F(TestSuit, QueryBandwidthData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    Dic::Protocol::BandwidthDataParam requestParams;
    Dic::Protocol::BandwidthDataResBody responseBody;
    requestParams.iterationId = "2";
    requestParams.operatorName = "hcom_send__822_0";
    requestParams.rankId = "1";
    requestParams.groupIdHash = "13337953553061386822";
    database->QueryBandwidthData(requestParams, responseBody);
    int expectSize = 4;
    EXPECT_EQ(responseBody.items.size(), expectSize);
    EXPECT_EQ(responseBody.items[0].transportType, "RDMA");
    EXPECT_EQ(responseBody.items[0].transitSize, 20.9715); // transitSize = 20.9715
    EXPECT_EQ(responseBody.items[0].transitTime, 0.8638); // transitTime = 0.8638
    EXPECT_EQ(responseBody.items[0].bandwidth, 24.2777); // bandwidth = 24.2777
    EXPECT_EQ(responseBody.items[0].largePacketRatio, 0.0);
}

TEST_F(TestSuit, QueryBandwidthDataWithErrorParamReturnExpectSize0) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    Dic::Protocol::BandwidthDataParam requestParams;
    Dic::Protocol::BandwidthDataResBody responseBody;
    requestParams.iterationId = "100";
    requestParams.stage = "p2p";
    requestParams.operatorName = "Total Op Info";
    requestParams.rankId = "1";
    database->QueryBandwidthData(requestParams, responseBody);
    const int expectSize = 0;
    EXPECT_EQ(responseBody.items.size(), expectSize);
}

TEST_F(TestSuit, QueryOperatorsCount) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    Dic::Protocol::OperatorDetailsParam requestParams;
    Dic::Protocol::OperatorDetailsResBody responseBody;
    requestParams.iterationId = "2";
    requestParams.rankId = "1";
    requestParams.groupIdHash = "13337953553061386822";
    database->QueryOperatorsCount(requestParams, responseBody);
    const int expectSize = 1;
    EXPECT_EQ(responseBody.count, expectSize);
    requestParams.iterationId = "2";
    requestParams.rankId = "1";
    requestParams.groupIdHash = "9350434047717501483";
    database->QueryOperatorsCount(requestParams, responseBody);
    const int stageExpectSize = 2;
    EXPECT_EQ(responseBody.count, stageExpectSize);
}

TEST_F(TestSuit, GetCommunicationGroups) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::vector<Dic::Module::GroupInfoDo> groupList;
    database->GetGroups(groupList);
    int expectSize = 28;
    EXPECT_EQ(groupList.size(), expectSize);
}

TEST_F(TestSuit, GetAllRankFromStepStatisticInfoSuccess) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::vector<std::string> res = database->GetAllRankFromStepStatisticInfo();
    int expectSize = 16;
    EXPECT_EQ(res.size(), expectSize);
}

TEST_F(TestSuit, QueryMatrixData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    Dic::Protocol::MatrixBandwidthParam requestParams;
    std::vector<Dic::Module::MatrixInfoDo> matrixList;
    requestParams.iterationId = "2";
    requestParams.operatorName = "hcom_send__822_0";
    requestParams.groupIdHash = "13337953553061386822";
    database->QueryMatrixList(requestParams, matrixList);
    int expectSize = 2;
    EXPECT_EQ(matrixList.size(), expectSize);
    EXPECT_EQ(matrixList[1].srcRank, 0);
    EXPECT_EQ(matrixList[1].dstRank, 8); // dstRank = 8
    EXPECT_EQ(matrixList[1].transportType, "RDMA");
    EXPECT_EQ(matrixList[1].transitSize, 20.9715); // transitSize = 20.9715
    EXPECT_EQ(matrixList[1].transitTime, 0.8638); // transitTime = 0.8638
    EXPECT_EQ(matrixList[1].bandwidth, 24.2778); // bandwidth = 24.2778
}

TEST_F(TestSuit, QueryAllCommunicationOperatorsDetails) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    Dic::Protocol::OperatorDetailsParam requestParams;
    Dic::Protocol::OperatorDetailsResBody responseBody;
    requestParams.iterationId = "2";
    requestParams.rankId = "1";
    requestParams.groupIdHash = "13337953553061386822";
    requestParams.currentPage = 1;
    requestParams.pageSize = 10; // pageSize = 10
    database->QueryAllOperators(requestParams, responseBody);
    int expectSize = 1;
    EXPECT_EQ(responseBody.allOperators.size(), expectSize);
    EXPECT_EQ(responseBody.allOperators[0].operatorName, "hcom_send__822_0");
    EXPECT_EQ(responseBody.allOperators[0].elapseTime, 0.8966); // elapseTime = 0.8966
    EXPECT_EQ(responseBody.allOperators[0].transitTime, 0.8638); // transitTime = 0.8638
    EXPECT_EQ(responseBody.allOperators[0].synchronizationTime, 0);
    EXPECT_EQ(responseBody.allOperators[0].waitTime, 0);
    EXPECT_EQ(responseBody.allOperators[0].idleTime, 0.0327); // idleTime = 0.0327
    EXPECT_EQ(responseBody.allOperators[0].synchronizationTimeRatio, 0.0);
    EXPECT_EQ(responseBody.allOperators[0].waitTimeRatio, 0.0);
}

TEST_F(TestSuit, QueryMatrixSortOpNames) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::vector<Dic::Protocol::OperatorNamesObject> responseBody;
    Dic::Protocol::OperatorNamesParams requestParams;
    requestParams.iterationId = "2";
    requestParams.groupIdHash = "9350434047717501483";
    requestParams.rankList = {};
    database->QueryMatrixSortOpNames(requestParams, responseBody);
    int expectSize = 2;
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TestSuit, QueryAllPerformanceDataByStepWhenSingleStep) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::string step = "2";
    std::unordered_map<std::uint32_t, Dic::Module::StepStatistic> data{};
    auto result = database->QueryAllPerformanceDataByStep(step, data);
    EXPECT_EQ(result, true);
    EXPECT_EQ(data.size(), 16); // 16
    EXPECT_EQ(data.at(0).prepareTime, 0);
    step = "3";
    data.clear();
    result = database->QueryAllPerformanceDataByStep(step, data);
    EXPECT_EQ(result, true);
    EXPECT_EQ(data.size(), 0);
}

TEST_F(TestSuit, GetCommTimeForRankDimByStepWhenSingleStep) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::string step = "2";
    std::vector<Dic::Module::CommInfoUnderRank> result = database->GetCommTimeForRankDim(step);
    const int exceptSize = 4;
    EXPECT_EQ(result.size(), exceptSize);
}

TEST_F(TestSuit, GetCommTimeForRankDimByStepWhenAllStep) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::string step = "";
    std::vector<Dic::Module::CommInfoUnderRank> result = database->GetCommTimeForRankDim(step);
    const int exceptSize = 4;
    EXPECT_EQ(result.size(), exceptSize);
}

TEST_F(TestSuit, QueryAllPerformanceDataByStepWhenAllStep) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::string step = "";
    std::unordered_map<std::uint32_t, Dic::Module::StepStatistic> data{};
    auto result = database->QueryAllPerformanceDataByStep(step, data);
    EXPECT_EQ(result, true);
    EXPECT_EQ(data.size(), 16); // 16
    EXPECT_EQ(data.at(0).prepareTime, 0);
    EXPECT_NEAR(data.at(0).freeTime, 69909.504, 0.0001); // 69909.504 for result, 0.0001 for error
    step = "All";
    data.clear();
    result = database->QueryAllPerformanceDataByStep(step, data);
    EXPECT_EQ(result, true);
    EXPECT_EQ(data.size(), 16); // 16
    EXPECT_EQ(data.at(0).prepareTime, 0);
    EXPECT_NEAR(data.at(0).freeTime, 69909.504, 0.0001); // 69909.504 for result, 0.0001 for error
}

TEST_F(TestSuit, QueryPacketAnalyzerDataTest) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::vector<Dic::Module::PacketAnalyzerData> data;
    bool result = database->QueryPacketAnalyzerData(data);
    int expectSize = 5; // 5
    ASSERT_TRUE(result);
    ASSERT_EQ(data.size(), expectSize);
    EXPECT_EQ(data[0].type, "RDMA");
    EXPECT_EQ(data[1].type, "RDMA");
    EXPECT_EQ(data[2].type, "SDMA"); // 2
    EXPECT_EQ(data[3].type, "SDMA"); // 3
    EXPECT_EQ(data[4].type, "SDMA"); // 4
}

TEST_F(TestSuit, QueryBandwidthContentionAnalyzerDataTest) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::vector<Dic::Module::BandwidthContentionSDMAInfo> res;
    std::string rankId = "0";
    bool result = database->QueryBandwidthContentionAnalyzerData(res, rankId);
    ASSERT_TRUE(result);
    ASSERT_EQ(res.size(), 2);
    EXPECT_EQ(res[0].name, "Total Op Info");
    EXPECT_EQ(res[1].name, "hcom_broadcast__483_1");
    rankId = "1";
    res.clear();
    result = database->QueryBandwidthContentionAnalyzerData(res, rankId);
    ASSERT_TRUE(result);
    ASSERT_EQ(res.size(), 3); // 3
    EXPECT_EQ(res[0].name, "Total Op Info");
    EXPECT_EQ(res[1].name, "hcom_broadcast__483_0");
    EXPECT_EQ(res[2].name, "hcom_broadcast__483_1");
}

TEST_F(TestSuit, QueryRetransmissionAnalyzerClassificationData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::vector<Dic::Module::RetransmissionClassificationInfo> data;
    bool result = database->QueryRetransmissionAnalyzerData(data);
    int expectSize = 2;
    ASSERT_TRUE(result);
    ASSERT_EQ(data.size(), expectSize);
}

TEST_F(TestSuit, GetOpStatByStepIdSuccess) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::string stepId = "2";
    std::vector<Dic::Module::OpTypeStatistics> res = database->GetOpStatByStepId(stepId);
    const int expectSize = 2;
    ASSERT_EQ(res.size(), expectSize);
    EXPECT_EQ(res[0].opType, "hcombroadcast");
    EXPECT_EQ(res[1].opType, "hcomsend");
}

TEST_F(TestSuit, GetCommTimeForRankDimTest) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::string iterationId = "2";
    auto commTimeForRankDim = database->GetCommTimeForRankDim(iterationId);
    EXPECT_EQ(commTimeForRankDim.size(), 4); // 2
}

TEST_F(TestSuit, QuerySlowOpByCommDurationTest) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetClusterDatabase(clusterPath);
    std::string fastestRankId = "1";
    Protocol::DurationListParams params;
    params.iterationId = "2";
    params.groupIdHash = "9350434047717501483";
    RankDetailsForSlowRank slowRank;
    slowRank.rankId = "0";
    auto res = database->QuerySlowOpByCommDuration(params, fastestRankId, slowRank);
    EXPECT_TRUE(res);
    EXPECT_EQ(slowRank.opDetails.size(), 2); // 2
}
