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
#include "TestSuit.h"
#include "FileUtil.h"
#include "WsSessionImpl.h"
#include "WsSessionManager.h"
#include "TraceTime.h"
#include "DataBaseManager.h"
#include "AffinityAPIAdvisor.h"
#include "AffinityOptimizerAdvisor.h"
#include "AclnnOpAdvisor.h"
#include "AICpuOpAdvisor.h"
#include "FusedOpAdvisor.h"
#include "OperatorDispatchAdvisor.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::Advisor;

class DBAdvisorTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        std::string dbPath = FileUtil::SplicePath(TestSuit::GetRootTestPath(), "data", "pytorch", "db", "level1",
            "rank0_ascend_pt", "ASCEND_PROFILER_OUTPUT", "ascend_pytorch_profiler_0.db");
        DataBaseManager::Instance().SetDataType(DataType::DB, dbPath);
        DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbPath);
        DataBaseManager::Instance().CreateTraceConnectionPool("0", dbPath);
    }

    static void TearDownTestSuite() { DataBaseManager::Instance().Clear(); }
};

TEST_F(DBAdvisorTest, QueryAffinityApiAdvisorSuccessInDb) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EXPECT_NE(db, nullptr);
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    Protocol::KernelDetailsParams params = {"duration", "DESC", 1, 10}; // 1是第1页，10是每页10条数据
    std::map<uint64_t, std::vector<Protocol::FlowLocation>> dataMap{};
    std::map<uint64_t, std::vector<uint32_t>> indexMap{};
    auto result = db->QueryAffinityAPIData(params, {"aten::reshape"}, startTime, dataMap, indexMap);
    const uint64_t expectTrackId = 13471134862269421;
    EXPECT_TRUE(result);
    EXPECT_EQ(dataMap.size(), 2);
    EXPECT_EQ(dataMap.begin()->first, expectTrackId);
    EXPECT_GT(dataMap.at(expectTrackId).size(), 1);
    EXPECT_EQ(indexMap.size(), 2);
    EXPECT_EQ(indexMap.begin()->first, expectTrackId);
    EXPECT_EQ(indexMap.at(expectTrackId).size(), 2);
}

TEST_F(DBAdvisorTest, QueryAffinityOptimizerAdvisorSuccessDb) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EXPECT_NE(db, nullptr);
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    Protocol::KernelDetailsParams params = {"duration", "DESC", 1, 10}; // 1是第1页，10是每页10条数据
    std::vector<Protocol::ThreadTraces> data{};
    auto result = db->QueryAffinityOptimizer(params, "'Optimizer.step#AdamW.step'", data, startTime);
    EXPECT_TRUE(result);
    EXPECT_EQ(data.size(), 0);
}

TEST_F(DBAdvisorTest, QueryAclNNOperatorAdvisorSuccessDb) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EXPECT_NE(db, nullptr);
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    std::vector<Protocol::KernelBaseInfo> data{};
    Protocol::KernelDetailsParams params = {"duration", "DESC", 1, 10}; // 1是第1页，10是每页10条数据
    auto result = db->QueryAclnnOpCountExceedThreshold(params, ACLNN_OP_CNT_THRESHOLD, data, startTime);
    EXPECT_TRUE(result);
    EXPECT_EQ(data.size(), 0);
}

TEST_F(DBAdvisorTest, QueryAICPUOperatorAdvisorSuccessDb) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EXPECT_NE(db, nullptr);
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    std::vector<Protocol::KernelBaseInfo> data{};
    Protocol::KernelDetailsParams params = {"duration", "DESC", 1, 10, 0, 0, "0", "0"}; // 1是第1页，10是每页10条数据
    auto result =
        db->QueryAICpuOpCanBeOptimized(params, AICPU_OP_EQUIVALENT_REPLACE, AICPU_OP_DATATYPE_RULE, data, startTime);
    EXPECT_TRUE(result);
    EXPECT_EQ(data.size(), 0);
}

TEST_F(DBAdvisorTest, QueryFusedOperatorAdvisorSuccessDb) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EXPECT_NE(db, nullptr);
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    Protocol::KernelDetailsParams params = {"duration", "DESC", 1, 10}; // 1是第1页，10是每页10条数据
    Protocol::OperatorFusionResBody resBody;
    auto result = db->QueryFusibleOpData(params, FUSEABLE_OPERATER_RULE_LIST, resBody, startTime);
    EXPECT_TRUE(result);
    EXPECT_EQ(resBody.size, 0);
}

TEST_F(DBAdvisorTest, QueryOperatorDispatchAdvisorSuccessOnDb) {
    auto db = Dic::Module::Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
    EXPECT_NE(db, nullptr);
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    Protocol::KernelDetailsParams params = {"duration", "DESC", 1, 10}; // 1是第1页，10是每页10条数据
    std::vector<Protocol::KernelBaseInfo> data{};
    auto result = db->QueryOperatorDispatchData(params, data, startTime, OPERATOR_COMPILE_CNT_THRESHOLD);
    EXPECT_TRUE(result); // less than threshold
    EXPECT_EQ(data.size(), 0); // The size of data is 19, but less than threshold 20 that data was clear
}
