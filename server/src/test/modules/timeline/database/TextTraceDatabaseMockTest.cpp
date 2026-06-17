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
#include "../../../DatabaseTestCaseMockUtil.h"
#include "SliceTable.h"
#include "FlowTable.h"
#include "ThreadTable.h"
#include "ProcessTable.h"
#include "CounterTable.h"
#include "TimelineProtocolResponse.h"
#include "TraceDatabaseHelper.h"
#include "TextTraceDatabase.h"

using namespace Dic::Global::PROFILER::MockUtil;
using namespace Dic::Module::Timeline;
class TextTraceDatabaseMockTest : public ::testing::Test {
protected:
    class MockDatabase : public Dic::Module::Timeline::TextTraceDatabase {
    public:
        explicit MockDatabase(std::recursive_mutex &sqlMutex) : TextTraceDatabase(sqlMutex) {}
        void SetDbPtr(sqlite3 *dbPtr)
        {
            isOpen = true;
            db = dbPtr;
            path = ":memory:";
        }
    };
    SliceTable sliceTable;
    ThreadTable threadTable;
    ProcessTable processTable;
    FlowTable flowTable;
    CounterTable counterTable;
    const std::string sliceTableSql =
        "CREATE TABLE slice (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, duration INTEGER, name TEXT, "
        "depth INTEGER, track_id INTEGER, cat TEXT, args TEXT, cname TEXT, end_time INTEGER, flag_id TEXT, "
        "group_id TEXT);";
    const std::string threadTableSql = "CREATE TABLE thread (track_id INTEGER PRIMARY KEY, tid TEXT, pid TEXT, "
        "thread_name TEXT, thread_sort_index INTEGER);";
    const std::string flowTableSql = "CREATE TABLE flow (id INTEGER PRIMARY KEY AUTOINCREMENT, flow_id TEXT, name "
        "TEXT, cat TEXT, track_id INTEGER, timestamp INTEGER, type TEXT);";
    const std::string counterTableSql = "CREATE TABLE counter (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, pid "
        "TEXT,timestamp INTEGER, cat TEXT, args TEXT);";
    const std::string processSql =
        "CREATE TABLE process (pid TEXT PRIMARY KEY, process_name TEXT, label TEXT, process_sort_index INTEGER);";
    const std::string kernelSql =
        "CREATE TABLE kernel_detail (id INTEGER PRIMARY KEY AUTOINCREMENT, deviceId TEXT, step_id TEXT, name TEXT, "
        "op_type TEXT, accelerator_core TEXT, start_time INTEGER, duration INTEGER, wait_time INTEGER, block_dim "
        "INTEGER, input_shapes TEXT, input_data_types TEXT, input_formats TEXT, output_shapes TEXT, output_data_types "
        "TEXT, output_formats TEXT, aicore_time_us_ TEXT, aic_total_cycles TEXT, aic_mac_time_us_ TEXT, aic_mac_ratio "
        "TEXT, aic_scalar_time_us_ TEXT, aic_scalar_ratio TEXT, aic_mte1_time_us_ TEXT, aic_mte1_ratio TEXT, "
        "aic_mte2_time_us_ TEXT, aic_mte2_ratio TEXT, aic_fixpipe_time_us_ TEXT, aic_fixpipe_ratio TEXT, "
        "aic_icache_miss_rate TEXT, aiv_time_us_ TEXT, aiv_total_cycles TEXT, aiv_vec_time_us_ TEXT, aiv_vec_ratio "
        "TEXT, aiv_scalar_time_us_ TEXT, aiv_scalar_ratio TEXT, aiv_mte2_time_us_ TEXT, aiv_mte2_ratio TEXT, "
        "aiv_mte3_time_us_ TEXT, aiv_mte3_ratio TEXT, aiv_icache_miss_rate TEXT, cube_utilization_PCT_ TEXT);";
};

/**
 * text场景创建表,如果db未打开，返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestCreateTableWhenDbNotOpenThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    bool success = database.CreateTable();
    EXPECT_EQ(success, false);
}

/**
 * text场景创建表,如果db打开，返回true
 */
TEST_F(TextTraceDatabaseMockTest, TestCreateTableWhenDbOpenThenReturnTrue)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    bool success = database.CreateTable();
    std::string sliceSql = "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", "
        "\"track_id\", \"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (1, "
        "1726717581355878760, 9470, 'Node@launch', NULL, 1, NULL, '{\"Thread "
        "Id\":\"206468\",\"Mode\":\"launch\",\"level\":\"node\",\"id\":\"0\",\"item_id\":\"aclnnCat_"
        "ConcatD_ConcatD\",\"connection_id\":\"63052\"}', '', 1726717581355888230, '');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceSql);
    EXPECT_EQ(success, true);
    SliceTable sliceTable;
    std::vector<SlicePO> slicepos;
    sliceTable.Select(SliceColumn::TIMESTAMP).ExcuteQuery(dbPtr, slicepos);
    const uint64_t expectTime = 1726717581355878760;
    const uint64_t expectSize = 1;
    EXPECT_EQ(slicepos.size(), expectSize);
    EXPECT_EQ(slicepos[0].timestamp, expectTime);
}

/**
 * text场景删除表,如果db没打开，返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestDropTableWhenDbNotOpenThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    bool success = database.DropTable();
    EXPECT_EQ(success, false);
}

/**
 * text场景删除表,如果db打开，返回true
 */
TEST_F(TextTraceDatabaseMockTest, TestDropTableWhenDbOpenThenReturnTrue)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sliceSql = "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", "
        "\"track_id\", \"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (1, "
        "1726717581355878760, 9470, 'Node@launch', NULL, 1, NULL, '{\"Thread "
        "Id\":\"206468\",\"Mode\":\"launch\",\"level\":\"node\",\"id\":\"0\",\"item_id\":\"aclnnCat_"
        "ConcatD_ConcatD\",\"connection_id\":\"63052\"}', '', 1726717581355888230, '');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceSql);
    bool success = database.DropTable();
    EXPECT_EQ(success, true);
    SliceTable sliceTable;
    std::vector<SlicePO> slicepos;
    sliceTable.Select(SliceColumn::TIMESTAMP).ExcuteQuery(dbPtr, slicepos);
    const uint64_t expectSize = 0;
    EXPECT_EQ(slicepos.size(), expectSize);
}

/**
 * text场景创建索引,如果db没打开，返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestCreateIndexWhenDbNotOpenThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    Dic::Module::Timeline::TextTraceDatabase database(sqlMutex);
    bool success = database.CreateIndex();
    EXPECT_EQ(success, false);
}

/**
 * text场景创建索引,如果db打开，返回true
 */
TEST_F(TextTraceDatabaseMockTest, TestCreateIndexWhenDbOpenThenReturnTrue)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    bool success = database.CreateIndex();
    EXPECT_EQ(success, true);
}

/**
 * text场景插入1000条数据，数据库里有1000条
 */
TEST_F(TextTraceDatabaseMockTest, TestInsertSliceWhenInsert1000SliceThenDbHave1000count)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    database.SetDbPtr(dbPtr);
    for (int i = 0; i < CACHE_SIZE; ++i) {
        Trace::Slice event;
        event.name = "hhhhhhhh";
        database.InsertSlice(event);
    }
    std::vector<SlicePO> slicePOS;
    sliceTable.Select(SliceColumn::NAME).ExcuteQuery(dbPtr, slicePOS);
    EXPECT_EQ(slicePOS.size(), CACHE_SIZE);
}

TEST_F(TextTraceDatabaseMockTest, TestInsertAndQuerySliceGroupId) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    database.SetDbPtr(dbPtr);
    Trace::Slice event;
    event.name = "slice";
    event.groupId = "groupA";
    ASSERT_TRUE(database.InsertSliceList({event}));
    std::vector<SliceDto> sliceDtos;
    ASSERT_TRUE(database.QuerySliceDtoList(sliceDtos));
    ASSERT_EQ(sliceDtos.size(), 1);
    EXPECT_EQ(sliceDtos[0].groupId, "groupA");
}

TEST_F(TextTraceDatabaseMockTest, TestReplaceAllSlicesKeepGroupId) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    database.SetDbPtr(dbPtr);
    SliceDto sliceDto;
    sliceDto.name = "slice";
    sliceDto.groupId = "groupA";
    ASSERT_TRUE(database.ReplaceAllSlices({sliceDto}));
    std::vector<SliceDto> sliceDtos;
    ASSERT_TRUE(database.QuerySliceDtoList(sliceDtos));
    ASSERT_EQ(sliceDtos.size(), 1);
    EXPECT_EQ(sliceDtos[0].groupId, "groupA");
}

TEST_F(TextTraceDatabaseMockTest, TestEnsureSliceGroupIdColumnWhenOldSliceTable) {
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    const std::string oldSliceTableSql =
        "CREATE TABLE slice (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, duration INTEGER, name TEXT, "
        "depth INTEGER, track_id INTEGER, cat TEXT, args TEXT, cname TEXT, end_time INTEGER, flag_id TEXT);";
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, oldSliceTableSql);
    database.SetDbPtr(dbPtr);
    ASSERT_FALSE(database.CheckColumnExist("slice", std::string(SliceColumn::GROUPID)));
    ASSERT_TRUE(database.EnsureSliceGroupIdColumn());
    EXPECT_TRUE(database.CheckColumnExist("slice", std::string(SliceColumn::GROUPID)));
}

