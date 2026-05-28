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
#include "OperatorMemoryService.h"
#include "../../../DatabaseTestCaseMockUtil.h"
#include "OperatorTable.h"
#include "OpMemoryTable.h"
using namespace Dic::Module::Memory;
class OperatorMemoryServiceTest : public ::testing::Test {
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
    const std::string operatorSql =
        "CREATE TABLE operator (name TEXT, size INTEGER, allocationTime INTEGER, releaseTime INTEGER, "
        "activeReleaseTime INTEGER, duration INTEGER, activeDuration INTEGER, allocationTotalAllocated INTEGER, "
        "allocationTotalReserved INTEGER, allocationTotalActive INTEGER, releaseTotalAllocated INTEGER, "
        "releaseTotalReserved INTEGER, releaseTotalActive INTEGER, streamPtr TEXT, deviceId TEXT);";

    const std::string opMemorySql =
        "CREATE TABLE OP_MEMORY (name INTEGER, size INTEGER, allocationTime INTEGER, releaseTime INTEGER, "
        "activeReleaseTime INTEGER, duration INTEGER, activeDuration INTEGER, allocationTotalAllocated INTEGER, "
        "allocationTotalReserved INTEGER, allocationTotalActive INTEGER, releaseTotalAllocated INTEGER, "
        "releaseTotalReserved INTEGER, releaseTotalActive INTEGER, streamPtr INTEGER, deviceId INTEGER);";
};

/**
 * text场景根据id查询算子内存分配信息
 */
TEST_F(OperatorMemoryServiceTest, TestComputeAllocationTimeByIdWhenTextSceneThenReturnTextData) {
    std::unique_ptr<OperatorTableMock> operatorTable = std::make_unique<OperatorTableMock>();
    sqlite3 *db = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(db);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::CreateTable(db, operatorSql);
    const std::string operatorData =
        "INSERT INTO \"main\".\"operator\" (\"name\", \"size\", \"allocationTime\", \"releaseTime\", "
        "\"activeReleaseTime\", \"duration\", \"activeDuration\", \"allocationTotalAllocated\", "
        "\"allocationTotalReserved\", "
        "\"allocationTotalActive\", \"releaseTotalAllocated\", \"releaseTotalReserved\", \"releaseTotalActive\", "
        "\"streamPtr\", "
        "\"deviceId\") VALUES ('aten::empty_strided', 32.5, 1724670453465053360, 1724670453467680330, 2626.97, "
        "1724670453467680020, 2626.66, 18180.814453125, 25750, 18180.814453125, 18181.8559570313, 25750, "
        "18181.8559570313, "
        "'187651271017536', '0');";
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::InsertData(db, operatorData);
    operatorTable->SetDb(db);
    OperatorMemoryService operatorMemoryService(std::move(operatorTable));
    OperatorDomain target = operatorMemoryService.ComputeAllocationTimeById("lll", "1");
    const uint64_t expectAllocationTime = 1724670453465053360;
    EXPECT_EQ(target.allocationTime, expectAllocationTime);
    EXPECT_EQ(target.metaType, "TEXT");
}

/**
 * db场景根据id查询算子内存分配信息
 */
TEST_F(OperatorMemoryServiceTest, TestComputeAllocationTimeByIdWhenDbSceneThenReturnDbData) {
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
    OperatorMemoryService operatorMemoryService(std::move(operatorTable), std::move(opMemoryTable));
    OperatorDomain target = operatorMemoryService.ComputeAllocationTimeById("lll", "1");
    const uint64_t expectAllocationTime = 1724670453468255710;
    EXPECT_EQ(target.allocationTime, expectAllocationTime);
    EXPECT_EQ(target.metaType, "PYTORCH_API");
}

/**
 * 都不存在
 */
TEST_F(OperatorMemoryServiceTest, TestComputeAllocationTimeByIdWhenDataNotExistThenMetaTypeIsEmpty) {
    std::unique_ptr<OperatorTableMock> operatorTable = std::make_unique<OperatorTableMock>();
    std::unique_ptr<OpMemoryTableMock> opMemoryTable = std::make_unique<OpMemoryTableMock>();
    sqlite3 *db = nullptr;
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::OpenDB(db);
    Dic::Global::PROFILER::MockUtil::DatabaseTestCaseMockUtil::CreateTable(db, opMemorySql);
    operatorTable->SetDb(db);
    opMemoryTable->SetDb(db);
    OperatorMemoryService operatorMemoryService(std::move(operatorTable), std::move(opMemoryTable));
    OperatorDomain target = operatorMemoryService.ComputeAllocationTimeById("lll", "1");
    EXPECT_EQ(std::empty(target.metaType), true);
}
