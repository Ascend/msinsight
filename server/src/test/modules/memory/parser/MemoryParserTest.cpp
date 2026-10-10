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
#include "pch.h"
#include "ParserStatusManager.h"
#include "DataBaseManager.h"
#include "TextMemoryDataBase.h"
#include "ThreadPool.h"
#include "FileParser.h"
#include "CurveContainer.h"
#include "TimelineProtocolEvent.h"
#define private public
#include "MemoryParse.h"
#undef private
#include "TestSuit.h"

using namespace Dic::Protocol;
using namespace Dic::Module::Timeline;
using namespace Dic::Module::Memory;
using namespace Dic;

class MemoryParserTestEnvironment : public ::testing::Environment {
  public:
    void TearDown() override {
        // Stop workers before process-wide logging and TLS cleanup begins.
        const auto *tests = ::testing::UnitTest::GetInstance();
        for (int i = 0; i < tests->total_test_suite_count(); ++i) {
            const auto *suite = tests->GetTestSuite(i);
            const std::string name = suite->name();
            if (suite->should_run() &&
                (name == "MemoryParserTest" || name == "TextMemoryDatabaseTest" ||
                    name == "MemoryRequestHandlerTest")) {
                MemoryParse::Instance().threadPool->ShutDown();
                return;
            }
        }
    }
};

[[maybe_unused]] const auto memoryParserTestEnvironment =
    ::testing::AddGlobalTestEnvironment(new MemoryParserTestEnvironment);

class MemoryParserTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {
        Server::ServerLog::Info("Initialize memory parser test dependencies.");
        TraceTime::Instance().Reset();
    }
    static void TearDownTestSuite() {}
};

