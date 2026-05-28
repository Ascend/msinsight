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
#include "MemoryProtocolRequest.h"
#include "DataBaseManager.h"
#include "DbMemoryDataBase.h"
#include "ParamsParser.h"
#include "FileUtil.h"
#include "TraceTime.h"
#include "MemoryTestUtil.h"
#include "OperatorMemoryService.h"
#include "NumberUtil.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;
using namespace Dic::Module::Memory;
using namespace Dic;

class DbMemoryDatabaseTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        std::string dbPath = TestSuit::GetTestDataFile("full_db", "ascend_pytorch_profiler.db");
        DataBaseManager::Instance().SetDataType(DataType::DB, dbPath);
        DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbPath);
        auto memoryDatabase = std::dynamic_pointer_cast<DbMemoryDataBase, Dic::Module::Memory::VirtualMemoryDataBase>(
            DataBaseManager::Instance().CreateMemoryDataBase("0", dbPath));
        memoryDatabase->OpenDb(dbPath, false);
        // minTime = 1734230739709945000, maxTime = 1734230739709945000
        TraceTime::Instance().Reset();
        TraceTime::Instance().UpdateTime(1734230739709945000, 1734230739709945000);
    }

    static void TearDownTestSuite() {
        auto memoryDatabase = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
        memoryDatabase->CloseDb();
        DataBaseManager::Instance().Clear();
    }
};

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryComponentData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryComponentParams requestParams;
    requestParams.rankId = "0";
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    std::vector<Protocol::MemoryTableColumnAttr> columnAttr;
    std::vector<Protocol::MemoryComponent> responseBody;
    auto result = database->QueryComponentDetail(requestParams, columnAttr, responseBody);
    EXPECT_TRUE(result);
    int expectSize = 0;
    int expectColumnSize = 3;
    EXPECT_EQ(responseBody.size(), expectSize);
    EXPECT_EQ(columnAttr.size(), expectColumnSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryEntireComponentTable) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileId("0");
    Dic::Protocol::MemoryComponentParams requestParams;
    requestParams.deviceId = "0";
    std::vector<Dic::Protocol::MemoryComponent> responseBody;
    bool result = database->QueryEntireComponentTable(requestParams, responseBody, offsetTime);
    int expectSize = 0;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryOperatorData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 10; // page size = 10
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    int64_t result = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 10;
    EXPECT_EQ(result, 359);
    EXPECT_EQ(responseBody.size(), expectSize);
    // 新增db多机多卡场景 rankId不等于deviceId的情况(rankId一般为"{host} {rankId}")
    requestParams.rankId = "host_0 0";
    responseBody.clear();
    result = database->QueryOperatorDetail(requestParams, responseBody);
    EXPECT_EQ(result, 359);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryOperatorWithTime) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startTime = 0;
    requestParams.endTime = 1695120000000; // end time = 1695120000000
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    int64_t result = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 100;
    EXPECT_EQ(result, 359);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryOperatorWithLimitedTime) {
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileId("0");
    const uint64_t timeStamp = 1734230739778225840;
    const double secondToMillisecond = 1000.0;
    const int precision = 3;
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startTime = NumberUtil::DoubleReservedNDigits(
        (timeStamp - startTime - offsetTime) / (secondToMillisecond * secondToMillisecond), precision);
    requestParams.endTime = NumberUtil::DoubleReservedNDigits(
        (timeStamp - startTime - offsetTime) / (secondToMillisecond * secondToMillisecond), precision);
    std::vector<Protocol::MemoryTableColumnAttr> columnAttr;
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    int64_t result = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 6;
    EXPECT_EQ(result, expectSize);
    EXPECT_EQ(responseBody.size(), expectSize);
}

