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
#include <mockcpp/mockcpp.hpp>
#include "QueryMemoryComponentHandler.h"
#include "QueryMemoryOperatorHandler.h"
#include "QueryMemoryResourceTypeHandler.h"
#include "QueryMemoryStaticOperatorGraphHandler.h"
#include "QueryMemoryStaticOperatorListHandler.h"
#include "QueryMemoryStaticOperatorSizeHandler.h"
#include "QueryMemoryTypeHandler.h"
#include "QueryMemoryViewHandler.h"
#include "QueryMemoryOperatorSizeHandler.h"
#include "MemoryProtocolRespose.h"
#include "DataBaseManager.h"
#include "DbMemoryDataBase.h"
#include "TraceTime.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::Memory;
using namespace Dic::Protocol;
using namespace Dic::Module::FullDb;
using namespace Dic;

class MemoryRequestHandlerTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        std::string currPath = Dic::FileUtil::GetCurrPath();
        int index = currPath.find("server");
        currPath = currPath.substr(0, index);
        std::string dbPath = R"(test/data/pytorch/db/level1/rank0_ascend_pt/ASCEND_PROFILER_OUTPUT/)";
        std::string dbPathFull = StringUtil::StrJoin(currPath, dbPath, "ascend_pytorch_profiler_0.db");
        DataBaseManager::Instance().SetDataType(DataType::DB, dbPathFull);
        DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbPathFull);
        auto memoryDatabase = std::dynamic_pointer_cast<DbMemoryDataBase, VirtualMemoryDataBase>(
            DataBaseManager::Instance().CreateMemoryDataBase("0", dbPathFull));
        DataBaseManager::Instance().UpdateRankIdToDeviceId(dbPathFull, "0", "0");
        memoryDatabase->OpenDb(dbPathFull, false);
    }
    static void TearDownTestSuite() {
        auto memoryDatabase = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
        memoryDatabase->CloseDb();
        DataBaseManager::Instance().Clear();
    }
};

