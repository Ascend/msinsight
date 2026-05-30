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
#include "HardWareRepo.h"
#include "TrackInfoManager.h"
#include "../../../DatabaseTestCaseMockUtil.h"
#include "TableDefaultMock.h"
using namespace Dic::Module::Timeline;
using namespace Dic::TimeLine::Table::Default::Mock;
using namespace Dic::Global::PROFILER::MockUtil;
class HardWareRepoTest : public ::testing::Test {
  protected:
    const std::string stringIdsSql = "CREATE TABLE STRING_IDS (id INTEGER PRIMARY KEY,value TEXT);";
    const std::string taskSql = "CREATE TABLE TASK (startNs INTEGER,endNs INTEGER,deviceId INTEGER,connectionId "
                                "INTEGER,globalTaskId INTEGER,globalPid INTEGER,taskType INTEGER,contextId "
                                "INTEGER,streamId INTEGER,taskId INTEGER,modelId INTEGER, depth integer);";
    const std::string computeSql =
        "CREATE TABLE COMPUTE_TASK_INFO (name INTEGER,globalTaskId INTEGER PRIMARY KEY,blockNum INTEGER,mixBlockDim "
        "INTEGER,taskType INTEGER,opType INTEGER,inputFormats INTEGER,inputDataTypes INTEGER,inputShapes "
        "INTEGER,outputFormats INTEGER,outputDataTypes INTEGER,outputShapes INTEGER,attrInfo INTEGER, waitNs INTEGER, "
        "gridDim INTEGER, blockDim INTEGER);";
    const std::string memoryInfoSql =
        "create table MEMCPY_INFO (globalTaskId integer,size integer, memcpyOperation integer);";
    const std::string mstxSql =
        "create table if not exists MSTX_EVENTS(startNs INTEGER,endNs INTEGER, "
        " eventType INTEGER,rangeId INTEGER, category INTEGER, message INTEGER, globalTid INTEGER, "
        " endGlobalTid INTEGER, domainId INTEGER, connectionId INTEGER, depth integer); ";
    const std::string memoryOperationEnumSql =
        "create table if not exists ENUM_MEMCPY_OPERATION(id INTEGER, name TEXT);";
    const std::string taskPmuInfoSql =
        "CREATE TABLE if not exists TASK_PMU_INFO (globalTaskId INTEGER, name INTEGER, value NUMERIC);";
    void SetUp() override { TrackInfoManager::Instance().Reset(); }

    void TearDown() override { TrackInfoManager::Instance().Reset(); }

    void TestQuerySliceDetailInfoNormalPrepare(HardWareDependency &dependency, sqlite3 *&db) {
        DatabaseTestCaseMockUtil::OpenDB(db);
        DatabaseTestCaseMockUtil::CreateTable(db, taskSql);
        DatabaseTestCaseMockUtil::CreateTable(db, computeSql);
        DatabaseTestCaseMockUtil::CreateTable(db, stringIdsSql);
        DatabaseTestCaseMockUtil::CreateTable(db, mstxSql);
        DatabaseTestCaseMockUtil::CreateTable(db, memoryOperationEnumSql);
        DatabaseTestCaseMockUtil::CreateTable(db, taskPmuInfoSql);
        std::string taskInsert =
            "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
            "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
            "(1718180918997521124, 1718180918999870771, 0, 7422, 5, 2045554, 320, 4294967295, 16, 3731, 4294967295, 0);"
            "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", \"globalTaskId\", "
            "\"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", \"modelId\", \"depth\") VALUES "
            "(1718180918997621124, 1718180918999870771, 0, 7422, 6, 2045554, 320, 4294967295, 16, 3731, 4294967295,0);";
        std::string computeInsert =
            "INSERT INTO \"main\".\"COMPUTE_TASK_INFO\" (\"name\", \"globalTaskId\", \"blockNum\", "
            "\"mixBlockDim\", \"taskType\", \"opType\", \"inputFormats\", \"inputDataTypes\", "
            "\"inputShapes\", \"outputFormats\", \"outputDataTypes\", \"outputShapes\", "
            "\"attrInfo\", \"waitNs\", \"gridDim\", \"blockDim\") "
            "VALUES (7, 5, 9, 0, 320, 8, 1, 2, 3, 4, 5, 6, 7, 5340, 11, 12);";
        std::string stringInsert =
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (320, 'KERNEL_SIMT');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (1, 'aaa');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (2, 'bbb');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (3, 'ccc');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (4, 'ddd');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (5, 'eee');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (6, 'fff');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (7, 'ggg');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (8, 'hhh');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (9, 'iii');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (10, 'jjj');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (11, '1,2,3');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (12, '4,5,6');";
        DatabaseTestCaseMockUtil::InsertData(db, taskInsert);
        DatabaseTestCaseMockUtil::InsertData(db, computeInsert);
        DatabaseTestCaseMockUtil::InsertData(db, stringInsert);
        dependency.stringIdsTableMock = std::make_unique<StringIdsTableMock>();
        dependency.stringIdsTableMock->SetDb(db);
        dependency.computeTaskInfoTableMock = std::make_unique<ComputeTaskInfoTableMock>();
        dependency.computeTaskInfoTableMock->SetDb(db);
        dependency.taskTableMock = std::make_unique<TaskTableMock>();
        dependency.taskTableMock->SetDb(db);
        dependency.taskPmuInfoTableMock = std::make_unique<TaskPmuInfoTableMock>();
        dependency.taskPmuInfoTableMock->SetDb(db);
    }