// 测试框选了开始时间、结束时间，且只显示在选中时间区间内分配、释放内存的数据查询是否正确
TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryOperatorWithLimitedTimeOnlyShowWithin) {
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileId("0");
    const uint64_t timeStamp = 1734230739778225840;
    const double secondToMillisecond = 1000.0;
    const int precision = 3;
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startTime = NumberUtil::DoubleReservedNDigits(
        (timeStamp - startTime - offsetTime) / (secondToMillisecond * secondToMillisecond), precision);
    requestParams.endTime = NumberUtil::DoubleReservedNDigits(
        (timeStamp - startTime - offsetTime) / (secondToMillisecond * secondToMillisecond), precision);
    requestParams.isOnlyShowAllocatedOrReleasedWithinInterval = true;
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    auto result = database->QueryOperatorDetail(requestParams, responseBody);
    EXPECT_EQ(result, true);
    int expectSize = 1;
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryOperatorWithSize) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 10; // page size = 10
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    requestParams.rangeFilters[std::string(OpMemoryColumn::SIZE)] = {10, 64};
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    int64_t result = database->QueryOperatorDetail(requestParams, responseBody);
    EXPECT_EQ(result, 118);
    int expectSize = 10;
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryOperatorByStreamExceptZero) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startTime = 0;
    requestParams.endTime = -1;
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    int64_t result = database->QueryOperatorDetail(requestParams, responseBody);
    EXPECT_EQ(result, 277);
    int expectSize = 100;
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryOperatorByStreamExceptSeveral) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    auto result = database->QueryOperatorDetail(requestParams, responseBody);
    EXPECT_EQ(result, 277);
    int expectSize = 100;
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryComponentsTotalNum) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryComponentParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    int64_t totalNum;
    auto result = database->QueryComponentsTotalNum(requestParams, totalNum);
    EXPECT_TRUE(result);
    int expectSize = 0;
    EXPECT_EQ(totalNum, expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorsTotalNum) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 359;
    EXPECT_EQ(totalNum, expectSize);
    EXPECT_EQ(responseBody.size(), min(database->defaultPageSize, expectSize));
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorsTotalNumWithSize) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.filters[std::string(OpMemoryColumn::NAME)] = "aten::empty_strided";
    requestParams.rangeFilters[std::string(OpMemoryColumn::SIZE)] = {0, 600000000};
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 18;
    EXPECT_EQ(totalNum, expectSize);
    EXPECT_EQ(operators.size(), min(database->defaultPageSize, expectSize));
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorsTotalNumWithTime) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.filters[std::string(OpMemoryColumn::NAME)] = "aten::empty_strided";
    requestParams.startTime = 0;
    requestParams.endTime = 1695120000000; // end time = 1695120000000
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 18;
    EXPECT_EQ(totalNum, expectSize);
    EXPECT_EQ(operators.size(), min(database->defaultPageSize, expectSize));
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorsTotalNumWithLimitedTime) {
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileId("0");
    const uint64_t timeStamp = 1734230739778225840;
    const double secondToMillisecond = 1000.0;
    const int precision = 3;
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.startTime = NumberUtil::DoubleReservedNDigits(
        (timeStamp - startTime - offsetTime) / (secondToMillisecond * secondToMillisecond), precision);
    requestParams.endTime = NumberUtil::DoubleReservedNDigits(
        (timeStamp - startTime - offsetTime) / (secondToMillisecond * secondToMillisecond), precision);
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 6;
    EXPECT_EQ(totalNum, expectSize);
    EXPECT_EQ(operators.size(), min(database->defaultPageSize, expectSize));
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorsTotalNumByStreamExpectZero) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.deviceId = "0";
    requestParams.filters[std::string(OpMemoryColumn::NAME)] = "aten::empty_stridedss";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 0;
    EXPECT_EQ(totalNum, expectSize);
    EXPECT_TRUE(operators.empty());
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorsTotalNumByStreamExpectSeveral) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    requestParams.startTime = -1;
    requestParams.endTime = -1; // end time = 1695120000000
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 277;
    EXPECT_EQ(totalNum, expectSize);
    EXPECT_EQ(operators.size(), min(database->defaultPageSize, expectSize));
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryEntireOperatorTable) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.deviceId = "0";
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileId("0");
    std::vector<Dic::Protocol::MemoryOperator> opDetails;
    bool result = database->QueryEntireOperatorTable(requestParams, opDetails, offsetTime);
    int expectSize = 359;
    EXPECT_TRUE(result);
    EXPECT_EQ(opDetails.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryResourceTypeData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    std::string type;
    bool result = database->QueryMemoryResourceType(type);
    std::string expectType = "Pytorch";
    EXPECT_TRUE(result);
    EXPECT_EQ(type, expectType);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryStaticOperatorGraph) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::StaticOperatorGraphParams requestParams;
    requestParams.rankId = "0";
    requestParams.graphId = "0";
    Dic::Protocol::StaticOperatorGraphItem graphItem;
    bool result = database->QueryStaticOperatorGraph(requestParams, graphItem);
    int expectLegendsSize = 0;
    int expectLinesSize = 0;
    EXPECT_FALSE(result);
    EXPECT_EQ(graphItem.legends.size(), expectLegendsSize);
    EXPECT_EQ(graphItem.lines.size(), expectLinesSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryStaticOperatorList) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "0";
    requestParams.graphId = "0";
    requestParams.currentPage = 0;
    requestParams.pageSize = 0;
    requestParams.startNodeIndex = -1;
    requestParams.endNodeIndex = -1;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Protocol::MemoryTableColumnAttr> columnAttr;
    std::vector<Protocol::StaticOperatorItem> responseBody;
    int64_t result = database->QueryStaticOperatorList(requestParams, responseBody);
    int expectSize = 0;
    int expectColumnSize = 0;
    EXPECT_EQ(result, -1);
    EXPECT_EQ(columnAttr.size(), expectColumnSize);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryEntireStaticOperatorTable) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "0";
    requestParams.graphId = "0";
    requestParams.currentPage = 0;
    requestParams.pageSize = 0;
    requestParams.startNodeIndex = -1;
    requestParams.endNodeIndex = -1;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Protocol::StaticOperatorItem> responseBody;
    bool result = database->QueryEntireStaticOperatorTable(requestParams, responseBody);
    int expectSize = 0;
    EXPECT_FALSE(result);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryStaticOperatorSize) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::StaticOperatorSizeParams requestParams;
    requestParams.rankId = "0";
    requestParams.graphId = "";
    double min;
    double max;
    bool result = database->QueryStaticOperatorSize(requestParams, min, max);
    EXPECT_FALSE(result);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryTypeDataDynamic) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    std::string type;
    std::vector<std::string> graphId;
    bool result = database->QueryMemoryType(type, graphId);
    int expectSize = 0;
    EXPECT_TRUE(result);
    EXPECT_EQ(type, Module::Memory::MEMORY_TYPE_DYNAMIC);
    EXPECT_EQ(graphId.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryViewData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    Dic::Protocol::MemoryViewData responseBody;
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileId("0");
    auto result = database->QueryMemoryView(requestParams, responseBody, offsetTime);
    EXPECT_EQ(result, true);
    const int expectSize = 8530 * 5;
    EXPECT_EQ(responseBody.tempData.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryViewDataByStreamExpectSeveral) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    Dic::Protocol::MemoryViewData responseBody;
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileId("0");
    auto result = database->QueryMemoryView(requestParams, responseBody, offsetTime);
    EXPECT_EQ(result, true);
    const int expectSize = 935 * 4;
    EXPECT_EQ(responseBody.tempData.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryMemoryViewDataByComponentExpectSeveral) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_COMPONENT_GROUP;
    Dic::Protocol::MemoryViewData responseBody;
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileId("0");
    auto result = database->QueryMemoryView(requestParams, responseBody, offsetTime);
    EXPECT_EQ(result, true);
    const int expectSize = 68240;
    EXPECT_EQ(responseBody.tempData.size(), expectSize);
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorSizeData) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorSizeParams requestParams;
    requestParams.deviceId = "0";
    double min;
    double max;
    auto result = database->QueryOperatorSize(requestParams, min, max);
    EXPECT_EQ(result, true);
    EXPECT_EQ(min, 0.5); // minSize = 0.5
    EXPECT_EQ(max, 32896); // maxSize = 32896
}

