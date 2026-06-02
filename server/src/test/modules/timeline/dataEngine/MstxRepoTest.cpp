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
#include "MstxRepo.h"
#include "TrackInfoManager.h"
#include "../../../DatabaseTestCaseMockUtil.h"
#include "TableDefaultMock.h"
using namespace Dic::Module::Timeline;
using namespace Dic::TimeLine::Table::Default::Mock;
using namespace Dic::Global::PROFILER::MockUtil;
class MstxRepoTest : public ::testing::Test {
  protected:
    const std::string stringIdsSql = "CREATE TABLE STRING_IDS (id INTEGER PRIMARY KEY,value TEXT);";
    const std::string mstxSql = "CREATE TABLE MSTX_EVENTS (startNs INTEGER,endNs INTEGER,eventType INTEGER,rangeId "
                                "INTEGER,category INTEGER,message INTEGER,globalTid INTEGER,endGlobalTid "
                                "INTEGER,domainId INTEGER,connectionId INTEGER, depth integer);";
    const std::string mstxTypeSql = "CREATE TABLE ENUM_MSTX_EVENT_TYPE (id INTEGER PRIMARY KEY,name TEXT);";
    void SetUp() override { TrackInfoManager::Instance().Reset(); }

    void TearDown() override { TrackInfoManager::Instance().Reset(); }
};

/**
 * 测试根据id查询算子详情,正常情况
 */
TEST_F(MstxRepoTest, TestQuerySliceDetailInfoNormal) {
    class MstxRepoMock : public MstxRepo {
      public:
        void SetMock(MstxDependency &dependency) {
            mstxEventsTable = std::move(dependency.mstxEventsTableMock);
            enumMstxEventTypeTable = std::move(dependency.enumMstxEventTypeTableMock);
            stringIdsTable = std::move(dependency.stringIdsTableMock);
        }
    };
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, mstxSql);
    DatabaseTestCaseMockUtil::CreateTable(db, mstxTypeSql);
    DatabaseTestCaseMockUtil::CreateTable(db, stringIdsSql);
    std::string mstxInsert = "INSERT INTO \"main\".\"MSTX_EVENTS\" (\"startNs\", \"endNs\", \"eventType\", "
                             "\"rangeId\", \"category\", \"message\", \"globalTid\", \"endGlobalTid\", \"domainId\", "
                             "\"connectionId\", \"depth\") VALUES (1718180918997410110, 1718180918997410110, 3, "
                             "4294967295, 4294967295, 513, 8785587534250072, 8785587534250072, 65535, 4000000000, 0);";
    std::string mstxTypeInsert =
        "INSERT INTO \"main\".\"ENUM_MSTX_EVENT_TYPE\" (\"id\", \"name\") VALUES (3, 'marker_ex');";
    std::string stringInsert = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (377, 'mmmm');\n"
                               "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (513, 'aaaa');";
    DatabaseTestCaseMockUtil::InsertData(db, mstxInsert);
    DatabaseTestCaseMockUtil::InsertData(db, mstxTypeInsert);
    DatabaseTestCaseMockUtil::InsertData(db, stringInsert);
    MstxDependency dependency;
    dependency.stringIdsTableMock = std::make_unique<StringIdsTableMock>();
    dependency.stringIdsTableMock->SetDb(db);
    dependency.mstxEventsTableMock = std::make_unique<MstxEventsTableMock>();
    dependency.mstxEventsTableMock->SetDb(db);
    dependency.enumMstxEventTypeTableMock = std::make_unique<EnumMstxEventTypeTableMock>();
    dependency.enumMstxEventTypeTableMock->SetDb(db);
    MstxRepoMock mstxRepoMock;
    mstxRepoMock.SetMock(dependency);
    SliceQuery query;
    query.sliceId = "1";
    query.rankId = "hhh";
    CompeteSliceDomain slice;
    bool result = mstxRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, true);
    EXPECT_EQ(slice.name, "aaaa");
    const uint64_t expectStart = 1718180918997410110;
    const uint64_t expectEnd = 1718180918997410110;
    EXPECT_EQ(slice.timestamp, expectStart);
    EXPECT_EQ(slice.endTime, expectEnd);
    const std::string expectArgs = "{\"eventType\":\"marker_ex\"}";
    EXPECT_EQ(slice.args, expectArgs);
}

/**
 * 测试根据id查询算子详情,算子不存在
 */