    void TestQueryMemoryInfoNormalPrepare(sqlite3 *&db) {
        DatabaseTestCaseMockUtil::CreateTable(db, memoryInfoSql);
        std::string memoryInsert = "INSERT INTO \"main\".\"MEMCPY_INFO\" (\"globalTaskId\", \"size\","
                                   " \"memcpyOperation\") VALUES (5, 1000, 1);\n";
        DatabaseTestCaseMockUtil::InsertData(db, memoryInsert);
        std::string memoryOperationInsert = "INSERT INTO \"main\".\"ENUM_MEMCPY_OPERATION\" (\"id\", \"name\") "
                                            "VALUES (1, \"host to device\");";
        DatabaseTestCaseMockUtil::InsertData(db, memoryOperationInsert);
    }

    void TestQueryTaskPmuInfoNormalPrepare(sqlite3 *&db) {
        DatabaseTestCaseMockUtil::CreateTable(db, taskPmuInfoSql);
        std::string taskPmuInfoInsert =
            "INSERT INTO \"main\".\"TASK_PMU_INFO\" (\"globalTaskId\", \"name\", \"value\") "
            "VALUES (5, 8, 111), (6, 9, 222), (6, 10, 333)";
        DatabaseTestCaseMockUtil::InsertData(db, taskPmuInfoInsert);
    }

    void TestQueryTaskPmuInfoNormalClean(sqlite3 *&db) {
        std::string taskPmuInfoClean = "TRUNCATE TABLE TASK_PMU_INFO;";
        DatabaseTestCaseMockUtil::ExecuteSql(db, taskPmuInfoClean);
    }
};

class HardWareRepoMock : public HardWareRepo {
  public:
    void SetMock(HardWareDependency &dependency) {
        taskTable = std::move(dependency.taskTableMock);
        computeTaskInfoTable = std::move(dependency.computeTaskInfoTableMock);
        stringIdsTable = std::move(dependency.stringIdsTableMock);
        taskPmuInfoTable = std::move(dependency.taskPmuInfoTableMock);
    }
    Stmt CreatPreparedStatement(const std::string &sql, const SliceQuery &sliceQuery) override {
        auto stmt = std::make_unique<Dic::Module::SqlitePreparedStatement>(db);
        if (!stmt->Prepare(sql)) {
            return nullptr;
        }
        return stmt;
    }
    sqlite3 *db = nullptr;
};

/**
 * 测试根据id查询算子详情,正常情况
 */
TEST_F(HardWareRepoTest, TestQuerySliceDetailInfoNormal) {
    HardWareDependency dependency;
    HardWareRepoMock hardWareRepoMock;
    TestQuerySliceDetailInfoNormalPrepare(dependency, hardWareRepoMock.db);
    hardWareRepoMock.SetMock(dependency);
    SliceQuery query;
    query.sliceId = "1";
    query.rankId = "0";
    CompeteSliceDomain slice;
    bool result = hardWareRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, true);
    const uint64_t expectStart = 1718180918997521124;
    const uint64_t expectEnd = 1718180918999870771;
    EXPECT_EQ(slice.timestamp, expectStart);
    EXPECT_EQ(slice.endTime, expectEnd);
    const std::string expectArgs =
        "{\"modelId\":\"4294967295\",\"taskType\":\"KERNEL_SIMT\",\"streamId\":\"16\","
        "\"taskId\":\"3731\",\"connectionId\":\"7422\",\"gridDim\":\"1,2,3\",\"blockDim\":\"4,5,6\"}";
    EXPECT_EQ(slice.args, expectArgs);
    EXPECT_EQ(slice.sliceShape.inputShapes, "ccc");
    EXPECT_EQ(slice.sliceShape.inputFormats, "aaa");
    EXPECT_EQ(slice.sliceShape.inputDataTypes, "bbb");
    EXPECT_EQ(slice.sliceShape.outputShapes, "fff");
    EXPECT_EQ(slice.sliceShape.outputFormats, "ddd");
    EXPECT_EQ(slice.sliceShape.outputDataTypes, "eee");
    EXPECT_EQ(slice.sliceShape.attrInfo, "ggg");
}

