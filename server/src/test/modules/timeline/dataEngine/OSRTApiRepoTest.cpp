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
#include "OSRTApiRepo.h"
#include "DbTraceDataBase.h"
using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;
class OSRTApiRepoTest : public OSRTApiRepo, public ::testing::Test {
  protected:
    std::string OSRTApiCreate =
        "CREATE TABLE OSRT_API (name NUMERIC, globalTid NUMERIC, startNs NUMERIC, endNs NUMERIC);";
    std::string stringIdsCreate = "CREATE TABLE STRING_IDS (id INTEGER, value TEXT);";
    std::string OSRTApiInsert = "INSERT INTO \"OSRT_API\" (\"name\", \"globalTid\", \"startNs\", \"endNs\")"
                                " VALUES (119, 885733925791211, 1736911444046987000, 1736911444046990000), "
                                " (123, 885733925791211, 1736911444046987000, 1736911444046997000),"
                                " (119, 885733925791211, 1736911444047007000, 1736911444047010000),"
                                " (124, 885733925791211, 1736911444047207000, 1736911444047210000),"
                                " (119, 885733925791211, 1736911444047407000, 1736911444047410000),"
                                " (128, 885733925791213, 1736911444046997000, 1736911444047000000),"
                                " (119, 885733925791213, 1736911444047197000, 1736911444047200000),"
                                " (123, 885733925791213, 1736911444047397000, 1736911444047400000),"
                                " (124, 885733925791213, 1736911444047597000, 1736911444047600000),"
                                " (119, 885733925791213, 1736911444047797000, 1736911444047800000);";
    std::string stringIdsInsert =
        "INSERT INTO \"STRING_IDS\" (\"id\", \"value\") VALUES"
        " (119, \"read\"), (123, \"openat\"), (124, \"clock_nanosleep\"), (128, \"mmap\"), (131, \"munmap\")";
};

TEST_F(OSRTApiRepoTest, QuerySimpleSliceWithOutNameByTrackIdExecuteSQLNormalTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    currPath = currPath.substr(0, index);
    std::string dbPath = R"(test/data/msprof/)";
    std::string completePath = currPath + dbPath + "OSPTApiRepoTest.db";
    std::recursive_mutex mutex;
    std::shared_ptr<VirtualTraceDatabase> database = std::make_shared<DbTraceDataBase>(mutex);
    database->OpenDb(completePath, false);
    database->ExecSql(OSRTApiCreate);
    database->ExecSql(stringIdsCreate);
    database->ExecSql(OSRTApiInsert);
    database->ExecSql(stringIdsInsert);
    std::string processId{"885733925791211"};
    std::vector<SliceDomain> sliceVec;
    SliceQuery sliceQuery;
    sliceQuery.endTime = UINT64_MAX;
    QuerySimpleSliceWithOutNameByTrackIdExecuteSQL(database, processId, sliceVec, sliceQuery);
    ASSERT_EQ(sliceVec.size(), 5); // 5
    EXPECT_EQ(sliceVec[0].id, 1);
    EXPECT_EQ(sliceVec[1].id, 2); // 2
    EXPECT_EQ(sliceVec[2].id, 3); // 2, 3
    EXPECT_EQ(sliceVec[3].id, 4); // 3, 4
    EXPECT_EQ(sliceVec[4].id, 5); // 4, 5
    sliceVec.clear();
    processId = "885733925791213";
    QuerySimpleSliceWithOutNameByTrackIdExecuteSQL(database, processId, sliceVec, sliceQuery);
    ASSERT_EQ(sliceVec.size(), 5); // 5
    EXPECT_EQ(sliceVec[0].id, 6); // 6
    EXPECT_EQ(sliceVec[1].id, 7); // 7
    EXPECT_EQ(sliceVec[2].id, 8); // 2, 8
    EXPECT_EQ(sliceVec[3].id, 9); // 3, 9
    EXPECT_EQ(sliceVec[4].id, 10); // 4, 10
    database->CloseDb();
    std::remove(completePath.c_str());
}

TEST_F(OSRTApiRepoTest, QuerySimpleSliceWithOutNameByTrackIdExecuteSQLFailedTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    currPath = currPath.substr(0, index);
    std::string dbPath = R"(test/data/msprof/)";
    std::string completePath = currPath + dbPath + "OSPTApiRepoTest.db";
    std::recursive_mutex mutex;
    std::shared_ptr<VirtualTraceDatabase> database = std::make_shared<DbTraceDataBase>(mutex);
    database->OpenDb(completePath, false);
    std::string processId{"885733925791211"};
    std::vector<SliceDomain> sliceVec;
    SliceQuery sliceQuery;
    sliceQuery.endTime = UINT64_MAX;
    QuerySimpleSliceWithOutNameByTrackIdExecuteSQL(database, processId, sliceVec, sliceQuery);
    ASSERT_EQ(sliceVec.size(), 0);
    database->CloseDb();
    std::remove(completePath.c_str());
}

