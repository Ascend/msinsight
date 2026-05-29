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
#include "CANNApiRepo_mock_data.h"
#include "CANNApiRepo.h"
#include "TrackInfoManager.h"
#include "../../../DatabaseTestCaseMockUtil.h"
#include "TableDefaultMock.h"
using namespace Dic::TimeLine::CANNApiRepo::Mock;
using namespace Dic::TimeLine::Table::Default::Mock;
using namespace Dic::Global::PROFILER::MockUtil;
class CANNApiRepoTest : public ::testing::Test {
  protected:
    const std::string stringIdsSql = "CREATE TABLE STRING_IDS (id INTEGER PRIMARY KEY,value TEXT);";
    const std::string cannapiSql = "CREATE TABLE CANN_API (startNs INTEGER,endNs INTEGER,type INTEGER,globalTid "
                                   "INTEGER,connectionId INTEGER PRIMARY KEY,name INTEGER, depth integer);";
    const std::string apiTypeSql = "CREATE TABLE ENUM_API_TYPE (id INTEGER PRIMARY KEY,name TEXT);";
    void SetUp() override { TrackInfoManager::Instance().Reset(); }

    void TearDown() override { TrackInfoManager::Instance().Reset(); }
};

/**
 * 测试全量DB的cann根据id空集合查询完整算子
 */
TEST_F(CANNApiRepoTest, test_QueryCompeteSliceByIds_with_empthIds) {
    CANNApiRepo cannApiRepo;
    SliceQuery sliceQuery;
    std::vector<uint64_t> sliceIds;
    std::vector<CompeteSliceDomain> competeSliceVec;
    cannApiRepo.QueryCompeteSliceByIds(sliceQuery, sliceIds, competeSliceVec);
    EXPECT_EQ(competeSliceVec.size(), 0);
}

/**
 * 测试全量DB的 cannApiRepo 转化 SliceInterface 的情况
 */
TEST_F(CANNApiRepoTest, TestDynamicCastOfMultiSliceInterface) {
    std::shared_ptr<IBaseSliceRepo> cannApiRepo = std::make_shared<CANNApiRepo>();
    // 转 IPythonFuncSlice 失败
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(cannApiRepo.get());
    EXPECT_EQ(pythonFuncRepo, nullptr);
    // 转 IFindSliceByNameList 成功
    const auto findSliceByNameList = dynamic_cast<IFindSliceByNameList *>(cannApiRepo.get());
    EXPECT_NE(findSliceByNameList, nullptr);
    // 转 IFindSliceByTimepointAndName 失败
    const auto findSliceByTimepointAndName = dynamic_cast<IFindSliceByTimepointAndName *>(cannApiRepo.get());
    EXPECT_EQ(findSliceByTimepointAndName, nullptr);
    // 转 ITextSlice 失败
    const auto textSliceRepo = dynamic_cast<ITextSlice *>(cannApiRepo.get());
    EXPECT_EQ(textSliceRepo, nullptr);
}

/**
 * 测试全量DB的cann根据trackId查询所有简单算子，无对应track
 */
TEST_F(CANNApiRepoTest, test_QuerySimpleSliceWithOutNameByTrackId) {
    CANNApiRepo cannApiRepo;
    SliceQuery sliceQuery;
    std::vector<uint64_t> sliceIds;
    std::vector<SliceDomain> sliceVec;
    cannApiRepo.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
    EXPECT_EQ(sliceVec.size(), 0);
}

/**
 * 测试全量DB的cann根据id集合查询完整算子,无对应track
 */
TEST_F(CANNApiRepoTest, test_QueryCompeteSliceByIds_with_track_empty) {
    CANNApiRepo cannApiRepo;
    SliceQuery sliceQuery;
    std::vector<uint64_t> sliceIds = {1, 2, 3};
    std::vector<CompeteSliceDomain> competeSliceVec;
    cannApiRepo.QueryCompeteSliceByIds(sliceQuery, sliceIds, competeSliceVec);
    EXPECT_EQ(competeSliceVec.size(), 0);
}

/**
 * 测试全量DB的cann的根据id集合查询完整算子,正常情况
 */
TEST_F(CANNApiRepoTest, test_QueryCompeteSliceByIds_with_normal) {
    TrackInfoManager::Instance().Reset();
    class TableMock : public Dic::Module::Timeline::CANNApiTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<CANNApiPO> &result) override {
            QueryCompeteSliceByIdsWithNormalExcuteQuery(fileId, result);
            ClearThreadLocal();
        }
    };
    std::unique_ptr<Dic::Module::Timeline::CANNApiTable> ptr = std::make_unique<TableMock>();
    CANNApiRepo cannApiRepo;
    cannApiRepo.SetCANNApiTable(std::move(ptr));
    SliceQuery sliceQuery;
    sliceQuery.trackId = TrackInfoManager::Instance().GetTrackId("999", "yyy", "999");
    std::vector<uint64_t> sliceIds = {1, 2, 3};
    std::vector<CompeteSliceDomain> competeSliceVec;
    cannApiRepo.QueryCompeteSliceByIds(sliceQuery, sliceIds, competeSliceVec);
    const uint64_t expectSize = 2;
    EXPECT_EQ(competeSliceVec.size(), expectSize);
    auto it = competeSliceVec.begin();
    const uint64_t firstTimestamp = 22;
    EXPECT_EQ(it->timestamp, firstTimestamp);
    it++;
    const uint64_t lastTimestamp = 23;
    EXPECT_EQ(it->timestamp, lastTimestamp);
    TrackInfoManager::Instance().Reset();
}

/**
 * 测试全量DB的cann的根据track查询完整算子,正常情况
 */