TEST_F(MemoryParserTest, ParseTextMemoryFilesIntoFreshDatabase) {
    const auto source = std::filesystem::path(TestSuit::GetRootTestPath()) / "data" / "pytorch" / "text" / "level1" /
        "rank0_ascend_pt" / "ASCEND_PROFILER_OUTPUT";
    const auto dbPath = std::filesystem::path(::testing::TempDir()) / "memory_parser_fresh_coverage.db";
    const std::string rankId = "memory_parser_fresh_coverage";
    std::filesystem::remove(dbPath);

    DataBaseManager::Instance().SetDataType(DataType::TEXT, dbPath.string());
    DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbPath.string());
    auto database = std::dynamic_pointer_cast<TextMemoryDataBase, VirtualMemoryDataBase>(
        DataBaseManager::Instance().CreateMemoryDataBase(rankId, dbPath.string()));
    ASSERT_NE(database, nullptr);
    ASSERT_TRUE(database->OpenDb(dbPath.string(), false));
    ASSERT_TRUE(database->CreateTable());
    ParserStatusManager::Instance().SetParserStatus(MEMORY_PREFIX + rankId, ParserStatus::RUNNING);

    EXPECT_TRUE(MemoryParse::Instance().OperatorParse((source / "operator_memory.csv").string(), rankId));
    EXPECT_TRUE(MemoryParse::Instance().RecordToParse((source / "memory_record.csv").string(), rankId));
    EXPECT_TRUE(MemoryParse::Instance().ComponentParse((source / "npu_module_mem.csv").string(), rankId));
    EXPECT_GT(database->QueryMinOperatorAllocationTime(), 0U);
    EXPECT_GT(database->QueryMinRecordTimestamp(), 0U);
    EXPECT_GT(database->QueryMinComponentTimestamp(), 0U);

    std::string memoryType;
    std::vector<std::string> graphIds;
    EXPECT_TRUE(database->QueryMemoryType(memoryType, graphIds));
    std::string resourceType;
    EXPECT_TRUE(database->QueryMemoryResourceType(resourceType));

    MemoryComponentParams componentParams;
    componentParams.rankId = rankId;
    componentParams.deviceId = "0";
    componentParams.currentPage = 1;
    componentParams.pageSize = 20;
    int64_t componentCount = -1;
    EXPECT_TRUE(database->QueryComponentsTotalNum(componentParams, componentCount));
    EXPECT_GE(componentCount, 0);
    std::vector<MemoryTableColumnAttr> columns;
    std::vector<MemoryComponent> components;
    EXPECT_TRUE(database->QueryComponentDetail(componentParams, columns, components));

    MemoryOperatorSizeParams sizeParams;
    sizeParams.rankId = rankId;
    sizeParams.deviceId = "0";
    double minSize = 0;
    double maxSize = 0;
    EXPECT_TRUE(database->QueryOperatorSize(sizeParams, minSize, maxSize));
    MemoryOperatorParams operatorParams;
    operatorParams.rankId = rankId;
    operatorParams.deviceId = "0";
    operatorParams.currentPage = 1;
    operatorParams.pageSize = 20;
    std::vector<MemoryOperator> operators;
    EXPECT_GE(database->QueryOperatorDetail(operatorParams, operators), 0);
    operators.clear();
    EXPECT_TRUE(database->QueryEntireOperatorTable(operatorParams, operators, 0));
    components.clear();
    EXPECT_TRUE(database->QueryEntireComponentTable(componentParams, components, 0));

    // Exercise the static-operator query chain against actual persisted rows.
    database->InsertStaticOpDetail({"0", "TOTAL", "model", "graph", 0, 10, 4096});
    database->InsertStaticOpDetail({"0", "MatMul", "model", "graph", 1, 5, 2048});
    database->InsertStaticOpDetail({"0", "Relu", "model", "graph", 6, 9, 1024});
    database->SaveStaticOpDetail();
    StaticOperatorSizeParams staticSizeParams;
    staticSizeParams.rankId = rankId;
    staticSizeParams.graphId = "graph";
    EXPECT_TRUE(database->QueryStaticOperatorSize(staticSizeParams, minSize, maxSize));
    EXPECT_LE(minSize, maxSize);
    StaticOperatorListParams staticListParams;
    staticListParams.rankId = rankId;
    staticListParams.graphId = "graph";
    staticListParams.currentPage = 1;
    staticListParams.pageSize = 20;
    staticListParams.endNodeIndex = 10;
    staticListParams.maxSize = std::numeric_limits<int64_t>::max();
    std::vector<StaticOperatorItem> staticOperators;
    EXPECT_GE(database->QueryStaticOperatorList(staticListParams, staticOperators), 2);
    EXPECT_EQ(staticOperators.size(), 2U);
    staticOperators.clear();
    EXPECT_TRUE(database->QueryEntireStaticOperatorTable(staticListParams, staticOperators));
    EXPECT_EQ(staticOperators.size(), 2U);
    StaticOperatorGraphParams staticGraphParams;
    staticGraphParams.rankId = rankId;
    staticGraphParams.graphId = "graph";
    StaticOperatorGraphItem staticGraph;
    EXPECT_TRUE(database->QueryStaticOperatorGraph(staticGraphParams, staticGraph));

    const auto staticCsv = std::filesystem::path(::testing::TempDir()) / "memory_static_coverage.csv";
    {
        std::ofstream out(staticCsv);
        out << "Device_id,Op Name,Model Name,Graph ID,Node Index Start,Node Index End,Size(KB)\n";
        out << "NPU:0,TOTAL,model,parsed-graph,0,5,4096\n";
        out << "NPU:0,Add,model,parsed-graph,1,4,1024\n";
    }
    EXPECT_TRUE(MemoryParse::Instance().StaticOpParse(staticCsv.string(), rankId));
    staticSizeParams.graphId = "parsed-graph";
    EXPECT_TRUE(database->QueryStaticOperatorSize(staticSizeParams, minSize, maxSize));
    EXPECT_DOUBLE_EQ(maxSize, 1024.0);
    std::filesystem::remove(staticCsv);

    database->CloseDb();
    ParserStatusManager::Instance().ClearAllParserStatus();
    database.reset();
    DataBaseManager::Instance().Clear();
    std::error_code cleanupError;
    std::filesystem::remove(dbPath, cleanupError);
}

TEST_F(MemoryParserTest, DiscoversPeerMemoryFilesFromRecordAndFolder) {
    const auto source = std::filesystem::path(TestSuit::GetRootTestPath()) / "data" / "pytorch" / "text" / "level1" /
        "rank0_ascend_pt" / "ASCEND_PROFILER_OUTPUT";
    auto fromRecord = MemoryParse::Instance().GetMemoryFile((source / "memory_record.csv").string());
    EXPECT_FALSE(fromRecord.recordFiles.empty());
    EXPECT_FALSE(fromRecord.operatorFiles.empty());
    auto fromFolder = MemoryParse::Instance().GetMemoryFile(source.string());
    EXPECT_FALSE(fromFolder.recordFiles.empty());
    EXPECT_TRUE(MemoryParse::Instance().GetMemoryFile((source / "missing.csv").string()).recordFiles.empty());
}

