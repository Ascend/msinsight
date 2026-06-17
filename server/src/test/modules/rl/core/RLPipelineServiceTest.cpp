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
#include "DataBaseManager.h"
#include "RLPipelineService.h"
#include "ParamsParser.h"
#include "RepositoryFactory.h"
#include "DataEngine.h"
#include "RenderEngine.h"


using namespace Dic::Module::FullDb;
using namespace Dic::Module::RL;
using namespace Dic::Module;
class RLPipelineServiceTest : public ::testing::Test {
  public:
    class DataEngineMock : public DataEngineInterface {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {};
        void QueryCompeteSliceByIds(const SliceQuery &sliceQuery, const std::vector<uint64_t> &sliceIds,
            std::vector<CompeteSliceDomain> &CompeteSliceVec) override {};
        bool QuerySliceDetailInfo(const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override {
            return true;
        };
        void QuerySliceIdsByCat(const SliceQuery &sliceQuery, std::vector<uint64_t> &sliceIds) override {};
        uint64_t QueryPythonFunctionCountByTrackId(const SliceQuery &sliceQuery) override { return 0; };
        bool QuerySliceByTimepointAndName(
            const SliceQuery &sliceQuery, CompeteSliceDomain &competeSliceDomain) override {
            return true;
        };
        void QueryCompeteSliceVecByTimeRangeAndTrackId(
            const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &sliceVec) override {};
        void QueryAllThreadInfo(const ThreadQuery &flowQuery,
            std::unordered_map<uint64_t, std::pair<std::string, std::string>> &threadInfo) override {};
        void QueryFlowPointByCategory(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override {};
        void QueryFlowPointByTimeRange(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override {};
        void QueryFlowPointByFlowId(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override {};
        void QueryAllFlagSlice(
            const SliceQuery &sliceQuery, std::vector<CompeteSliceDomain> &competeSliceDomain) override {};
        void SetRepositoryFactory(std::shared_ptr<RepositoryFactoryInterface>) override{};

        bool QuerySliceDetailInfoByNameList(
            const SliceQueryByNameList &params, std::vector<CompeteSliceDomain> &res) override {
            CompeteSliceDomain domain1;
            domain1.name = "generate_sequences";
            domain1.timestamp = 100;
            domain1.endTime = 1000;
            res.push_back(domain1);
            return true;
        }
    };
};

TEST_F(RLPipelineServiceTest, GetPipelineInfoSuccess) {
    auto renderEngine = RenderEngine::Instance();
    std::shared_ptr<DataEngineMock> dataEngineMock = std::make_shared<DataEngineMock>();
    DataBaseManager::Instance().Clear();
    renderEngine->SetDataEngineInterface(dataEngineMock);
    DataBaseManager::Instance().SetDataType(DataType::DB, "dbPath");
    DataBaseManager::Instance().CreateTraceConnectionPool("uboot14286042774212449010_0 0", "dbPath");
    Protocol::RLPipelineResponse response;
    bool res = Dic::Module::RL::RLPipelineService::Instance().GetPipelineInfo(response);
    const uint64_t expectMinTime = 100;
    const uint64_t expectMaxTime = 1000;
    const uint64_t expectSize = 1;
    EXPECT_EQ(res, true);
    EXPECT_EQ(response.body.minTime, expectMinTime);
    EXPECT_EQ(response.body.maxTime, expectMaxTime);
    EXPECT_EQ(response.body.taskData.size(), expectSize);
    EXPECT_EQ(response.body.stageTypeList.size(), expectSize);
    EXPECT_EQ(response.body.taskData[0].hostName, "uboot14286042774212449010");
}