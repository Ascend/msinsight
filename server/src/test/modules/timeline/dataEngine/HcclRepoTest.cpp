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
#include "HcclRepo_mock_data.h"
#include "CommucationOpTable.h"
#include "HcclRepo.h"
#include "../../../DatabaseTestCaseMockUtil.h"
#include "EnumHcclDataTypeTable.h"
#include "TableDefaultMock.h"
#include "MetaDataCacheManager.h"
#include "TrackInfoManager.h"
using namespace Dic::TimeLine::HcclRepo::Mock;
using namespace Dic::Global::PROFILER::MockUtil;
using namespace Dic::TimeLine::Table::Default::Mock;
class HcclRepoTest : public ::testing::Test {
  protected:
    const std::string opTableSql =
        "CREATE TABLE COMMUNICATION_OP (opName INTEGER,startNs INTEGER,endNs INTEGER,connectionId INTEGER,groupName "
        "INTEGER,opId INTEGER PRIMARY KEY,relay INTEGER,retry INTEGER,dataType INTEGER,algType INTEGER,count "
        "NUMERIC,opType INTEGER, waitNs INTEGER);";
    const std::string dataTypeDataSql = "CREATE TABLE ENUM_HCCL_DATA_TYPE (id INTEGER PRIMARY KEY,name TEXT);";
    const std::string stringIdsSql = "CREATE TABLE STRING_IDS (id INTEGER PRIMARY KEY,value TEXT);";
    const std::string taskSql = "CREATE TABLE TASK (startNs INTEGER,endNs INTEGER,deviceId INTEGER,connectionId "
                                "INTEGER,globalTaskId INTEGER,globalPid INTEGER,taskType INTEGER,contextId "
                                "INTEGER,streamId INTEGER,taskId INTEGER,modelId INTEGER, depth integer);";
    const std::string taskInfoSql =
        "CREATE TABLE COMMUNICATION_TASK_INFO (name INTEGER,globalTaskId INTEGER,taskType INTEGER,planeId "
        "INTEGER,groupName INTEGER,notifyId INTEGER,rdmaType INTEGER,srcRank INTEGER,dstRank INTEGER,transportType "
        "INTEGER,size INTEGER,dataType INTEGER,linkType INTEGER,opId INTEGER);";
    const std::string linkTypeSql = "CREATE TABLE ENUM_HCCL_LINK_TYPE (id INTEGER PRIMARY KEY,name TEXT);";
    const std::string rdmaSql = "CREATE TABLE ENUM_HCCL_RDMA_TYPE (id INTEGER PRIMARY KEY,name TEXT);";
    const std::string transSql = "CREATE TABLE ENUM_HCCL_TRANSPORT_TYPE (id INTEGER PRIMARY KEY,name TEXT);";

    void SetUp() override { TrackInfoManager::Instance().Reset(); }

    void TearDown() override { TrackInfoManager::Instance().Reset(); }

    void CreateTestTable(sqlite3 *db) const {
        DatabaseTestCaseMockUtil::CreateTable(db, taskSql);
        DatabaseTestCaseMockUtil::CreateTable(db, taskInfoSql);
        DatabaseTestCaseMockUtil::CreateTable(db, stringIdsSql);
        DatabaseTestCaseMockUtil::CreateTable(db, dataTypeDataSql);
        DatabaseTestCaseMockUtil::CreateTable(db, linkTypeSql);
        DatabaseTestCaseMockUtil::CreateTable(db, rdmaSql);
        DatabaseTestCaseMockUtil::CreateTable(db, transSql);
    }