TEST_F(MemoryParserTest, ParseTaskCompletesAndReusesFinishedDatabase) {
    const auto source = std::filesystem::path(TestSuit::GetTestDataFile("test_rank_0", "ASCEND_PROFILER_OUTPUT"));
    const auto folder = std::filesystem::path(::testing::TempDir()) / "memory_async_coverage";
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    for (const std::string name : {"operator_memory.csv", "memory_record.csv", "npu_module_mem.csv"}) {
        std::filesystem::copy_file(source / name, folder / name);
    }
    const std::string rank = "memory_async_coverage";
    const auto dbPath = FileUtil::GetDbPath((folder / "memory_record.csv").string(), rank);
    DataBaseManager::Instance().SetDataType(DataType::TEXT, dbPath);
    DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbPath);
    ParserStatusManager::Instance().NotifyStartParse();
    auto &parser = MemoryParse::Instance();
    std::string message;
    EXPECT_FALSE(MemoryParse::InitParser({}, rank, message));
    EXPECT_FALSE(parser.Parse({dbPath, rank, (folder / "missing").string()}));
    EXPECT_FALSE(parser.Parse({}, rank, (folder / "missing").string(), dbPath));
    MemoryParse::SetParseCallBack();
    for (int iteration = 0; iteration < 2; ++iteration) {
        const auto files = parser.GetMemoryFiles({folder.string()}, rank, dbPath);
        ASSERT_EQ(files.size(), 1U);
        ParserStatusManager::Instance().SetParserStatus(MEMORY_PREFIX + rank, ParserStatus::INIT);
        // Exercise the actual task synchronously to avoid Windows DLL TLS teardown races.
        MemoryParse::PreParseTask(files.at(rank), rank);
        EXPECT_EQ(ParserStatusManager::Instance().GetParserStatus(MEMORY_PREFIX + rank), ParserStatus::FINISH);
        auto database =
            std::dynamic_pointer_cast<TextMemoryDataBase>(DataBaseManager::Instance().GetMemoryDatabaseByRankId(rank));
        ASSERT_NE(database, nullptr);
        EXPECT_TRUE(database->HasFinishedParseLastTime());
        EXPECT_GT(database->QueryMinOperatorAllocationTime(), 0U);
        EXPECT_GT(database->QueryMinRecordTimestamp(), 0U);
        database.reset();
        parser.Reset();
    }
    MemoryParse::ParseCallBack("", "", true, "");
    ParserStatusManager::Instance().ClearAllParserStatus();
    std::filesystem::remove_all(folder);
}

TEST_F(MemoryParserTest, OperatorParseNormalTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string fileId = "0";
    const std::string dataPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                                 "/ASCEND_PROFILER_OUTPUT/operator_memory.csv";
    const std::string dbPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                               "/ASCEND_PROFILER_OUTPUT/mindstudio_insight_data.db";
    const std::string filePath = std::filesystem::path(currPath.substr(0, index) + dataPath).make_preferred().string();
    const std::string dbFilePath = std::filesystem::path(currPath.substr(0, index) + dbPath).make_preferred().string();
    DataBaseManager::Instance().SetDataType(DataType::TEXT, dbFilePath);
    DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbFilePath);
    ParserStatusManager::Instance().SetParserStatus(MEMORY_PREFIX + fileId, ParserStatus::RUNNING);
    auto memoryDatabase = std::dynamic_pointer_cast<TextMemoryDataBase, VirtualMemoryDataBase>(
        DataBaseManager::Instance().CreateMemoryDataBase(fileId, dbFilePath));
    memoryDatabase->OpenDb(dbFilePath, false);
    memoryDatabase->CreateTable();
    bool result = MemoryParse::Instance().OperatorParse(filePath, fileId);
    EXPECT_TRUE(result);
    memoryDatabase->CloseDb();
    memoryDatabase.reset();
    ParserStatusManager::Instance().ClearAllParserStatus();
    DataBaseManager::Instance().Clear();
    std::remove(dbFilePath.c_str());
}