/**
 * text场景插入999条数据，数据库里有0条
 */
TEST_F(TextTraceDatabaseMockTest, TestInsertSliceWhenInsert999SliceThenDbHave0count)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    database.SetDbPtr(dbPtr);
    for (int i = 0; i < CACHE_SIZE - 1; ++i) {
        Trace::Slice event;
        event.name = "hhhhhhhh";
        database.InsertSlice(event);
    }
    std::vector<SlicePO> slicePOS;
    sliceTable.Select(SliceColumn::NAME).ExcuteQuery(dbPtr, slicePOS);
    EXPECT_EQ(slicePOS.size(), 0);
}

/**
 * text场景插入flow1000条数据，数据库里有1000条
 */
TEST_F(TextTraceDatabaseMockTest, TestInsertSliceWhenInsert1000FlowThenDbHave1000count)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    database.SetDbPtr(dbPtr);
    const std::string expectName = "hhhhh";
    for (int i = 0; i < CACHE_SIZE; ++i) {
        Trace::Flow event;
        event.name = expectName;
        database.InsertFlow(event);
    }
    std::vector<FlowPO> flowPOS;
    flowTable.Select(FlowColumn::NAME).ExcuteQuery(dbPtr, flowPOS);
    EXPECT_EQ(flowPOS.size(), CACHE_SIZE);
    EXPECT_EQ(flowPOS[0].name, expectName);
}

/**
 * text场景插入flow999条数据，数据库里有0条
 */
TEST_F(TextTraceDatabaseMockTest, TestInsertSliceWhenInsert999FlowThenDbHave0count)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    database.SetDbPtr(dbPtr);
    for (int i = 0; i < CACHE_SIZE - 1; ++i) {
        Trace::Flow event;
        event.name = "hhhhhhhh";
        database.InsertFlow(event);
    }
    std::vector<FlowPO> flowPOS;
    flowTable.Select(FlowColumn::NAME).ExcuteQuery(dbPtr, flowPOS);
    EXPECT_EQ(flowPOS.size(), 0);
}

/**
 * text场景插入counter1000条数据，数据库里有1000条
 */
TEST_F(TextTraceDatabaseMockTest, TestInsertSliceWhenInsert1000counterThenDbHave1000count)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    database.SetDbPtr(dbPtr);
    const std::string expectName = "hhhhh";
    for (int i = 0; i < CACHE_SIZE; ++i) {
        Trace::Counter event;
        event.name = expectName;
        database.InsertCounter(event);
    }
    std::vector<CounterPO> counterPOS;
    counterTable.Select(CounterColumn::NAME).ExcuteQuery(dbPtr, counterPOS);
    EXPECT_EQ(counterPOS.size(), CACHE_SIZE);
    EXPECT_EQ(counterPOS[0].name, expectName);
}

/**
 * text场景插入counter999条数据，数据库里有0条
 */
TEST_F(TextTraceDatabaseMockTest, TestInsertSliceWhenInsert999counterThenDbHave0count)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    database.SetDbPtr(dbPtr);
    const std::string expectName = "hhhhh";
    for (int i = 0; i < CACHE_SIZE - 1; ++i) {
        Trace::Counter event;
        event.name = expectName;
        database.InsertCounter(event);
    }
    std::vector<CounterPO> counterPOS;
    counterTable.Select(CounterColumn::NAME).ExcuteQuery(dbPtr, counterPOS);
    EXPECT_EQ(counterPOS.size(), 0);
}

/**
 * text场景修改线程名，如果未初始化返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateThreadNameWhenNotInitThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, threadTableSql);
    std::string threadDataSql = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (4, '15', '207552992', 'Plane 3', 15);";
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadDataSql);
    Trace::MetaData event;
    bool success = database.UpdateThreadName(event);
    EXPECT_EQ(success, false);
}
/**
 * text场景修改线程名，如果初始化了，但表不存在，返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateThreadNameWhenTableNotExistThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, threadTableSql);
    database.InitStmt();
    const std::string alterThreadTable = "DROP TABLE thread;";
    DatabaseTestCaseMockUtil::ExecuteSql(dbPtr, alterThreadTable);
    Trace::MetaData event;
    bool success = database.UpdateThreadName(event);
    EXPECT_EQ(success, false);
}

/**
 * text场景修改线程名，如果初始化了，表也存在啊，返回true
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateThreadNameWhenNormalThenReturnTrue)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, threadTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    std::string threadDataSql = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (4, '15', '207552992', 'Plane 3', 15);";
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadDataSql);
    database.InitStmt();
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::THREAD_NAME, ThreadColumn::THREAD_SORT_INDEX)
        .Select(ThreadColumn::PID)
        .ExcuteQuery(dbPtr, threadPOS);
    const uint64_t expectCount = 1;
    const std::string oldThreadName = "Plane 3";
    const std::string nowThreadName = "Plane 4";
    const std::string newTid = "17";
    const std::string newPid = "999";
    EXPECT_EQ(threadPOS.size(), expectCount);
    EXPECT_EQ(threadPOS[0].threadName, oldThreadName);
    Trace::MetaData event;
    event.trackId = threadPOS[0].trackId;
    event.args.name = nowThreadName;
    event.tid = newTid;
    event.pid = newPid;
    bool success = database.UpdateThreadName(event);
    EXPECT_EQ(success, true);
    threadPOS.clear();
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::THREAD_NAME, ThreadColumn::THREAD_SORT_INDEX)
        .Select(ThreadColumn::PID)
        .ExcuteQuery(dbPtr, threadPOS);
    EXPECT_EQ(threadPOS[0].threadName, nowThreadName);
    EXPECT_EQ(threadPOS[0].tid, newTid);
    EXPECT_EQ(threadPOS[0].pid, newPid);
}

/**
 * text场景修改线程顺序，如果未初始化返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateThreadSortIndexWhenNotInitThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, threadTableSql);
    database.SetDbPtr(dbPtr);
    Trace::MetaData event;
    bool success = database.UpdateThreadSortIndex(event);
    EXPECT_EQ(success, false);
}
/**
 * text场景修改线程顺序，如果初始化了，但表不存在，返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateThreadSortIndexWhenTableNotExistThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, threadTableSql);
    database.InitStmt();
    const std::string alterThreadTable = "DROP TABLE thread;";
    DatabaseTestCaseMockUtil::ExecuteSql(dbPtr, alterThreadTable);
    Trace::MetaData event;
    bool success = database.UpdateThreadSortIndex(event);
    EXPECT_EQ(success, false);
}

/**
 * text场景修改线程名，如果初始化了，表也存在啊，返回true
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateThreadSortIndexWhenNormalThenReturnTrue)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, threadTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    std::string threadDataSql = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (4, '15', '207552992', 'Plane 3', 15);";
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadDataSql);
    database.InitStmt();
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::THREAD_NAME, ThreadColumn::THREAD_SORT_INDEX)
        .Select(ThreadColumn::PID)
        .ExcuteQuery(dbPtr, threadPOS);
    const uint64_t expectCount = 1;
    const uint64_t oldSort = 15;
    const uint64_t newSort = 19;
    EXPECT_EQ(threadPOS.size(), expectCount);
    EXPECT_EQ(threadPOS[0].threadSortIndex, oldSort);
    Trace::MetaData event;
    event.trackId = threadPOS[0].trackId;
    event.args.sortIndex = newSort;
    bool success = database.UpdateThreadSortIndex(event);
    EXPECT_EQ(success, true);
    threadPOS.clear();
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::THREAD_NAME, ThreadColumn::THREAD_SORT_INDEX)
        .Select(ThreadColumn::PID)
        .ExcuteQuery(dbPtr, threadPOS);
    EXPECT_EQ(threadPOS[0].threadSortIndex, newSort);
}

/**
 * text场景修改进程名，如果未初始化返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateProcessNameWhenNotInitThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    std::string processDataSql = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('20', 'SCALARLDST', 'kkk', 20);";
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processDataSql);
    Trace::MetaData event;
    bool success = database.UpdateProcessName(event);
    EXPECT_EQ(success, false);
}

/**
 * text场景修改进程名，如果初始化了，但表不存在，返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateProcessNameWhenTableNotExistThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    database.InitStmt();
    const std::string alterThreadTable = "DROP TABLE process;";
    DatabaseTestCaseMockUtil::ExecuteSql(dbPtr, alterThreadTable);
    Trace::MetaData event;
    bool success = database.UpdateProcessName(event);
    EXPECT_EQ(success, false);
}

/**
 * text场景修改进程名，如果初始化了，表也存在啊，返回true
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateProcessNameWhenNormalThenReturnTrue)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    std::string processDataSql = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('20', 'SCALARLDST', 'kkk', 20);";
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processDataSql);
    database.InitStmt();
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
        .Select(ProcessColumn::LABEL, ProcessColumn::PROCESS_SORT_INDEX)
        .ExcuteQuery(dbPtr, processPOS);
    const uint64_t expectCount = 1;
    const std::string oldProcessName = "SCALARLDST";
    const std::string nowProcessName = "kkkkkk";
    const std::string newPid = "20";
    EXPECT_EQ(processPOS.size(), expectCount);
    EXPECT_EQ(processPOS[0].processName, oldProcessName);
    Trace::MetaData event;
    event.args.name = nowProcessName;
    event.pid = newPid;
    bool success = database.UpdateProcessName(event);
    EXPECT_EQ(success, true);
    processPOS.clear();
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
        .Select(ProcessColumn::LABEL, ProcessColumn::PROCESS_SORT_INDEX)
        .ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS[0].processName, nowProcessName);
}

/**
 * text场景修改进程label，如果未初始化返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateProcessLabelWhenNotInitThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    std::string processDataSql = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('20', 'SCALARLDST', 'kkk', 20);";
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processDataSql);
    Trace::MetaData event;
    bool success = database.UpdateProcessLabel(event);
    EXPECT_EQ(success, false);
}

/**
 * text场景修改进程label，如果初始化了，但表不存在，返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateProcessLabelWhenTableNotExistThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    database.InitStmt();
    const std::string alterThreadTable = "DROP TABLE process;";
    DatabaseTestCaseMockUtil::ExecuteSql(dbPtr, alterThreadTable);
    Trace::MetaData event;
    bool success = database.UpdateProcessLabel(event);
    EXPECT_EQ(success, false);
}

/**
 * text场景修改进程label，如果初始化了，表也存在啊，返回true
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateProcessLabelWhenNormalThenReturnTrue)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    std::string processDataSql = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('20', 'SCALARLDST', 'kkk', 20);";
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processDataSql);
    database.InitStmt();
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
        .Select(ProcessColumn::LABEL, ProcessColumn::PROCESS_SORT_INDEX)
        .ExcuteQuery(dbPtr, processPOS);
    const uint64_t expectCount = 1;
    const std::string oldProcessLable = "kkk";
    const std::string nowProcessLable = "mmmm";
    const std::string newPid = "20";
    EXPECT_EQ(processPOS.size(), expectCount);
    EXPECT_EQ(processPOS[0].label, oldProcessLable);
    Trace::MetaData event;
    event.args.labels = nowProcessLable;
    event.pid = newPid;
    bool success = database.UpdateProcessLabel(event);
    EXPECT_EQ(success, true);
    processPOS.clear();
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
        .Select(ProcessColumn::LABEL, ProcessColumn::PROCESS_SORT_INDEX)
        .ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS[0].label, nowProcessLable);
}

/**
 * text场景修改进程顺序，如果未初始化返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateProcessSortWhenNotInitThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    std::string processDataSql = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('20', 'SCALARLDST', 'kkk', 20);";
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processDataSql);
    Trace::MetaData event;
    bool success = database.UpdateProcessSortIndex(event);
    EXPECT_EQ(success, false);
}

/**
 * text场景修改进程label，如果初始化了，但表不存在，返回false
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateProcessSortWhenTableNotExistThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    database.InitStmt();
    const std::string alterThreadTable = "DROP TABLE process;";
    DatabaseTestCaseMockUtil::ExecuteSql(dbPtr, alterThreadTable);
    Trace::MetaData event;
    bool success = database.UpdateProcessSortIndex(event);
    EXPECT_EQ(success, false);
}

/**
 * text场景修改进程label，如果初始化了，表也存在啊，返回true
 */