    void TestPlaneQueryGroupSliceDetailInfoPrepare(HcclDependency &dependency) {
        sqlite3 *db = nullptr;
        DatabaseTestCaseMockUtil::OpenDB(db);
        CreateTestTable(db);
        std::string taskInsert =
            "INSERT INTO \"main\".\"TASK\" (\"startNs\", \"endNs\", \"deviceId\", \"connectionId\", "
            "\"globalTaskId\", \"globalPid\", \"taskType\", \"contextId\", \"streamId\", \"taskId\", "
            "\"modelId\", \"depth\") VALUES (1718180918997464923, 1718180918997464923, 0, 4294967295, "
            "1669, 2045554, 319, 4294967295, 16, 3730, 4294967295, 0);";
        DatabaseTestCaseMockUtil::InsertData(db, taskInsert);
        std::string taskInfoInsert =
            "INSERT INTO \"main\".\"COMMUNICATION_TASK_INFO\" (\"name\", \"globalTaskId\", \"taskType\", \"planeId\", "
            "\"groupName\", \"notifyId\", \"rdmaType\", \"srcRank\", \"dstRank\", \"transportType\", \"size\", "
            "\"dataType\", \"linkType\", \"opId\") VALUES (377, 1669, 319, 0, 379, 9223372036854775807, 4, 1, 2, 2, "
            "40, 3, 2, 1);";
        DatabaseTestCaseMockUtil::InsertData(db, taskInfoInsert);
        std::string dataTypeInsert =
            "INSERT INTO \"main\".\"ENUM_HCCL_DATA_TYPE\" (\"id\", \"name\") VALUES (3, 'INT16');";
        DatabaseTestCaseMockUtil::InsertData(db, dataTypeInsert);
        std::string linkInsert = "INSERT INTO \"main\".\"ENUM_HCCL_LINK_TYPE\" (\"id\", \"name\") VALUES (2, 'PCIE');";
        DatabaseTestCaseMockUtil::InsertData(db, linkInsert);
        std::string rdmaInsert =
            "INSERT INTO \"main\".\"ENUM_HCCL_RDMA_TYPE\" (\"id\", \"name\") VALUES (4, 'RDMA_SEND_OP');";
        DatabaseTestCaseMockUtil::InsertData(db, rdmaInsert);
        std::string transInsert =
            "INSERT INTO \"main\".\"ENUM_HCCL_TRANSPORT_TYPE\" (\"id\", \"name\") VALUES (2, 'LOCAL');";
        DatabaseTestCaseMockUtil::InsertData(db, transInsert);
        std::string strInsert = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (319, 'kkkk'), (379, "
                                "'172.16.4.45%eth0_64000_0_1739179801171386');";
        DatabaseTestCaseMockUtil::InsertData(db, strInsert);
        dependency.taskTableMock = std::make_unique<TaskTableMock>();
        dependency.taskTableMock->SetDb(db);
        dependency.commucationTaskInfoTableMock = std::make_unique<CommucationTaskInfoTableMock>();
        dependency.commucationTaskInfoTableMock->SetDb(db);
        dependency.stringIdsTableMock = std::make_unique<StringIdsTableMock>();
        dependency.stringIdsTableMock->SetDb(db);
        dependency.enumHcclDataTypeTableMock = std::make_unique<EnumHcclDataTypeTableMock>();
        dependency.enumHcclDataTypeTableMock->SetDb(db);
        dependency.enumHcclRdmaTypeTableMock = std::make_unique<EnumHcclRdmaTypeTableMock>();
        dependency.enumHcclRdmaTypeTableMock->SetDb(db);
        dependency.enumHcclLinkTypeTableMock = std::make_unique<EnumHcclLinkTypeTableMock>();
        dependency.enumHcclLinkTypeTableMock->SetDb(db);
        dependency.enumHcclTransportTypeTableMock = std::make_unique<EnumHcclTransportTypeTableMock>();
        dependency.enumHcclTransportTypeTableMock->SetDb(db);
    }

    void TestMockGroupInfoCache() const {
        std::vector<ParallelGroupInfo> infos = {{"172.16.4.45%eth0_64000_0_1739179801171386", "tp", {"0", "5", "6"}}};
        MetaDataCacheManager::Instance().AddParallelGroupInfo(infos);
    }
};
/**
 * 测试全量DB的hccl的group泳道的根据id空集合查询完整算子
 */
TEST_F(HcclRepoTest, test_QueryCompeteSliceByIds_group_track_with_empthIds) {
    HcclRepo hcclRepo;
    SliceQuery sliceQuery;
    std::vector<uint64_t> sliceIds;
    std::vector<CompeteSliceDomain> competeSliceVec;
    hcclRepo.QueryCompeteSliceByIds(sliceQuery, sliceIds, competeSliceVec);
    EXPECT_EQ(competeSliceVec.size(), 0);
}

/**
 * 测试全量DB的hccl下转化 SliceInterface 的情况
 */
TEST_F(HcclRepoTest, TestDynamicCastOfMultiSliceInterface) {
    std::shared_ptr<IBaseSliceRepo> hcclRepo = std::make_shared<HcclRepo>();
    // 转 IPythonFuncSlice 失败
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(hcclRepo.get());
    EXPECT_EQ(pythonFuncRepo, nullptr);
    // 转 IFindSliceByNameList 失败
    const auto findSliceByNameList = dynamic_cast<IFindSliceByNameList *>(hcclRepo.get());
    EXPECT_EQ(findSliceByNameList, nullptr);
    // 转 IFindSliceByTimepointAndName 失败
    const auto findSliceByTimepointAndName = dynamic_cast<IFindSliceByTimepointAndName *>(hcclRepo.get());
    EXPECT_EQ(findSliceByTimepointAndName, nullptr);
    // 转 ITextSlice 失败
    const auto textSliceRepo = dynamic_cast<ITextSlice *>(hcclRepo.get());
    EXPECT_EQ(textSliceRepo, nullptr);
}

/**
 * 测试全量DB的hccl的group泳道的根据trackId查询所有简单算子，无对应track
 */
TEST_F(HcclRepoTest, test_QuerySimpleSliceWithOutNameByTrackId) {
    HcclRepo hcclRepo;
    SliceQuery sliceQuery;
    std::vector<uint64_t> sliceIds;
    std::vector<SliceDomain> sliceVec;
    hcclRepo.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
    EXPECT_EQ(sliceVec.size(), 0);
}

/**
 * 测试全量DB的hccl的group泳道的根据id集合查询完整算子,无对应track
 */
TEST_F(HcclRepoTest, test_QueryCompeteSliceByIds_group_track_with_track_empty) {
    HcclRepo hcclRepo;
    SliceQuery sliceQuery;
    std::vector<uint64_t> sliceIds = {1, 2, 3};
    std::vector<CompeteSliceDomain> competeSliceVec;
    hcclRepo.QueryCompeteSliceByIds(sliceQuery, sliceIds, competeSliceVec);
    EXPECT_EQ(competeSliceVec.size(), 0);
}

/**
 * 测试全量DB的hccl的group泳道的根据id集合查询完整算子,正常情况
 */
TEST_F(HcclRepoTest, test_QueryCompeteSliceByIds_group_track_with_normal) {
    TrackInfoManager::Instance().Reset();
    class TableMock : public Dic::Module::Timeline::CommucationOpTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<CommucationTaskOpPO> &result) override {
            QueryCompeteSliceByIdsGroupTrackWithNormalExcuteQuery(fileId, result);
            ClearThreadLocal();
        }
    };
    std::unique_ptr<Dic::Module::Timeline::CommucationOpTable> ptr = std::make_unique<TableMock>();
    HcclRepo hcclRepo;
    hcclRepo.SetCommucationOpTable(std::move(ptr));
    SliceQuery sliceQuery;
    sliceQuery.trackId = TrackInfoManager::Instance().GetTrackId("999", "yyy", "999group");
    std::vector<uint64_t> sliceIds = {1, 2, 3};
    std::vector<CompeteSliceDomain> competeSliceVec;
    hcclRepo.QueryCompeteSliceByIds(sliceQuery, sliceIds, competeSliceVec);
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
 * 测试全量DB的hccl的plane泳道的根据id集合查询完整算子,正常情况
 */