TEST_F(MemoryParserTest, OperatorParseEmptyLineTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string fileId = "0";
    const std::string dataPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                                 "/ASCEND_PROFILER_OUTPUT/operator_memory_invalid.csv";
    const std::string dbPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                               "/ASCEND_PROFILER_OUTPUT/mindstudio_insight_data.db";
    const std::string filePath = std::filesystem::path(currPath.substr(0, index) + dataPath).make_preferred().string();
    const std::string dbFilePath = std::filesystem::path(currPath.substr(0, index) + dbPath).make_preferred().string();
    std::ofstream outfile;
    outfile.open(filePath, std::ios::out | std::ios::trunc);
    outfile << "\n";
    const std::string tableColumn = "Name,Size(KB),Allocation Time(us),Release Time(us),Active Release Time(us),"
                                    "Duration(us),Active Duration(us),Allocation Total Allocated(MB),"
                                    "Allocation Total Reserved(MB),Allocation Total Active(MB),"
                                    "Release Total Allocated(MB),Release Total Reserved(MB),"
                                    "Release Total Active(MB),Stream Ptr,Device Type";
    outfile << tableColumn;
    outfile.close();
    DataBaseManager::Instance().SetDataType(DataType::TEXT, dbFilePath);
    DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbFilePath);
    ParserStatusManager::Instance().SetParserStatus(MEMORY_PREFIX + fileId, ParserStatus::RUNNING);
    auto memoryDatabase = std::dynamic_pointer_cast<TextMemoryDataBase, VirtualMemoryDataBase>(
        DataBaseManager::Instance().CreateMemoryDataBase(fileId, dbFilePath));
    memoryDatabase->OpenDb(dbFilePath, false);
    memoryDatabase->CreateTable();
    bool result = MemoryParse::Instance().OperatorParse(filePath, fileId);
    EXPECT_FALSE(result);
    memoryDatabase->CloseDb();
    memoryDatabase.reset();
    ParserStatusManager::Instance().ClearAllParserStatus();
    DataBaseManager::Instance().Clear();
    std::remove(dbFilePath.c_str());
    std::remove(filePath.c_str());
}

TEST_F(MemoryParserTest, RecordParseNormalTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string fileId = "0";
    const std::string dataPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                                 "/ASCEND_PROFILER_OUTPUT/memory_record.csv";
    const std::string dbPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                               "/ASCEND_PROFILER_OUTPUT/mindstudio_insight_data.db";
    const std::string filePath = std::filesystem::path(currPath.substr(0, index) + dataPath).make_preferred().string();
    const std::string dbFilePath = std::filesystem::path(currPath.substr(0, index) + dbPath).make_preferred().string();
    DataBaseManager::Instance().SetDataType(DataType::TEXT, dbFilePath);
    DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbFilePath);
    ParserStatusManager::Instance().SetParserStatus(MEMORY_PREFIX + fileId, ParserStatus::RUNNING);
    auto memoryDatabase = std::dynamic_pointer_cast<TextMemoryDataBase, VirtualMemoryDataBase>(
        DataBaseManager::Instance().CreateMemoryDataBase(fileId, dbFilePath));
    memoryDatabase->OpenDb(dbFilePath, false);
    memoryDatabase->CreateTable();
    bool result = MemoryParse::Instance().RecordToParse(filePath, fileId);
    EXPECT_TRUE(result);
    memoryDatabase->CloseDb();
    memoryDatabase.reset();
    ParserStatusManager::Instance().ClearAllParserStatus();
    DataBaseManager::Instance().Clear();
    std::remove(dbFilePath.c_str());
}

TEST_F(MemoryParserTest, RecordParseEmptyLineTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string fileId = "0";
    const std::string dataPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                                 "/ASCEND_PROFILER_OUTPUT/memory_record_invalid.csv";
    const std::string dbPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                               "/ASCEND_PROFILER_OUTPUT/mindstudio_insight_data.db";
    const std::string filePath = std::filesystem::path(currPath.substr(0, index) + dataPath).make_preferred().string();
    const std::string dbFilePath = std::filesystem::path(currPath.substr(0, index) + dbPath).make_preferred().string();
    std::ofstream outfile;
    outfile.open(filePath, std::ios::out | std::ios::trunc);
    outfile << "\n";
    const std::string tableColumn = "Component,Timestamp(us),Total Allocated(MB),Total Reserved(MB),Total Active(MB),"
                                    "Stream Ptr,Device Type";
    outfile << tableColumn;
    outfile.close();
    DataBaseManager::Instance().SetDataType(DataType::TEXT, dbFilePath);
    DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbFilePath);
    ParserStatusManager::Instance().SetParserStatus(MEMORY_PREFIX + fileId, ParserStatus::RUNNING);
    auto memoryDatabase = std::dynamic_pointer_cast<TextMemoryDataBase, VirtualMemoryDataBase>(
        DataBaseManager::Instance().CreateMemoryDataBase(fileId, dbFilePath));
    memoryDatabase->OpenDb(dbFilePath, false);
    memoryDatabase->CreateTable();
    bool result = MemoryParse::Instance().RecordToParse(filePath, fileId);
    EXPECT_FALSE(result);
    memoryDatabase->CloseDb();
    memoryDatabase.reset();
    ParserStatusManager::Instance().ClearAllParserStatus();
    DataBaseManager::Instance().Clear();
    std::remove(dbFilePath.c_str());
    std::remove(filePath.c_str());
}

