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
#include <filesystem>
#include "MemoryParse.h"
#include "OperatorTable.h"
#include "BaselineManager.h"
#include "QueryMemoryComponentHandler.h"
#include "QueryMemoryOperatorHandler.h"
#include "QueryMemoryOperatorSizeHandler.h"
#include "QueryMemoryViewHandler.h"
#include "QueryMemoryStaticOperatorGraphHandler.h"
#include "QueryMemoryStaticOperatorListHandler.h"
#include "QueryMemoryStaticOperatorSizeHandler.h"
#include "TestSuit.h"
#include "MemoryProtocolRequest.h"
#include "DataBaseManager.h"
#include "TraceTime.h"
#include "MemoryTestUtil.h"
#include "../../../TestSuit.h"

using namespace Dic::Module::Timeline;

class TextMemoryDatabaseTest : public ::testing::Test {
  protected:
    static void SetUpTestSuite() {
        using namespace Dic::Module::Memory;
        TraceTime::Instance().Reset();
        TraceTime::Instance().UpdateTime(1695115378000000000ULL, 1695115379000000000ULL);
        for (const std::string rank : {"0", "1"}) {
            const auto source =
                std::filesystem::path(TestSuit::GetTestDataFile("test_rank_" + rank, "ASCEND_PROFILER_OUTPUT"));
            const auto dbPath = std::filesystem::path(::testing::TempDir()) / ("text_memory_coverage_" + rank + ".db");
            std::filesystem::remove(dbPath);
            DataBaseManager::Instance().SetDataType(DataType::TEXT, dbPath.string());
            DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbPath.string());
            auto database = DataBaseManager::Instance().CreateMemoryDataBase(rank, dbPath.string());
            ASSERT_TRUE(database->OpenDb(dbPath.string(), false));
            auto textDatabase = std::dynamic_pointer_cast<TextMemoryDataBase>(database);
            ASSERT_NE(textDatabase, nullptr);
            ASSERT_TRUE(textDatabase->CreateTable());
            DataBaseManager::Instance().UpdateRankIdToDeviceId(dbPath.string(), rank, rank);
            ParserStatusManager::Instance().SetParserStatus(MEMORY_PREFIX + rank, ParserStatus::RUNNING);
            ASSERT_TRUE(MemoryParse::Instance().OperatorParse((source / "operator_memory.csv").string(), rank));
            ASSERT_TRUE(MemoryParse::Instance().RecordToParse((source / "memory_record.csv").string(), rank));
            if (std::filesystem::exists(source / "static_op_mem.csv")) {
                ASSERT_TRUE(MemoryParse::Instance().StaticOpParse((source / "static_op_mem.csv").string(), rank));
            }
            if (std::filesystem::exists(source / "npu_module_mem.csv")) {
                ASSERT_TRUE(MemoryParse::Instance().ComponentParse((source / "npu_module_mem.csv").string(), rank));
            }
        }
    }

    static void TearDownTestSuite() {
        for (const std::string rank : {"0", "1"}) {
            auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId(rank);
            if (database != nullptr) {
                database->CloseDb();
            }
        }
        DataBaseManager::Instance().Clear(DatabaseType::MEMORY);
        ParserStatusManager::Instance().ClearAllParserStatus();
        for (const std::string rank : {"0", "1"}) {
            std::error_code error;
            std::filesystem::remove(
                std::filesystem::path(::testing::TempDir()) / ("text_memory_coverage_" + rank + ".db"), error);
        }
    }
};

