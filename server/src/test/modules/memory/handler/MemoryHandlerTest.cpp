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
#include <cmath>
#include "OperatorMemoryService.h"
#include "RenderEngine.h"
#include "../../../DatabaseTestCaseMockUtil.h"
#include "ServerLog.h"
#include "FindSliceByAllocationTimeHandler.h"
#include "QueryMemoryViewHandler.h"

using namespace Dic::Module::Memory;
class MemoryHandlerTest : public ::testing::Test {
  protected:
    class MemoryTableDefaultMock {
      public:
        void SetDb(sqlite3 *dbPtr) { db = dbPtr; }

      protected:
        sqlite3 *db = nullptr;
    };
    class OperatorTableMock : public OperatorTable, public MemoryTableDefaultMock {
      protected:
        void ExcuteQuery(const std::string &fileId, std::vector<OperatorPO> &result) override {
            OperatorTable::ExcuteQuery(db, result);
            ClearThreadLocal();
        }
    };

    class OpMemoryTableMock : public OpMemoryTable, public MemoryTableDefaultMock {
      protected:
        void ExcuteQuery(const std::string &fileId, std::vector<OpMemoryPO> &result) override {
            OpMemoryTable::ExcuteQuery(db, result);
            ClearThreadLocal();
        }
    };
    const std::string opMemorySql =
        "CREATE TABLE OP_MEMORY (name INTEGER, size INTEGER, allocationTime INTEGER, releaseTime INTEGER, "
        "activeReleaseTime INTEGER, duration INTEGER, activeDuration INTEGER, allocationTotalAllocated INTEGER, "
        "allocationTotalReserved INTEGER, allocationTotalActive INTEGER, releaseTotalAllocated INTEGER, "
        "releaseTotalReserved INTEGER, releaseTotalActive INTEGER, streamPtr INTEGER, deviceId INTEGER);";
};

/**
 * timeline不存在
 */
TEST_F(MemoryHandlerTest, TestFindSliceByAllocationTimeHandlerWhenTimelineNotExist) {
    FindSliceByAllocationTimeHandler handler(nullptr);
    std::unique_ptr<Dic::Protocol::MemoryFindSliceRequest> request =
        std::make_unique<Dic::Protocol::MemoryFindSliceRequest>();
    bool res = handler.HandleRequest(std::move(request));
    EXPECT_EQ(res, false);
}

TEST_F(MemoryHandlerTest, CompareMemoryCurvesMergesEarlierEqualAndLaterTimestamps) {
    QueryMemoryViewHandler handler;
    Dic::Protocol::MemoryViewData compare;
    compare.legends = {"Time", "Allocated"};
    compare.tempData = {1, 10, 3, 30, 5, 50};
    Dic::Protocol::MemoryViewData baseline;
    baseline.legends = {"Time", "Reserved"};
    baseline.tempData = {2, 20, 3, 33, 4, 40};
    Dic::Protocol::MemoryViewResponse response;
    handler.ExecuteComparisonAlgorithm(compare, baseline, response);
    EXPECT_EQ(
        response.data.legends, (std::vector<std::string>{"Time", "Allocated of Compare", "Reserved of Baseline"}));
    ASSERT_EQ(response.data.tempData.size(), 15U);
    EXPECT_DOUBLE_EQ(response.data.tempData[0], 1);
    EXPECT_TRUE(std::isnan(response.data.tempData[2]));
    EXPECT_DOUBLE_EQ(response.data.tempData[3], 2);
    EXPECT_TRUE(std::isnan(response.data.tempData[4]));
    EXPECT_DOUBLE_EQ(response.data.tempData[8], 33);
    EXPECT_DOUBLE_EQ(response.data.tempData[13], 50);
}

TEST_F(MemoryHandlerTest, CompareMemoryCurvesHandlesEmptySides) {
    QueryMemoryViewHandler handler;
    Dic::Protocol::MemoryViewData empty;
    Dic::Protocol::MemoryViewData baseline;
    baseline.legends = {"Time", "Reserved"};
    baseline.tempData = {2, 20};
    Dic::Protocol::MemoryViewResponse response;
    handler.ExecuteComparisonAlgorithm(empty, baseline, response);
    EXPECT_EQ(response.data.legends, (std::vector<std::string>{"Reserved of Baseline"}));
    EXPECT_EQ(response.data.tempData, (std::vector<double>{2, 20}));
    Dic::Protocol::MemoryViewResponse reverse;
    handler.ExecuteComparisonAlgorithm(baseline, empty, reverse);
    ASSERT_EQ(reverse.data.tempData.size(), 2U);
    EXPECT_DOUBLE_EQ(reverse.data.tempData[1], 20);
}

