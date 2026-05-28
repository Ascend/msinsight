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
#include "SummaryService.h"
#include "ConstantDefs.h"
#include "ClusterFileParser.h"
#include "DataBaseManager.h"
#include "TimeUtil.h"
#include "BaselineManager.h"
#include "TestSuit.h"

using namespace Dic::Module::Summary;
const int NUMBER_ZERO = 0;
const int NUMBER_SIXTEEN = 16;

class SummaryServiceTest : public ::testing::Test {
  protected:
    std::string filePath;
    std::string baselineFilePath;
    std::string dbPath;

    void SetUp() override {
        filePath = TestSuit::GetTestDataFile("cluster_analysis_output");
        baselineFilePath = TestSuit::GetTestDataFile("baseline_cluster", "cluster_analysis_output");
        dbPath = filePath + FILE_SEPARATOR + "cluster.db";
    }

    void InitParser(const std::string &dataPath) {
        // 集群解析，如果集群已解析，则只会初始化db，然后结束流程
        // database直接传空指针，建立连接池的动作在ParseClusterFiles中
        Dic::Module::FullDb::ClusterFileParser clusterFileParser(
            dataPath, nullptr, dataPath + Dic::TimeUtil::Instance().NowStr());
        clusterFileParser.ParseClusterFiles();
        clusterFileParser.ParseClusterStep2Files();
    }

    void Clear() { Dic::Module::FullDb::DataBaseManager::Instance().ClearClusterDb(); }
};

TEST_F(SummaryServiceTest, QueryCompareSummaryBaseInfoAllFail) {
    Clear();
    SummaryTopRankRequest request;
    request.params.isCompare = false;
    SummaryTopRankResponse response;
    SummaryService::QueryCompareSummaryBaseInfo(request, response);
    EXPECT_EQ(response.body.baseInfo.compare.filePath, "");
    EXPECT_EQ(response.body.baseInfo.baseline.filePath, "");
}

TEST_F(SummaryServiceTest, QueryCompareSummaryBaseInfoAllSuccess) {
    Clear();
    InitParser(filePath);
    InitParser(baselineFilePath);
    SummaryTopRankRequest request;
    request.params.isCompare = true;
    request.params.clusterPath = filePath;
    BaselineManager::Instance().SetBaselineClusterPath(baselineFilePath);
    SummaryTopRankResponse response;
    SummaryService::QueryCompareSummaryBaseInfo(request, response);
    EXPECT_EQ(response.body.baseInfo.compare.rankCount, NUMBER_SIXTEEN);
    EXPECT_EQ(response.body.baseInfo.baseline.rankCount, NUMBER_SIXTEEN);
    Clear();
}

TEST_F(SummaryServiceTest, QueryCompareSummaryBaseInfoOnlyCompareSuccess) {
    Clear();
    InitParser(filePath);
    SummaryTopRankRequest request;
    request.params.isCompare = true;
    request.params.clusterPath = filePath;
    SummaryTopRankResponse response;
    SummaryService::QueryCompareSummaryBaseInfo(request, response);
    EXPECT_EQ(response.body.baseInfo.compare.rankCount, NUMBER_SIXTEEN);
    EXPECT_EQ(response.body.baseInfo.baseline.rankCount, NUMBER_ZERO);
    Clear();
}

TEST_F(SummaryServiceTest, QueryCompareSummaryBaseInfoOnlyBaselineSuccess) {
    Clear();
    InitParser(baselineFilePath);
    SummaryTopRankRequest request;
    request.params.isCompare = true;
    BaselineManager::Instance().SetBaselineClusterPath(baselineFilePath);
    SummaryTopRankResponse response;
    SummaryService::QueryCompareSummaryBaseInfo(request, response);
    EXPECT_EQ(response.body.baseInfo.compare.rankCount, NUMBER_ZERO);
    EXPECT_EQ(response.body.baseInfo.baseline.rankCount, NUMBER_SIXTEEN);
    Clear();
}

TEST_F(SummaryServiceTest, QueryCompareSummaryParallelStrategyWithAlgIsNull) {
    Clear();
    InitParser(baselineFilePath);
    InitParser(filePath);
    ParallelismPerformance params;
    params.isCompare = true;
    params.clusterPath = filePath;
    BaselineManager::Instance().SetBaselineClusterPath(baselineFilePath);
    PerformanceIndicatorData indicatorData;
    SummaryService::QueryParallelismPerformanceInfo(params, indicatorData);
    EXPECT_EQ(indicatorData.indicators.size(), NUMBER_ZERO);
}

TEST_F(SummaryServiceTest, QueryCompareSummaryParallelStrategySuccessWhenIsCompare) {
    Clear();
    InitParser(baselineFilePath);
    InitParser(filePath);
    Dic::Module::ParallelStrategyConfig config;
    const int defaultSize = 2;
    config.ppSize = defaultSize;
    config.tpSize = defaultSize;
    config.dpSize = defaultSize;
    config.cpSize = defaultSize;
    config.epSize = defaultSize;
    std::string err;
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(dbPath, config, err);
    ParallelismPerformance params;
    params.dimension = "ep-dp-pp-cp-tp";
    params.step = "2";
    params.isCompare = true;
    params.config = config;
    params.clusterPath = filePath;
    BaselineManager::Instance().SetBaselineClusterPath(baselineFilePath);
    PerformanceIndicatorData indicatorData;
    SummaryService::QueryParallelismPerformanceInfo(params, indicatorData);
    EXPECT_EQ(indicatorData.performanceData.size(), NUMBER_SIXTEEN);
}