TEST_F(TextMemoryDatabaseTest, ComparisonHandlersQueryBothOpenDatabases) {
    using namespace Dic::Module::Memory;
    using namespace Dic::Protocol;
    for (const std::string rank : {"0", "1"}) {
        auto db =
            std::dynamic_pointer_cast<TextMemoryDataBase>(DataBaseManager::Instance().GetMemoryDatabaseByRankId(rank));
        ASSERT_NE(db, nullptr);
        db->InsertStaticOpDetail({rank, "TOTAL", "model", "coverage_graph", 0, 10, 96});
        db->InsertStaticOpDetail({rank, "SharedOp", "model", "coverage_graph", 1, 5, 64});
        db->InsertStaticOpDetail({rank, "UniqueOp" + rank, "model", "coverage_graph", 6, 9, 32});
        db->SaveStaticOpDetail();
    }
    Dic::Module::Global::BaselineInfo baseline;
    baseline.rankId = "0";
    ParserStatusManager::Instance().SetParserStatus("0", ParserStatus::FINISH);
    ParserStatusManager::Instance().SetParserStatus(KERNEL_PREFIX + std::string("0"), ParserStatus::FINISH);
    ParserStatusManager::Instance().SetParserStatus(MEMORY_PREFIX + std::string("0"), ParserStatus::FINISH);
    Dic::Module::Global::BaselineManager::Instance().SetBaselineInfo(baseline);

    auto component = std::make_unique<MemoryComponentRequest>();
    component->params.rankId = "1";
    component->params.isCompare = true;
    component->params.currentPage = 1;
    component->params.pageSize = 10;
    EXPECT_TRUE(QueryMemoryComponentHandler().HandleRequest(std::move(component)));

    auto operators = std::make_unique<MemoryOperatorRequest>();
    operators->params.rankId = "1";
    operators->params.type = MEMORY_OVERALL_GROUP;
    operators->params.isCompare = true;
    operators->params.currentPage = 1;
    operators->params.pageSize = 10;
    EXPECT_TRUE(QueryMemoryOperatorHandler().HandleRequest(std::move(operators)));

    auto size = std::make_unique<MemoryOperatorSizeRequest>();
    size->params.rankId = "1";
    size->params.type = MEMORY_OVERALL_GROUP;
    size->params.isCompare = true;
    EXPECT_TRUE(QueryMemoryOperatorSizeHandler().HandleRequest(std::move(size)));

    auto view = std::make_unique<MemoryViewRequest>();
    view->params.rankId = "1";
    view->params.type = MEMORY_OVERALL_GROUP;
    view->params.isCompare = true;
    EXPECT_TRUE(QueryMemoryViewHandler().HandleRequest(std::move(view)));

    auto staticSize = std::make_unique<MemoryStaticOperatorSizeRequest>();
    staticSize->params.rankId = "1";
    staticSize->params.graphId = "coverage_graph";
    staticSize->params.isCompare = true;
    EXPECT_TRUE(QueryMemoryStaticOperatorSizeHandler().HandleRequest(std::move(staticSize)));

    auto staticList = std::make_unique<MemoryStaticOperatorListRequest>();
    staticList->params.rankId = "1";
    staticList->params.graphId = "coverage_graph";
    staticList->params.endNodeIndex = 10;
    staticList->params.minSize = std::numeric_limits<int64_t>::min();
    staticList->params.maxSize = std::numeric_limits<int64_t>::max();
    staticList->params.isCompare = true;
    staticList->params.currentPage = 1;
    staticList->params.pageSize = 10;
    EXPECT_TRUE(QueryMemoryStaticOperatorListHandler().HandleRequest(std::move(staticList)));
    auto graph = std::make_unique<MemoryStaticOperatorGraphRequest>();
    graph->params.rankId = "1";
    graph->params.graphId = "coverage_graph";
    graph->params.isCompare = true;
    EXPECT_TRUE(QueryMemoryStaticOperatorGraphHandler().HandleRequest(std::move(graph)));
    Dic::Module::Global::BaselineManager::Instance().Reset();
    for (const std::string rank : {"0", "1"}) {
        auto db = DataBaseManager::Instance().GetMemoryDatabaseByRankId(rank);
        EXPECT_TRUE(db->ExecSql("DELETE FROM static_op WHERE graphId = 'coverage_graph'"));
    }
}