TEST_F(HcclRepoTest, test_QueryCompeteSliceByIds_plane_track_with_normal) {
    TrackInfoManager::Instance().Reset();
    class TaskMock : public Dic::Module::Timeline::TaskTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<TaskPO> &result) override {
            QueryCompeteSliceByIdsPlaneTrackWithTaskTableMock(fileId, result);
            ClearThreadLocal();
        }
    };
    class CommucationTaskInfoTableMock : public Dic::Module::Timeline::CommucationTaskInfoTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<CommucationTaskInfoPO> &result) override {
            QueryCompeteSliceByIdsPlaneTrackWithCommucationTaskInfoTableMock(fileId, result);
            ClearThreadLocal();
        }
    };
    std::unique_ptr<Dic::Module::Timeline::TaskTable> tPtr = std::make_unique<TaskMock>();
    std::unique_ptr<Dic::Module::Timeline::CommucationTaskInfoTable> cmPtr =
        std::make_unique<CommucationTaskInfoTableMock>();
    HcclRepo hcclRepo;
    hcclRepo.SetTaskTable(std::move(tPtr));
    hcclRepo.SetCommucationTaskInfoTable(std::move(cmPtr));
    SliceQuery sliceQuery;
    sliceQuery.trackId = TrackInfoManager::Instance().GetTrackId("999", "yyy", "999");
    std::vector<uint64_t> sliceIds = {1, 2, 3};
    std::vector<CompeteSliceDomain> competeSliceVec;
    hcclRepo.QueryCompeteSliceByIds(sliceQuery, sliceIds, competeSliceVec);
    const uint64_t expectSize = 2;
    EXPECT_EQ(competeSliceVec.size(), expectSize);
    auto it = competeSliceVec.begin();
    const uint64_t firstTimestamp = 15;
    EXPECT_EQ(it->timestamp, firstTimestamp);
    it++;
    const uint64_t lastTimestamp = 16;
    EXPECT_EQ(it->timestamp, lastTimestamp);
    TrackInfoManager::Instance().Reset();
}

