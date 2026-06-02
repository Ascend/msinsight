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
#include "ClusterService.h"
#include "ConstantDefs.h"
#include "ClusterFileParser.h"
#include "DataBaseManager.h"
#include "TimeUtil.h"
#include "BaselineManager.h"
#include "TestSuit.h"

using namespace Dic::Module::Communication;

const int NUMBER_ZERO = 0;
const int NUMBER_ONE = 1;
const int NUMBER_TWO = 2;

class ClusterServiceTest : public ::testing::Test {
  protected:
    std::string filePath;
    std::string baselineFilePath;
    std::string dbPath;
    std::string dbBaselinePath;

    void SetUp() override {
        filePath = TestSuit::GetTestDataFile("cluster_analysis_output");
        baselineFilePath = TestSuit::GetTestDataFile("baseline_cluster", "cluster_analysis_output");
        dbPath = filePath + FILE_SEPARATOR + "cluster.db";
        dbBaselinePath = baselineFilePath + FILE_SEPARATOR + "cluster.db";
    }

    static void InitParser(const std::string &dataPath) {
        // 集群解析，如果集群已解析，则只会初始化db，然后结束流程
        // database直接传空指针，建立连接池的动作在ParseClusterFiles中
        Dic::Module::FullDb::ClusterFileParser clusterFileParser(
            dataPath, nullptr, dataPath + Dic::TimeUtil::Instance().NowStr());
        clusterFileParser.ParseClusterFiles();
        clusterFileParser.ParseClusterStep2Files();
    }

    static void Clear() { Dic::Module::FullDb::DataBaseManager::Instance().ClearClusterDb(); }
};

TEST_F(ClusterServiceTest, TestEraseClusterDb) {
    InitParser(filePath);
    auto res = Dic::Module::FullDb::DataBaseManager::Instance().GetAllClusterDatabase();
    EXPECT_EQ(res.size(), 1);
    res[0].reset();
    Dic::Module::FullDb::DataBaseManager::Instance().EraseClusterDb(dbPath);
    auto res2 = Dic::Module::FullDb::DataBaseManager::Instance().GetAllClusterDatabase();
    EXPECT_EQ(res2.size(), 0);
}

TEST_F(ClusterServiceTest, TestClearClusterDb) {
    InitParser(filePath);
    InitParser(baselineFilePath);
    auto res = Dic::Module::FullDb::DataBaseManager::Instance().GetAllClusterDatabase();
    EXPECT_EQ(res.size(), 2); // 2
    res[0].reset();
    res[1].reset();
    Dic::Module::FullDb::DataBaseManager::Instance().ClearClusterDb();
    auto res2 = Dic::Module::FullDb::DataBaseManager::Instance().GetAllClusterDatabase();
    EXPECT_EQ(res2.size(), NUMBER_ZERO);
}

TEST_F(ClusterServiceTest, QueryIterationsAllFail) {
    Clear();
    Dic::Protocol::IterationsRequest request;
    request.params.isCompare = false;
    Dic::Protocol::IterationsOrRanksResponse response;
    ClusterService::QueryIterations(request, response);
    EXPECT_EQ(response.body.compare.size(), NUMBER_ZERO);
    EXPECT_EQ(response.body.baseline.size(), NUMBER_ZERO);
}

TEST_F(ClusterServiceTest, QueryIterationsAllSuccess) {
    Clear();
    InitParser(filePath);
    InitParser(baselineFilePath);
    Dic::Protocol::IterationsRequest request;
    request.params.isCompare = true;
    request.params.clusterPath = filePath;
    BaselineManager::Instance().SetBaselineClusterPath(baselineFilePath);
    Dic::Protocol::IterationsOrRanksResponse response;
    ClusterService::QueryIterations(request, response);
    EXPECT_EQ(response.body.compare.size(), NUMBER_ONE);
    EXPECT_EQ(response.body.compare[0].iterationOrRankId, "2");
    EXPECT_EQ(response.body.baseline.size(), NUMBER_ONE);
    Clear();
}

TEST_F(ClusterServiceTest, QueryIterationsOnlyCompareSuccess) {
    Clear();
    InitParser(filePath);
    Dic::Protocol::IterationsRequest request;
    request.params.isCompare = true;
    request.params.clusterPath = filePath;
    Dic::Protocol::IterationsOrRanksResponse response;
    ClusterService::QueryIterations(request, response);
    EXPECT_EQ(response.body.compare.size(), NUMBER_ONE);
    EXPECT_EQ(response.body.baseline.size(), NUMBER_ZERO);
    Clear();
}

