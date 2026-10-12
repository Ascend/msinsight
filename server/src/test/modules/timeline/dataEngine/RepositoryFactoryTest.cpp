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
#include <array>
#include <algorithm>
#include <typeinfo>
#include "RepositoryFactory.h"
#include "DataEngine.h"
#include "HardWareRepo.h"
#include "HcclRepo.h"
#include "CcuRepo.h"
#include "DpuRepo.h"
#include "OverlapAnsRepo.h"
#include "CANNApiRepo.h"
#include "PythonApiRepo.h"
#include "OSRTApiRepo.h"
#include "MstxRepo.h"
#include "PythonGcRepo.h"
#include "TextRepository.h"
#include "DbFlowRepo.h"

using namespace Dic::Module::Timeline;
namespace {
struct SliceMapping {
    PROCESS_TYPE type;
    const std::type_info *implementation;
    bool python;
    bool text;
    bool findByTime;
    bool findByName;
};
const std::array<SliceMapping, 11> SLICE_MAPPINGS{{
    {PROCESS_TYPE::ASCEND_HARDWARE, &typeid(HardWareRepo), false, false, false, true},
    {PROCESS_TYPE::HCCL, &typeid(HcclRepo), false, false, false, false},
    {PROCESS_TYPE::CCU, &typeid(CcuRepo), false, false, false, false},
    {PROCESS_TYPE::DPU, &typeid(DpuRepo), false, false, false, false},
    {PROCESS_TYPE::OVERLAP_ANALYSIS, &typeid(OverlapAnsRepo), false, false, false, false},
    {PROCESS_TYPE::CANN_API, &typeid(CANNApiRepo), false, false, false, true},
    {PROCESS_TYPE::API, &typeid(PythonApiRepo), true, false, true, false},
    {PROCESS_TYPE::OSRT_API, &typeid(OSRTApiRepo), false, false, false, false},
    {PROCESS_TYPE::MS_TX, &typeid(MstxRepo), false, false, false, true},
    {PROCESS_TYPE::PYTHON_GC, &typeid(PythonGcRepo), false, false, false, false},
    {PROCESS_TYPE::TEXT, &typeid(TextRepository), true, true, true, true},
}};
}

TEST(RepositoryFactoryTest, SliceMappingsExposeExpectedImplementationAndCapabilities) {
    RepositoryFactory factory;
    for (const auto &mapping : SLICE_MAPPINGS) {
        SCOPED_TRACE(static_cast<int>(mapping.type));
        const auto repo = factory.GetSliceRespo(mapping.type);
        ASSERT_NE(repo, nullptr);
        EXPECT_TRUE(typeid(*repo) == *mapping.implementation);
        EXPECT_EQ(factory.GetSliceRespo(mapping.type), repo);
        EXPECT_EQ(dynamic_cast<IPythonFuncSlice *>(repo.get()) != nullptr, mapping.python);
        EXPECT_EQ(dynamic_cast<ITextSlice *>(repo.get()) != nullptr, mapping.text);
        EXPECT_EQ(dynamic_cast<IFindSliceByTimepointAndName *>(repo.get()) != nullptr, mapping.findByTime);
        EXPECT_EQ(dynamic_cast<IFindSliceByNameList *>(repo.get()) != nullptr, mapping.findByName);
    }
}