TEST_F(HardWareRepoTest, TestQuerySliceDetailInfoNormalWithMemory) {
    HardWareDependency dependency;
    HardWareRepoMock hardWareRepoMock;
    TestQuerySliceDetailInfoNormalPrepare(dependency, hardWareRepoMock.db);
    TestQueryMemoryInfoNormalPrepare(hardWareRepoMock.db);
    hardWareRepoMock.SetMock(dependency);
    SliceQuery query;
    query.sliceId = "1";
    query.rankId = "0";
    CompeteSliceDomain slice;
    bool result = hardWareRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, true);
    const std::string expectArgs = "{\"modelId\":\"4294967295\",\"taskType\":\"KERNEL_SIMT\",\"streamId\":\"16\","
                                   "\"taskId\":\"3731\",\"connectionId\":\"7422\",\"gridDim\":\"1,2,3\","
                                   "\"blockDim\":\"4,5,6\",\"operation\":\"host to device\",\"size(B)\":1000,"
                                   "\"bandwidth(GB/s)\":0.000426}";
    EXPECT_EQ(slice.args, expectArgs);
}

TEST_F(HardWareRepoTest, TestQuerySliceDetailInfoNormalWithTaskPmuInfo) {
    HardWareDependency dependency;
    HardWareRepoMock hardWareRepoMock;
    TestQuerySliceDetailInfoNormalPrepare(dependency, hardWareRepoMock.db);
    TestQueryTaskPmuInfoNormalPrepare(hardWareRepoMock.db);
    hardWareRepoMock.SetMock(dependency);
    SliceQuery query;
    query.sliceId = "1";
    query.rankId = "0";
    CompeteSliceDomain slice;
    bool result = hardWareRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, true);
    const std::string expectArgs =
        "{\"modelId\":\"4294967295\",\"taskType\":\"KERNEL_SIMT\",\"streamId\":\"16\","
        "\"taskId\":\"3731\",\"connectionId\":\"7422\",\"gridDim\":\"1,2,3\",\"blockDim\":\"4,5,6\","
        "\"hhh\":111.0}";
    EXPECT_EQ(slice.args, expectArgs);
    TestQueryTaskPmuInfoNormalClean(hardWareRepoMock.db);
}

/**
 * 测试根据id查询算子详情,算子不存在的情况
 */
TEST_F(HardWareRepoTest, TestQuerySliceDetailInfoWhenSliceNotExistThenReturnFalse) {
    HardWareRepo hardWareRepo;
    SliceQuery query;
    query.sliceId = "1\u007F\"'\'<>";
    query.rankId = "hhh";
    CompeteSliceDomain slice;
    bool result = hardWareRepo.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, false);
}

/**
 * 测试全量DB的 hardWareRepo 转化 SliceInterface 的情况
 */
TEST_F(HardWareRepoTest, TestDynamicCastOfMultiSliceInterface) {
    std::shared_ptr<IBaseSliceRepo> hardWareRepo = std::make_shared<HardWareRepo>();
    // 转 IPythonFuncSlice 失败
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(hardWareRepo.get());
    EXPECT_EQ(pythonFuncRepo, nullptr);
    // 转 IFindSliceByNameList 成功
    const auto findSliceByNameList = dynamic_cast<IFindSliceByNameList *>(hardWareRepo.get());
    EXPECT_NE(findSliceByNameList, nullptr);
    // 转 IFindSliceByTimepointAndName 失败
    const auto findSliceByTimepointAndName = dynamic_cast<IFindSliceByTimepointAndName *>(hardWareRepo.get());
    EXPECT_EQ(findSliceByTimepointAndName, nullptr);
    // 转 ITextSlice 失败
    const auto textSliceRepo = dynamic_cast<ITextSlice *>(hardWareRepo.get());
    EXPECT_EQ(textSliceRepo, nullptr);
}