TEST_F(ClusterServiceTest, QueryGroupInfoAllFail) {
    Clear();
    Dic::Protocol::MatrixGroupRequest request;
    request.params.isCompare = false;
    Dic::Protocol::MatrixGroupResponse response;
    ClusterService::QueryGroupInfo(request, response);
    EXPECT_EQ(response.body.groupList.size(), NUMBER_ZERO);
    EXPECT_EQ(response.body.groupList.size(), NUMBER_ZERO);
}

TEST_F(ClusterServiceTest, QueryGroupInfoAllSuccess) {
    InitParser(filePath);
    InitParser(baselineFilePath);
    Dic::Protocol::MatrixGroupRequest request;
    request.params.iterationId = "2";
    request.params.baselineIterationId = "2";
    request.params.isCompare = true;
    request.params.clusterPath = filePath;
    BaselineManager::Instance().SetBaselineClusterPath(baselineFilePath);
    Dic::Protocol::MatrixGroupResponse response;
    ClusterService::QueryGroupInfo(request, response);
    EXPECT_EQ(response.body.groupList.size(), NUMBER_ONE);
    Clear();
}

TEST_F(ClusterServiceTest, QueryGroupInfoOnlyCompareSuccess) {
    InitParser(filePath);
    Dic::Protocol::MatrixGroupRequest request;
    request.params.iterationId = "2";
    request.params.baselineIterationId = "1";
    request.params.isCompare = true;
    request.params.clusterPath = filePath;
    Dic::Protocol::MatrixGroupResponse response;
    ClusterService::QueryGroupInfo(request, response);
    EXPECT_EQ(response.body.groupList.size(), NUMBER_ONE);
    Clear();
}

TEST_F(ClusterServiceTest, QueryMatrixInfoSuccess) {
    InitParser(filePath);
    InitParser(baselineFilePath);
    Dic::Protocol::MatrixBandwidthParam params = {"(0, 1, 2, 3, 4, 5, 6, 7)", "hcom_broadcast__483_0", "2", "",
        "9350434047717501483", true, "2", filePath, "9350434047717501483"};
    BaselineManager::Instance().SetBaselineClusterPath(baselineFilePath);
    Dic::Protocol::MatrixListResponseBody body;
    ClusterService::QueryMatrixInfo(params, body);
    EXPECT_EQ(body.matrixList.size(), NUMBER_TWO);
    EXPECT_EQ(body.matrixList[0].srcRank, NUMBER_ZERO);
    Clear();
}

TEST_F(ClusterServiceTest, QueryMatrixInfoFailOfDatabaseNotExist) {
    Clear();
    Dic::Protocol::MatrixBandwidthParam params = {
        "(0, 1, 2, 3, 4, 5, 6, 7)", "hcom_broadcast__483_0", "2", "", "9350434047717501483", true, "2", filePath};
    Dic::Protocol::MatrixListResponseBody body;
    ClusterService::QueryMatrixInfo(params, body);
    EXPECT_EQ(body.matrixList.size(), NUMBER_ZERO);
}

TEST_F(ClusterServiceTest, QueryOperatorListSuccess) {
    InitParser(filePath);
    InitParser(baselineFilePath);
    Dic::Protocol::DurationListParams params;
    params.operatorName = "hcom_broadcast__483_1";
    params.iterationId = "2";
    params.groupIdHash = "9350434047717501483";
    params.baselineGroupIdHash = "9350434047717501483";
    params.baselineIterationId = "2";
    params.isCompare = true;
    params.clusterPath = filePath;
    BaselineManager::Instance().SetBaselineClusterPath(baselineFilePath);
    Dic::Protocol::OperatorListsResponseBody body;
    ClusterService::QueryOperatorList(params, body);
    const uint64_t expectMaxTime = 11229980;
    const uint64_t expectMinTime = 10397500;
    EXPECT_EQ(body.opLists.size(), NUMBER_TWO);
    EXPECT_EQ(body.maxTime, expectMaxTime);
    EXPECT_EQ(body.minTime, expectMinTime);
    EXPECT_EQ(body.opLists[0].compare.size(), NUMBER_ONE);
    EXPECT_EQ(body.opLists[0].baseline.size(), NUMBER_ONE);
    Clear();
}