/***
 * 组合筛选/排序/范围筛选测试
 */
TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorWithFilterNameAndOrderBy) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams params;
    params.deviceId = "0";
    params.startTime = -1;
    params.endTime = -1;
    // 测试过滤name并按照释放时间降序排列
    std::string expectFilterName = "aten";
    params.filters[std::string(OpMemoryColumn::NAME)] = expectFilterName;
    params.orderBy = OpMemoryColumn::RELEASE_TIME;
    params.currentPage = 1;
    params.pageSize = 10;
    params.desc = true;
    std::vector<MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(params, operators);
    EXPECT_GT(totalNum, 0);
    EXPECT_EQ(operators.size(), min(totalNum, params.pageSize));
    EXPECT_TRUE(OperatorMemoryTestUtil::IsOperatorsSorted(operators, params.orderBy, params.desc));
    EXPECT_TRUE(OperatorMemoryTestUtil::IsOperatorsNameAllContains(operators, expectFilterName));
    // 测试过滤name并按照name排序
    operators.clear();
    params.orderBy = OpMemoryColumn::NAME;
    totalNum = database->QueryOperatorDetail(params, operators);
    EXPECT_GT(totalNum, 0);
    EXPECT_EQ(operators.size(), min(totalNum, params.pageSize));
    EXPECT_TRUE(OperatorMemoryTestUtil::IsOperatorsSorted(operators, params.orderBy, params.desc));
    EXPECT_TRUE(OperatorMemoryTestUtil::IsOperatorsNameAllContains(operators, expectFilterName));
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorWithFilterNameAndOrderByAndRangeFilter) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams params;
    params.deviceId = "0";
    params.startTime = -1;
    params.endTime = -1;
    params.currentPage = 1;
    params.pageSize = 10;
    double minTimeMs = 0;
    double maxTimeMs = 100000;
    double minSize = -1000;
    double maxSize = 100000;
    std::string expectFilterName = "a";
    params.filters[std::string(OpMemoryColumn::NAME)] = expectFilterName;
    params.rangeFilters[std::string(OpMemoryColumn::ALLOCATION_TIME)] = {minTimeMs, maxTimeMs};
    params.rangeFilters[std::string(OpMemoryColumn::SIZE)] = {minSize, maxSize};
    params.orderBy = OpMemoryColumn::ALLOCATION_TIME;
    std::vector<MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(params, operators);
    EXPECT_GT(totalNum, 0);
    EXPECT_EQ(operators.size(), min(totalNum, params.pageSize));
    for (auto op : operators) {
        EXPECT_TRUE(op.size >= minSize && op.size <= maxSize);
        double allocationTime = NumberUtil::StringToDouble(op.allocationTime);
        EXPECT_TRUE(allocationTime >= minTimeMs && allocationTime <= maxTimeMs);
    }
    EXPECT_TRUE(OperatorMemoryTestUtil::IsOperatorsSorted(operators, params.orderBy, params.desc));
    EXPECT_TRUE(OperatorMemoryTestUtil::IsOperatorsNameAllContains(operators, expectFilterName));
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorWithFullCondition) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams params;
    params.deviceId = "0";
    params.startTime = 100;
    params.endTime = 10000;
    params.currentPage = 1;
    params.pageSize = 10;
    double minTimeMs = 0;
    double maxTimeMs = 100000;
    double minSize = -1000;
    double maxSize = 100000;
    std::string expectFilterName = "a";
    params.filters[std::string(OpMemoryColumn::NAME)] = expectFilterName;
    params.rangeFilters[std::string(OpMemoryColumn::ALLOCATION_TIME)] = {minTimeMs, maxTimeMs};
    params.rangeFilters[std::string(OpMemoryColumn::SIZE)] = {minSize, maxSize};
    params.orderBy = OpMemoryColumn::ALLOCATION_TIME;
    std::vector<MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(params, operators);
    EXPECT_GT(totalNum, 0);
    EXPECT_EQ(operators.size(), min(totalNum, params.pageSize));
    for (auto op : operators) {
        EXPECT_TRUE(op.size >= minSize && op.size <= maxSize);
        double allocationTime = NumberUtil::StringToDouble(op.allocationTime);
        double releaseTime = NumberUtil::StringToDouble(op.releaseTime);
        if (releaseTime <= 0) {
            releaseTime = std::numeric_limits<double>::max();
        }
        EXPECT_TRUE(allocationTime >= minTimeMs && allocationTime <= maxTimeMs);
        EXPECT_TRUE(allocationTime <= params.endTime && releaseTime >= params.startTime);
    }
    EXPECT_TRUE(OperatorMemoryTestUtil::IsOperatorsSorted(operators, params.orderBy, params.desc));
    EXPECT_TRUE(OperatorMemoryTestUtil::IsOperatorsNameAllContains(operators, expectFilterName));
}