TEST_F(TextMemoryDatabaseTest, TableQueriesBindAllSupportedTypesAndClearConditions) {
    using namespace Dic::Module::Memory;
    OperatorTable table;
    table.Select(OpMemoryColumn::SIZE);
    table.GreaterEq(OpMemoryColumn::SIZE, uint32_t{0});
    table.GreaterEq(OpMemoryColumn::ALLOCATION_TIME, uint64_t{0});
    table.NotEq(OpMemoryColumn::NAME, std::string("missing-name"));
    const auto records = table.ExcuteQuery("0");
    ASSERT_FALSE(records.empty());
    for (const auto &record : records) {
        EXPECT_GT(record.size, 0U);
    }
    table.GreaterEq(OpMemoryColumn::SIZE, uint32_t{0});
    table.GreaterEq(OpMemoryColumn::ALLOCATION_TIME, uint64_t{0});
    table.NotEq(OpMemoryColumn::NAME, std::string("missing-name"));
    EXPECT_EQ(table.Count("0"), records.size());
    EXPECT_EQ(table.Count("0"), records.size());
    table.Select("missing_column");
    EXPECT_TRUE(table.ExcuteQuery("0").empty());
    table.Eq("missing_column", uint32_t{0});
    EXPECT_EQ(table.Count("0"), 0U);
    table.Select(OpMemoryColumn::SIZE);
    EXPECT_TRUE(table.ExcuteQuery("missing-rank").empty());
    EXPECT_EQ(table.Count("missing-rank"), 0U);
}

TEST_F(TextMemoryDatabaseTest, ComparisonHandlersRejectMissingBaseline) {
    using namespace Dic::Module::Memory;
    using namespace Dic::Protocol;
    Dic::Module::Global::BaselineManager::Instance().Reset();
    auto component = std::make_unique<MemoryComponentRequest>();
    component->params.rankId = "1";
    component->params.isCompare = true;
    component->params.currentPage = 1;
    component->params.pageSize = 10;
    EXPECT_FALSE(QueryMemoryComponentHandler().HandleRequest(std::move(component)));
    auto operators = std::make_unique<MemoryOperatorRequest>();
    operators->params.rankId = "1";
    operators->params.type = MEMORY_OVERALL_GROUP;
    operators->params.isCompare = true;
    operators->params.currentPage = 1;
    operators->params.pageSize = 10;
    EXPECT_FALSE(QueryMemoryOperatorHandler().HandleRequest(std::move(operators)));
    auto size = std::make_unique<MemoryOperatorSizeRequest>();
    size->params.rankId = "1";
    size->params.type = MEMORY_OVERALL_GROUP;
    size->params.isCompare = true;
    EXPECT_FALSE(QueryMemoryOperatorSizeHandler().HandleRequest(std::move(size)));
    auto view = std::make_unique<MemoryViewRequest>();
    view->params.rankId = "1";
    view->params.type = MEMORY_COMPONENT_GROUP;
    view->params.isCompare = true;
    EXPECT_FALSE(QueryMemoryViewHandler().HandleRequest(std::move(view)));
    auto staticSize = std::make_unique<MemoryStaticOperatorSizeRequest>();
    staticSize->params.rankId = "1";
    staticSize->params.isCompare = true;
    EXPECT_FALSE(QueryMemoryStaticOperatorSizeHandler().HandleRequest(std::move(staticSize)));
    auto staticList = std::make_unique<MemoryStaticOperatorListRequest>();
    staticList->params.rankId = "1";
    staticList->params.isCompare = true;
    staticList->params.currentPage = 1;
    staticList->params.pageSize = 10;
    EXPECT_FALSE(QueryMemoryStaticOperatorListHandler().HandleRequest(std::move(staticList)));
    auto graph = std::make_unique<MemoryStaticOperatorGraphRequest>();
    graph->params.rankId = "1";
    graph->params.isCompare = true;
    EXPECT_FALSE(QueryMemoryStaticOperatorGraphHandler().HandleRequest(std::move(graph)));
}

