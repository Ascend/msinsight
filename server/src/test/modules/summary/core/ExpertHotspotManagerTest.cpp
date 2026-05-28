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
#include "ConstantDefs.h"
#include "ClusterFileParser.h"
#include "DataBaseManager.h"
#include "ExpertHotspotManager.h"
#include "ClusterDef.h"
#include "TimeUtil.h"
#include "DataBaseManager.h"
#include "TraceFileParser.h"
#include "ParserStatusManager.h"
#include "RepositoryFactory.h"
#include "DataEngine.h"
#include "RenderEngine.h"
#include "ProjectParserFactory.h"
#include "TestSuit.h"

using namespace Dic::Module::FullDb;
class ExpertHotspotManagerTest : public ::testing::Test {
  protected:
    std::string filePath;
    std::string baselineFilePath;
    std::string hotspotPath;
    std::string deploymentPath;
    std::string heatMapProfilingPath;
    void SetUp() override {
        filePath = TestSuit::GetTestDataFile("cluster_analysis_output");
        baselineFilePath = TestSuit::GetTestDataFile("baseline_cluster", "cluster_analysis_output");
        hotspotPath = TestSuit::GetTestDataFile("expert_hotspot");
        deploymentPath = TestSuit::GetTestDataFile("expert_deployment");
        heatMapProfilingPath = TestSuit::GetTestDataFile("heatMap");
    }

    void InitParser(const std::string &dataPath) {
        // 集群解析，如果集群已解析，则只会初始化db，然后结束流程
        // database直接传空指针，建立连接池的动作在ParseClusterFiles中
        Dic::Module::FullDb::ClusterFileParser clusterFileParser(
            dataPath, nullptr, dataPath + Dic::TimeUtil::Instance().NowStr());
        clusterFileParser.ParseClusterFiles();
        clusterFileParser.ParseClusterStep2Files();
    }

    static void WaitParseEnd(std::vector<std::string> statusList) {
        while (true) {
            size_t i = 0;
            for (const auto &tmp : statusList) {
                if (ParserStatusManager::Instance().GetParserStatus(tmp) != ParserStatus::FINISH) {
                    break;
                } else {
                    i++;
                }
            }
            if (i < statusList.size()) {
                continue;
            } else {
                Dic::Server::ServerLog::Info("parse end");
                return;
            }
        }
    }

    static void InitProfiling(const std::string &filePath) {
        const std::string dbPath = filePath + "mindstudio_insight_data.db";
        DataBaseManager::Instance().SetDataType(DataType::TEXT, dbPath);
        DataBaseManager::Instance().CreateTraceConnectionPool("100", dbPath);
        JsonFileParserManager::GetTraceFileParser().Parse({filePath + "trace_view.json"}, "100", "", "");
        WaitParseEnd({"100"});
    }

    void Clear() {
        auto db = Dic::Module::FullDb::DataBaseManager::Instance().GetClusterDatabase(filePath);
        if (db != nullptr) {
            // 清空热点数据
            db->DeleteExpertHotspot("decode", "1");
            db->DeleteExpertHotspot("prefill", "1");
            db->DeleteExpertHotspot("", "profiling");
            // baseinfo内容置0
            std::map<std::string, std::string> baseInfoMap;
            baseInfoMap[Dic::Module::KEY_DENSE_LAYER_LIST] = "";
            baseInfoMap[Dic::Module::KEY_MOE_LAYER] = "0";
            baseInfoMap[Dic::Module::KEY_RANK_NUMBER] = "0";
            baseInfoMap[Dic::Module::KEY_EXPERT_NUMBER] = "0";
            baseInfoMap[Dic::Module::KEY_MODEL_LAYER] = "0";
            db->InsertDuplicateUpdateBaseInfo(baseInfoMap);
        }
        db.reset();
        Dic::Module::FullDb::DataBaseManager::Instance().ClearClusterDb();
        TraceFileParser::DeleteParseFiles({"100"});
    }
};

TEST_F(ExpertHotspotManagerTest, InitExpertHotspotDataSuccess) {
    InitParser(filePath);
    std::string error;
    Dic::Module::Summary::ExpertHotspotManager::InitExpertHotspotData(hotspotPath, "1", error, filePath);
    auto res = Dic::Module::Summary::ExpertHotspotManager::QueryExpertHotspotData(filePath, "decode", "1");
    const int exceptSize = 232;
    EXPECT_EQ(res.size(), exceptSize);
    Clear();
}

TEST_F(ExpertHotspotManagerTest, UpdateModelInfoSuccess) {
    InitParser(filePath);
    std::string error;
    Dic::Module::Summary::ExpertHotspotManager::InitExpertHotspotData(hotspotPath, "1", error, filePath);
    Dic::Module::ModelInfo modelInfo{{0, 2}, 0, 0, 4, 61};
    Dic::Module::Summary::ExpertHotspotManager::UpdateModelInfo(filePath, modelInfo, error);
    auto res = Dic::Module::Summary::ExpertHotspotManager::QueryExpertHotspotData(filePath, "decode", "1");
    const int exceptSize = 244;
    const int numberZero = 0;
    const int exceptVisits = 35661;
    const int numberOne = 1;
    const int numberFive = 5;
    const int numberEight = 8;
    EXPECT_EQ(res.size(), exceptSize);
    EXPECT_EQ(res[numberOne].visits, numberZero);
    EXPECT_EQ(res[numberFive].visits, exceptVisits);
    EXPECT_EQ(res[numberEight].visits, numberZero);
    EXPECT_EQ(res[res.size() - 1].visits, numberZero);
    Clear();
}

TEST_F(ExpertHotspotManagerTest, QueryWithoutHotspotData) {
    InitParser(filePath);
    std::string error;
    Dic::Module::ModelInfo modelInfo{{0, 2}, 0, 0, 4, 61};
    Dic::Module::Summary::ExpertHotspotManager::UpdateModelInfo(filePath, modelInfo, error);
    auto res = Dic::Module::Summary::ExpertHotspotManager::QueryExpertHotspotData(filePath, "decode", "1");
    const int exceptSize = 244;
    const int numberZero = 0;
    EXPECT_EQ(res.size(), exceptSize);
    EXPECT_EQ(res[1].visits, numberZero);
    Clear();
}

TEST_F(ExpertHotspotManagerTest, InitExpertDeploymentDataSuccess) {
    InitParser(filePath);
    std::string error;
    Dic::Module::Summary::ExpertHotspotManager::InitExpertHotspotData(deploymentPath, "1", error, filePath);
    auto res = Dic::Module::Summary::ExpertHotspotManager::QueryExpertHotspotData(filePath, "prefill", "1");
    const int exceptSize = 18560;
    EXPECT_EQ(res.size(), exceptSize);
    Clear();
}

TEST_F(ExpertHotspotManagerTest, ParseHeatMapFromProfilingWithoutTraceDb) {
    Dic::Module::ParserFactory::Reset();
    InitParser(filePath);
    auto respotoryFactory = RepositoryFactory::Instance();
    auto dataEngine = DataEngine::Instance();
    dataEngine->SetRepositoryFactory(respotoryFactory);
    auto renderEngine = RenderEngine::Instance();
    renderEngine->SetDataEngineInterface(dataEngine);
    std::string errorMsg;
    bool res = Dic::Module::Summary::ExpertHotspotManager::UpdateHeatMapFromProfiling(errorMsg, filePath, {"0"});
    EXPECT_EQ(res, false);
    Clear();
    Dic::Module::ParserFactory::Reset();
}