TEST_F(DbMemoryDatabaseTest, FullDbQueryOperatorWithFullConditionAndOnlyShowInterval) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams params;
    params.deviceId = "0";
    params.startTime = 100;
    params.endTime = 10000;
    params.currentPage = 1;
    params.pageSize = 10;
    double minTimeMs = 0;
    double maxTimeMs = 100000;
    double minSize = -1000;
    double maxSize = 100000;
    std::string expectFilterName = "a";
    params.filters[std::string(OpMemoryColumn::NAME)] = expectFilterName;
    params.rangeFilters[std::string(OpMemoryColumn::ALLOCATION_TIME)] = {minTimeMs, maxTimeMs};
    params.rangeFilters[std::string(OpMemoryColumn::SIZE)] = {minSize, maxSize};
    params.orderBy = OpMemoryColumn::ALLOCATION_TIME;
    params.isOnlyShowAllocatedOrReleasedWithinInterval = true;
    std::vector<MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(params, operators);
    EXPECT_GT(totalNum, 0);
    EXPECT_EQ(operators.size(), min(totalNum, params.pageSize));
    for (auto op : operators) {
        EXPECT_TRUE(op.size >= minSize && op.size <= maxSize);
        double allocationTime = NumberUtil::StringToDouble(op.allocationTime);
        double releaseTime = NumberUtil::StringToDouble(op.releaseTime);
        if (releaseTime <= 0) {
            releaseTime = std::numeric_limits<double>::max();
        }
        EXPECT_TRUE(allocationTime >= minTimeMs && allocationTime <= maxTimeMs);
        bool allocIn = allocationTime >= params.startTime && allocationTime <= params.endTime;
        bool releaseIn = releaseTime >= params.startTime && releaseTime <= params.endTime;
        EXPECT_TRUE(allocIn || releaseIn);
    }
    EXPECT_TRUE(OperatorMemoryTestUtil::IsOperatorsSorted(operators, params.orderBy, params.desc));
    EXPECT_TRUE(OperatorMemoryTestUtil::IsOperatorsNameAllContains(operators, expectFilterName));
}