TEST_F(TextMemoryDatabaseTest, QueriesRejectMissingTablesWithoutAppendingRows) {
    using namespace Dic::Module::Memory;
    using namespace Dic::Protocol;
    std::recursive_mutex mutex;
    TextMemoryDataBase database(mutex);
    const auto dbPath = std::filesystem::path(::testing::TempDir()) / "memory_missing_tables_coverage.db";
    std::filesystem::remove(dbPath);
    ASSERT_TRUE(database.OpenDb(dbPath.string(), false));
    MemoryOperatorParams operatorParams;
    operatorParams.rankId = "0";
    operatorParams.deviceId = "0";
    operatorParams.currentPage = 1;
    operatorParams.pageSize = 10;
    std::vector<MemoryOperator> operators;
    EXPECT_EQ(database.QueryOperatorDetail(operatorParams, operators), -1);
    EXPECT_FALSE(database.QueryEntireOperatorTable(operatorParams, operators, 0));
    MemoryComponentParams componentParams;
    componentParams.rankId = "0";
    componentParams.deviceId = "0";
    componentParams.currentPage = 1;
    componentParams.pageSize = 10;
    std::vector<MemoryTableColumnAttr> columns;
    std::vector<MemoryComponent> components;
    EXPECT_FALSE(database.QueryComponentDetail(componentParams, columns, components));
    EXPECT_FALSE(database.QueryEntireComponentTable(componentParams, components, 0));
    int64_t count = 0;
    EXPECT_FALSE(database.QueryComponentsTotalNum(componentParams, count));
    MemoryOperatorSizeParams sizeParams;
    double minSize = 0;
    double maxSize = 0;
    EXPECT_FALSE(database.QueryOperatorSize(sizeParams, minSize, maxSize));
    StaticOperatorSizeParams staticSizeParams;
    EXPECT_FALSE(database.QueryStaticOperatorSize(staticSizeParams, minSize, maxSize));
    StaticOperatorListParams listParams;
    listParams.currentPage = 1;
    listParams.pageSize = 10;
    std::vector<StaticOperatorItem> staticOperators;
    EXPECT_EQ(database.QueryStaticOperatorList(listParams, staticOperators), -1);
    EXPECT_FALSE(database.QueryEntireStaticOperatorTable(listParams, staticOperators));
    StaticOperatorGraphParams graphParams;
    StaticOperatorGraphItem graph;
    EXPECT_FALSE(database.QueryStaticOperatorGraph(graphParams, graph));
    MemoryViewParams viewParams;
    viewParams.type = MEMORY_OVERALL_GROUP;
    MemoryViewData view;
    EXPECT_FALSE(database.QueryMemoryView(viewParams, view, 0));
    std::string resourceType;
    EXPECT_FALSE(database.QueryMemoryResourceType(resourceType));
    EXPECT_TRUE(operators.empty());
    EXPECT_TRUE(components.empty());
    EXPECT_TRUE(staticOperators.empty());
    database.CloseDb();
    std::filesystem::remove(dbPath);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryComponentDataExpectSeveral) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryComponentParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.currentPage = 1;
    requestParams.pageSize = 10; // page size = 10
    requestParams.order = "ascend";
    requestParams.orderBy = "component";
    std::vector<Protocol::MemoryTableColumnAttr> columnAttr;
    std::vector<Protocol::MemoryComponent> responseBody;
    bool result = database->QueryComponentDetail(requestParams, columnAttr, responseBody);
    int expectSize = 2;
    int expectColumnSize = 3;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.size(), expectSize);
    EXPECT_EQ(columnAttr.size(), expectColumnSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryComponentDataExpectZero) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryComponentParams requestParams;
    requestParams.rankId = "0";
    requestParams.currentPage = 2; // current page = 2
    requestParams.pageSize = 10; // page size = 10
    requestParams.order = "ascend";
    requestParams.orderBy = "component";
    std::vector<Protocol::MemoryTableColumnAttr> columnAttr;
    std::vector<Protocol::MemoryComponent> responseBody;
    bool result = database->QueryComponentDetail(requestParams, columnAttr, responseBody);
    int expectSize = 0;
    int expectColumnSize = 3;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.size(), expectSize);
    EXPECT_EQ(columnAttr.size(), expectColumnSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryEntireComponentTable) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp("0");
    Dic::Protocol::MemoryComponentParams requestParams;
    requestParams.deviceId = "0";
    std::vector<Protocol::MemoryComponent> responseBody;
    bool result = database->QueryEntireComponentTable(requestParams, responseBody, offsetTime);
    int expectSize = 2;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryOperatorData) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 10; // page size = 10
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    bool result = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 10;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryOperatorWithTime) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startTime = 0;
    requestParams.endTime = 1695120000000; // end time = 1695120000000
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    int64_t result = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 28;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryOperatorWithLimitedTime) {
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp("0");
    const uint64_t timeStamp = 1695115378729750000;
    const double secondToMillisecond = 1000.0;
    const int precision = 3;
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
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
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    int64_t result = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 1;
    EXPECT_EQ(responseBody.size(), min(result, requestParams.pageSize));
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryOperatorWithLimitedTimeOnlyShowWithin) {
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp("0");
    const uint64_t timeStamp = 1695115378729750000;
    const double secondToMillisecond = 1000.0;
    const int precision = 3;
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
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
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    int64_t result = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 0;
    EXPECT_EQ(result, 0);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryEntireOperatorTable) {
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp("0");
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.deviceId = "0";
    std::vector<Protocol::MemoryOperator> opDetails;
    bool result = database->QueryEntireOperatorTable(requestParams, opDetails, offsetTime);
    int expectSize = 28;
    EXPECT_TRUE(result);
    EXPECT_EQ(opDetails.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryTypeDynamic) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    std::string type = Module::Memory::MEMORY_TYPE_STATIC;
    std::vector<std::string> graphId;
    bool result = database->QueryMemoryType(type, graphId);
    int expectSize = 0;
    EXPECT_TRUE(result);
    EXPECT_EQ(type, Module::Memory::MEMORY_TYPE_DYNAMIC);
    EXPECT_EQ(graphId.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryTypeStatic) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    std::string type = Module::Memory::MEMORY_TYPE_DYNAMIC;
    std::vector<std::string> graphId;
    bool result = database->QueryMemoryType(type, graphId);
    int expectSize = 2;
    EXPECT_TRUE(result);
    EXPECT_EQ(type, Module::Memory::MEMORY_TYPE_STATIC);
    EXPECT_EQ(graphId.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryResourceTypePytorch) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    std::string type = Module::Memory::MEMORY_RESOURCE_TYPE_MIND_SPORE;
    bool result = database->QueryMemoryResourceType(type);
    EXPECT_TRUE(result);
    EXPECT_EQ(type, Module::Memory::MEMORY_RESOURCE_TYPE_PYTORCH);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorListParamsException) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "0";
    requestParams.graphId = "0";
    requestParams.currentPage = 0;
    requestParams.startNodeIndex = -1;
    requestParams.endNodeIndex = -1;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Dic::Protocol::StaticOperatorItem> opDetails;
    int64_t result = database->QueryStaticOperatorList(requestParams, opDetails);
    int expectSize = 0;
    EXPECT_EQ(result, 0);
    EXPECT_EQ(opDetails.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorListParams) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "1";
    requestParams.graphId = "0";
    requestParams.currentPage = 3; // 选择最后一页 3/3
    requestParams.pageSize = 10;
    requestParams.startNodeIndex = -1;
    requestParams.endNodeIndex = -1;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Dic::Protocol::StaticOperatorItem> responseBody;
    int64_t result = database->QueryStaticOperatorList(requestParams, responseBody);
    EXPECT_EQ(result, (requestParams.currentPage - 1) * requestParams.pageSize + responseBody.size());
    EXPECT_EQ(responseBody.size(), 7);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorListTotal) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "1";
    requestParams.graphId = "0";
    requestParams.currentPage = 3; // 选择最后一页 3/3
    requestParams.pageSize = 10;
    requestParams.startNodeIndex = -1;
    requestParams.endNodeIndex = -1;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Dic::Protocol::StaticOperatorItem> opDetails;
    int64_t result = database->QueryStaticOperatorList(requestParams, opDetails);
    int expectNumber = 27;
    EXPECT_EQ(result, expectNumber);
    EXPECT_EQ(opDetails.size(), 7);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorListParamsWithNodeIndex) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "1";
    requestParams.graphId = "0";
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startNodeIndex = 0;
    requestParams.endNodeIndex = 10; // 结束节点索引 = 10
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Protocol::MemoryTableColumnAttr> columnAttr;
    std::vector<Dic::Protocol::StaticOperatorItem> responseBody;
    int64_t result = database->QueryStaticOperatorList(requestParams, responseBody);
    int expectSize = 16;
    EXPECT_EQ(result, expectSize);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorListParamsWithSizeFileter) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "1";
    requestParams.graphId = "0";
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startNodeIndex = -1;
    requestParams.endNodeIndex = -1;
    requestParams.minSize = 800; // 筛选 min Size = 800
    requestParams.maxSize = 3000; // 筛选 max Size = 3000
    std::vector<Protocol::MemoryTableColumnAttr> columnAttr;
    std::vector<Dic::Protocol::StaticOperatorItem> responseBody;
    int64_t result = database->QueryStaticOperatorList(requestParams, responseBody);
    int expectSize = 4;
    EXPECT_EQ(result, expectSize);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorListParamsWithAllFilter) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "1";
    requestParams.graphId = "1";
    requestParams.desc = false;
    requestParams.orderBy = StaticOpColumn::OP_NAME;
    requestParams.searchName = "re";
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startNodeIndex = -1;
    requestParams.endNodeIndex = -1;
    requestParams.minSize = 600; // 筛选 min Size = 600
    requestParams.maxSize = 3000; // 筛选 max Size = 3000
    std::vector<Protocol::MemoryTableColumnAttr> columnAttr;
    std::vector<Dic::Protocol::StaticOperatorItem> responseBody;
    int64_t result = database->QueryStaticOperatorList(requestParams, responseBody);
    int expectSize = 4;
    EXPECT_EQ(result, expectSize);
    EXPECT_EQ(responseBody.size(), expectSize);
    EXPECT_EQ(responseBody[0].opName, "reducemax_03");
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorListTotalWithAllFilter) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "1";
    requestParams.graphId = "1";
    requestParams.searchName = "re";
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startNodeIndex = -1;
    requestParams.endNodeIndex = -1;
    requestParams.minSize = 600; // 筛选 min Size = 600
    requestParams.maxSize = 3000; // 筛选 max Size = 3000
    std::vector<Dic::Protocol::StaticOperatorItem> opDetails;
    int64_t result = database->QueryStaticOperatorList(requestParams, opDetails);
    int expectSize = 4;
    EXPECT_EQ(result, expectSize);
    EXPECT_EQ(opDetails.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorListTotalWithNodeIndex) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "1";
    requestParams.graphId = "1";
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startNodeIndex = 0;
    requestParams.endNodeIndex = 10; // end Node Index = 10
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Dic::Protocol::StaticOperatorItem> opDetails;
    int64_t result = database->QueryStaticOperatorList(requestParams, opDetails);
    int expectSize = 16;
    EXPECT_EQ(result, expectSize);
    EXPECT_EQ(opDetails.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryEntireStaticOperatorTable) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorListParams requestParams;
    requestParams.rankId = "1";
    requestParams.currentPage = 0;
    requestParams.pageSize = 0;
    requestParams.startNodeIndex = -1;
    requestParams.endNodeIndex = -1;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Protocol::StaticOperatorItem> opDetails;
    bool result = database->QueryEntireStaticOperatorTable(requestParams, opDetails);
    int expectSize = 54;
    EXPECT_TRUE(result);
    EXPECT_EQ(opDetails.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorSizeDataWithoutGraphId) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorSizeParams requestParams;
    requestParams.graphId = "";
    double min;
    double max;
    bool result = database->QueryStaticOperatorSize(requestParams, min, max);
    double expectMin = 0.21191406300000001;
    double expectMax = 2343.78125;
    EXPECT_TRUE(result);
    EXPECT_EQ(min, expectMin);
    EXPECT_EQ(max, expectMax);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorSizeDataWithGraphId) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorSizeParams requestParams;
    requestParams.graphId = "0";
    double min;
    double max;
    bool result = database->QueryStaticOperatorSize(requestParams, min, max);
    double expectMin = 0.21191406300000001;
    double expectMax = 2343.78125;
    EXPECT_TRUE(result);
    EXPECT_EQ(min, expectMin);
    EXPECT_EQ(max, expectMax);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorGraph) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorGraphParams requestParams;
    requestParams.rankId = "1";
    requestParams.graphId = "0";
    StaticOperatorGraphItem data;
    bool result = database->QueryStaticOperatorGraph(requestParams, data);
    int expectSize = 3;
    int expectColumnSize = 24;
    int pickedIndex = 5;
    string exceptPickedData = "4.35";
    EXPECT_TRUE(result);
    EXPECT_EQ(data.legends.size(), expectSize);
    EXPECT_EQ(data.lines.size(), expectColumnSize);
    EXPECT_EQ(data.lines[pickedIndex][1], exceptPickedData);
}