TEST_F(MemoryParserTest, StaticOpParseNormalTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string fileId = "0";
    const std::string dataPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                                 "/ASCEND_PROFILER_OUTPUT/static_op_mem.csv";
    const std::string dbPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                               "/ASCEND_PROFILER_OUTPUT/mindstudio_insight_data.db";
    const std::string filePath = std::filesystem::path(currPath.substr(0, index) + dataPath).make_preferred().string();
    const std::string dbFilePath = std::filesystem::path(currPath.substr(0, index) + dbPath).make_preferred().string();
    std::ofstream outfile;
    outfile.open(filePath, std::ios::out | std::ios::trunc);
    const std::string tableColumn = "Device_id,Op Name,Model Name,Graph ID,Node Index Start,Node Index End,Size(KB)";
    outfile << tableColumn;
    outfile.close();
    DataBaseManager::Instance().SetDataType(DataType::TEXT, dbFilePath);
    DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbFilePath);
    ParserStatusManager::Instance().SetParserStatus(MEMORY_PREFIX + fileId, ParserStatus::RUNNING);
    auto memoryDatabase = std::dynamic_pointer_cast<TextMemoryDataBase, VirtualMemoryDataBase>(
        DataBaseManager::Instance().CreateMemoryDataBase(fileId, dbFilePath));
    memoryDatabase->OpenDb(dbFilePath, false);
    memoryDatabase->CreateTable();
    bool result = MemoryParse::Instance().StaticOpParse(filePath, fileId);
    EXPECT_TRUE(result);
    memoryDatabase->CloseDb();
    memoryDatabase.reset();
    ParserStatusManager::Instance().ClearAllParserStatus();
    DataBaseManager::Instance().Clear();
    std::remove(dbFilePath.c_str());
    std::remove(filePath.c_str());
}

TEST_F(MemoryParserTest, StaticOpParseEmptyLineTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string fileId = "0";
    const std::string dataPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                                 "/ASCEND_PROFILER_OUTPUT/stat_invalid.csv";
    const std::string dbPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                               "/ASCEND_PROFILER_OUTPUT/mindstudio_insight_data.db";
    const std::string filePath = std::filesystem::path(currPath.substr(0, index) + dataPath).make_preferred().string();
    const std::string dbFilePath = std::filesystem::path(currPath.substr(0, index) + dbPath).make_preferred().string();
    std::ofstream outfile;
    outfile.open(filePath, std::ios::out | std::ios::trunc);
    outfile << "\n";
    const std::string tableColumn = "Device_id,Op Name,Model Name,Graph ID,Node Index Start,Node Index End,Size(KB)";
    outfile << tableColumn;
    outfile.close();
    DataBaseManager::Instance().SetDataType(DataType::TEXT, dbFilePath);
    DataBaseManager::Instance().SetFileType(FileType::PYTORCH, dbFilePath);
    ParserStatusManager::Instance().SetParserStatus(MEMORY_PREFIX + fileId, ParserStatus::RUNNING);
    auto memoryDatabase = std::dynamic_pointer_cast<TextMemoryDataBase, VirtualMemoryDataBase>(
        DataBaseManager::Instance().CreateMemoryDataBase(fileId, dbFilePath));
    memoryDatabase->OpenDb(dbFilePath, false);
    memoryDatabase->CreateTable();
    bool result = MemoryParse::Instance().StaticOpParse(filePath, fileId);
    EXPECT_FALSE(result);
    memoryDatabase->CloseDb();
    memoryDatabase.reset();
    ParserStatusManager::Instance().ClearAllParserStatus();
    DataBaseManager::Instance().Clear();
    std::remove(dbFilePath.c_str());
    std::remove(filePath.c_str());
}