TEST_F(SummaryServiceTest, QueryCompareSummaryParallelStrategySuccessWithDpDimWhenIsNotCompare) {
    Clear();
    InitParser(baselineFilePath);
    InitParser(filePath);
    Dic::Module::ParallelStrategyConfig config;
    const int defaultSize = 2;
    config.ppSize = defaultSize;
    config.tpSize = defaultSize;
    config.dpSize = defaultSize;
    config.cpSize = defaultSize;
    config.epSize = defaultSize;
    std::string err;
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(dbPath, config, err);
    ParallelismPerformance params;
    params.dimension = "ep-dp";
    params.step = "2";
    params.isCompare = false;
    params.config = config;
    params.clusterPath = filePath;
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName(dbPath);
    algorithm->UpdateParallelDimension(params.dimension, config, err);
    PerformanceIndicatorData indicatorData;
    SummaryService::QueryParallelismPerformanceInfo(params, indicatorData);
    EXPECT_EQ(indicatorData.performanceData.size(), 2); // 2 for dpSize
}

TEST_F(SummaryServiceTest, QueryCompareSummaryParallelStrategySuccessWithPpDimWhenIsNotCompare) {
    Clear();
    InitParser(baselineFilePath);
    InitParser(filePath);
    Dic::Module::ParallelStrategyConfig config;
    const int defaultSize = 2;
    config.ppSize = defaultSize;
    config.tpSize = defaultSize;
    config.dpSize = defaultSize;
    config.cpSize = defaultSize;
    config.epSize = defaultSize;
    std::string err;
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(dbPath, config, err);
    ParallelismPerformance params;
    params.dimension = "ep-dp-pp";
    params.step = "2";
    params.isCompare = false;
    params.config = config;
    params.clusterPath = filePath;
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName(dbPath);
    algorithm->UpdateParallelDimension(params.dimension, config, err);
    PerformanceIndicatorData indicatorData;
    SummaryService::QueryParallelismPerformanceInfo(params, indicatorData);
    EXPECT_EQ(indicatorData.performanceData.size(), 4); // 4 for dpSize * ppSize
}

TEST_F(SummaryServiceTest, QueryCompareSummaryParallelStrategySuccessWithCpDimWhenIsNotCompare) {
    Clear();
    InitParser(baselineFilePath);
    InitParser(filePath);
    Dic::Module::ParallelStrategyConfig config;
    const int defaultSize = 2;
    config.ppSize = defaultSize;
    config.tpSize = defaultSize;
    config.dpSize = defaultSize;
    config.cpSize = defaultSize;
    config.epSize = defaultSize;
    std::string err;
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(dbPath, config, err);
    ParallelismPerformance params;
    params.dimension = "ep-dp-pp-cp";
    params.step = "2";
    params.isCompare = false;
    params.config = config;
    params.clusterPath = filePath;
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName(dbPath);
    algorithm->UpdateParallelDimension(params.dimension, config, err);
    PerformanceIndicatorData indicatorData;
    SummaryService::QueryParallelismPerformanceInfo(params, indicatorData);
    EXPECT_EQ(indicatorData.performanceData.size(), 8); // 8 for dpSize * ppSize * cpSize
}

TEST_F(SummaryServiceTest, QueryCompareSummaryParallelStrategySuccessWithTpDimWhenIsNotCompare) {
    Clear();
    InitParser(baselineFilePath);
    InitParser(filePath);
    Dic::Module::ParallelStrategyConfig config;
    const int defaultSize = 2;
    config.ppSize = defaultSize;
    config.tpSize = defaultSize;
    config.dpSize = defaultSize;
    config.cpSize = defaultSize;
    config.epSize = defaultSize;
    std::string err;
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(dbPath, config, err);
    ParallelismPerformance params;
    params.dimension = "ep-dp-pp-cp-tp";
    params.step = "2";
    params.isCompare = false;
    params.config = config;
    params.clusterPath = filePath;
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName(dbPath);
    algorithm->UpdateParallelDimension(params.dimension, config, err);
    PerformanceIndicatorData indicatorData;
    SummaryService::QueryParallelismPerformanceInfo(params, indicatorData);
    EXPECT_EQ(indicatorData.performanceData.size(), 16); // 16 for dpSize * ppSize * cpSize * tpSize
}

TEST_F(SummaryServiceTest, QueryCompareSummaryParallelStrategySuccessWithoutConfigWhenIsNotCompare) {
    Clear();
    InitParser(baselineFilePath);
    InitParser(filePath);
    Dic::Module::ParallelStrategyConfig config;
    const int defaultSize = 1;
    config.ppSize = defaultSize;
    config.tpSize = defaultSize;
    config.dpSize = defaultSize;
    config.cpSize = defaultSize;
    config.epSize = defaultSize;
    config.moeTpSize = defaultSize;
    std::string err;
    ParallelStrategyAlgorithmManager::Instance().AddOrUpdateAlgorithm(dbPath, config, err);
    ParallelismPerformance params;
    params.dimension = "ep-dp-pp-cp-tp";
    params.step = "2";
    params.isCompare = false;
    params.config = config;
    params.clusterPath = filePath;
    auto algorithm = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName(dbPath);
    algorithm->UpdateParallelDimension(params.dimension, config, err);
    PerformanceIndicatorData indicatorData;
    SummaryService::QueryParallelismPerformanceInfo(params, indicatorData);
    EXPECT_EQ(indicatorData.performanceData.size(), 16); // 16 ranks for cluster
}