TEST_F(TextMemoryDatabaseTest, QueryStaticOperatorGraphWithGraphId) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::StaticOperatorGraphParams requestParams;
    requestParams.rankId = "1";
    requestParams.graphId = "1";
    StaticOperatorGraphItem data;
    bool result = database->QueryStaticOperatorGraph(requestParams, data);
    int expectColumnSize = 26;
    string exceptPickedData = "4.48";
    int pickedIndex = 19;
    EXPECT_TRUE(result);
    EXPECT_EQ(data.lines.size(), expectColumnSize);
    EXPECT_EQ(data.lines[pickedIndex][1], exceptPickedData);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryOperatorWithSize) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 10; // page size = 10
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    requestParams.minSize = 10; // min size = 10
    requestParams.maxSize = 100; // min size = 100
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    bool result = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 10;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryOperatorByStreamExceptZero) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startTime = 0;
    requestParams.endTime = -1;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    int64_t result = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 0;
    EXPECT_EQ(result, expectSize);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryOperatorByStreamExceptSeveral) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "1";
    requestParams.deviceId = "1";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    requestParams.currentPage = 0;
    requestParams.pageSize = 100; // page size = 100
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<Dic::Protocol::MemoryOperator> responseBody;
    bool result = database->QueryOperatorDetail(requestParams, responseBody);
    int expectSize = 21;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryComponentsTotalNum) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryComponentParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.currentPage = 1;
    requestParams.pageSize = 10; // page size = 10
    requestParams.order = "descend";
    requestParams.orderBy = "timestamp";
    int64_t totalNum;
    bool result = database->QueryComponentsTotalNum(requestParams, totalNum);
    int expectSize = 2;
    EXPECT_TRUE(result);
    EXPECT_EQ(totalNum, expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryOperatorsTotalNum) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.searchName = "cann::Graph_";
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 28;
    EXPECT_EQ(totalNum, expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryOperatorsTotalNumWithSize) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.filters[std::string(OpMemoryColumn::NAME)] = "cann::Graph_";
    requestParams.rangeFilters[std::string(OpMemoryColumn::SIZE)] = {10, 1000};
    requestParams.startTime = -1;
    requestParams.endTime = -1;
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 15;
    EXPECT_EQ(totalNum, expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryOperatorsTotalNumWithTime) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.searchName = "cann::Graph_";
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    requestParams.startTime = 0;
    requestParams.endTime = 1695120000000; // end time = 1695120000000
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 28;
    EXPECT_EQ(totalNum, expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryOperatorsTotalNumWithLimitedTime) {
    uint64_t startTime = Dic::Module::Timeline::TraceTime::Instance().GetStartTime();
    uint64_t offsetTime = Dic::Module::Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp("0");
    const uint64_t timeStamp = 1695115378729750000;
    const double secondToMillisecond = 1000.0;
    const int precision = 3;
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    requestParams.searchName = "cann::Graph_";
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    requestParams.startTime = NumberUtil::DoubleReservedNDigits(
        (timeStamp - startTime - offsetTime) / (secondToMillisecond * secondToMillisecond), precision);
    requestParams.endTime = NumberUtil::DoubleReservedNDigits(
        (timeStamp - startTime - offsetTime) / (secondToMillisecond * secondToMillisecond), precision);
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 1;
    EXPECT_EQ(totalNum, expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryOperatorsTotalNumByStreamExpectZero) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    requestParams.startTime = -1;
    requestParams.endTime = -1; // end time = 1695120000000
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 0;
    EXPECT_EQ(totalNum, expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryOperatorsTotalNumByStreamExpectSeveral) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::MemoryOperatorParams requestParams;
    requestParams.rankId = "1";
    requestParams.deviceId = "1";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    requestParams.minSize = std::numeric_limits<int64_t>::min();
    requestParams.maxSize = std::numeric_limits<int64_t>::max();
    requestParams.startTime = -1;
    requestParams.endTime = -1; // end time = 1695120000000
    std::vector<Dic::Protocol::MemoryOperator> operators;
    int64_t totalNum = database->QueryOperatorDetail(requestParams, operators);
    int expectSize = 21;
    EXPECT_EQ(totalNum, expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryViewData) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_OVERALL_GROUP;
    Dic::Protocol::MemoryViewData responseBody;
    uint64_t offsetTime = 0;
    bool result = database->QueryMemoryView(requestParams, responseBody, offsetTime);
    const int expectSize = 5 * 4;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.tempData.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryViewDataByStreamExpectZero) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryViewParams requestParams;
    requestParams.rankId = "0";
    requestParams.deviceId = "0";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    Dic::Protocol::MemoryViewData responseBody;
    uint64_t offsetTime = 0;
    bool result = database->QueryMemoryView(requestParams, responseBody, offsetTime);
    int expectSize = 0;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.lines.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryViewDataByStreamExpectSeveral) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::MemoryViewParams requestParams;
    requestParams.rankId = "1";
    requestParams.deviceId = "1";
    requestParams.type = Protocol::MEMORY_STREAM_GROUP;
    Dic::Protocol::MemoryViewData responseBody;
    uint64_t offsetTime = 0;
    bool result = database->QueryMemoryView(requestParams, responseBody, offsetTime);
    const int expectSize = 1 * 4;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.tempData.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryMemoryViewDataByComponentExpectSeveral) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("1");
    Dic::Protocol::MemoryViewParams requestParams;
    requestParams.rankId = "1";
    requestParams.deviceId = "1";
    requestParams.type = Protocol::MEMORY_COMPONENT_GROUP;
    Dic::Protocol::MemoryViewData responseBody;
    uint64_t offsetTime = 0;
    bool result = database->QueryMemoryView(requestParams, responseBody, offsetTime);
    const int expectSize = 145;
    EXPECT_TRUE(result);
    EXPECT_EQ(responseBody.tempData.size(), expectSize);
}