TEST_F(OSRTApiRepoTest, QueryCompeteSliceByIdsExecuteSQLNormalTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    currPath = currPath.substr(0, index);
    std::string dbPath = R"(test/data/msprof/)";
    std::string completePath = currPath + dbPath + "OSPTApiRepoTest.db";
    std::recursive_mutex mutex;
    std::shared_ptr<VirtualTraceDatabase> database = std::make_shared<DbTraceDataBase>(mutex);
    database->OpenDb(completePath, false);
    database->ExecSql(OSRTApiCreate);
    database->ExecSql(stringIdsCreate);
    database->ExecSql(OSRTApiInsert);
    database->ExecSql(stringIdsInsert);
    std::vector<uint64_t> sliceIds{1, 3, 7};
    std::vector<CompeteSliceDomain> competeSliceVec;
    QueryCompeteSliceByIdsExecuteSQL(database, sliceIds, competeSliceVec);
    ASSERT_EQ(competeSliceVec.size(), 3); // 3
    EXPECT_EQ(competeSliceVec[0].id, 1);
    EXPECT_EQ(competeSliceVec[0].name, "read");
    EXPECT_EQ(competeSliceVec[1].id, 3); // 3
    EXPECT_EQ(competeSliceVec[1].name, "read");
    EXPECT_EQ(competeSliceVec[2].id, 7); // 2, 7
    EXPECT_EQ(competeSliceVec[2].name, "read"); // 2
    competeSliceVec.clear();
    sliceIds = {2, 5, 8};
    QueryCompeteSliceByIdsExecuteSQL(database, sliceIds, competeSliceVec);
    ASSERT_EQ(competeSliceVec.size(), 3); // 3
    EXPECT_EQ(competeSliceVec[0].id, 2); // 2
    EXPECT_EQ(competeSliceVec[0].name, "openat");
    EXPECT_EQ(competeSliceVec[1].id, 5); // 5
    EXPECT_EQ(competeSliceVec[1].name, "read");
    EXPECT_EQ(competeSliceVec[2].id, 8); // 2, 8
    EXPECT_EQ(competeSliceVec[2].name, "openat"); // 2
    database->CloseDb();
    std::remove(completePath.c_str());
}

TEST_F(OSRTApiRepoTest, QueryCompeteSliceByIdsExecuteSQLFailedTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    currPath = currPath.substr(0, index);
    std::string dbPath = R"(test/data/msprof/)";
    std::string completePath = currPath + dbPath + "OSPTApiRepoTest.db";
    std::recursive_mutex mutex;
    std::shared_ptr<VirtualTraceDatabase> database = std::make_shared<DbTraceDataBase>(mutex);
    database->OpenDb(completePath, false);
    std::vector<uint64_t> sliceIds{1, 3, 7};
    std::vector<CompeteSliceDomain> competeSliceVec;
    QueryCompeteSliceByIdsExecuteSQL(database, sliceIds, competeSliceVec);
    ASSERT_EQ(competeSliceVec.size(), 0);
    database->CloseDb();
    std::remove(completePath.c_str());
}

TEST_F(OSRTApiRepoTest, QuerySliceDetailInfoExecuteSQLNormalTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    currPath = currPath.substr(0, index);
    std::string dbPath = R"(test/data/msprof/)";
    std::string completePath = currPath + dbPath + "OSPTApiRepoTest.db";
    std::recursive_mutex mutex;
    std::shared_ptr<VirtualTraceDatabase> database = std::make_shared<DbTraceDataBase>(mutex);
    database->OpenDb(completePath, false);
    database->ExecSql(OSRTApiCreate);
    database->ExecSql(stringIdsCreate);
    database->ExecSql(OSRTApiInsert);
    database->ExecSql(stringIdsInsert);
    std::string sliceId{"1"};
    CompeteSliceDomain competeSliceDomain;
    bool result = QuerySliceDetailInfoExecuteSQL(database, sliceId, competeSliceDomain);
    ASSERT_TRUE(result);
    EXPECT_EQ(competeSliceDomain.id, 1);
    EXPECT_EQ(competeSliceDomain.name, "read");
    sliceId = "6";
    result = QuerySliceDetailInfoExecuteSQL(database, sliceId, competeSliceDomain);
    ASSERT_TRUE(result);
    EXPECT_EQ(competeSliceDomain.id, 6); // 6
    EXPECT_EQ(competeSliceDomain.name, "mmap");
    database->CloseDb();
    std::remove(completePath.c_str());
}

TEST_F(OSRTApiRepoTest, QuerySliceDetailInfoExecuteSQLFailedTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    currPath = currPath.substr(0, index);
    std::string dbPath = R"(test/data/msprof/)";
    std::string completePath = currPath + dbPath + "OSPTApiRepoTest.db";
    std::recursive_mutex mutex;
    std::shared_ptr<VirtualTraceDatabase> database = std::make_shared<DbTraceDataBase>(mutex);
    database->OpenDb(completePath, false);
    std::string sliceId{"1"};
    CompeteSliceDomain competeSliceDomain;
    bool result = QuerySliceDetailInfoExecuteSQL(database, sliceId, competeSliceDomain);
    // 打开数据库时会创建OSRT_API临时表，所以SQL查询不会报错
    ASSERT_TRUE(result);
    database->CloseDb();
    std::remove(completePath.c_str());
}

/**
 * 测试全量DB的 osrtApiRepo 转化 SliceInterface 的情况
 */
TEST_F(OSRTApiRepoTest, TestDynamicCastOfMultiSliceInterface) {
    std::shared_ptr<IBaseSliceRepo> osrtApiRepo = std::make_shared<OSRTApiRepo>();
    // 转 IPythonFuncSlice 失败
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(osrtApiRepo.get());
    EXPECT_EQ(pythonFuncRepo, nullptr);
    // 转 IFindSliceByNameList 失败
    const auto findSliceByNameList = dynamic_cast<IFindSliceByNameList *>(osrtApiRepo.get());
    EXPECT_EQ(findSliceByNameList, nullptr);
    // 转 IFindSliceByTimepointAndName 失败
    const auto findSliceByTimepointAndName = dynamic_cast<IFindSliceByTimepointAndName *>(osrtApiRepo.get());
    EXPECT_EQ(findSliceByTimepointAndName, nullptr);
    // 转 ITextSlice 失败
    const auto textSliceRepo = dynamic_cast<ITextSlice *>(osrtApiRepo.get());
    EXPECT_EQ(textSliceRepo, nullptr);
}