TEST_F(TextTraceDatabaseMockTest, TestUpdateProcessSortWhenNormalThenReturnTrue)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    std::string processDataSql = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('20', 'SCALARLDST', 'kkk', 20);";
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processDataSql);
    database.InitStmt();
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
        .Select(ProcessColumn::LABEL, ProcessColumn::PROCESS_SORT_INDEX)
        .ExcuteQuery(dbPtr, processPOS);
    const uint64_t expectCount = 1;
    const uint64_t oldProcessSort = 20;
    const uint64_t nowProcessSort = 30;
    const std::string newPid = "20";
    EXPECT_EQ(processPOS.size(), expectCount);
    EXPECT_EQ(processPOS[0].processSortIndex, oldProcessSort);
    Trace::MetaData event;
    event.args.sortIndex = nowProcessSort;
    event.pid = newPid;
    bool success = database.UpdateProcessSortIndex(event);
    EXPECT_EQ(success, true);
    processPOS.clear();
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
        .Select(ProcessColumn::LABEL, ProcessColumn::PROCESS_SORT_INDEX)
        .ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS[0].processSortIndex, nowProcessSort);
}

/**
 * 算子调优场景测试CommitData，未初始化，插入失败
 */
TEST_F(TextTraceDatabaseMockTest, TestSimulationCommitDataWhenNotInitThenInsertFailed)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, threadTableSql);
    const uint64_t trackId = 1;
    Trace::ThreadEvent threadEvent;
    threadEvent.trackId = trackId;
    threadEvent.tid = "ggg";
    threadEvent.pid = "lll";
    threadEvent.threadName = "mmmm";
    threadEvent.SetThreadSortIndex();
    database.AddSimulationThreadCache(std::move(threadEvent));
    database.CommitData();
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::THREAD_NAME, ThreadColumn::THREAD_SORT_INDEX)
        .Select(ThreadColumn::PID)
        .ExcuteQuery(dbPtr, threadPOS);
    EXPECT_EQ(threadPOS.size(), 0);
}

/**
 * 算子调优场景测试CommitData，初始化了，插入成功
 */
TEST_F(TextTraceDatabaseMockTest, TestSimulationCommitDataWhenInitThenInsertSuccess)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, threadTableSql);
    const uint64_t trackId = 1;
    Trace::ThreadEvent threadEvent;
    threadEvent.trackId = trackId;
    threadEvent.tid = "ggg";
    threadEvent.pid = "lll";
    threadEvent.threadName = "mmmm";
    threadEvent.SetThreadSortIndex();
    database.InitStmt();
    database.AddSimulationThreadCache(std::move(threadEvent));
    database.CommitData();
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::THREAD_NAME, ThreadColumn::THREAD_SORT_INDEX)
        .Select(ThreadColumn::PID)
        .ExcuteQuery(dbPtr, threadPOS);
    const uint64_t expectSize = 1;
    EXPECT_EQ(threadPOS.size(), expectSize);
    EXPECT_EQ(threadPOS[0].trackId, trackId);
    EXPECT_EQ(threadPOS[0].tid, "ggg");
    EXPECT_EQ(threadPOS[0].pid, "lll");
    EXPECT_EQ(threadPOS[0].threadName, "mmmm");
    EXPECT_EQ(threadPOS[0].threadSortIndex, 0);
}

/**
 * 先对线程泳道排序，再补充线程泳道信息，数据完整
 */
TEST_F(TextTraceDatabaseMockTest, TestFirstOrderThreadThenUpdataThreadInfo)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, threadTableSql);
    database.InitStmt();
    const uint64_t trackId = 1;
    const uint64_t order = 10;
    Trace::MetaData event;
    event.trackId = trackId;
    event.args.sortIndex = order;
    database.UpdateThreadSortIndex(event);
    Trace::ThreadEvent threadEvent;
    threadEvent.trackId = trackId;
    threadEvent.tid = "ggg";
    threadEvent.pid = "lll";
    threadEvent.threadName = "mmmm";
    threadEvent.SetThreadSortIndex();
    database.AddSimulationThreadCache(std::move(threadEvent));
    database.CommitData();
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::THREAD_NAME, ThreadColumn::THREAD_SORT_INDEX)
        .Select(ThreadColumn::PID)
        .ExcuteQuery(dbPtr, threadPOS);
    const uint64_t expectSize = 1;
    EXPECT_EQ(threadPOS.size(), expectSize);
    EXPECT_EQ(threadPOS[0].trackId, trackId);
    EXPECT_EQ(threadPOS[0].tid, "ggg");
    EXPECT_EQ(threadPOS[0].pid, "lll");
    EXPECT_EQ(threadPOS[0].threadName, "mmmm");
    EXPECT_EQ(threadPOS[0].threadSortIndex, order);
}

/* *
 * 算子调优场景测试CommitData，初始化了，但表不存在，插入失败
 */
TEST_F(TextTraceDatabaseMockTest, TestSimulationCommitDataWhenInitAndTableNotExistThenInsertFailed)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, threadTableSql);
    const uint64_t trackId = 1;
    Trace::ThreadEvent threadEvent;
    threadEvent.trackId = trackId;
    threadEvent.tid = "ggg";
    threadEvent.pid = "lll";
    threadEvent.threadName = "mmmm";
    threadEvent.SetThreadSortIndex();
    database.InitStmt();
    database.AddSimulationThreadCache(std::move(threadEvent));
    const std::string alterThreadTable = "DROP TABLE thread;";
    DatabaseTestCaseMockUtil::ExecuteSql(dbPtr, alterThreadTable);
    database.CommitData();
    std::vector<ThreadPO> threadPOS;
    threadTable.Select(ThreadColumn::TRACK_ID, ThreadColumn::TID)
        .Select(ThreadColumn::THREAD_NAME, ThreadColumn::THREAD_SORT_INDEX)
        .Select(ThreadColumn::PID)
        .ExcuteQuery(dbPtr, threadPOS);
    const uint64_t expectSize = 0;
    EXPECT_EQ(threadPOS.size(), expectSize);
}

/**
 * 算子调优场景进程数据测试CommitData，未初始化，插入失败
 */
TEST_F(TextTraceDatabaseMockTest, TestSimulationCommitDataWhenProcessNotInitThenInsertFailed)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    Trace::ProcessEvent processEvent;
    processEvent.pid = "yy";
    processEvent.processName = "mm";
    database.AddSimulationProcessCache(std::move(processEvent));
    database.CommitData();
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
        .Select(ProcessColumn::LABEL, ProcessColumn::PROCESS_SORT_INDEX)
        .ExcuteQuery(dbPtr, processPOS);
    EXPECT_EQ(processPOS.size(), 0);
}