TEST_F(TextMemoryDatabaseTest, QueryOperatorSizeData) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorSizeParams requestParams;
    requestParams.deviceId = "0";
    double min;
    double max;
    bool result = database->QueryOperatorSize(requestParams, min, max);
    double expectMin = 64.0;
    double expectMax = 81984.0;
    EXPECT_TRUE(result);
    EXPECT_EQ(min, expectMin);
    EXPECT_EQ(max, expectMax);
}

/***
 * 组合筛选/排序/范围筛选测试
 */
TEST_F(TextMemoryDatabaseTest, TextDbQueryOperatorWithFilterNameAndOrderBy) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    MemoryOperatorParams params;
    params.deviceId = "0";
    params.rankId = "0";
    params.startTime = -1;
    params.endTime = -1;
    // 测试过滤name并按照释放时间降序排列
    std::string expectFilterName = "cann";
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

TEST_F(TextMemoryDatabaseTest, TextDbQueryOperatorWithFilterNameAndOrderByAndRangeFilter) {
    auto database = DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    MemoryOperatorParams params;
    params.deviceId = "0";
    params.rankId = "0";
    params.startTime = -1;
    params.endTime = -1;
    params.currentPage = 1;
    params.pageSize = 10;
    double minTimeMs = 0;
    double maxTimeMs = 100000;
    double minSize = -1000;
    double maxSize = 100000;
    std::string expectFilterName = "cann";
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

TEST_F(TextMemoryDatabaseTest, TextDbQueryOperatorWithFullCondition) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams params;
    params.deviceId = "0";
    params.rankId = "0";
    params.startTime = 100;
    params.endTime = 10000;
    params.currentPage = 1;
    params.pageSize = 10;
    double minTimeMs = 0;
    double maxTimeMs = 100000;
    double minSize = -1000;
    double maxSize = 100000;
    std::string expectFilterName = "can";
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

TEST_F(TextMemoryDatabaseTest, TextDbQueryOperatorWithFullConditionAndOnlyShowInterval) {
    auto database = Dic::Module::Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId("0");
    Dic::Protocol::MemoryOperatorParams params;
    params.deviceId = "0";
    params.rankId = "0";
    params.startTime = 100;
    params.endTime = 10000;
    params.currentPage = 1;
    params.pageSize = 10;
    double minTimeMs = 0;
    double maxTimeMs = 100000;
    double minSize = -1000;
    double maxSize = 100000;
    std::string expectFilterName = "can";
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