/**
 * 测试全量DB的hccl的group泳道的根据trackId查询所有简单算子,正常情况
 */
TEST_F(HcclRepoTest, test_QuerySimpleSliceWithOutNameByTrackId_group_track_with_normal) {
    TrackInfoManager::Instance().Reset();
    class TaskMock : public Dic::Module::Timeline::TaskTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<TaskPO> &result) override {
            QueryGlobalTaskIdsByRankWithTaskTableMock(fileId, result);
            ClearThreadLocal();
        }
    };
    class CommucationTaskInfoTableMock : public Dic::Module::Timeline::CommucationTaskInfoTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<CommucationTaskInfoPO> &result) override {
            QueryOpIdsByGlabalTaskIdsForCommucationTaskInfoTable(fileId, result);
            ClearThreadLocal();
        }
    };
    class CommucationOpTableMock : public Dic::Module::Timeline::CommucationOpTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<CommucationTaskOpPO> &result) override {
            QuerySimpleSliceByIdsGroupTrackForCommucationOpTable(fileId, result);
            ClearThreadLocal();
        }
    };
    std::unique_ptr<Dic::Module::Timeline::TaskTable> tPtr = std::make_unique<TaskMock>();
    std::unique_ptr<Dic::Module::Timeline::CommucationTaskInfoTable> cmPtr =
        std::make_unique<CommucationTaskInfoTableMock>();
    std::unique_ptr<Dic::Module::Timeline::CommucationOpTable> copPtr = std::make_unique<CommucationOpTableMock>();
    class HcclRepoMockForGroup : public HcclRepo {
      protected:
        void QuerySimpleSliceFromGroupTrackByDevice(std::vector<CommucationTaskOpPO> &commucationTaskOpPOVec,
            const TrackInfo &trackInfo, const std::string &tid, const SliceQuery &sliceQuery) override {
            QuerySimpleSliceByIdsGroupTrackForCommucationOpTable(trackInfo.cardId, commucationTaskOpPOVec);
        }
    };
    HcclRepoMockForGroup hcclRepo;
    hcclRepo.SetTaskTable(std::move(tPtr));
    hcclRepo.SetCommucationTaskInfoTable(std::move(cmPtr));
    hcclRepo.SetCommucationOpTable(std::move(copPtr));
    SliceQuery sliceQuery;
    sliceQuery.trackId = TrackInfoManager::Instance().GetTrackId("999", "yyy", "999group");
    sliceQuery.endTime = UINT64_MAX;
    std::vector<SliceDomain> sliceVec;
    hcclRepo.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
    const uint64_t expectSize = 2;
    ASSERT_EQ(sliceVec.size(), expectSize);
    auto it = sliceVec.begin();
    const uint64_t firstTimestamp = 22;
    EXPECT_EQ(it->timestamp, firstTimestamp);
    it++;
    const uint64_t lastTimestamp = 23;
    EXPECT_EQ(it->timestamp, lastTimestamp);
    TrackInfoManager::Instance().Reset();
}