TEST_F(ClusterServiceTest, QueryOperatorListFailWithoutDb) {
    Clear();
    Dic::Protocol::DurationListParams params;
    params.operatorName = "hcom_broadcast__483_1";
    params.iterationId = "2";
    params.groupIdHash = "9350434047717501483";
    params.isCompare = true;
    params.clusterPath = filePath;
    Dic::Protocol::OperatorListsResponseBody body;
    ClusterService::QueryOperatorList(params, body);
    EXPECT_EQ(body.opLists.size(), NUMBER_ZERO);
}

TEST_F(ClusterServiceTest, QueryDurationListSuccess) {
    InitParser(filePath);
    InitParser(baselineFilePath);
    Dic::Protocol::DurationListParams params;
    params.operatorName = "hcom_broadcast__483_1";
    params.iterationId = "2";
    params.groupIdHash = "9350434047717501483";
    params.baselineGroupIdHash = "9350434047717501483";
    params.baselineIterationId = "2";
    params.isCompare = true;
    params.clusterPath = filePath;
    BaselineManager::Instance().SetBaselineClusterPath(baselineFilePath);
    Dic::Protocol::DurationListsResponseBody body;
    ClusterService::QueryDurationList(params, body);
    const double expectStartTime = 10.6855;
    EXPECT_EQ(body.durationList.size(), NUMBER_TWO);
    EXPECT_EQ(body.durationList[0].rankId, "0");
    EXPECT_EQ(body.durationList[0].durationData.compare.startTime, expectStartTime);
    EXPECT_EQ(body.durationList[0].durationData.baseline.startTime, expectStartTime);
    EXPECT_EQ(body.bwStatistics.size(), NUMBER_ONE);
    Clear();
}

TEST_F(ClusterServiceTest, QueryDurationListFailWithoutDb) {
    Clear();
    Dic::Protocol::DurationListParams params;
    params.operatorName = "hcom_broadcast__483_1";
    params.iterationId = "2";
    params.groupIdHash = "9350434047717501483";
    params.baselineIterationId = "2";
    params.isCompare = true;
    params.clusterPath = filePath;
    Dic::Protocol::DurationListsResponseBody body;
    ClusterService::QueryDurationList(params, body);
    EXPECT_EQ(body.durationList.size(), NUMBER_ZERO);
}

// operatorName不为Total Op Info时，无专家建议
TEST_F(ClusterServiceTest, AnalyzeCommunicationSlowRanksCheckOperatorNameFalse) {
    Clear();
    InitParser(filePath);
    Dic::Protocol::DurationListParams params;
    params.operatorName = "hcom_broadcast__483_1";
    params.iterationId = "2";
    params.groupIdHash = "9350434047717501483";
    params.isCompare = false;
    params.clusterPath = filePath;
    Dic::Protocol::CommunicationSlowRankAnalysisResponseBody body;
    auto res = ClusterService::AnalyzeCommunicationSlowRanks(params, body);
    EXPECT_EQ(res, true);
    EXPECT_EQ(body.hasAdvice, false);
}

// pgName为pp时，无专家建议
TEST_F(ClusterServiceTest, AnalyzeCommunicationSlowRanksCheckPgNameFalse) {
    Clear();
    InitParser(filePath);
    Dic::Protocol::DurationListParams params;
    params.operatorName = "Total Op Info";
    params.iterationId = "2";
    params.groupIdHash = "9350434047717501483";
    params.isCompare = false;
    params.pgName = "pp";
    params.clusterPath = filePath;
    Dic::Protocol::CommunicationSlowRankAnalysisResponseBody body;
    auto res = ClusterService::AnalyzeCommunicationSlowRanks(params, body);
    EXPECT_EQ(res, true);
    EXPECT_EQ(body.hasAdvice, false);
}

TEST_F(ClusterServiceTest, AnalyzeCommunicationSlowRanksSuccess) {
    Clear();
    InitParser(filePath);
    Dic::Protocol::DurationListParams params;
    params.operatorName = "Total Op Info";
    params.iterationId = "2";
    params.groupIdHash = "9350434047717501483";
    params.isCompare = false;
    params.clusterPath = filePath;
    Dic::Protocol::CommunicationSlowRankAnalysisResponseBody body;
    auto res = ClusterService::AnalyzeCommunicationSlowRanks(params, body);
    EXPECT_EQ(res, true);
    EXPECT_EQ(body.hasAdvice, true);
}