TEST(RepositoryFactoryTest, AllProcessTypesAndUnknownValuesHaveExplicitRouting) {
    RepositoryFactory factory;
    const auto simulation = factory.GetSimulationSliceRespo(PROCESS_TYPE::TEXT);
    ASSERT_NE(simulation, nullptr);
    EXPECT_NE(dynamic_cast<TextRepository *>(simulation.get()), nullptr);
    // PROCESS_TYPE is contiguous; include values immediately outside its declared range.
    for (int value = -1; value <= static_cast<int>(PROCESS_TYPE::UB) + 1; ++value) {
        const auto type = static_cast<PROCESS_TYPE>(value);
        SCOPED_TRACE(value);
        const bool hasSlice = std::any_of(
            SLICE_MAPPINGS.begin(), SLICE_MAPPINGS.end(), [type](const auto &mapping) { return mapping.type == type; });
        EXPECT_EQ(factory.GetSliceRespo(type) != nullptr, hasSlice);
        const auto flow = factory.GetFlowRespo(type);
        if (type == PROCESS_TYPE::TEXT) {
            ASSERT_NE(flow, nullptr);
            EXPECT_NE(dynamic_cast<TextRepository *>(flow.get()), nullptr);
            EXPECT_EQ(factory.GetFlowRespo(type), flow);
        } else if (type == PROCESS_TYPE::DB) {
            ASSERT_NE(flow, nullptr);
            EXPECT_NE(dynamic_cast<DbFlowRepo *>(flow.get()), nullptr);
            EXPECT_EQ(factory.GetFlowRespo(type), flow);
        } else {
            EXPECT_EQ(flow, nullptr);
        }
        EXPECT_EQ(factory.GetCounterRespo(type), nullptr);
        // Simulation deliberately uses the shared TEXT implementation for every type.
        EXPECT_EQ(factory.GetSimulationSliceRespo(type), simulation);
    }
}

TEST(RepositoryFactoryTest, DataEngineWithRealRepositoriesReturnsEmptyForMissingDataSource) {
    DataEngine engine;
    engine.SetRepositoryFactory(std::make_shared<RepositoryFactory>());
    for (const auto &mapping : SLICE_MAPPINGS) {
        SCOPED_TRACE(static_cast<int>(mapping.type));
        SliceQuery query;
        query.metaType = mapping.type;
        query.rankId = "timeline-ut-factory-missing-source";
        query.sliceId = "1";
        std::vector<SliceDomain> simple;
        engine.QuerySimpleSliceWithOutNameByTrackId(query, simple);
        EXPECT_TRUE(simple.empty());
        std::vector<uint64_t> ids;
        engine.QuerySliceIdsByCat(query, ids);
        EXPECT_TRUE(ids.empty());
        EXPECT_EQ(engine.QueryPythonFunctionCountByTrackId(query), 0u);
        std::vector<CompeteSliceDomain> complete;
        engine.QueryCompeteSliceVecByTimeRangeAndTrackId(query, complete);
        EXPECT_TRUE(complete.empty());
        engine.QueryCompeteSliceByIds(query, {1}, complete);
        EXPECT_TRUE(complete.empty());
        CompeteSliceDomain detail;
        EXPECT_FALSE(engine.QuerySliceDetailInfo(query, detail));
        EXPECT_TRUE(detail.name.empty());
        EXPECT_FALSE(engine.QuerySliceByTimepointAndName(query, detail));
        ThreadQuery threadQuery;
        threadQuery.metaType = mapping.type;
        threadQuery.fileId = query.rankId;
        std::unordered_map<uint64_t, std::pair<std::string, std::string>> threads;
        engine.QueryAllThreadInfo(threadQuery, threads);
        EXPECT_TRUE(threads.empty());
    }
    for (const auto type : {PROCESS_TYPE::TEXT, PROCESS_TYPE::DB, PROCESS_TYPE::NONE}) {
        SCOPED_TRACE(static_cast<int>(type));
        FlowQuery query;
        query.metaType = type;
        query.fileId = "timeline-ut-factory-missing-source";
        query.flowId = "missing-flow";
        std::vector<FlowPoint> points;
        engine.QueryFlowPointByTimeRange(query, points);
        EXPECT_TRUE(points.empty());
        engine.QueryFlowPointByFlowId(query, points);
        EXPECT_TRUE(points.empty());
        engine.QueryFlowPointByCategory(query, points);
        EXPECT_TRUE(points.empty());
    }
    SliceQuery query;
    query.rankId = "timeline-ut-factory-missing-source";
    std::vector<CompeteSliceDomain> flags;
    engine.QueryAllFlagSlice(query, flags);
    EXPECT_TRUE(flags.empty());
}