HcclRepo GetHcclRepoMock() {
    class TaskMock : public Dic::Module::Timeline::TaskTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<TaskPO> &result) override {
            QueryGlobalTaskIdsByRankWithTaskTableMock(fileId, result);
            ClearThreadLocal();
        }
    };
    class CommucationTaskInfoTableMock : public Dic::Module::Timeline::CommucationTaskInfoTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<CommucationTaskInfoPO> &result) override {
            QueryOpIdsByGlabalTaskIdsForCommucationTaskInfoTable(fileId, result);
            ClearThreadLocal();
        }
    };
    class CommucationOpTableMock : public Dic::Module::Timeline::CommucationOpTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<CommucationTaskOpPO> &result) override {
            QuerySimpleSliceByIdsGroupTrackForCommucationOpTable(fileId, result);
            ClearThreadLocal();
        }
    };
    class NpuInfoTableMock : public Dic::Module::Timeline::NpuInfoTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<NpuInfoPo> &result) override {
            QueryUniqueDeviceIdForNpuInfoTableMock(fileId, result);
            ClearThreadLocal();
        }
    };
    std::unique_ptr<Dic::Module::Timeline::TaskTable> tPtr = std::make_unique<TaskMock>();
    std::unique_ptr<Dic::Module::Timeline::CommucationTaskInfoTable> cmPtr =
        std::make_unique<CommucationTaskInfoTableMock>();
    std::unique_ptr<Dic::Module::Timeline::CommucationOpTable> copPtr = std::make_unique<CommucationOpTableMock>();
    std::unique_ptr<Dic::Module::Timeline::NpuInfoTable> niPtr = std::make_unique<NpuInfoTableMock>();
    HcclRepo hcclRepo;
    hcclRepo.SetTaskTable(std::move(tPtr));
    hcclRepo.SetCommucationTaskInfoTable(std::move(cmPtr));
    hcclRepo.SetCommucationOpTable(std::move(copPtr));
    std::unique_ptr<NpuInfoRepo> npr = std::make_unique<NpuInfoRepo>();
    npr->SetNpuInfoTable(std::move(niPtr));
    hcclRepo.SetNpuInfoRepo(std::move(npr));
    return hcclRepo;
}
/**
 * 测试全量DB的hccl的group泳道的根据trackId查询所有简单算子,正常情况
 */