TEST_F(MemoryHandlerTest, MemoryViewRejectsBadParametersAndUnavailableRank) {
    QueryMemoryViewHandler handler;
    auto invalid = std::make_unique<Dic::Protocol::MemoryViewRequest>();
    EXPECT_FALSE(handler.HandleRequest(std::move(invalid)));

    auto missing = std::make_unique<Dic::Protocol::MemoryViewRequest>();
    missing->params.rankId = "unopened-rank";
    missing->params.type = Dic::Protocol::MEMORY_OVERALL_GROUP;
    EXPECT_FALSE(handler.HandleRequest(std::move(missing)));
}

TEST_F(MemoryHandlerTest, TestFindSliceByAllocationTimeHandlerWhenMemoryDataNotExist) {
    class RenderEngineMock : public Dic::Module::Timeline::RenderEngine {};
    std::unique_ptr<RenderEngineMock> renderEngineMockMock = std::make_unique<RenderEngineMock>();
    FindSliceByAllocationTimeHandler handler(std::move(renderEngineMockMock));
    std::unique_ptr<Dic::Protocol::MemoryFindSliceRequest> request =
        std::make_unique<Dic::Protocol::MemoryFindSliceRequest>();
    bool res = handler.HandleRequest(std::move(request));
    EXPECT_EQ(res, false);
}

TEST_F(MemoryHandlerTest, TestFindSliceByAllocationTimeHandlerNormal) {
    std::unique_ptr<OperatorTableMock> operatorTable = std::make_unique<OperatorTableMock>();
    std::unique_ptr<OpMemoryTableMock> opMemoryTable = std::make_unique<OpMemoryTableMock>();
    sqlite3 *db = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(db);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::CreateTable(db, opMemorySql);
    const std::string opMemoryData =
        "INSERT INTO \"main\".\"OP_MEMORY\" (\"name\", \"size\", \"allocationTime\", \"releaseTime\", "
        "\"activeReleaseTime\", \"duration\", \"activeDuration\", \"allocationTotalAllocated\", "
        "\"allocationTotalReserved\", \"allocationTotalActive\", \"releaseTotalAllocated\", \"releaseTotalReserved\", "
        "\"releaseTotalActive\", \"streamPtr\", \"deviceId\") VALUES (536870922, 4608, 1724670453468255710, "
        "1724670453468599630, 1724670453468599320, 343920, 343610, 19065050112, 27000832000, 19065050112, 19065045504, "
        "27000832000, 19065045504, 187651271017536, 0);";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(db, opMemoryData);
    operatorTable->SetDb(db);
    opMemoryTable->SetDb(db);
    std::unique_ptr<OperatorMemoryService> service =
        std::make_unique<OperatorMemoryService>(std::move(operatorTable), std::move(opMemoryTable));
    class RenderEngineMock : public Dic::Module::Timeline::RenderEngine {
      public:
        Dic::Module::Timeline::CompeteSliceDomain FindSliceByTimePoint(const std::string &fileId,
            const std::string &name, uint64_t timePoint, const std::string &metaType) override {
            Dic::Module::Timeline::CompeteSliceDomain slice;
            slice.pid = "kk";
            slice.tid = "kk";
            return slice;
        }
    };
    std::unique_ptr<RenderEngineMock> renderEngineMockMock = std::make_unique<RenderEngineMock>();
    FindSliceByAllocationTimeHandler handler(std::move(renderEngineMockMock), std::move(service));
    std::unique_ptr<Dic::Protocol::MemoryFindSliceRequest> request =
        std::make_unique<Dic::Protocol::MemoryFindSliceRequest>();
    request->params.id = "1";
    request->params.rankId = "0";
    bool res = handler.HandleRequest(std::move(request));
    EXPECT_EQ(res, true);
}