/**
 * 算子调优场景测试进程数据CommitData，初始化了，插入成功
 */
TEST_F(TextTraceDatabaseMockTest, TestSimulationCommitDataWhenProcessInitThenInsertSuccess)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    Trace::ProcessEvent processEvent;
    processEvent.pid = "yy";
    processEvent.processName = "mm";
    database.InitStmt();
    database.AddSimulationProcessCache(std::move(processEvent));
    database.CommitData();
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
        .Select(ProcessColumn::LABEL, ProcessColumn::PROCESS_SORT_INDEX)
        .ExcuteQuery(dbPtr, processPOS);
    const uint64_t expectSize = 1;
    EXPECT_EQ(processPOS.size(), expectSize);
    EXPECT_EQ(processPOS[0].processName, "mm");
    EXPECT_EQ(processPOS[0].pid, "yy");
}

/**
 * 先对进程泳道排序，再补充进程泳道信息，数据完整
 */
TEST_F(TextTraceDatabaseMockTest, TestFirstOrderProcessThenUpdataProcessInfo)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    database.InitStmt();
    const uint64_t order = 10;
    Trace::MetaData event;
    event.pid = "yy";
    event.args.sortIndex = order;
    database.UpdateProcessSortIndex(event);
    Trace::ProcessEvent processEvent;
    processEvent.pid = "yy";
    processEvent.processName = "mm";
    database.AddSimulationProcessCache(std::move(processEvent));
    database.CommitData();
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
            .Select(ProcessColumn::LABEL, ProcessColumn::PROCESS_SORT_INDEX)
            .ExcuteQuery(dbPtr, processPOS);
    const uint64_t expectSize = 1;
    EXPECT_EQ(processPOS.size(), expectSize);
    EXPECT_EQ(processPOS[0].processName, "mm");
    EXPECT_EQ(processPOS[0].pid, "yy");
    EXPECT_EQ(processPOS[0].processSortIndex, order);
}

/**
 * 算子调优场景测试进程数据CommitData，初始化了，但表不存在，插入失败
 */
TEST_F(TextTraceDatabaseMockTest, TestSimulationCommitDataWhenProcessInitAndTableNotExistThenInsertFailed)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, sliceTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, flowTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, counterTableSql);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, processSql);
    Trace::ProcessEvent processEvent;
    processEvent.pid = "yy";
    processEvent.processName = "mm";
    database.InitStmt();
    const std::string alterThreadTable = "DROP TABLE process;";
    DatabaseTestCaseMockUtil::ExecuteSql(dbPtr, alterThreadTable);
    database.AddSimulationProcessCache(std::move(processEvent));
    database.CommitData();
    std::vector<ProcessPO> processPOS;
    processTable.Select(ProcessColumn::PID, ProcessColumn::PROCESS_NAME)
        .Select(ProcessColumn::LABEL, ProcessColumn::PROCESS_SORT_INDEX)
        .ExcuteQuery(dbPtr, processPOS);
    const uint64_t expectSize = 0;
    EXPECT_EQ(processPOS.size(), expectSize);
}

/**
 * 删除多余空泳道，存在多余空泳道的情况
 */
TEST_F(TextTraceDatabaseMockTest, TestDeleteEmptyThreadWhenExistEmptyThread)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sliceSql =
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (1, 1726830796027903782, 2140, 'Computing', "
        "NULL, 1, NULL, NULL, '', 1726830796027905922, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (2, 1726830796027905942, 1880, "
        "'MulF16Tactic', NULL, 2, NULL, '{\"Model Id\":\"4294967295\",\"Task Type\":\"AI_CORE\",\"Physic Stream "
        "Id\":\"3\",\"Task Id\":\"3922\",\"Batch Id\":\"0\",\"Subtask "
        "Id\":\"4294967295\",\"connection_id\":\"64679\"}', '', 1726830796027907822, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (3, 1726830796027905942, 1880, 'Computing', "
        "NULL, 1, NULL, NULL, '', 1726830796027907822, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (4, 1726830796027905922, 20, 'Free', NULL, 3, "
        "NULL, NULL, '', 1726830796027905942, '');";
    std::string threadSql = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (1, '0', '42506633', 'Computing', NULL);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (2, '3', '42506505', 'Stream 3', 3);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (3, '3', '42506633', 'Free', NULL);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (5, '3', '42506601', 'Plane 0', 3);";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceSql);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadSql);
    database.DeleteEmptyThread();
    ThreadTable threadTable;
    std::vector<ThreadPO> result;
    threadTable.Select(ThreadColumn::TRACK_ID)
        .OrderBy(ThreadColumn::TRACK_ID, TableOrder::ASC)
        .ExcuteQuery(dbPtr, result);
    const uint64_t zero = 0;
    const uint64_t one = 1;
    const uint64_t two = 2;
    const uint64_t three = 3;
    EXPECT_EQ(result.size(), three);
    EXPECT_EQ(result[zero].trackId, one);
    EXPECT_EQ(result[one].trackId, two);
    EXPECT_EQ(result[two].trackId, three);
}

/**
 * 删除多余空泳道，不存在多余空泳道的情况
 */
TEST_F(TextTraceDatabaseMockTest, TestDeleteEmptyThreadWhenNotExistEmptyThread)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sliceSql =
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (1, 1726830796027903782, 2140, 'Computing', "
        "NULL, 1, NULL, NULL, '', 1726830796027905922, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (2, 1726830796027905942, 1880, "
        "'MulF16Tactic', NULL, 2, NULL, '{\"Model Id\":\"4294967295\",\"Task Type\":\"AI_CORE\",\"Physic Stream "
        "Id\":\"3\",\"Task Id\":\"3922\",\"Batch Id\":\"0\",\"Subtask "
        "Id\":\"4294967295\",\"connection_id\":\"64679\"}', '', 1726830796027907822, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (3, 1726830796027905942, 1880, 'Computing', "
        "NULL, 1, NULL, NULL, '', 1726830796027907822, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (4, 1726830796027905922, 20, 'Free', NULL, 3, "
        "NULL, NULL, '', 1726830796027905942, '');";
    std::string threadSql = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (1, '0', '42506633', 'Computing', NULL);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (2, '3', '42506505', 'Stream 3', 3);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (3, '3', '42506633', 'Free', NULL);";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceSql);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadSql);
    database.DeleteEmptyThread();
    ThreadTable threadTable;
    std::vector<ThreadPO> result;
    threadTable.Select(ThreadColumn::TRACK_ID)
        .OrderBy(ThreadColumn::TRACK_ID, TableOrder::ASC)
        .ExcuteQuery(dbPtr, result);
    const uint64_t zero = 0;
    const uint64_t one = 1;
    const uint64_t two = 2;
    const uint64_t three = 3;
    EXPECT_EQ(result.size(), three);
    EXPECT_EQ(result[zero].trackId, one);
    EXPECT_EQ(result[one].trackId, two);
    EXPECT_EQ(result[two].trackId, three);
}

/**
 * 删除多余空泳道，数据库未打开
 */
TEST_F(TextTraceDatabaseMockTest, TestDeleteEmptyThreadWhenDbNotOpenThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    bool result = database.DeleteEmptyThread();
    EXPECT_EQ(result, false);
}

/**
 * 删除多余空连线，数据库未打开
 */
TEST_F(TextTraceDatabaseMockTest, TestDeleteEmptyFlowWhenDbNotOpenThenReturnFalse)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    bool result = database.DeleteEmptyFlow();
    EXPECT_EQ(result, false);
}

/**
 * 删除多余空泳道，存在多余空连线的情况
 */
TEST_F(TextTraceDatabaseMockTest, TestDeleteEmptyThreadWhenExistEmptyFlow)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sliceSql =
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (3, 1726830796027905942, 1880, 'Computing', "
        "NULL, 1, NULL, NULL, '', 1726830796027907822, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (4, 1726830796027905922, 20, 'Free', NULL, 3, "
        "NULL, NULL, '', 1726830796027905942, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (5, 1726830796027907842, 5180, "
        "'DynamicQuant', NULL, 2, NULL, '{\"Model Id\":\"4294967295\",\"Task Type\":\"AI_CORE\",\"Physic Stream "
        "Id\":\"3\",\"Task Id\":\"3923\",\"Batch Id\":\"0\",\"Subtask "
        "Id\":\"4294967295\",\"connection_id\":\"64685\"}', '', 1726830796027913022, '');";
    std::string flowSql = "INSERT INTO \"main\".\"flow\" (\"id\", \"flow_id\", \"name\", \"cat\", \"track_id\", "
        "\"timestamp\", \"type\") VALUES (3, '56444740029741268992', "
        "'HostToDevice56444740029741268992', 'HostToDevice', 2, 1726830796027914803, 'f');\n"
        "INSERT INTO \"main\".\"flow\" (\"id\", \"flow_id\", \"name\", \"cat\", \"track_id\", \"timestamp\", \"type\") "
        "VALUES (4, '56466413607242956799', 'HostToDevice56466413607242956799', 'HostToDevice', 4, "
        "1726830796027921376, 's');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceSql);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, flowSql);
    database.DeleteEmptyFlow();
    FlowTable flowTable;
    std::vector<FlowPO> result;
    flowTable.Select(FlowColumn::TRACK_ID).OrderBy(FlowColumn::TRACK_ID, TableOrder::ASC).ExcuteQuery(dbPtr, result);
    const uint64_t zero = 0;
    const uint64_t one = 1;
    const uint64_t two = 2;
    EXPECT_EQ(result.size(), one);
    EXPECT_EQ(result[zero].trackId, two);
}