TEST_F(HcclRepoTest, test_QuerySimpleSliceWithOutNameByTrackId_group_track_with_unique_key) {
    TrackInfoManager::Instance().Reset();
    HcclRepo baseRepo = GetHcclRepoMock();
    class HcclRepoMockForGroup : public HcclRepo {
      protected:
        void QuerySimpleSliceFromGroupTrackByDevice(std::vector<CommucationTaskOpPO> &commucationTaskOpPOVec,
            const TrackInfo &trackInfo, const std::string &tid, const SliceQuery &sliceQuery) override {
            QuerySimpleSliceByIdsGroupTrackForCommucationOpTable(trackInfo.cardId, commucationTaskOpPOVec);
        }
    };
    HcclRepoMockForGroup hcclRepo;
    SliceQuery sliceQuery;
    sliceQuery.endTime = UINT64_MAX;
    sliceQuery.trackId = TrackInfoManager::Instance().GetTrackId("999", "yyy", "999group");
    std::vector<SliceDomain> sliceVec;
    hcclRepo.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
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
 * 测试全量DB的hccl的plane泳道的根据trackId查询所有简单算子,正常情况
 */
TEST_F(HcclRepoTest, test_QuerySimpleSliceWithOutNameByTrackId_plane_track_with_normal) {
    TrackInfoManager::Instance().Reset();
    class TaskMock : public Dic::Module::Timeline::TaskTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<TaskPO> &result) override {
            QueryAllPlaneSliceForTaskTableMock(fileId, result);
            ClearThreadLocal();
        }
    };
    class CommucationTaskInfoTableMock : public Dic::Module::Timeline::CommucationTaskInfoTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<CommucationTaskInfoPO> &result) override {
            QueryOpIdsByGlabalTaskIdsForCommucationTaskInfoTable(fileId, result);
            ClearThreadLocal();
        }
    };
    std::unique_ptr<Dic::Module::Timeline::TaskTable> tPtr = std::make_unique<TaskMock>();
    std::unique_ptr<Dic::Module::Timeline::CommucationTaskInfoTable> cmPtr =
        std::make_unique<CommucationTaskInfoTableMock>();
    HcclRepo hcclRepo;
    hcclRepo.SetTaskTable(std::move(tPtr));
    hcclRepo.SetCommucationTaskInfoTable(std::move(cmPtr));
    SliceQuery sliceQuery;
    sliceQuery.trackId = TrackInfoManager::Instance().GetTrackId("999", "yyy", "999");
    sliceQuery.endTime = UINT64_MAX;
    std::vector<SliceDomain> sliceVec;
    hcclRepo.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
    const uint64_t expectSize = 2;
    EXPECT_EQ(sliceVec.size(), expectSize);
    auto it = sliceVec.begin();
    const uint64_t firstTimestamp = 28;
    EXPECT_EQ(it->timestamp, firstTimestamp);
    it++;
    const uint64_t lastTimestamp = 29;
    EXPECT_EQ(it->timestamp, lastTimestamp);
    TrackInfoManager::Instance().Reset();
}

/**
 * 测试查询group泳道详情
 */
TEST_F(HcclRepoTest, TestGroupQueryGroupSliceDetailInfo) {
    class HcclRepoMock : public HcclRepo {
      public:
        void SetMock(std::unique_ptr<CommucationOpTableMock> cTable, std::unique_ptr<EnumHcclDataTypeTable> eTable,
            std::unique_ptr<StringIdsTableMock> sTable) {
            commucationOpTable = std::move(cTable);
            enumHcclDataTypeTable = std::move(eTable);
            stringIdsTable = std::move(sTable);
        }
    };
    std::unique_ptr<CommucationOpTableMock> cTable = std::make_unique<CommucationOpTableMock>();
    std::unique_ptr<EnumHcclDataTypeTableMock> eTable = std::make_unique<EnumHcclDataTypeTableMock>();
    std::unique_ptr<StringIdsTableMock> sTable = std::make_unique<StringIdsTableMock>();
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, opTableSql);
    DatabaseTestCaseMockUtil::CreateTable(db, dataTypeDataSql);
    DatabaseTestCaseMockUtil::CreateTable(db, stringIdsSql);
    std::string opInsert =
        "INSERT INTO \"main\".\"COMMUNICATION_OP\" (\"opName\", \"startNs\", \"endNs\", \"connectionId\", "
        "\"groupName\", \"opId\", \"relay\", \"retry\", \"dataType\", \"algType\", \"count\", \"opType\", \"waitNs\") "
        "VALUES (377, 1718180919002130516, 1718180919038847310, 7429, 379, 1, 1, 0, 5, 380, 5, 336, 2259745);";
    std::string dataTypeInsert = "INSERT INTO \"main\".\"ENUM_HCCL_DATA_TYPE\" (\"id\", \"name\") VALUES (5, 'INT16');";
    std::string stringInsert = "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (377, 'mmmm');\n"
                               "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (380, 'aaaa');";
    DatabaseTestCaseMockUtil::InsertData(db, opInsert);
    DatabaseTestCaseMockUtil::InsertData(db, dataTypeInsert);
    DatabaseTestCaseMockUtil::InsertData(db, stringInsert);
    cTable->SetDb(db);
    eTable->SetDb(db);
    sTable->SetDb(db);
    HcclRepoMock hcclRepoMock;
    hcclRepoMock.SetMock(std::move(cTable), std::move(eTable), std::move(sTable));
    SliceQuery query;
    CompeteSliceDomain slice;
    query.sliceId = "1";
    const uint64_t trackId = TrackInfoManager::Instance().GetTrackId("hhh", "ppp", "379group");
    query.trackId = trackId;
    query.rankId = "hhh";
    bool result = hcclRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, true);
    EXPECT_EQ(slice.name, "mmmm");
    const uint64_t expectStart = 1718180919002130516;
    const uint64_t expectEnd = 1718180919038847310;
    EXPECT_EQ(slice.timestamp, expectStart);
    EXPECT_EQ(slice.endTime, expectEnd);
    EXPECT_EQ(slice.args,
        "{\"connectionId\":\"7429\",\"dataType\":\"INT16\",\"algType\":\"aaaa\",\"count\":\"5\","
        "\"relay\":\"yes\",\"retry\":\"no\"}");
}