TEST_F(MemoryRequestHandlerTest, QueryMemoryComponentHandlerNormalTest) {
    const int64_t defaultPageSize = 10;
    Dic::Module::Memory::QueryMemoryComponentHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryComponentRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryComponentRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.currentPage = 1;
    requestPtr.get()->params.pageSize = defaultPageSize;
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_TRUE(result);
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryComponentHandlerInvalidParamTest) {
    const int64_t defaultPageSize = 10;
    Dic::Module::Memory::QueryMemoryComponentHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryComponentRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryComponentRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.currentPage = 1;
    requestPtr.get()->params.pageSize = defaultPageSize;
    requestPtr.get()->params.order = "ascend";
    requestPtr.get()->params.orderBy = "name";
    bool result = handler.HandleRequest(std::move(requestPtr));
    EXPECT_FALSE(result);
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryComponentHandlerGetComponentDiffCompareEmptyTest) {
    const std::vector<MemoryComponent> compareData;
    const std::vector<MemoryComponent> baselineData = {{"SLOG", "196.327", 259.36, ""}, {"TSYNC", "12.789", 10.65, ""}};
    std::vector<MemoryComponentComparison> diffData;
    Dic::Module::Memory::QueryMemoryComponentHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryComponentRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryComponentRequest>();
    handler.GetComponentDiff(compareData, baselineData, diffData);
    ASSERT_EQ(diffData.size(), baselineData.size());
    for (size_t i = 0; i < diffData.size(); ++i) {
        EXPECT_EQ(diffData[i].diff.component, baselineData[i].component);
        EXPECT_EQ(diffData[i].diff.timestamp, "-" + baselineData[i].timestamp);
        EXPECT_EQ(diffData[i].diff.totalReserved, -baselineData[i].totalReserved);
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryComponentHandlerGetComponentDiffBaselineEmptyTest) {
    const std::vector<MemoryComponent> compareData = {{"SLOG", "196.327", 259.36, ""}, {"TSYNC", "12.789", 10.65, ""}};
    const std::vector<MemoryComponent> baselineData;
    std::vector<MemoryComponentComparison> diffData;
    Dic::Module::Memory::QueryMemoryComponentHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryComponentRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryComponentRequest>();
    handler.GetComponentDiff(compareData, baselineData, diffData);
    ASSERT_EQ(diffData.size(), compareData.size());
    for (size_t i = 0; i < diffData.size(); ++i) {
        EXPECT_EQ(diffData[i].diff.component, compareData[i].component);
        EXPECT_EQ(diffData[i].diff.timestamp, compareData[i].timestamp);
        EXPECT_EQ(diffData[i].diff.totalReserved, compareData[i].totalReserved);
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryComponentHandlerGetComponentDiffBothNotEmptyTest) {
    const int precision = 3;
    const std::vector<MemoryComponent> compareData = {{"SLOG", "196.327", 259.36, ""}, {"TSYNC", "12.789", 10.65, ""}};
    const std::vector<MemoryComponent> baselineData = {{"SLOG", "25.397", 1285.27, ""}, {"TSYNC", "21.569", 22.67, ""}};
    std::vector<MemoryComponentComparison> diffData;
    Dic::Module::Memory::QueryMemoryComponentHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryComponentRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryComponentRequest>();
    handler.GetComponentDiff(compareData, baselineData, diffData);
    ASSERT_EQ(diffData.size(), compareData.size());
    ASSERT_EQ(diffData.size(), baselineData.size());
    for (size_t i = 0; i < diffData.size(); ++i) {
        EXPECT_EQ(diffData[i].diff.component, compareData[i].component);
        EXPECT_EQ(diffData[i].diff.component, baselineData[i].component);
        EXPECT_EQ(diffData[i].diff.timestamp,
            NumberUtil::StringDoubleMinus(compareData[i].timestamp, baselineData[i].timestamp));
        EXPECT_EQ(diffData[i].diff.totalReserved,
            NumberUtil::DoubleReservedNDigits(compareData[i].totalReserved - baselineData[i].totalReserved, precision));
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryComponentHandlerSelectResultEmptyTest) {
    std::vector<MemoryComponentComparison> fullDiffResult(10, {{}, {}, {"TSYNC", "125.697", 45.98, ""}});
    MemoryComponentRequest request;
    request.params.pageSize = 10; // page size = 10
    request.params.currentPage = 2; // current page = 2
    std::unique_ptr<MemoryComponentComparisonResponse> responsePtr =
        std::make_unique<MemoryComponentComparisonResponse>();
    QueryMemoryComponentHandler handler;
    handler.SelectResult(request, *responsePtr.get(), fullDiffResult);
    int expectNum = fullDiffResult.size();
    int expectSize = 0;
    EXPECT_EQ(responsePtr.get()->totalNum, expectNum);
    EXPECT_EQ(responsePtr.get()->componentDiffDetails.size(), expectSize);
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryComponentHandlerSelectResultNotEmptyTest) {
    std::vector<MemoryComponentComparison> fullDiffResult(135, {{}, {}, {"TSYNC", "125.697", 45.98, ""}});
    MemoryComponentRequest request;
    request.params.pageSize = 10; // page size = 10
    request.params.currentPage = 14; // current page = 14
    std::unique_ptr<MemoryComponentComparisonResponse> responsePtr =
        std::make_unique<MemoryComponentComparisonResponse>();
    QueryMemoryComponentHandler handler;
    handler.SelectResult(request, *responsePtr.get(), fullDiffResult);
    int expectNum = fullDiffResult.size();
    int expectSize = 5;
    EXPECT_EQ(responsePtr.get()->totalNum, expectNum);
    EXPECT_EQ(responsePtr.get()->componentDiffDetails.size(), expectSize);
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryComponentHandlerSortResultAscendTest) {
    std::vector<MemoryComponentComparison> result;
    MemoryComponent componentFirst = {"SLOG", "189.657", 29.67, ""};
    MemoryComponent componentSecond = {"APP", "29.301", 89.14, ""};
    MemoryComponent componentThird = {"PTA", "350.417", 125.99, ""};
    result.push_back({{}, {}, componentFirst});
    result.push_back({{}, {}, componentSecond});
    result.push_back({{}, {}, componentThird});
    Dic::Module::Memory::QueryMemoryComponentHandler handler;
    MemoryComponentRequest request;
    const size_t second = 2;
    request.params.order = "ascend";
    request.params.orderBy = "component";
    handler.SortResult(request, result);
    EXPECT_EQ(result[0].diff.component, "APP");
    EXPECT_EQ(result[1].diff.component, "PTA");
    EXPECT_EQ(result[second].diff.component, "SLOG");
    request.params.orderBy = "timestamp";
    handler.SortResult(request, result);
    EXPECT_EQ(result[0].diff.component, "APP");
    EXPECT_EQ(result[1].diff.component, "SLOG");
    EXPECT_EQ(result[second].diff.component, "PTA");
    request.params.orderBy = "totalReserved";
    handler.SortResult(request, result);
    EXPECT_EQ(result[0].diff.component, "SLOG");
    EXPECT_EQ(result[1].diff.component, "APP");
    EXPECT_EQ(result[second].diff.component, "PTA");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryComponentHandlerSortResultDescendTest) {
    std::vector<MemoryComponentComparison> result;
    MemoryComponent componentFirst = {"SLOG", "189.657", 29.67, ""};
    MemoryComponent componentSecond = {"APP", "29.301", 89.14, ""};
    MemoryComponent componentThird = {"PTA", "350.417", 125.99, ""};
    result.push_back({{}, {}, componentFirst});
    result.push_back({{}, {}, componentSecond});
    result.push_back({{}, {}, componentThird});
    Dic::Module::Memory::QueryMemoryComponentHandler handler;
    MemoryComponentRequest request;
    const size_t second = 2;
    request.params.order = "descend";
    request.params.orderBy = "component";
    handler.SortResult(request, result);
    EXPECT_EQ(result[0].diff.component, "SLOG");
    EXPECT_EQ(result[1].diff.component, "PTA");
    EXPECT_EQ(result[second].diff.component, "APP");
    request.params.orderBy = "timestamp";
    handler.SortResult(request, result);
    EXPECT_EQ(result[0].diff.component, "PTA");
    EXPECT_EQ(result[1].diff.component, "SLOG");
    EXPECT_EQ(result[second].diff.component, "APP");
    request.params.orderBy = "totalReserved";
    handler.SortResult(request, result);
    EXPECT_EQ(result[0].diff.component, "PTA");
    EXPECT_EQ(result[1].diff.component, "APP");
    EXPECT_EQ(result[second].diff.component, "SLOG");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerNormalTest) {
    const int64_t defaultPageSize = 10;
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryOperatorRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryOperatorRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.type = "Overall";
    requestPtr.get()->params.currentPage = 1;
    requestPtr.get()->params.pageSize = defaultPageSize;
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerInvalidParamTest) {
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryOperatorRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryOperatorRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.type = "Overall";
    requestPtr.get()->params.rangeFilters[std::string(OpMemoryColumn::SIZE)] = {1, -1};
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerComparisonGroupByStreamTest) {
    const int64_t defaultPageSize = 10;
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryOperatorRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryOperatorRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.type = "Stream";
    requestPtr.get()->params.currentPage = 1;
    requestPtr.get()->params.pageSize = defaultPageSize;
    requestPtr.get()->params.isCompare = true;
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerComparisonTestReturnFalse) {
    const int64_t defaultPageSize = 10;
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    MOCKER(&QueryMemoryOperatorHandler::GetRespectiveData).expects(once()).will(returnValue(false));
    std::unique_ptr<Dic::Protocol::MemoryOperatorRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryOperatorRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.type = "Overall";
    requestPtr.get()->params.minSize = 0;
    requestPtr.get()->params.maxSize = 0;
    requestPtr.get()->params.startTime = 0.0;
    requestPtr.get()->params.endTime = 0.0;
    requestPtr.get()->params.currentPage = 1;
    requestPtr.get()->params.pageSize = defaultPageSize;
    requestPtr.get()->params.isCompare = true;
    EXPECT_FALSE(handler.HandleRequest(std::move(requestPtr)));
    GlobalMockObject::verify();
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerGetOperatorDiffCompareEmptyTest) {
    std::vector<MemoryOperator> compareData;
    std::vector<MemoryOperator> baselineData;
    std::vector<MemoryOperatorComparison> diffData;
    MemoryOperator operatorFirst = {
        "aten::empty_strided", 100, "0.000", "0.000", 1000, "0.000", 1000, 1000, 1000, 1000, 1000, 1000, 1000, "", ""};
    MemoryOperator operatorSecond = {"matmul", 1000, "0.000", "0.000", 25, "0.000", 25, 25, 25, 25, 25, 25, 25, "", ""};
    baselineData.push_back(operatorFirst);
    baselineData.push_back(operatorSecond);
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    handler.GetOperatorDiff(compareData, baselineData, diffData);
    ASSERT_EQ(diffData.size(), baselineData.size());
    for (size_t i = 0; i < baselineData.size() && i < diffData.size(); ++i) {
        EXPECT_EQ(diffData[i].diff.name, baselineData[i].name);
        EXPECT_EQ(diffData[i].diff.size, -baselineData[i].size);
        EXPECT_EQ(diffData[i].diff.allocationTime, baselineData[i].allocationTime);
        EXPECT_EQ(diffData[i].diff.releaseTime, baselineData[i].releaseTime);
        EXPECT_EQ(diffData[i].diff.duration, -baselineData[i].duration);
        EXPECT_EQ(diffData[i].diff.activeReleaseTime, baselineData[i].activeReleaseTime);
        EXPECT_EQ(diffData[i].diff.activeDuration, -baselineData[i].activeDuration);
        EXPECT_EQ(diffData[i].diff.allocationAllocated, -baselineData[i].allocationAllocated);
        EXPECT_EQ(diffData[i].diff.allocationReserved, -baselineData[i].allocationReserved);
        EXPECT_EQ(diffData[i].diff.allocationActive, -baselineData[i].allocationActive);
        EXPECT_EQ(diffData[i].diff.releaseAllocated, -baselineData[i].releaseAllocated);
        EXPECT_EQ(diffData[i].diff.releaseReserved, -baselineData[i].releaseReserved);
        EXPECT_EQ(diffData[i].diff.releaseActive, -baselineData[i].releaseActive);
        EXPECT_EQ(diffData[i].diff.streamId, "");
        EXPECT_EQ(diffData[i].diff.deviceType, "");
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerGetOperatorDiffBaselineEmptyTest) {
    std::vector<MemoryOperator> compareData;
    std::vector<MemoryOperator> baselineData;
    std::vector<MemoryOperatorComparison> diffData;
    MemoryOperator operatorFirst = {
        "aten::empty_strided", 100, "0.000", "0.000", 1000, "0.000", 1000, 1000, 1000, 1000, 1000, 1000, 1000, "", ""};
    MemoryOperator operatorSecond = {"matmul", 1000, "0.000", "0.000", 25, "0.000", 25, 25, 25, 25, 25, 25, 25, "", ""};
    compareData.push_back(operatorFirst);
    compareData.push_back(operatorSecond);
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    handler.GetOperatorDiff(compareData, baselineData, diffData);
    ASSERT_EQ(diffData.size(), compareData.size());
    for (size_t i = 0; i < compareData.size() && i < diffData.size(); ++i) {
        EXPECT_EQ(diffData[i].diff.name, compareData[i].name);
        EXPECT_EQ(diffData[i].diff.size, compareData[i].size);
        EXPECT_EQ(diffData[i].diff.allocationTime, compareData[i].allocationTime);
        EXPECT_EQ(diffData[i].diff.releaseTime, compareData[i].releaseTime);
        EXPECT_EQ(diffData[i].diff.duration, compareData[i].duration);
        EXPECT_EQ(diffData[i].diff.activeReleaseTime, compareData[i].activeReleaseTime);
        EXPECT_EQ(diffData[i].diff.activeDuration, compareData[i].activeDuration);
        EXPECT_EQ(diffData[i].diff.allocationAllocated, compareData[i].allocationAllocated);
        EXPECT_EQ(diffData[i].diff.allocationReserved, compareData[i].allocationReserved);
        EXPECT_EQ(diffData[i].diff.allocationActive, compareData[i].allocationActive);
        EXPECT_EQ(diffData[i].diff.releaseAllocated, compareData[i].releaseAllocated);
        EXPECT_EQ(diffData[i].diff.releaseReserved, compareData[i].releaseReserved);
        EXPECT_EQ(diffData[i].diff.releaseActive, compareData[i].releaseActive);
        EXPECT_EQ(diffData[i].diff.streamId, "");
        EXPECT_EQ(diffData[i].diff.deviceType, "");
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerGetOperatorDiffBothNotEmptyTest) {
    std::vector<MemoryOperator> compareData;
    std::vector<MemoryOperator> baselineData;
    std::vector<MemoryOperatorComparison> diffData;
    MemoryOperator operatorCompareFirst = {
        "aten::empty_strided", 100, "19.235", "21.478", 2, "21.477", 2, 100, 100, 100, 100, 100, 100, "4589", ""};
    MemoryOperator operatorCompareSecond = {
        "matmul", 1000, "178.254", "199.375", 20, "199.372", 20, 1000, 1000, 1000, 1000, 1000, 1000, "2733", ""};
    compareData.push_back(operatorCompareFirst);
    compareData.push_back(operatorCompareSecond);
    MemoryOperator operatorBaselineFirst = {
        "aten::empty_strided", 173, "33.980", "49.211", 16, "49.210", 16, 173, 173, 173, 173, 173, 173, "3011", ""};
    MemoryOperator operatorBaselineSecond = {
        "matmul", 477, "1.230", "120.211", 119, "120.210", 119, 477, 477, 477, 477, 477, 477, "8924", ""};
    baselineData.push_back(operatorBaselineFirst);
    baselineData.push_back(operatorBaselineSecond);
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    handler.GetOperatorDiff(compareData, baselineData, diffData);
    ASSERT_EQ(diffData.size(), compareData.size());
    ASSERT_EQ(diffData.size(), baselineData.size());
    for (size_t i = 0; i < diffData.size(); ++i) {
        const MemoryOperator &result = diffData[i].diff;
        EXPECT_EQ(result.name, compareData[i].name);
        EXPECT_EQ(result.name, baselineData[i].name);
        EXPECT_EQ(result.size, compareData[i].size - baselineData[i].size);
        EXPECT_EQ(result.allocationTime,
            NumberUtil::StringDoubleMinus(compareData[i].allocationTime, baselineData[i].allocationTime));
        EXPECT_EQ(
            result.releaseTime, NumberUtil::StringDoubleMinus(compareData[i].releaseTime, baselineData[i].releaseTime));
        EXPECT_EQ(result.duration, compareData[i].duration - baselineData[i].duration);
        EXPECT_EQ(result.activeReleaseTime,
            NumberUtil::StringDoubleMinus(compareData[i].activeReleaseTime, baselineData[i].activeReleaseTime));
        EXPECT_EQ(result.activeDuration, compareData[i].activeDuration - baselineData[i].activeDuration);
        EXPECT_EQ(result.allocationAllocated, compareData[i].allocationAllocated - baselineData[i].allocationAllocated);
        EXPECT_EQ(result.allocationReserved, compareData[i].allocationReserved - baselineData[i].allocationReserved);
        EXPECT_EQ(result.allocationActive, compareData[i].allocationActive - baselineData[i].allocationActive);
        EXPECT_EQ(result.releaseAllocated, compareData[i].releaseAllocated - baselineData[i].releaseAllocated);
        EXPECT_EQ(result.releaseReserved, compareData[i].releaseReserved - baselineData[i].releaseReserved);
        EXPECT_EQ(result.releaseActive, compareData[i].releaseActive - baselineData[i].releaseActive);
        EXPECT_EQ(result.streamId, "");
        EXPECT_EQ(result.deviceType, "");
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerSelectDiffResultSelectNameAndSizeTest) {
    std::vector<MemoryOperatorComparison> fullDiffResult;
    MemoryOperator opCompareFirst = {"aten::ge", 50, "190.235", "211.478", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opBaselineFirst = {"aten::ge", -150, "186.235", "215.478", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opDiffFirst = {"aten::ge", -100, "4.000", "-4.000", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opCompareSecond = {"matmul", 200.25, "25.230", "120.233", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opBaselineSecond = {"matmul", 100.25, "17.650", "95.23", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opDiffSecond = {"matmul", 100, "7.58", "25.003", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opCompareThird = {"matmulv3", 18.2, "133.100", "145.257", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opBaselineThird = {"matmulv3", -18.2, "13.100", "148.257", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opDiffThird = {"matmulv3", 36.4, "120.000", "-3.000", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    fullDiffResult.push_back({opCompareFirst, opBaselineFirst, opDiffFirst});
    fullDiffResult.push_back({opCompareSecond, opBaselineSecond, opDiffSecond});
    fullDiffResult.push_back({opCompareThird, opBaselineThird, opDiffThird});
    MemoryOperatorRequest request;
    request.params.searchName = "matmul";
    const int64_t minSize = -75;
    const int64_t maxSize = 99;
    request.params.minSize = minSize;
    request.params.maxSize = maxSize;
    request.params.startTime = -1;
    request.params.endTime = -1;
    const int defaultPageSize = 10;
    request.params.pageSize = defaultPageSize;
    request.params.currentPage = 1;
    request.params.orderBy = "";
    std::unique_ptr<MemoryOperatorComparisonResponse> responsePtr =
        std::make_unique<MemoryOperatorComparisonResponse>();
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    handler.SelectDiffResult(request, *responsePtr.get(), fullDiffResult);
    ASSERT_EQ(responsePtr.get()->totalNum, 1);
    EXPECT_EQ(responsePtr.get()->operatorDiffDetails[0].diff.name, "matmulv3");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerSelectDiffResultSelectStartTimeAndEndTimeTest) {
    std::vector<MemoryOperatorComparison> fullDiffResult;
    MemoryOperator opCompareFirst = {"aten::ge", 50, "190.235", "211.478", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opBaselineFirst = {"aten::ge", -150, "186.235", "215.478", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opDiffFirst = {"aten::ge", -100, "4.000", "-4.000", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opCompareSecond = {"matmul", 200.25, "25.230", "120.233", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opBaselineSecond = {"matmul", 100.25, "17.650", "95.23", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opDiffSecond = {"matmul", 100, "7.58", "25.003", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opCompareThird = {"matmulv3", 18.2, "133.100", "145.257", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opBaselineThird = {"matmulv3", -18.2, "13.100", "148.257", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opDiffThird = {"matmulv3", 36.4, "120.000", "-3.000", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    fullDiffResult.push_back({opCompareFirst, opBaselineFirst, opDiffFirst});
    fullDiffResult.push_back({opCompareSecond, opBaselineSecond, opDiffSecond});
    fullDiffResult.push_back({opCompareThird, opBaselineThird, opDiffThird});
    MemoryOperatorRequest request;
    request.params.searchName = "";
    request.params.minSize = std::numeric_limits<int64_t>::min();
    request.params.maxSize = std::numeric_limits<int64_t>::max();
    const double startTime = 140.012;
    const double endTime = 216.054;
    request.params.startTime = startTime;
    request.params.endTime = endTime;
    const int defaultPageSize = 10;
    request.params.pageSize = defaultPageSize;
    request.params.currentPage = 1;
    request.params.orderBy = "";
    std::unique_ptr<MemoryOperatorComparisonResponse> responsePtr =
        std::make_unique<MemoryOperatorComparisonResponse>();
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    handler.SelectDiffResult(request, *responsePtr.get(), fullDiffResult);
    const int expectNum = 2;
    ASSERT_EQ(responsePtr.get()->totalNum, expectNum);
    EXPECT_EQ(responsePtr.get()->operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(responsePtr.get()->operatorDiffDetails[1].diff.name, "matmulv3");
}

// 测试框选了开始时间、结束时间，且只显示在选中时间区间内分配、释放内存的数据查询是否正确
TEST_F(
    MemoryRequestHandlerTest, QueryMemoryOperatorHandlerSelectDiffResultSelectStartTimeAndEndTimeOnlyShowWithinTest) {
    std::vector<MemoryOperatorComparison> fullDiffResult;
    MemoryOperator opCompareFirst = {"aten::ge", 50, "190.235", "211.478", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opBaselineFirst = {"aten::ge", -150, "136.235", "216.478", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opDiffFirst = {"aten::ge", -100, "4.000", "-4.000", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opCompareSecond = {"matmul", 200.25, "25.230", "120.233", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opBaselineSecond = {"matmul", 100.25, "17.650", "95.23", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opDiffSecond = {"matmul", 100, "7.58", "25.003", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opCompareThird = {"matmulv3", 18.2, "133.100", "145.257", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opBaselineThird = {"matmulv3", -18.2, "13.100", "148.257", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    MemoryOperator opDiffThird = {"matmulv3", 36.4, "120.000", "-3.000", 0, "", 0, 0, 0, 0, 0, 0, 0, "", ""};
    fullDiffResult.push_back({opCompareFirst, opBaselineFirst, opDiffFirst});
    fullDiffResult.push_back({opCompareSecond, opBaselineSecond, opDiffSecond});
    fullDiffResult.push_back({opCompareThird, opBaselineThird, opDiffThird});
    MemoryOperatorRequest request;
    request.params.searchName = "";
    request.params.minSize = std::numeric_limits<int64_t>::min();
    request.params.maxSize = std::numeric_limits<int64_t>::max();
    const double startTime = 140.012;
    const double endTime = 216.054;
    request.params.startTime = startTime;
    request.params.endTime = endTime;
    request.params.isOnlyShowAllocatedOrReleasedWithinInterval = true;
    const int defaultPageSize = 10;
    request.params.pageSize = defaultPageSize;
    request.params.currentPage = 1;
    request.params.orderBy = "";
    std::unique_ptr<MemoryOperatorComparisonResponse> responsePtr =
        std::make_unique<MemoryOperatorComparisonResponse>();
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    handler.SelectDiffResult(request, *responsePtr.get(), fullDiffResult);
    const int expectNum = 1;
    ASSERT_EQ(responsePtr.get()->totalNum, expectNum);
    EXPECT_EQ(responsePtr.get()->operatorDiffDetails[0].diff.name, "matmulv3");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerSelectDiffResultSortAscendTestPartOne) {
    MemoryOperatorComparisonResponse result;
    std::vector<MemoryOperatorComparison> opDiffDetails;
    MemoryOperator opDiffFirst = {"aten::ge", -100, "4.000", "-4.000", 12.4, "100.253", 898.74, -25.897, -111.28, 44.9,
        18.7, 25.39, -19.36, "1213", "NA2"};
    MemoryOperator opDiffSecond = {"matmul", 100, "7.58", "25.003", -10.8, "-15.780", -714.28, 14.289, 10.3, -25.1,
        19.6, 20.95, 22.3, "1210", "NA1"};
    MemoryOperator opDiffThird = {"matmulv3", 36.4, "120.000", "-3.000", 3, "21.962", 1.025, 0.213, -20.1, -776.9, 16.1,
        27.011, 100, "1212", "NA"};
    opDiffDetails.push_back({{}, {}, opDiffFirst});
    opDiffDetails.push_back({{}, {}, opDiffSecond});
    opDiffDetails.push_back({{}, {}, opDiffThird});
    result.operatorDiffDetails = opDiffDetails;
    MemoryOperatorRequest request;
    request.params.desc = false;
    const size_t second = 2;
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    request.params.orderBy = OpMemoryColumn::NAME;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmulv3");
    request.params.orderBy = OpMemoryColumn::SIZE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmul");
    request.params.orderBy = OpMemoryColumn::ALLOCATION_TIME;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmulv3");
    request.params.orderBy = OpMemoryColumn::RELEASE_TIME;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmul");
    request.params.orderBy = OpMemoryColumn::DURATION;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::ACTIVE_RELEASE_TIME;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerSelectDiffResultSortAscendTestPartTwo) {
    MemoryOperatorComparisonResponse result;
    std::vector<MemoryOperatorComparison> opDiffDetails;
    MemoryOperator opDiffFirst = {"aten::ge", -100, "4.000", "-4.000", 12.4, "100.253", 898.74, -25.897, -111.28, 44.9,
        18.7, 25.39, -19.36, "1213", "NA2"};
    MemoryOperator opDiffSecond = {"matmul", 100, "7.58", "25.003", -10.8, "-15.780", -714.28, 14.289, 10.3, -25.1,
        19.6, 20.95, 22.3, "1210", "NA1"};
    MemoryOperator opDiffThird = {"matmulv3", 36.4, "120.000", "-3.000", 3, "21.962", 1.025, 0.213, -20.1, -776.9, 16.1,
        27.011, 100, "1212", "NA"};
    opDiffDetails.push_back({{}, {}, opDiffFirst});
    opDiffDetails.push_back({{}, {}, opDiffSecond});
    opDiffDetails.push_back({{}, {}, opDiffThird});
    result.operatorDiffDetails = opDiffDetails;
    MemoryOperatorRequest request;
    request.params.desc = false;
    const size_t second = 2;
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    request.params.orderBy = OpMemoryColumn::ACTIVE_DURATION;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::ALLOCATION_ALLOCATED;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmul");
    request.params.orderBy = OpMemoryColumn::ALLOCATION_RESERVE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmul");
    request.params.orderBy = OpMemoryColumn::ALLOCATION_ACTIVE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::RELEASE_ALLOCATED;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmul");
    request.params.orderBy = OpMemoryColumn::RELEASE_RESERVE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmulv3");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerSelectDiffResultSortAscendTestPartThree) {
    MemoryOperatorComparisonResponse result;
    std::vector<MemoryOperatorComparison> opDiffDetails;
    MemoryOperator opDiffFirst = {"aten::ge", -100, "4.000", "-4.000", 12.4, "100.253", 898.74, -25.897, -111.28, 44.9,
        18.7, 25.39, -19.36, "1213", "NA2"};
    MemoryOperator opDiffSecond = {"matmul", 100, "7.58", "25.003", -10.8, "-15.780", -714.28, 14.289, 10.3, -25.1,
        19.6, 20.95, 22.3, "1210", "NA1"};
    MemoryOperator opDiffThird = {"matmulv3", 36.4, "120.000", "-3.000", 3, "21.962", 1.025, 0.213, -20.1, -776.9, 16.1,
        27.011, 100, "1212", "NA"};
    opDiffDetails.push_back({{}, {}, opDiffFirst});
    opDiffDetails.push_back({{}, {}, opDiffSecond});
    opDiffDetails.push_back({{}, {}, opDiffThird});
    result.operatorDiffDetails = opDiffDetails;
    MemoryOperatorRequest request;
    request.params.desc = false;
    const size_t second = 2;
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    request.params.orderBy = OpMemoryColumn::RELEASE_ACTIVE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmulv3");
    request.params.orderBy = OpMemoryColumn::STREAM;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::DEVICE_ID;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerSelectDiffResultSortDescendTestPartOne) {
    MemoryOperatorComparisonResponse result;
    std::vector<MemoryOperatorComparison> opDiffDetails;
    MemoryOperator opDiffFirst = {"aten::ge", -100, "4.000", "-4.000", 12.4, "100.253", 898.74, -25.897, -111.28, 44.9,
        18.7, 25.39, -19.36, "1213", "NA2"};
    MemoryOperator opDiffSecond = {"matmul", 100, "7.58", "25.003", -10.8, "-15.780", -714.28, 14.289, 10.3, -25.1,
        19.6, 20.95, 22.3, "1210", "NA1"};
    MemoryOperator opDiffThird = {"matmulv3", 36.4, "120.000", "-3.000", 3, "21.962", 1.025, 0.213, -20.1, -776.9, 16.1,
        27.011, 100, "1212", "NA"};
    opDiffDetails.push_back({{}, {}, opDiffFirst});
    opDiffDetails.push_back({{}, {}, opDiffSecond});
    opDiffDetails.push_back({{}, {}, opDiffThird});
    result.operatorDiffDetails = opDiffDetails;
    MemoryOperatorRequest request;
    request.params.desc = true;
    const size_t second = 2;
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    request.params.orderBy = OpMemoryColumn::NAME;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::SIZE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::ALLOCATION_TIME;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::RELEASE_TIME;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::DURATION;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmul");
    request.params.orderBy = OpMemoryColumn::ACTIVE_RELEASE_TIME;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmul");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerSelectDiffResultSortDescendTestPartTwo) {
    MemoryOperatorComparisonResponse result;
    std::vector<MemoryOperatorComparison> opDiffDetails;
    MemoryOperator opDiffFirst = {"aten::ge", -100, "4.000", "-4.000", 12.4, "100.253", 898.74, -25.897, -111.28, 44.9,
        18.7, 25.39, -19.36, "1213", "NA2"};
    MemoryOperator opDiffSecond = {"matmul", 100, "7.58", "25.003", -10.8, "-15.780", -714.28, 14.289, 10.3, -25.1,
        19.6, 20.95, 22.3, "1210", "NA1"};
    MemoryOperator opDiffThird = {"matmulv3", 36.4, "120.000", "-3.000", 3, "21.962", 1.025, 0.213, -20.1, -776.9, 16.1,
        27.011, 100, "1212", "NA"};
    opDiffDetails.push_back({{}, {}, opDiffFirst});
    opDiffDetails.push_back({{}, {}, opDiffSecond});
    opDiffDetails.push_back({{}, {}, opDiffThird});
    result.operatorDiffDetails = opDiffDetails;
    MemoryOperatorRequest request;
    request.params.desc = true;
    const size_t second = 2;
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    request.params.orderBy = OpMemoryColumn::ACTIVE_DURATION;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmul");
    request.params.orderBy = OpMemoryColumn::ALLOCATION_ALLOCATED;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::ALLOCATION_RESERVE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::ALLOCATION_ACTIVE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmulv3");
    request.params.orderBy = OpMemoryColumn::RELEASE_ALLOCATED;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmulv3");
    request.params.orderBy = OpMemoryColumn::RELEASE_RESERVE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmul");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorHandlerSelectDiffResultSortDescendTestPartThree) {
    MemoryOperatorComparisonResponse result;
    std::vector<MemoryOperatorComparison> opDiffDetails;
    MemoryOperator opDiffFirst = {"aten::ge", -100, "4.000", "-4.000", 12.4, "100.253", 898.74, -25.897, -111.28, 44.9,
        18.7, 25.39, -19.36, "1213", "NA2"};
    MemoryOperator opDiffSecond = {"matmul", 100, "7.58", "25.003", -10.8, "-15.780", -714.28, 14.289, 10.3, -25.1,
        19.6, 20.95, 22.3, "1210", "NA1"};
    MemoryOperator opDiffThird = {"matmulv3", 36.4, "120.000", "-3.000", 3, "21.962", 1.025, 0.213, -20.1, -776.9, 16.1,
        27.011, 100, "1212", "NA"};
    opDiffDetails.push_back({{}, {}, opDiffFirst});
    opDiffDetails.push_back({{}, {}, opDiffSecond});
    opDiffDetails.push_back({{}, {}, opDiffThird});
    result.operatorDiffDetails = opDiffDetails;
    MemoryOperatorRequest request;
    request.params.desc = true;
    const size_t second = 2;
    Dic::Module::Memory::QueryMemoryOperatorHandler handler;
    request.params.orderBy = OpMemoryColumn::RELEASE_ACTIVE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "aten::ge");
    request.params.orderBy = OpMemoryColumn::STREAM;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmulv3");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmul");
    request.params.orderBy = OpMemoryColumn::DEVICE_ID;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.name, "aten::ge");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.name, "matmul");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.name, "matmulv3");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryResourceTypeHandlerNormalTest) {
    Dic::Module::Memory::QueryMemoryResourceTypeHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryResourceTypeRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryResourceTypeRequest>();
    requestPtr.get()->rankId = "0";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryResourceTypeHandlerInvalidParamTest) {
    const int invalidLength = 600;
    Dic::Module::Memory::QueryMemoryResourceTypeHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryResourceTypeRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryResourceTypeRequest>();
    std::string invalidRankId(invalidLength, '0');
    requestPtr.get()->rankId = invalidRankId;
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorGraphHandlerNormalTest) {
    Dic::Module::Memory::QueryMemoryStaticOperatorGraphHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryStaticOperatorGraphRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryStaticOperatorGraphRequest>();
    requestPtr.get()->params.rankId = "0";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorGraphHandlerInvalidParamTest) {
    Dic::Module::Memory::QueryMemoryStaticOperatorGraphHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryStaticOperatorGraphRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryStaticOperatorGraphRequest>();
    requestPtr.get()->params.rankId = "";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorGraphHandlerGetCompareGraphLegendsTest) {
    StaticOperatorGraphItem compareData{{"Node Index", "Size", "Total Size"}, {}};
    StaticOperatorGraphItem baselineData{{"Node Index", "Size", "Total Size"}, {}};
    StaticOperatorGraphItem resultData;
    QueryMemoryStaticOperatorGraphHandler handler;
    handler.GetCompareGraphLegends(compareData, baselineData, resultData);
    ASSERT_EQ(resultData.legends.size(), compareData.legends.size() + baselineData.legends.size() - 1);
    EXPECT_EQ(resultData.legends[0], compareData.legends[0]);
    for (size_t i = 1; i < compareData.legends.size(); ++i) {
        EXPECT_EQ(resultData.legends[i], compareData.legends[i] + " of Compare");
    }
    for (size_t i = 1; i < baselineData.legends.size(); ++i) {
        EXPECT_EQ(resultData.legends[i + compareData.legends.size() - 1], baselineData.legends[i] + " of Baseline");
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorGraphHandlerGetCompareGraphLinesCompareEmptyTest) {
    StaticOperatorGraphItem compareData{{"Node Index", "Size", "Total Size"}, {}};
    StaticOperatorGraphItem baselineData{
        {"Node Index", "Size", "Total Size"}, {{"100", "2311.25", "106974.58"}, {"256", "3657.89", "106974.58"}}};
    StaticOperatorGraphItem resultData;
    QueryMemoryStaticOperatorGraphHandler handler;
    handler.GetCompareGraphLines(compareData, baselineData, resultData);
    ASSERT_EQ(resultData.lines.size(), baselineData.lines.size());
    for (size_t i = 0; i < resultData.lines.size(); ++i) {
        ASSERT_EQ(resultData.lines[i].size(), compareData.legends.size() + baselineData.legends.size() - 1);
        EXPECT_EQ(resultData.lines[i][0], baselineData.lines[i][0]);
        for (size_t j = 1; j < compareData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j], "NULL");
        }
        for (size_t j = 1; j < baselineData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j + compareData.legends.size() - 1], baselineData.lines[i][j]);
        }
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorGraphHandlerGetCompareGraphLinesBaselineEmptyTest) {
    StaticOperatorGraphItem compareData{
        {"Node Index", "Size", "Total Size"}, {{"100", "2311.25", "106974.58"}, {"256", "3657.89", "106974.58"}}};
    StaticOperatorGraphItem baselineData{{"Node Index", "Size", "Total Size"}, {}};
    StaticOperatorGraphItem resultData;
    QueryMemoryStaticOperatorGraphHandler handler;
    handler.GetCompareGraphLines(compareData, baselineData, resultData);
    ASSERT_EQ(resultData.lines.size(), compareData.lines.size());
    for (size_t i = 0; i < resultData.lines.size(); ++i) {
        ASSERT_EQ(resultData.lines[i].size(), compareData.legends.size() + baselineData.legends.size() - 1);
        EXPECT_EQ(resultData.lines[i][0], compareData.lines[i][0]);
        for (size_t j = 1; j < compareData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j], compareData.lines[i][j]);
        }
        for (size_t j = 1; j < baselineData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j + compareData.legends.size() - 1], "NULL");
        }
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorGraphHandlerGetCompareGraphLinesBothNotEmptyTest) {
    StaticOperatorGraphItem compareData{
        {"Node Index", "Size", "Total Size"}, {{"100", "2311.25", "106974.58"}, {"256", "3657.89", "106974.58"}}};
    StaticOperatorGraphItem baselineData{
        {"Node Index", "Size", "Total Size"}, {{"100", "2311.25", "106974.58"}, {"256", "3657.89", "106974.58"}}};
    StaticOperatorGraphItem resultData;
    QueryMemoryStaticOperatorGraphHandler handler;
    handler.GetCompareGraphLines(compareData, baselineData, resultData);
    ASSERT_EQ(resultData.lines.size(), compareData.lines.size());
    ASSERT_EQ(resultData.lines.size(), baselineData.lines.size());
    for (size_t i = 0; i < resultData.lines.size(); ++i) {
        ASSERT_EQ(resultData.lines[i].size(), compareData.legends.size() + baselineData.legends.size() - 1);
        EXPECT_EQ(resultData.lines[i][0], compareData.lines[i][0]);
        for (size_t j = 1; j < compareData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j], compareData.lines[i][j]);
        }
        for (size_t j = 1; j < baselineData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j + compareData.legends.size() - 1], baselineData.lines[i][j]);
        }
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorListHandlerNormalTest) {
    const int64_t defaultPageSize = 10;
    Dic::Module::Memory::QueryMemoryStaticOperatorListHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryStaticOperatorListRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryStaticOperatorListRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.currentPage = 1;
    requestPtr.get()->params.pageSize = defaultPageSize;
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorListHandlerInvalidParamTest) {
    Dic::Module::Memory::QueryMemoryStaticOperatorListHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryStaticOperatorListRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryStaticOperatorListRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.startNodeIndex = 1;
    requestPtr.get()->params.endNodeIndex = -1;
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorListHandlerGetOperatorDiffCompareEmptyTest) {
    std::vector<StaticOperatorItem> compareData;
    std::vector<StaticOperatorItem> baselineData;
    std::vector<StaticOperatorCompItem> diffData;
    StaticOperatorItem operatorFirst = {"host", "FlashAttentionScore-op8", 192, 337, 4096.031};
    StaticOperatorItem operatorSecond = {"host", "TensorMove-op0", 30, 1816, 88064.031};
    baselineData.push_back(operatorFirst);
    baselineData.push_back(operatorSecond);
    Dic::Module::Memory::QueryMemoryStaticOperatorListHandler handler;
    handler.GetOperatorDiff(compareData, baselineData, diffData);
    ASSERT_EQ(diffData.size(), baselineData.size());
    for (size_t i = 0; i < baselineData.size() && i < diffData.size(); ++i) {
        EXPECT_EQ(diffData[i].diff.deviceId, "");
        EXPECT_EQ(diffData[i].diff.opName, baselineData[i].opName);
        EXPECT_EQ(diffData[i].diff.nodeIndexStart, -baselineData[i].nodeIndexStart);
        EXPECT_EQ(diffData[i].diff.nodeIndexEnd, -baselineData[i].nodeIndexEnd);
        EXPECT_EQ(diffData[i].diff.size, -baselineData[i].size);
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorListHandlerGetOperatorDiffBaselineEmptyTest) {
    std::vector<StaticOperatorItem> compareData;
    std::vector<StaticOperatorItem> baselineData;
    std::vector<StaticOperatorCompItem> diffData;
    StaticOperatorItem operatorFirst = {"host", "FlashAttentionScore-op8", 192, 337, 4096.031};
    StaticOperatorItem operatorSecond = {"host", "TensorMove-op0", 30, 1816, 88064.031};
    compareData.push_back(operatorFirst);
    compareData.push_back(operatorSecond);
    Dic::Module::Memory::QueryMemoryStaticOperatorListHandler handler;
    handler.GetOperatorDiff(compareData, baselineData, diffData);
    ASSERT_EQ(diffData.size(), compareData.size());
    for (size_t i = 0; i < compareData.size() && i < diffData.size(); ++i) {
        EXPECT_EQ(diffData[i].diff.deviceId, compareData[i].deviceId);
        EXPECT_EQ(diffData[i].diff.opName, compareData[i].opName);
        EXPECT_EQ(diffData[i].diff.nodeIndexStart, compareData[i].nodeIndexStart);
        EXPECT_EQ(diffData[i].diff.nodeIndexEnd, compareData[i].nodeIndexEnd);
        EXPECT_EQ(diffData[i].diff.size, compareData[i].size);
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorListHandlerGetOperatorDiffBothNotEmptyTest) {
    std::vector<StaticOperatorItem> compareData;
    std::vector<StaticOperatorItem> baselineData;
    std::vector<StaticOperatorCompItem> diffData;
    StaticOperatorItem operatorCompareFirst = {"host", "FlashAttentionScore-op8", 192, 337, 4096.03125};
    StaticOperatorItem operatorCompareSecond = {"host", "TensorMove-op0", 30, 1816, 88064.03125};
    compareData.push_back(operatorCompareFirst);
    compareData.push_back(operatorCompareSecond);
    StaticOperatorItem operatorBaselineFirst = {"host", "FlashAttentionScore-op8", 398, 543, 4096.03125};
    StaticOperatorItem operatorBaselineSecond = {"host", "TensorMove-op0", 35, 1808, 88064.03125};
    baselineData.push_back(operatorBaselineFirst);
    baselineData.push_back(operatorBaselineSecond);
    Dic::Module::Memory::QueryMemoryStaticOperatorListHandler handler;
    handler.GetOperatorDiff(compareData, baselineData, diffData);
    const int precision = 3;
    ASSERT_EQ(diffData.size(), compareData.size());
    ASSERT_EQ(diffData.size(), baselineData.size());
    for (size_t i = 0; i < diffData.size(); ++i) {
        EXPECT_EQ(diffData[i].diff.deviceId, compareData[i].deviceId);
        EXPECT_EQ(diffData[i].diff.opName, compareData[i].opName);
        EXPECT_EQ(diffData[i].diff.nodeIndexStart, compareData[i].nodeIndexStart - baselineData[i].nodeIndexStart);
        EXPECT_EQ(diffData[i].diff.nodeIndexEnd, compareData[i].nodeIndexEnd - baselineData[i].nodeIndexEnd);
        EXPECT_EQ(diffData[i].diff.size,
            NumberUtil::DoubleReservedNDigits(compareData[i].size - baselineData[i].size, precision));
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorListHandlerSelectDiffResultSelectNameAndSizeTest) {
    std::vector<StaticOperatorCompItem> fullDiffResult;
    StaticOperatorItem opCompareFirst = {"host", "RmsNorm-op8", 198, 328, 16.031};
    StaticOperatorItem opBaselineFirst = {"host", "RmsNorm-op8", 105, 300, 15.000};
    StaticOperatorItem opDiffFirst = {"host", "RmsNorm-op8", 93, 28, 1.031};
    StaticOperatorItem opCompareSecond = {"host", "ReduceMax-op6", 1270, 1271, 24.095};
    StaticOperatorItem opBaselineSecond = {"host", "ReduceMax-op6", 1277, 1278, 15.092};
    StaticOperatorItem opDiffSecond = {"host", "ReduceMax-op6", -7, -7, 9.003};
    StaticOperatorItem opCompareThird = {"host", "OneHot-op8", 1608, 1685, 27.044};
    StaticOperatorItem opBaselineThird = {"host", "OneHot-op8", 1601, 1700, 28.049};
    StaticOperatorItem opDiffThird = {"host", "OneHot-op8", 7, -15, -1.005};
    fullDiffResult.push_back({opCompareFirst, opBaselineFirst, opDiffFirst});
    fullDiffResult.push_back({opCompareSecond, opBaselineSecond, opDiffSecond});
    fullDiffResult.push_back({opCompareThird, opBaselineThird, opDiffThird});
    MemoryStaticOperatorListRequest request;
    request.params.searchName = "op8";
    const int64_t minSize = -1;
    const int64_t maxSize = 2;
    request.params.minSize = minSize;
    request.params.maxSize = maxSize;
    request.params.startNodeIndex = -1;
    request.params.endNodeIndex = -1;
    const int defaultPageSize = 10;
    request.params.pageSize = defaultPageSize;
    request.params.currentPage = 1;
    request.params.orderBy = "";
    std::unique_ptr<MemoryStaticOperatorListCompResponse> responsePtr =
        std::make_unique<MemoryStaticOperatorListCompResponse>();
    Dic::Module::Memory::QueryMemoryStaticOperatorListHandler handler;
    handler.SelectDiffResult(request, *responsePtr.get(), fullDiffResult);
    ASSERT_EQ(responsePtr.get()->totalNum, 1);
    EXPECT_EQ(responsePtr.get()->operatorDiffDetails[0].diff.opName, "RmsNorm-op8");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorListHandlerSelectDiffResultSelectNodeIndexTest) {
    std::vector<StaticOperatorCompItem> fullDiffResult;
    StaticOperatorItem opCompareFirst = {"host", "RmsNorm-op8", 198, 328, 16.031};
    StaticOperatorItem opBaselineFirst = {"host", "RmsNorm-op8", 105, 300, 15.000};
    StaticOperatorItem opDiffFirst = {"host", "RmsNorm-op8", 93, 28, 1.031};
    StaticOperatorItem opCompareSecond = {"host", "ReduceMax-op6", 1270, 1271, 24.095};
    StaticOperatorItem opBaselineSecond = {"host", "ReduceMax-op6", 1277, 1278, 15.092};
    StaticOperatorItem opDiffSecond = {"host", "ReduceMax-op6", -7, -7, 9.003};
    StaticOperatorItem opCompareThird = {"host", "OneHot-op8", 1608, 1685, 27.044};
    StaticOperatorItem opBaselineThird = {"host", "OneHot-op8", 1601, 1700, 28.049};
    StaticOperatorItem opDiffThird = {"host", "OneHot-op8", 7, -15, -1.005};
    fullDiffResult.push_back({opCompareFirst, opBaselineFirst, opDiffFirst});
    fullDiffResult.push_back({opCompareSecond, opBaselineSecond, opDiffSecond});
    fullDiffResult.push_back({opCompareThird, opBaselineThird, opDiffThird});
    MemoryStaticOperatorListRequest request;
    request.params.searchName = "";
    request.params.minSize = std::numeric_limits<int64_t>::min();
    request.params.maxSize = std::numeric_limits<int64_t>::max();
    const int64_t start = 1200;
    const int64_t end = 1300;
    request.params.startNodeIndex = start;
    request.params.endNodeIndex = end;
    const int defaultPageSize = 10;
    request.params.pageSize = defaultPageSize;
    request.params.currentPage = 1;
    request.params.orderBy = "";
    std::unique_ptr<MemoryStaticOperatorListCompResponse> responsePtr =
        std::make_unique<MemoryStaticOperatorListCompResponse>();
    Dic::Module::Memory::QueryMemoryStaticOperatorListHandler handler;
    handler.SelectDiffResult(request, *responsePtr.get(), fullDiffResult);
    ASSERT_EQ(responsePtr.get()->totalNum, 1);
    EXPECT_EQ(responsePtr.get()->operatorDiffDetails[0].diff.opName, "ReduceMax-op6");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorListHandlerSelectDiffResultSortAscendTest) {
    MemoryStaticOperatorListCompResponse result;
    std::vector<StaticOperatorCompItem> opDiffDetails;
    StaticOperatorItem opDiffFirst = {"host", "RmsNorm-op8", 93, 28, 1.031};
    StaticOperatorItem opDiffSecond = {"host", "ReduceMax-op6", -7, -7, 9.003};
    StaticOperatorItem opDiffThird = {"host", "OneHot-op8", 7, -15, -1.005};
    opDiffDetails.push_back({{}, {}, opDiffFirst});
    opDiffDetails.push_back({{}, {}, opDiffSecond});
    opDiffDetails.push_back({{}, {}, opDiffThird});
    result.operatorDiffDetails = opDiffDetails;
    MemoryStaticOperatorListRequest request;
    request.params.desc = false;
    const size_t second = 2;
    Dic::Module::Memory::QueryMemoryStaticOperatorListHandler handler;
    request.params.orderBy = StaticOpColumn::DEVICE_ID;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.opName, "RmsNorm-op8");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.opName, "ReduceMax-op6");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.opName, "OneHot-op8");
    request.params.orderBy = StaticOpColumn::OP_NAME;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.opName, "OneHot-op8");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.opName, "ReduceMax-op6");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.opName, "RmsNorm-op8");
    request.params.orderBy = StaticOpColumn::NODE_INDEX_START;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.opName, "ReduceMax-op6");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.opName, "OneHot-op8");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.opName, "RmsNorm-op8");
    request.params.orderBy = StaticOpColumn::NODE_INDEX_END;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.opName, "OneHot-op8");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.opName, "ReduceMax-op6");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.opName, "RmsNorm-op8");
    request.params.orderBy = StaticOpColumn::SIZE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.opName, "OneHot-op8");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.opName, "RmsNorm-op8");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.opName, "ReduceMax-op6");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorListHandlerSelectDiffResultSortDescendTest) {
    MemoryStaticOperatorListCompResponse result;
    std::vector<StaticOperatorCompItem> opDiffDetails;
    StaticOperatorItem opDiffFirst = {"host", "RmsNorm-op8", 93, 28, 1.031};
    StaticOperatorItem opDiffSecond = {"host", "ReduceMax-op6", -7, -7, 9.003};
    StaticOperatorItem opDiffThird = {"host", "OneHot-op8", 7, -15, -1.005};
    opDiffDetails.push_back({{}, {}, opDiffFirst});
    opDiffDetails.push_back({{}, {}, opDiffSecond});
    opDiffDetails.push_back({{}, {}, opDiffThird});
    result.operatorDiffDetails = opDiffDetails;
    MemoryStaticOperatorListRequest request;
    request.params.desc = true;
    const size_t second = 2;
    Dic::Module::Memory::QueryMemoryStaticOperatorListHandler handler;
    request.params.orderBy = StaticOpColumn::DEVICE_ID;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.opName, "RmsNorm-op8");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.opName, "ReduceMax-op6");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.opName, "OneHot-op8");
    request.params.orderBy = StaticOpColumn::OP_NAME;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.opName, "RmsNorm-op8");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.opName, "ReduceMax-op6");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.opName, "OneHot-op8");
    request.params.orderBy = StaticOpColumn::NODE_INDEX_START;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.opName, "RmsNorm-op8");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.opName, "OneHot-op8");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.opName, "ReduceMax-op6");
    request.params.orderBy = StaticOpColumn::NODE_INDEX_END;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.opName, "RmsNorm-op8");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.opName, "ReduceMax-op6");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.opName, "OneHot-op8");
    request.params.orderBy = StaticOpColumn::SIZE;
    handler.SortResult(request, result);
    EXPECT_EQ(result.operatorDiffDetails[0].diff.opName, "ReduceMax-op6");
    EXPECT_EQ(result.operatorDiffDetails[1].diff.opName, "RmsNorm-op8");
    EXPECT_EQ(result.operatorDiffDetails[second].diff.opName, "OneHot-op8");
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorSizeHandlerNormalTest) {
    Dic::Module::Memory::QueryMemoryStaticOperatorSizeHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryStaticOperatorSizeRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryStaticOperatorSizeRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.graphId = "";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorSizeHandlerInvalidParamTest) {
    Dic::Module::Memory::QueryMemoryStaticOperatorSizeHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryStaticOperatorSizeRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryStaticOperatorSizeRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.graphId = "0&1";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryStaticOperatorSizeHandlerExecuteComparisonAlgorithmTest) {
    Protocol::StaticOperatorSize compareData{120.14, 259.99};
    Protocol::StaticOperatorSize baselineData{12.30, 558.97};
    Protocol::MemoryStaticOperatorSizeResponse response;
    Dic::Module::Memory::QueryMemoryStaticOperatorSizeHandler handler;
    handler.ExecuteComparisonAlgorithm(compareData, baselineData, response);
    EXPECT_EQ(response.size.minSize, compareData.minSize - baselineData.maxSize);
    EXPECT_EQ(response.size.maxSize, compareData.maxSize - baselineData.minSize);
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryTypeHandlerNormalTest) {
    Dic::Module::Memory::QueryMemoryTypeHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryTypeRequest> requestPtr = std::make_unique<Dic::Protocol::MemoryTypeRequest>();
    requestPtr.get()->rankId = "0";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryTypeHandlerInvalidParamTest) {
    Dic::Module::Memory::QueryMemoryTypeHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryTypeRequest> requestPtr = std::make_unique<Dic::Protocol::MemoryTypeRequest>();
    requestPtr.get()->rankId = "0&";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryViewHandlerNormalTest) {
    Dic::Module::Memory::QueryMemoryViewHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryViewRequest> requestPtr = std::make_unique<Dic::Protocol::MemoryViewRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.type = "Overall";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryViewHandlerInvalidParamTest) {
    Dic::Module::Memory::QueryMemoryViewHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryViewRequest> requestPtr = std::make_unique<Dic::Protocol::MemoryViewRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.type = "Invalid";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryViewHandlerGetCompareGraphLegendsTest) {
    MemoryViewData compareData{
        "", {"Time (ms)", "Operators Allocated", "Operators Activated", "Operators Reserved", "App Reserved"}, {}};
    MemoryViewData baselineData{
        "", {"Time (ms)", "Operators Allocated", "Operators Activated", "Operators Reserved", "App Reserved"}, {}};
    MemoryViewData resultData;
    QueryMemoryViewHandler handler;
    handler.GetCompareGraphLegends(compareData, baselineData, resultData);
    EXPECT_EQ(resultData.title, "");
    ASSERT_EQ(resultData.legends.size(), compareData.legends.size() + baselineData.legends.size() - 1);
    EXPECT_EQ(resultData.legends[0], compareData.legends[0]);
    for (size_t i = 1; i < compareData.legends.size(); ++i) {
        EXPECT_EQ(resultData.legends[i], compareData.legends[i] + " of Compare");
    }
    for (size_t i = 1; i < baselineData.legends.size(); ++i) {
        EXPECT_EQ(resultData.legends[i + compareData.legends.size() - 1], baselineData.legends[i] + " of Baseline");
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryViewHandlerGetCompareGraphLinesCompareEmptyTest) {
    MemoryViewData compareData{
        "", {"Time (ms)", "Operators Allocated", "Operators Activated", "Operators Reserved", "App Reserved"}, {}};
    MemoryViewData baselineData{"",
        {"Time (ms)", "Operators Allocated", "Operators Activated", "Operators Reserved", "App Reserved"},
        {{"10.032", "25.37", "13.22", "101.29", "1000.28"}, {"21.389", "100.88", "94.63", "150.32", "2000.01"}}};
    MemoryViewData resultData;
    QueryMemoryViewHandler handler;
    handler.GetCompareGraphLines(compareData, baselineData, resultData);
    ASSERT_EQ(resultData.tempData.size(), baselineData.tempData.size());
    for (size_t i = 0; i < resultData.lines.size(); ++i) {
        ASSERT_EQ(resultData.lines[i].size(), compareData.legends.size() + baselineData.legends.size() - 1);
        EXPECT_EQ(resultData.lines[i][0], baselineData.lines[i][0]);
        for (size_t j = 1; j < compareData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j], "NULL");
        }
        for (size_t j = 1; j < baselineData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j + compareData.legends.size() - 1], baselineData.lines[i][j]);
        }
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryViewHandlerGetCompareGraphLinesBaselineEmptyTest) {
    MemoryViewData compareData{"",
        {"Time (ms)", "Operators Allocated", "Operators Activated", "Operators Reserved", "App Reserved"},
        {{"10.032", "25.37", "13.22", "101.29", "1000.28"}, {"21.389", "100.88", "94.63", "150.32", "2000.01"}}};
    MemoryViewData baselineData{
        "", {"Time (ms)", "Operators Allocated", "Operators Activated", "Operators Reserved", "App Reserved"}, {}};
    MemoryViewData resultData;
    QueryMemoryViewHandler handler;
    handler.GetCompareGraphLines(compareData, baselineData, resultData);
    ASSERT_EQ(resultData.tempData.size(), compareData.tempData.size());
    for (size_t i = 0; i < resultData.lines.size(); ++i) {
        ASSERT_EQ(resultData.lines[i].size(), compareData.legends.size() + baselineData.legends.size() - 1);
        EXPECT_EQ(resultData.lines[i][0], compareData.lines[i][0]);
        for (size_t j = 1; j < compareData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j], compareData.lines[i][j]);
        }
        for (size_t j = 1; j < baselineData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j + compareData.legends.size() - 1], "NULL");
        }
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryViewHandlerGetCompareGraphLinesBothNotEmptyTest) {
    MemoryViewData compareData{"",
        {"Time (ms)", "Operators Allocated", "Operators Activated", "Operators Reserved", "App Reserved"},
        {{"10.032", "25.37", "13.22", "101.29", "1000.28"}, {"21.389", "100.88", "94.63", "150.32", "2000.01"}}};
    MemoryViewData baselineData{"",
        {"Time (ms)", "Operators Allocated", "Operators Activated", "Operators Reserved", "App Reserved"},
        {{"10.032", "25.37", "13.22", "101.29", "1000.28"}, {"21.389", "100.88", "94.63", "150.32", "2000.01"}}};
    MemoryViewData resultData;
    QueryMemoryViewHandler handler;
    handler.GetCompareGraphLines(compareData, baselineData, resultData);
    ASSERT_EQ(resultData.tempData.size(), compareData.tempData.size());
    ASSERT_EQ(resultData.tempData.size(), baselineData.tempData.size());
    for (size_t i = 0; i < resultData.lines.size(); ++i) {
        ASSERT_EQ(resultData.lines[i].size(), compareData.legends.size() + baselineData.legends.size() - 1);
        EXPECT_EQ(resultData.lines[i][0], compareData.lines[i][0]);
        for (size_t j = 1; j < compareData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j], compareData.lines[i][j]);
        }
        for (size_t j = 1; j < baselineData.legends.size(); ++j) {
            EXPECT_EQ(resultData.lines[i][j + compareData.legends.size() - 1], baselineData.lines[i][j]);
        }
    }
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorSizeHandlerNormalTest) {
    Dic::Module::Memory::QueryMemoryOperatorSizeHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryOperatorSizeRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryOperatorSizeRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.type = "Overall";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorSizeHandlerInvalidParamTest) {
    Dic::Module::Memory::QueryMemoryOperatorSizeHandler handler;
    std::unique_ptr<Dic::Protocol::MemoryOperatorSizeRequest> requestPtr =
        std::make_unique<Dic::Protocol::MemoryOperatorSizeRequest>();
    requestPtr.get()->params.rankId = "0";
    requestPtr.get()->params.type = "";
    ASSERT_NO_THROW(handler.HandleRequest(std::move(requestPtr)));
}

TEST_F(MemoryRequestHandlerTest, QueryMemoryOperatorSizeHandlerExecuteComparisonAlgorithmTest) {
    Protocol::OperatorSize compareData{120.14, 259.99};
    Protocol::OperatorSize baselineData{12.30, 558.97};
    Protocol::MemoryOperatorSizeResponse response;
    Dic::Module::Memory::QueryMemoryOperatorSizeHandler handler;
    handler.ExecuteComparisonAlgorithm(compareData, baselineData, response);
    EXPECT_EQ(response.size.minSize, compareData.minSize - baselineData.maxSize);
    EXPECT_EQ(response.size.maxSize, compareData.maxSize - baselineData.minSize);
}