/**
 * 删除多余空泳道，不存在多余空连线的情况
 */
TEST_F(TextTraceDatabaseMockTest, TestDeleteEmptyThreadWhenNotExistEmptyFlow)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sliceSql =
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (3, 1726830796027905942, 1880, 'Computing', "
        "NULL, 1, NULL, NULL, '', 1726830796027907822, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (4, 1726830796027905922, 20, 'Free', NULL, 3, "
        "NULL, NULL, '', 1726830796027905942, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (5, 1726830796027907842, 5180, "
        "'DynamicQuant', NULL, 2, NULL, '{\"Model Id\":\"4294967295\",\"Task Type\":\"AI_CORE\",\"Physic Stream "
        "Id\":\"3\",\"Task Id\":\"3923\",\"Batch Id\":\"0\",\"Subtask "
        "Id\":\"4294967295\",\"connection_id\":\"64685\"}', '', 1726830796027913022, '');";
    std::string flowSql = "INSERT INTO \"main\".\"flow\" (\"id\", \"flow_id\", \"name\", \"cat\", \"track_id\", "
        "\"timestamp\", \"type\") VALUES (3, '56444740029741268992', "
        "'HostToDevice56444740029741268992', 'HostToDevice', 2, 1726830796027914803, 'f');\n"
        "INSERT INTO \"main\".\"flow\" (\"id\", \"flow_id\", \"name\", \"cat\", \"track_id\", \"timestamp\", \"type\") "
        "VALUES (4, '56466413607242956799', 'HostToDevice56466413607242956799', 'HostToDevice', 3, "
        "1726830796027921376, 's');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceSql);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, flowSql);
    database.DeleteEmptyFlow();
    FlowTable flowTable;
    std::vector<FlowPO> result;
    flowTable.Select(FlowColumn::TRACK_ID).OrderBy(FlowColumn::TRACK_ID, TableOrder::ASC).ExcuteQuery(dbPtr, result);
    const uint64_t zero = 0;
    const uint64_t one = 1;
    const uint64_t two = 2;
    const uint64_t three = 3;
    EXPECT_EQ(result.size(), two);
    EXPECT_EQ(result[zero].trackId, two);
    EXPECT_EQ(result[one].trackId, three);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryP2PCommunicationOpDataWhenDbNotOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    const std::string rankId;
    const uint64_t offset = 9;
    Dic::Protocol::ExtremumTimestamp range;
    std::vector<Dic::Protocol::ThreadTraces> p2pOpData;
    bool result = database.QueryP2PCommunicationOpData(rankId, offset, range, p2pOpData);
    EXPECT_EQ(result, false);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryFusibleOpDataWhenDbNotOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    const Dic::Protocol::KernelDetailsParams params;
    std::vector<Dic::Module::Timeline::FuseableOpRule> rule;
    Protocol::OperatorFusionResBody resBody;
    const uint64_t minTimestamp = 9;
    rule.push_back({{"19"}, "", ""});
    bool result = database.QueryFusibleOpData(params, rule, resBody, minTimestamp);
    EXPECT_EQ(result, false);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryFusibleOpDataWhenDbOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    DatabaseTestCaseMockUtil::CreateTable(dbPtr, kernelSql);
    std::string kerData =
        "INSERT INTO \"main\".\"kernel_detail\" (\"id\", \"deviceId\", \"step_id\", \"name\", \"op_type\", "
        "\"accelerator_core\", \"start_time\", \"duration\", \"wait_time\", \"block_dim\", \"input_shapes\", "
        "\"input_data_types\", \"input_formats\", \"output_shapes\", \"output_data_types\", \"output_formats\", "
        "\"aicore_time_us_\", \"aic_total_cycles\", \"aic_mac_time_us_\", \"aic_mac_ratio\", \"aic_scalar_time_us_\", "
        "\"aic_scalar_ratio\", \"aic_mte1_time_us_\", \"aic_mte1_ratio\", \"aic_mte2_time_us_\", \"aic_mte2_ratio\", "
        "\"aic_fixpipe_time_us_\", \"aic_fixpipe_ratio\", \"aic_icache_miss_rate\", \"aiv_time_us_\", "
        "\"aiv_total_cycles\", \"aiv_vec_time_us_\", \"aiv_vec_ratio\", \"aiv_scalar_time_us_\", \"aiv_scalar_ratio\", "
        "\"aiv_mte2_time_us_\", \"aiv_mte2_ratio\", \"aiv_mte3_time_us_\", \"aiv_mte3_ratio\", "
        "\"aiv_icache_miss_rate\", \"cube_utilization_PCT_\") VALUES (1, '11', '', "
        "'DynamicQuant', 'Transpose', 'AI_VECTOR_CORE', 1726830796027907842, 169.927, 0, 48, "
        "'\"6656,4992,1;3\"', 'INT8;INT64', 'ND;ND', '\"4992,6656,1\"', 'INT8', 'ND', '0.0', '0', '0.0', '0.0', '0.0', "
        "'0.0', '0.0', '0.0', '0.0', '0.0', '0.0', '0.0', '0.0', '59.62', '5151508', '28.259', '0.474', '12.263', "
        "'0.206', '14.739', '0.247', '11.443', '0.192', '0.01', '0');";
    std::string sliceData =
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (5, 1726830796027907842, 5180, "
        "'DynamicQuant', NULL, 2, NULL, '{\"Model Id\":\"4294967295\",\"Task Type\":\"AI_CORE\",\"Physic Stream "
        "Id\":\"3\",\"Task Id\":\"3923\",\"Batch Id\":\"0\",\"Subtask "
        "Id\":\"4294967295\",\"connection_id\":\"64685\"}', '', 1726830796027913022, '');";
    std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (2, '41725', '42506313', 'Thread 41725', 41725);";
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    DatabaseTestCaseMockUtil::InsertData(dbPtr, kerData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    Dic::Protocol::KernelDetailsParams params;
    std::vector<Dic::Module::Timeline::FuseableOpRule> rule;
    Protocol::OperatorFusionResBody resBody;
    const uint64_t minTimestamp = 9;
    rule.push_back({{"Transpose"}, "", ""});
    params.orderBy = "name";
    params.order = "DESC";
    bool result = database.QueryFusibleOpData(params, rule, resBody, minTimestamp);
    EXPECT_EQ(result, true);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryHostInfo)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    std::string result = database.QueryHostInfo();
    EXPECT_EQ(std::empty(result), true);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryFwdBwdDataByFlowWhenDbNotOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    const std::string rankId;
    const uint64_t offset = 0;
    const Dic::Protocol::ExtremumTimestamp range;
    std::vector<Dic::Protocol::ThreadTraces> fwdBwdData;
    bool result = database.QueryFwdBwdDataByFlow(rankId, offset, range, fwdBwdData);
    EXPECT_EQ(result, false);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryFwdBwdDataByFlowWhenDbOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sliceData =
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (1, 1726830772807463934, 2140, 'Computing', "
        "NULL, 34, 'cpu_op', NULL, '', 1726830772807464000, '');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (2, 1726830772803060779, 1880, "
        "'MulF16Tactic', NULL, 51, 'cpu_op', '{\"Model Id\":\"4294967295\",\"Task Type\":\"AI_CORE\",\"Physic Stream "
        "Id\":\"3\",\"Task Id\":\"3922\",\"Batch Id\":\"0\",\"Subtask "
        "Id\":\"4294967295\",\"connection_id\":\"64679\"}', '', 1726830796027907822, '');";
    std::string flowData = "INSERT INTO \"main\".\"flow\" (\"id\", \"flow_id\", \"name\", \"cat\", \"track_id\", "
        "\"timestamp\", \"type\") VALUES (34353, '100205096003960831', "
        "'HostToDevice100205096003960831', 'fwdbwd', 34, 1726830772807463934, 'f');\n"
        "INSERT INTO \"main\".\"flow\" (\"id\", \"flow_id\", \"name\", \"cat\", \"track_id\", \"timestamp\", \"type\") "
        "VALUES (34354, '100205096003960831', 'HostToDevice100205096003960831', 'fwdbwd', 51, "
        "1726830772803060779, 's');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, flowData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceData);
    const std::string rankId;
    const uint64_t offset = 0;
    Dic::Protocol::ExtremumTimestamp range;
    const uint64_t st = 1626830772803060779;
    const uint64_t en = 1826830772803060779;
    range.minTimestamp = st;
    range.maxTimestamp = en;
    std::vector<Dic::Protocol::ThreadTraces> fwdBwdData;
    bool result = database.QueryFwdBwdDataByFlow(rankId, offset, range, fwdBwdData);
    EXPECT_EQ(fwdBwdData.size(), 0);
    EXPECT_EQ(result, false);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryAffinityAPIDataWhenDbNotOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    const Dic::Protocol::KernelDetailsParams params;
    const std::set<std::string> pattern;
    const uint64_t minTimestamp = 0;
    std::map<uint64_t, std::vector<Dic::Protocol::FlowLocation>> data;
    std::map<uint64_t, std::vector<uint32_t>> indexes;
    bool result = database.QueryAffinityAPIData(params, pattern, minTimestamp, data, indexes);
    EXPECT_EQ(result, false);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryAclnnOpCountExceedThresholdWhenDbNotOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    const Dic::Protocol::KernelDetailsParams params;
    const uint64_t threshold = 10000;
    std::vector<Dic::Protocol::KernelBaseInfo> data;
    const uint64_t minTimestamp = 10000;
    bool result = database.QueryAclnnOpCountExceedThreshold(params, threshold, data, minTimestamp);
    EXPECT_EQ(result, false);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryAclnnOpCountExceedThresholdWhenDbOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sliceData =
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (9, 1726830796027914803, 48342, "
        "'aclnnQuantMatmulV4_QuantBatchMatmulV3_QuantBatchMatmulV3', NULL, 2, NULL, '{\"Model "
        "Id\":\"4294967295\",\"Task Type\":\"MIX_AIC\",\"Physic Stream Id\":\"3\",\"Task Id\":\"3924\",\"Batch "
        "Id\":\"0\",\"Subtask Id\":\"0\",\"connection_id\":\"64693\"}', '', 1726830796027963145, '');";
    std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (2, '4', '42506507', 'Stream 4', 4);";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceData);
    Dic::Protocol::KernelDetailsParams params;
    params.orderBy = "name";
    params.order = "DESC";
    const uint64_t threshold = 1;
    std::vector<Dic::Protocol::KernelBaseInfo> data;
    const uint64_t minTimestamp = 10000;
    bool result = database.QueryAclnnOpCountExceedThreshold(params, threshold, data, minTimestamp);
    EXPECT_EQ(result, true);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryStepDurationWhenDbNotOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    const std::string stepId;
    const uint64_t emin = 1000;
    const uint64_t emax = 10000;
    uint64_t min = emin;
    uint64_t max = emax;
    bool result = database.QueryStepDuration(stepId, min, max);
    EXPECT_EQ(result, false);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryStepDurationWhenDbOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sliceData =
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (5, 1726830796027907842, 5180, "
        "'ProfilerStep#5', NULL, 2, NULL, '{\"Model Id\":\"4294967295\",\"Task Type\":\"AI_CORE\",\"Physic Stream "
        "Id\":\"3\",\"Task Id\":\"3923\",\"Batch Id\":\"0\",\"Subtask "
        "Id\":\"4294967295\",\"connection_id\":\"64685\"}', '', 1726830796027913022, '');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceData);
    const std::string stepId = "5";
    const uint64_t emin = 1000;
    const uint64_t emax = 10000;
    uint64_t min = emin;
    uint64_t max = emax;
    bool result = database.QueryStepDuration(stepId, min, max);
    EXPECT_EQ(result, true);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryGroupedAscendHardwareThreads_InvalidModelId)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (54, '2', '3513236896', 'Stream 2', 2), (55, '6', '3513236896', 'Stream 6', 6),"
        " (56, '3', '3513236896', 'Stream 3', 3);";
    std::string sliceData = "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\","
        " \"track_id\", \"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES"
        " (26553, 1764246877086724740, 10160, 'aclnnMuls_MulAiCore_Mul', NULL, 54, NULL, "
        "'{\"Model Id\":\"4294967295\",\"Task Type\":\"AI_VECTOR_CORE\"}', '', 1764246877084183180, ''),"
        " (26494, 1764246877076775260, 20, 'EVENT_WAIT', NULL, 55, NULL, "
        "'{\"Model Id\":\"4294967295\",\"Task Type\":\"AI_VECTOR_CORE\"}', '', 1764246877084183180, ''),"
        " (26537, 1764246877083361500, 821680, 'EVENT_WAIT', NULL, 56, NULL, "
        "'{\"Model Id\":\"4294967295\",\"Task Type\":\"AI_VECTOR_CORE\"}', '', 1764246877084183180, '');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceData);
    const std::string fileId = "9";
    std::vector<Dic::Protocol::ThreadGroup> groups;
    const bool result = database.QueryGroupedAscendHardwareThreadsByModelId(groups);
    EXPECT_EQ(result, true);
    EXPECT_EQ(groups.size(), 0);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryGroupedAscendHardwareThreads_ValidModelId)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (54, '2', '3513236896', 'Stream 2', 2), (55, '6', '3513236896', 'Stream 6', 6),"
        " (56, '3', '3513236896', 'Stream 3', 3);";
    std::string sliceData = "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\","
        " \"track_id\", \"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES"
        " (26553, 1764246877086724740, 10160, 'aclnnMuls_MulAiCore_Mul', NULL, 54, NULL, "
        "'{\"Model Id\":\"1\",\"Task Type\":\"AI_VECTOR_CORE\"}', '', 1764246877084183180, ''),"
        " (26494, 1764246877076775260, 20, 'EVENT_WAIT', NULL, 55, NULL, "
        "'{\"Model Id\":\"1\",\"Task Type\":\"AI_VECTOR_CORE\"}', '', 1764246877084183180, ''),"
        " (26537, 1764246877083361500, 821680, 'EVENT_WAIT', NULL, 56, NULL, "
        "'{\"Model Id\":\"2\",\"Task Type\":\"AI_VECTOR_CORE\"}', '', 1764246877084183180, '');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceData);
    const std::string fileId = "9";
    std::vector<Dic::Protocol::ThreadGroup> groups;
    const bool result = database.QueryGroupedAscendHardwareThreadsByModelId(groups);
    EXPECT_EQ(result, true);
    ASSERT_EQ(groups.size(), 2);
    EXPECT_EQ(groups[0].threadIds.size(), 2);
    EXPECT_EQ(groups[0].threadIds[0], "2");
    EXPECT_EQ(groups[0].threadIds[1], "6");
    EXPECT_EQ(groups[1].threadIds.size(), 1);
    EXPECT_EQ(groups[1].threadIds[0], "3");
}

TEST_F(TextTraceDatabaseMockTest, TestQueryUnitsMetadatanWhenDbNotOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    const std::string fileId = "9";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    bool result = database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(result, true);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryUnitsMetadatanWhenDbOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string processData = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('42506507', 'Ascend Hardware', 'NPU', 8);\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\") VALUES "
        "('42506731', 'HCCL', 'NPU', 15);\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\") VALUES "
        "('42506539', 'AI Core Freq', 'NPU', 9);";
    std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (23, '3', '42506507', 'Stream 3', 3);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (39, '1', '42506731', 'Plane 0', 1);";
    std::string counterData =
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (6, 'AI "
        "Core Freq', '42506539', 1726830775547610426, NULL, '{\"MHz\":\"1800\"}');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, counterData);
    const std::string fileId = "9";
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    bool result = database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(result, true);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryUnitsMetadataWithGroupNameValueWhenDbOpen)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    const std::string processData = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('42506507', 'Ascend Hardware', 'NPU', 8);\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\") VALUES "
        "('42506731', 'HCCL', 'NPU', 15);\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\") VALUES "
        "('42506539', 'AI Core Freq', 'NPU', 9);";
    const std::string groupNameValue = "90.90.97.96%enp194s0f0_60008_8_1735556595505601";
    const std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (23, '3', '42506507', 'Stream 3', 3);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (39, '1', '42506731', 'Group " +
        groupNameValue + " Communication', 1);";
    const std::string counterData =
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (6, 'AI "
        "Core Freq', '42506539', 1726830775547610426, NULL, '{\"MHz\":\"1800\"}');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, counterData);
    const std::string fileId = "9";
    const uint8_t expectProcessCount = 3;
    const uint8_t first = 0;
    const uint8_t second = 1;
    const uint8_t third = 2;
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    bool result = database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(result, true);
    EXPECT_EQ(metaData.size(), expectProcessCount);
    EXPECT_EQ(metaData[third]->children.size(), second);
    EXPECT_EQ(metaData[third]->children[first]->metaData.groupNameValue, groupNameValue);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryUnitsMetadataWithCounter)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    const std::string processData = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('1', '1', NULL, NULL);\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\") VALUES "
        "('319667', '319667', NULL, NULL);";
    const std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (1, 'http', '319667', 'http', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (16, 'CPU Usage', '1', 'CPU Usage', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (17, 'NPU Usage', '1', 'NPU Usage', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (18, 'KVCache', '319667', 'KVCache', 0);";
    const std::string counterData =
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (3749, "
        "'KVCache', '319667', 1735124813464727800, NULL, '{\"Device Block\":\"1969.0\"}');"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (749, "
        "'NPU Usage', '1', 1735124807612323800, NULL, '{\"Usage\":\"0.0\"}');"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (1, "
        "'CPU Usage', '1', 1735124784269897500, NULL, '{\"CPU Usage\":\"0.520833\"}');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, counterData);
    const std::string fileId = "9";
    const uint8_t expectProcessCount = 2;
    const uint8_t first = 0;
    const uint8_t second = 1;
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    bool result = database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(result, true);
    EXPECT_EQ(metaData.size(), expectProcessCount);
    EXPECT_EQ(metaData[first]->type, "process");
    EXPECT_EQ(metaData[second]->type, "process");
    EXPECT_EQ(metaData[first]->children.size(), expectProcessCount);
    EXPECT_EQ(metaData[first]->children[first]->type, "counter");
    EXPECT_EQ(metaData[first]->children[second]->type, "counter");
    EXPECT_EQ(metaData[second]->children[first]->type, "counter");
    EXPECT_EQ(metaData[second]->children[second]->type, "thread");
}

TEST_F(TextTraceDatabaseMockTest, TestQueryUnitsMetadataWithCounterInvalidJSON)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    const std::string processData = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('1', '1', NULL, NULL);";
    const std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\","
        " \"thread_sort_index\") VALUES (17, 'NPU Usage', '1', 'NPU Usage', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\","
        " \"thread_sort_index\") VALUES (18, 'CPU Usage', '1', 'CPU Usage', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\","
        " \"thread_sort_index\") VALUES (19, 'CPU1 Usage', '1', 'CPU1 Usage', 0);";
    const std::string counterData = "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\","
        " \"cat\", \"args\") VALUES (749, 'NPU Usage', '1', 1735124807612323800, NULL, '');\n" // counter args 为空
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\","
        " \"cat\", \"args\") VALUES (750, 'CPU Usage', '1', 1735124807612323800, NULL, '{1:\"0.0\"}');\n"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\","
        " \"cat\", \"args\") VALUES (751, 'CPU1 Usage', '1', 1735124807612323800, NULL, '[1,\"0.0\"]');"; // counter args 中 key 不为字符串
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, counterData);
    const std::string fileId = "9";
    const uint8_t first = 0;
    const std::vector<std::string> expectedDataType = {};
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    bool result = database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(result, true);
    EXPECT_EQ(metaData[first]->type, "process");
    EXPECT_EQ(metaData[first]->children.size(), 3);
    EXPECT_EQ(metaData[first]->children[0]->metaData.dataType, expectedDataType);
    EXPECT_EQ(metaData[first]->children[1]->metaData.dataType, expectedDataType);
    EXPECT_EQ(metaData[first]->children[2]->metaData.dataType, expectedDataType);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryUnitsMetadataWithPidAndProcessNameIsSame)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    const std::string processData = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('1', '1', NULL, NULL);\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\") VALUES "
        "('319667', '319667', NULL, NULL);";
    const std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (1, 'http', '319667', 'http', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (16, 'CPU Usage', '1', 'CPU Usage', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (17, 'NPU Usage', '1', 'NPU Usage', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (18, 'KVCache', '319667', 'KVCache', 0);";
    const std::string counterData =
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (3749, "
        "'KVCache', '319667', 1735124813464727800, NULL, '{\"Device Block\":\"1969.0\"}');"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (749, "
        "'NPU Usage', '1', 1735124807612323800, NULL, '{\"Usage\":\"0.0\"}');"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (1, "
        "'CPU Usage', '1', 1735124784269897500, NULL, '{\"CPU Usage\":\"0.520833\"}');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, counterData);
    const std::string fileId = "9";
    const uint8_t expectProcessCount = 2;
    const uint8_t first = 0;
    const uint8_t second = 1;
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    bool result = database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(result, true);
    EXPECT_EQ(metaData.size(), expectProcessCount);
    EXPECT_EQ(metaData[first]->type, "process");
    EXPECT_EQ(metaData[first]->metaData.processName, "1");
    EXPECT_EQ(metaData[second]->type, "process");
    EXPECT_EQ(metaData[second]->metaData.processName, "319667");
}

TEST_F(TextTraceDatabaseMockTest, TestQueryUnitsMetadataWithMutiLayerProcess)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    const std::string processData =
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\", "
        "\"parentPid\") VALUES ('259836', '259836', 'ubuntu2204', 3, 'ubuntu2204');\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\", "
        "\"parentPid\") VALUES ('260039', '260039', 'ubuntu2204,dp0', 4, 'dp0');\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\", "
        "\"parentPid\") VALUES ('260041', '260041', 'ubuntu2204,dp0', 5, 'dp0');\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\", "
        "\"parentPid\") VALUES ('1', '1', NULL, NULL, '0');INSERT INTO \"main\".\"process\" (\"pid\", "
        "\"process_name\", \"label\", \"process_sort_index\", \"parentPid\") VALUES ('ubuntu2204', 'ubuntu2204', NULL, "
        "1, '0');\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\", "
        "\"parentPid\") VALUES ('dp0', 'dp0', NULL, 2, 'ubuntu2204');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processData);
    const std::string fileId = "9";
    const uint8_t first = 0;
    const uint8_t second = 1;
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    bool result = database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(result, true);
    EXPECT_EQ(metaData.size(), 2); // 2
    EXPECT_EQ(metaData[first]->type, "process");
    EXPECT_EQ(metaData[first]->metaData.processName, "1");
    EXPECT_EQ(metaData[second]->type, "process");
    EXPECT_EQ(metaData[second]->metaData.processName, "ubuntu2204");
    EXPECT_EQ(metaData[second]->children[first]->metaData.processName, "dp0");
    EXPECT_EQ(metaData[second]->children[first]->children[first]->metaData.processName, "260039");
    EXPECT_EQ(metaData[second]->children[first]->children[second]->metaData.processName, "260041");
    EXPECT_EQ(metaData[second]->children[second]->metaData.processName, "259836");
}

TEST_F(TextTraceDatabaseMockTest, TestQueryUnitsMetadataWithPidAndProcessNameIsNotSame)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    const std::string processData = "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", "
        "\"process_sort_index\") VALUES ('1', 'wwf', NULL, NULL);\n"
        "INSERT INTO \"main\".\"process\" (\"pid\", \"process_name\", \"label\", \"process_sort_index\") VALUES "
        "('319667', 'nnm', NULL, NULL);";
    const std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (1, 'http', '319667', 'http', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (16, 'CPU Usage', '1', 'CPU Usage', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (17, 'NPU Usage', '1', 'NPU Usage', 0);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (18, 'KVCache', '319667', 'KVCache', 0);";
    const std::string counterData =
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (3749, "
        "'KVCache', '319667', 1735124813464727800, NULL, '{\"Device Block\":\"1969.0\"}');"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (749, "
        "'NPU Usage', '1', 1735124807612323800, NULL, '{\"Usage\":\"0.0\"}');"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (1, "
        "'CPU Usage', '1', 1735124784269897500, NULL, '{\"CPU Usage\":\"0.520833\"}');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, processData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, counterData);
    const std::string fileId = "9";
    const uint8_t expectProcessCount = 2;
    const uint8_t first = 0;
    const uint8_t second = 1;
    std::vector<std::unique_ptr<Dic::Protocol::UnitTrack>> metaData;
    bool result = database.QueryUnitsMetadata(fileId, metaData);
    EXPECT_EQ(result, true);
    EXPECT_EQ(metaData.size(), expectProcessCount);
    EXPECT_EQ(metaData[first]->type, "process");
    EXPECT_EQ(metaData[first]->metaData.processName, "nnm (319667)");
    EXPECT_EQ(metaData[second]->type, "process");
    EXPECT_EQ(metaData[second]->metaData.processName, "wwf (1)");
}

TEST_F(TextTraceDatabaseMockTest, TestQuerySimulationUintFlows)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sliceData = "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", "
        "\"track_id\", \"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (24463, "
        "31081, 0, 'SET_FLAG', NULL, 15, NULL, '', 'thread_state_running', 31081, '99');\n"
        "INSERT INTO \"main\".\"slice\" (\"id\", \"timestamp\", \"duration\", \"name\", \"depth\", \"track_id\", "
        "\"cat\", \"args\", \"cname\", \"end_time\", \"flag_id\") VALUES (24674, 31249, 0, 'WAIT_FLAG', NULL, 16, "
        "NULL, '', 'thread_state_iowait', 31249, '99');";
    std::string threadData = "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", "
        "\"thread_sort_index\") VALUES (15, 'MTE1', 'core0.cubecore0', 'MTE1', 3);\n"
        "INSERT INTO \"main\".\"thread\" (\"track_id\", \"tid\", \"pid\", \"thread_name\", \"thread_sort_index\") "
        "VALUES (16, 'MTE2', 'core0.cubecore0', 'MTE2', 6);";
    std::string flowData = "INSERT INTO \"main\".\"flow\" (\"id\", \"flow_id\", \"name\", \"cat\", \"track_id\", "
        "\"timestamp\", \"type\") VALUES (769, '99', 'flow', 'MTE1ToMTE2', 15, 31081, 's');\n"
        "INSERT INTO \"main\".\"flow\" (\"id\", \"flow_id\", \"name\", \"cat\", \"track_id\", \"timestamp\", \"type\") "
        "VALUES (770, '99', 'flow', 'MTE1ToMTE2', 16, 31249, 't');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sliceData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, threadData);
    DatabaseTestCaseMockUtil::InsertData(dbPtr, flowData);
    Dic::Protocol::UnitFlowsParams requestParams;
    requestParams.isSimulation = true;
    requestParams.id = "24463";
    Dic::Protocol::UnitFlowsBody responseBody;
    const uint64_t minTimestamp = 0;
    const uint64_t trackId = 15;
    bool result = database.QueryUnitFlows(requestParams, responseBody, minTimestamp, trackId);
    EXPECT_EQ(result, true);
}

TEST_F(TextTraceDatabaseMockTest, TestGetTableList)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::vector<std::string> tableList;
    database.GetTableList(tableList);
    const uint64_t expectSize = 5;
    EXPECT_EQ(tableList.size(), expectSize);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryCounter)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3 *dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string flowData =
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (14375, "
        "'APP/Memory', '662921300', 1695375189320960800, NULL, '{\"KB\":\"18167880.0\"}');\n"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (14393, "
        "'APP/Memory', '662921300', 1695375189340960800, NULL, '{\"KB\":\"18167880.0\"}');\n"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (14585, "
        "'APP/Memory', '662921300', 1695375189360960800, NULL, '{\"KB\":\"18167880.0\"}');\n"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (14603, "
        "'APP/Memory', '662921300', 1695375189380961800, NULL, '{\"KB\":\"18167880.0\"}');\n"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (14795, "
        "'APP/Memory', '662921300', 1695375189400962800, NULL, '{\"KB\":\"18167880.0\"}');\n"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (14813, "
        "'APP/Memory', '662921300', 1695375189420961800, NULL, '{\"KB\":\"18167880.0\"}');\n"
        "INSERT INTO \"main\".\"counter\" (\"id\", \"name\", \"pid\", \"timestamp\", \"cat\", \"args\") VALUES (14831, "
        "'APP/Memory', '662921300', 1695375189440961800, NULL, '{\"KB\":\"18167880.0\"}');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, flowData);
    Dic::Protocol::UnitCounterParams params;
    const uint64_t minTimestamp = 0;
    std::vector<Dic::Protocol::UnitCounterData> dataList;
    bool emptyResult = database.QueryUnitCounter(params, minTimestamp, dataList);
    EXPECT_EQ(emptyResult, true);
    EXPECT_EQ(dataList.empty(), true);
    params.pid = "662921300";
    params.threadName = "APP/Memory";
    const uint64_t start = 1595375189320960800;
    const uint64_t end = 1795375189320960800;
    params.startTime = start;
    params.endTime = end;
    bool result = database.QueryUnitCounter(params, minTimestamp, dataList);
    EXPECT_EQ(result, true);
    const uint64_t expectSize = 2;
    EXPECT_EQ(dataList.size(), expectSize);
}

TEST_F(TextTraceDatabaseMockTest, TestQueryTableDataNameListNormal)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3* dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sqlTable = "CREATE TABLE \"data_table\" (\n"
                           "\"id\" INTEGER NOT NULL,\n"
                           "\"name\" TEXT,\n"
                           "\"view_name\" TEXT,\n"
                           "PRIMARY KEY (\"id\")\n"
                           ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sqlTable);
    std::string sqlData =
        "INSERT INTO \"main\".\"data_table\" (\"id\", \"name\", \"view_name\") VALUES (1, 'batch', 'batch info');\n"
        "INSERT INTO \"main\".\"data_table\" (\"id\", \"name\", \"view_name\") VALUES (2, 'kvcache', 'kvcache');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sqlData);
    auto res = database.QueryTableDataNameList();
    const uint64_t expectSize = 2;
    EXPECT_EQ(res.size(), expectSize);
    EXPECT_EQ(res.front().first, "batch info");
    EXPECT_EQ(res.back().second, "kvcache");
}

TEST_F(TextTraceDatabaseMockTest, TestQueryTableDataNameListErr)
{
    std::recursive_mutex sqlMutex;
    MockDatabase database(sqlMutex);
    sqlite3* dbPtr = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(dbPtr);
    database.SetDbPtr(dbPtr);
    database.CreateTable();
    std::string sqlTable = "CREATE TABLE \"data_table2\" (\n"
                           "\"id\" INTEGER NOT NULL,\n"
                           "\"name\" TEXT,\n"
                           "\"view_name\" TEXT,\n"
                           "PRIMARY KEY (\"id\")\n"
                           ");";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sqlTable);
    std::string sqlData =
        "INSERT INTO \"main\".\"data_table2\" (\"id\", \"name\", \"view_name\") VALUES (1, 'batch', 'batch info');\n"
        "INSERT INTO \"main\".\"data_table2\" (\"id\", \"name\", \"view_name\") VALUES (2, 'kvcache', 'kvcache');";
    DatabaseTestCaseMockUtil::InsertData(dbPtr, sqlData);
    auto res = database.QueryTableDataNameList();
    const uint64_t expectSize = 0;
    EXPECT_EQ(res.size(), expectSize);
}

TEST_F(TextTraceDatabaseMockTest, ProcessByteAlignmentAnalyzerDataForTextTest)
{
    std::vector<Dic::Module::CommunicationLargeOperatorInfo> result;
    std::vector<std::pair<std::string, std::string>> rawData;
    const std::string argsStringMemcpy = "{\"notify_id\": 1.8446744073709552e+19,"
        " \"duration estimated(us)\": 0.6020725388601036, \"stream id\": 22, \"task id\": 224, \"context id\": 0,"
        " \"task type\": \"Memcpy\", \"src rank\": 0, \"dst rank\": 0, \"transport type\": \"SDMA\","
        " \"size(Byte)\": \"40\", \"data type\": \"INVALID_TYPE\", \"link type\": \"ON_CHIP\","
        " \"bandwidth(GB/s)\": 0.04082706706962131}";
    const std::string argsStringReduceInline = "{\"notify_id\": 0, \"duration estimated(us)\": 1087.2072538860102,"
        " \"stream id\": 22, \"task id\": 234, \"context id\": 15, \"task type\": \"Reduce_Inline\", \"src rank\": 0,"
        " \"dst rank\": 1, \"transport type\": \"SDMA\", \"size(Byte)\": \"20971520\", \"data type\": \"INT8\","
        " \"link type\": \"HCCS\", \"bandwidth(GB/s)\": 18.109199702033724}";
    rawData.push_back({"Memcpy", argsStringMemcpy});
    rawData.push_back({"Reduce_Inline", argsStringReduceInline});
    rawData.push_back({"hcom_broadcast__008_1_1", ""});
    rawData.push_back({"Memcpy", argsStringMemcpy});
    rawData.push_back({"Reduce_Inline", argsStringReduceInline});
    rawData.push_back({"hcom_broadcast__008_1_2", ""});
    rawData.push_back({"Memcpy", argsStringMemcpy});
    rawData.push_back({"Reduce_Inline", argsStringReduceInline});
    rawData.push_back({"Reduce_Inline", argsStringReduceInline});
    rawData.push_back({"hcom_broadcast__008_1_3", ""});
    rawData.push_back({"hcom_broadcast__008_1_4", ""});
    rawData.push_back({"Memcpy", argsStringMemcpy});
    rawData.push_back({"Memcpy", argsStringMemcpy});
    rawData.push_back({"Memcpy", argsStringMemcpy});
    TextTraceDatabase::ProcessByteAlignmentAnalyzerDataForText(result, rawData);
    const int forty = 40;
    const int bigNumber = 20971520;
    ASSERT_EQ(result.size(), 4); // 4
    EXPECT_EQ(result[0].name, "hcom_broadcast__008_1_1");
    ASSERT_EQ(result[0].memcpyTasks.size(), 1);
    EXPECT_EQ(result[0].memcpyTasks[0].size, forty);
    ASSERT_EQ(result[0].reduceInlineTasks.size(), 1);
    EXPECT_EQ(result[0].reduceInlineTasks[0].size, bigNumber);
    EXPECT_EQ(result[1].name, "hcom_broadcast__008_1_2");
    ASSERT_EQ(result[1].memcpyTasks.size(), 1);
    EXPECT_EQ(result[1].memcpyTasks[0].size, forty);
    ASSERT_EQ(result[1].reduceInlineTasks.size(), 2); // 2
    EXPECT_EQ(result[1].reduceInlineTasks[0].size, bigNumber);
    EXPECT_EQ(result[1].reduceInlineTasks[1].size, bigNumber);
    EXPECT_EQ(result[2].name, "hcom_broadcast__008_1_3"); // 2
    ASSERT_EQ(result[2].memcpyTasks.size(), 0); // 2
    ASSERT_EQ(result[2].reduceInlineTasks.size(), 0); // 2
    EXPECT_EQ(result[3].name, "hcom_broadcast__008_1_4"); // 3
    ASSERT_EQ(result[3].memcpyTasks.size(), 3); // 3
    EXPECT_EQ(result[3].memcpyTasks[0].size, forty); // 3
    EXPECT_EQ(result[3].memcpyTasks[1].size, forty); // 3
    ASSERT_EQ(result[3].reduceInlineTasks.size(), 0);
}

TEST_F(TextTraceDatabaseMockTest, ProcessByteAlignmentAnalyzerDataForTextTestInvalidJson)
{
    std::vector<Dic::Module::CommunicationLargeOperatorInfo> result;
    std::vector<std::pair<std::string, std::string>> rawData;
    const std::string argsStringMemcpy = "{\"notify_id\": 1.8446744073709552e+19,"
        " \"duration estimated(us)\": 0.6020725388601036, \"stream id\": 22, \"task id\": 224, \"context id\": 0,"
        " \"task type\": \"Memcpy\", \"src rank\": 0, \"dst rank\": 0, \"transport type\": \"SDMA\","
        " \"data type\": \"INVALID_TYPE\", \"link type\": \"ON_CHIP\","
        " \"bandwidth(GB/s)\": 0.04082706706962131}";
    rawData.push_back({"hcom_broadcast__008_1_1", ""});
    rawData.push_back({"Memcpy", argsStringMemcpy});
    rawData.push_back({"Memcpy", argsStringMemcpy});
    rawData.push_back({"Memcpy", argsStringMemcpy});
    TextTraceDatabase::ProcessByteAlignmentAnalyzerDataForText(result, rawData);
    ASSERT_EQ(result.size(), 1);
    ASSERT_EQ(result[0].memcpyTasks.size(), 3); // 3
    ASSERT_EQ(result[0].reduceInlineTasks.size(), 0);
}