/* *
 * 测试查询group泳道详情,算子不存在
 */
TEST_F(HcclRepoTest, TestGroupQueryGroupSliceDetailInfoWhenSliceNotExistThenRetuenFalse) {
    HcclRepo hcclRepoMock;
    SliceQuery query;
    CompeteSliceDomain slice;
    query.sliceId = "1|&$\n";
    const uint64_t trackId = TrackInfoManager::Instance().GetTrackId("hhh", "ppp", "379group");
    query.trackId = trackId;
    query.rankId = "hhh";
    bool result = hcclRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, false);
}

/**
 * 测试查询plane泳道详情,算子不存在
 */
TEST_F(HcclRepoTest, TestPlaneQueryGroupSliceDetailInfoWhenSliceNotExistThenRetuenFalse) {
    HcclRepo hcclRepoMock;
    SliceQuery query;
    CompeteSliceDomain slice;
    query.sliceId = "1|&$";
    const uint64_t trackId = TrackInfoManager::Instance().GetTrackId("hhh", "ppp", "1669");
    query.trackId = trackId;
    query.rankId = "hhh";
    bool result = hcclRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, false);
}

/**
 * 测试查询hccl泳道详情,泳道不存在
 */
TEST_F(HcclRepoTest, TestHcclQueryGroupSliceDetailInfoWhenSliceNotExistThenRetuenFalse) {
    HcclRepo hcclRepoMock;
    SliceQuery query;
    CompeteSliceDomain slice;
    query.sliceId = "1";
    query.trackId = 0;
    query.rankId = "hhh";
    bool result = hcclRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, false);
}

/**
 * 测试查询plane泳道详情
 */