TEST_F(CANNApiRepoTest, test_QuerySimpleSliceWithOutNameByTrackId_with_normal) {
    TrackInfoManager::Instance().Reset();
    class TableMock : public Dic::Module::Timeline::CANNApiTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<CANNApiPO> &result) override {
            QuerySimpleSliceWithOutNameByTrackIdWithNormalExcuteQuery(fileId, result);
            ClearThreadLocal();
        }
    };
    std::unique_ptr<Dic::Module::Timeline::CANNApiTable> ptr = std::make_unique<TableMock>();
    CANNApiRepo cannApiRepo;
    cannApiRepo.SetCANNApiTable(std::move(ptr));
    SliceQuery sliceQuery;
    sliceQuery.trackId = TrackInfoManager::Instance().GetTrackId("999", "yyy", "999");
    sliceQuery.endTime = UINT64_MAX;
    std::vector<uint64_t> sliceIds = {1, 2, 3};
    std::vector<SliceDomain> sliceVec;
    cannApiRepo.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
    const uint64_t expectSize = 2;
    EXPECT_EQ(sliceVec.size(), expectSize);
    auto it = sliceVec.begin();
    const uint64_t firstTimestamp = 22;
    EXPECT_EQ(it->timestamp, firstTimestamp);
    it++;
    const uint64_t lastTimestamp = 23;
    EXPECT_EQ(it->timestamp, lastTimestamp);
    TrackInfoManager::Instance().Reset();
}

/**
 * 测试根据id查询算子详情,正常情况
 */
TEST_F(CANNApiRepoTest, TestQuerySliceDetailInfoNormal) {
    class CANNApiRepoMock : public CANNApiRepo {
      public:
        void SetMock(CANNDependency &dependency) {
            apiTypeTable = std::move(dependency.enumApiTypeTableMock);
            cannApiTable = std::move(dependency.cannApiTableMock);
            stringIdsTable = std::move(dependency.stringIdsTableMock);
        }
    };
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, apiTypeSql);
    DatabaseTestCaseMockUtil::CreateTable(db, cannapiSql);
    DatabaseTestCaseMockUtil::CreateTable(db, stringIdsSql);
    std::string apiInsert = "INSERT INTO \"main\".\"ENUM_API_TYPE\" (\"id\", \"name\") VALUES (10000, 'node');";
    std::string cannInsert = "INSERT INTO \"main\".\"CANN_API\" (\"startNs\", \"endNs\", \"type\", \"globalTid\", "
                             "\"connectionId\", \"name\", \"depth\") VALUES (1718180918997508370, 1718180918997541810, "
                             "10000, 8785587534250072, 7421, 327, 0);";
    std::string stringInsert = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (377, 'mmmm');\n"
                               "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (327, 'aaaa');";
    DatabaseTestCaseMockUtil::InsertData(db, apiInsert);
    DatabaseTestCaseMockUtil::InsertData(db, cannInsert);
    DatabaseTestCaseMockUtil::InsertData(db, stringInsert);
    CANNDependency dependency;
    dependency.stringIdsTableMock = std::make_unique<StringIdsTableMock>();
    dependency.stringIdsTableMock->SetDb(db);
    dependency.cannApiTableMock = std::make_unique<CANNApiTableMock>();
    dependency.cannApiTableMock->SetDb(db);
    dependency.enumApiTypeTableMock = std::make_unique<EnumApiTypeTableMock>();
    dependency.enumApiTypeTableMock->SetDb(db);
    CANNApiRepoMock cannApiRepoMock;
    cannApiRepoMock.SetMock(dependency);
    SliceQuery query;
    query.sliceId = "7421";
    query.rankId = "hhh";
    CompeteSliceDomain slice;
    bool result = cannApiRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, true);
    EXPECT_EQ(slice.name, "aaaa");
    const uint64_t expectStart = 1718180918997508370;
    const uint64_t expectEnd = 1718180918997541810;
    EXPECT_EQ(slice.timestamp, expectStart);
    EXPECT_EQ(slice.endTime, expectEnd);
    const std::string expectArgs =
        "{\"globalTid\":\"8785587534250072\",\"type\":\"node\",\"name\":\"aaaa\",\"connectionId\":\"7421\"}";
    EXPECT_EQ(slice.args, expectArgs);
}

/**
 * 测试根据id查询算子详情,id不存在的情况
 */
TEST_F(CANNApiRepoTest, TestQuerySliceDetailInfoWhenIdNotExistThenReturnFalse) {
    class CANNApiRepoMock : public CANNApiRepo {
      public:
        void SetMock(CANNDependency &dependency) {
            apiTypeTable = std::move(dependency.enumApiTypeTableMock);
            cannApiTable = std::move(dependency.cannApiTableMock);
            stringIdsTable = std::move(dependency.stringIdsTableMock);
        }
    };
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, cannapiSql);
    std::string cannInsert = "INSERT INTO \"main\".\"CANN_API\" (\"startNs\", \"endNs\", \"type\", \"globalTid\", "
                             "\"connectionId\", \"name\", \"depth\") VALUES (1718180918997508370, 1718180918997541810, "
                             "10000, 8785587534250072, 7421, 327, 0);";
    DatabaseTestCaseMockUtil::InsertData(db, cannInsert);
    CANNDependency dependency;
    dependency.cannApiTableMock = std::make_unique<CANNApiTableMock>();
    dependency.cannApiTableMock->SetDb(db);
    CANNApiRepoMock cannApiRepoMock;
    cannApiRepoMock.SetMock(dependency);
    SliceQuery query;
    query.sliceId = "9999999999\r\f\b\v";
    query.rankId = "hhh";
    CompeteSliceDomain slice;
    bool result = cannApiRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, false);
}
