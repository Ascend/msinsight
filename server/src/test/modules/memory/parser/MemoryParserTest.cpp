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
#include "ParserStatusManager.h"
#include "DataBaseManager.h"
#include "MemoryParse.h"

using namespace Dic::Protocol;
using namespace Dic::Module::Timeline;
using namespace Dic::Module::Memory;
using namespace Dic;

class MemoryParserTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
};

TEST_F(MemoryParserTest, OperatorParseNormalTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    const std::string fileId = "0";
    const std::string dataPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                                 "/ASCEND_PROFILER_OUTPUT/operator_memory.csv";
    const std::string dbPath = "test/data/pytorch/text/level1/rank0_ascend_pt"
                               "/ASCEND_PROFILER_OUTPUT/mindstudio_insight_data.db";
    const std::string filePath = currPath.substr(0, index) + dataPath;
    const std::string dbFilePath = currPath.substr(0, index) + dbPath;
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
    const std::string filePath = currPath.substr(0, index) + dataPath;
    const std::string dbFilePath = currPath.substr(0, index) + dbPath;
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
    const std::string filePath = currPath.substr(0, index) + dataPath;
    const std::string dbFilePath = currPath.substr(0, index) + dbPath;
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
    const std::string filePath = currPath.substr(0, index) + dataPath;
    const std::string dbFilePath = currPath.substr(0, index) + dbPath;
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
    const std::string filePath = currPath.substr(0, index) + dataPath;
    const std::string dbFilePath = currPath.substr(0, index) + dbPath;
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
    const std::string filePath = currPath.substr(0, index) + dataPath;
    const std::string dbFilePath = currPath.substr(0, index) + dbPath;
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
    ParserStatusManager::Instance().ClearAllParserStatus();
    DataBaseManager::Instance().Clear();
    std::remove(dbFilePath.c_str());
    std::remove(filePath.c_str());
}