TEST_F(HcclRepoTest, TestPlaneQueryGroupSliceDetailInfo) {
    class HcclRepoMock : public HcclRepo {
      public:
        void SetMock(HcclDependency &dependency) {
            taskTable = std::move(dependency.taskTableMock);
            commucationTaskInfoTable = std::move(dependency.commucationTaskInfoTableMock);
            stringIdsTable = std::move(dependency.stringIdsTableMock);
            enumHcclDataTypeTable = std::move(dependency.enumHcclDataTypeTableMock);
            enumHcclRdmaTypeTable = std::move(dependency.enumHcclRdmaTypeTableMock);
            enumHcclLinkTypeTable = std::move(dependency.enumHcclLinkTypeTableMock);
            enumHcclTransportTypeTable = std::move(dependency.enumHcclTransportTypeTableMock);
        }
    };
    HcclDependency dependency;
    TestPlaneQueryGroupSliceDetailInfoPrepare(dependency);
    HcclRepoMock hcclRepoMock;
    hcclRepoMock.SetMock(dependency);
    SliceQuery query;
    CompeteSliceDomain slice;
    query.sliceId = "1";
    const uint64_t trackId = TrackInfoManager::Instance().GetTrackId("hhh", "hccl", "1669");
    query.trackId = trackId;
    query.rankId = "hhh";
    bool result = hcclRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, true);
    EXPECT_EQ(slice.name, "kkkk");
    const uint64_t expectStart = 1718180918997464923;
    const uint64_t expectEnd = 1718180918997464923;
    EXPECT_EQ(slice.timestamp, expectStart);
    EXPECT_EQ(slice.endTime, expectEnd);
    const std::string expectArgs =
        "{\"notifyId\":\"0\",\"streamId\":\"16\",\"taskId\":\"3730\",\"contextId\":\"4294967295\",\"taskType\":"
        "\"kkkk\",\"srcRank\":\"1\",\"dstRank\":\"2\",\"transportType\":\"LOCAL\",\"size(Byte)\":\"40\",\"dataType\":"
        "\"INT16\","
        "\"linkType\":\"PCIE\",\"bandwidth(B/s)\":\"\",\"rdmaType\":\"RDMA_SEND_OP\"}";
    EXPECT_EQ(slice.args, expectArgs);
}

/**
 * 测试查询plane泳道详情(带有group信息，会根据局部rank获取全局rank)
 */
TEST_F(HcclRepoTest, TestPlaneQueryGroupSliceDetailInfoWithGroupInfo) {
    class HcclRepoMock : public HcclRepo {
      public:
        void SetMock(HcclDependency &dependency) {
            taskTable = std::move(dependency.taskTableMock);
            commucationTaskInfoTable = std::move(dependency.commucationTaskInfoTableMock);
            stringIdsTable = std::move(dependency.stringIdsTableMock);
            enumHcclDataTypeTable = std::move(dependency.enumHcclDataTypeTableMock);
            enumHcclRdmaTypeTable = std::move(dependency.enumHcclRdmaTypeTableMock);
            enumHcclLinkTypeTable = std::move(dependency.enumHcclLinkTypeTableMock);
            enumHcclTransportTypeTable = std::move(dependency.enumHcclTransportTypeTableMock);
        }
    };
    HcclDependency dependency;
    TestPlaneQueryGroupSliceDetailInfoPrepare(dependency);
    HcclRepoMock hcclRepoMock;
    hcclRepoMock.SetMock(dependency);
    TestMockGroupInfoCache();
    SliceQuery query;
    CompeteSliceDomain slice;
    query.sliceId = "1";
    const uint64_t trackId = TrackInfoManager::Instance().GetTrackId("hhh", "hccl", "1669");
    query.trackId = trackId;
    query.rankId = "hhh";
    bool result = hcclRepoMock.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, true);
    EXPECT_EQ(slice.name, "kkkk");
    const uint64_t expectStart = 1718180918997464923;
    const uint64_t expectEnd = 1718180918997464923;
    EXPECT_EQ(slice.timestamp, expectStart);
    EXPECT_EQ(slice.endTime, expectEnd);
    const std::string expectArgs =
        "{\"notifyId\":\"0\",\"streamId\":\"16\",\"taskId\":\"3730\",\"contextId\":\"4294967295\",\"taskType\":"
        "\"kkkk\",\"srcRank\":\"1\",\"dstRank\":\"2\",\"globalSrcRank\":\"5\",\"globalDstRank\":\"6\","
        "\"transportType\":\"LOCAL\",\"size(Byte)\":\"40\",\"dataType\":"
        "\"INT16\",\"linkType\":\"PCIE\",\"bandwidth(B/s)\":\"\",\"rdmaType\":\"RDMA_SEND_OP\"}";
    EXPECT_EQ(slice.args, expectArgs);
    MetaDataCacheManager::Instance().Clear();
}