TEST_F(MstxRepoTest, TestQuerySliceDetailInfoWhenIdNotExistThenReturnFalse) {
    class MstxRepoMock : public MstxRepo {
      public:
        void SetMock(MstxDependency &dependency) {
            mstxEventsTable = std::move(dependency.mstxEventsTableMock);
            enumMstxEventTypeTable = std::move(dependency.enumMstxEventTypeTableMock);
            stringIdsTable = std::move(dependency.stringIdsTableMock);
        }
    };
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, mstxSql);
    std::string mstxInsert = "INSERT INTO \"main\".\"MSTX_EVENTS\" (\"startNs\", \"endNs\", \"eventType\", "
                             "\"rangeId\", \"category\", \"message\", \"globalTid\", \"endGlobalTid\", \"domainId\", "
                             "\"connectionId\", \"depth\") VALUES (1718180918997410110, 1718180918997410110, 3, "
                             "4294967295, 4294967295, 513, 8785587534250072, 8785587534250072, 65535, 4000000000, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, mstxInsert);
    MstxDependency dependency;
    dependency.mstxEventsTableMock = std::make_unique<MstxEventsTableMock>();
    dependency.mstxEventsTableMock->SetDb(db);
    MstxRepoMock mstxRepoMock;
    mstxRepoMock.SetMock(dependency);
    SliceQuery query;
    query.sliceId = "99998\t\b\v";
    query.rankId = "hhh";
    CompeteSliceDomain slice;
    bool result = mstxRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, false);
}

TEST_F(MstxRepoTest, QuerySliceDetailInfoByNameListSuccess) {
    class MstxRepoMock : public MstxRepo {
      public:
        void SetMock(MstxDependency &dependency) {
            mstxEventsTable = std::move(dependency.mstxEventsTableMock);
            enumMstxEventTypeTable = std::move(dependency.enumMstxEventTypeTableMock);
            stringIdsTable = std::move(dependency.stringIdsTableMock);
        }
    };
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, mstxSql);
    DatabaseTestCaseMockUtil::CreateTable(db, stringIdsSql);
    std::string mstxInsert = "INSERT INTO \"main\".\"MSTX_EVENTS\" (\"startNs\", \"endNs\", \"eventType\", "
                             "\"rangeId\", \"category\", \"message\", \"globalTid\", \"endGlobalTid\", \"domainId\", "
                             "\"connectionId\", \"depth\") VALUES (1718180918997410110, 1718180918997410110, 3, "
                             "4294967295, 4294967295, 513, 8785587534250072, 8785587534250072, 65535, 4000000000, 0);";
    std::string stringInsert = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (377, 'mmmm');\n"
                               "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (513, 'compute_log');";
    DatabaseTestCaseMockUtil::InsertData(db, mstxInsert);
    DatabaseTestCaseMockUtil::InsertData(db, stringInsert);
    MstxDependency dependency;
    dependency.stringIdsTableMock = std::make_unique<StringIdsTableMock>();
    dependency.stringIdsTableMock->SetDb(db);
    dependency.mstxEventsTableMock = std::make_unique<MstxEventsTableMock>();
    dependency.mstxEventsTableMock->SetDb(db);
    MstxRepoMock mstxRepoMock;
    mstxRepoMock.SetMock(dependency);

    SliceQueryByNameList params{"hhh", "", {"compute_log"}};
    std::vector<CompeteSliceDomain> res;
    bool result = mstxRepoMock.QuerySliceDetailInfoByNameList(params, res);
    const uint64_t expectSize = 1;
    const uint64_t expectTIme = 1718180918997410110;
    EXPECT_EQ(result, true);
    EXPECT_EQ(res.size(), expectSize);
    EXPECT_EQ(res[0].timestamp, expectTIme);
    EXPECT_EQ(res[0].endTime, expectTIme);
}

/**
 * 测试全量DB的 mstxRepo 转化 SliceInterface 的情况
 */
TEST_F(MstxRepoTest, TestDynamicCastOfMultiSliceInterface) {
    std::shared_ptr<IBaseSliceRepo> mstxRepo = std::make_shared<MstxRepo>();
    // 转 IPythonFuncSlice 失败
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(mstxRepo.get());
    EXPECT_EQ(pythonFuncRepo, nullptr);
    // 转 IFindSliceByTimepointAndName 失败
    const auto findSliceByTimepointAndName = dynamic_cast<IFindSliceByTimepointAndName *>(mstxRepo.get());
    EXPECT_EQ(findSliceByTimepointAndName, nullptr);
    // 转 ITextSlice 失败
    const auto textSliceRepo = dynamic_cast<ITextSlice *>(mstxRepo.get());
    EXPECT_EQ(textSliceRepo, nullptr);
}
