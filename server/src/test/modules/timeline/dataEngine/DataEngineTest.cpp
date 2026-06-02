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
#include "DataEngine.h"
#include "RepositoryFactory.h"
#include "HcclRepo.h"
#include "TrackInfoManager.h"
#include "DominQuery.h"
#include "FileUtil.h"

using namespace Dic::Module::Timeline;
class DataEngineTest : public ::testing::Test {
  public:
    static std::string g_testDbPath;
    static std::recursive_mutex g_testMutex;
    static Module::Database g_testDataBase;
    const std::string pytorch_api =
        "CREATE TABLE PYTORCH_API (startNs TEXT, endNs TEXT, globalTid INTEGER, "
        "connectionId INTEGER, name INTEGER, sequenceNumber INTEGER, fwdThreadId INTEGER, inputDtypes INTEGER, "
        "inputShapes INTEGER, callchainId INTEGER, type INTEGER, depth integer);";
    const std::string enum_api_type = "CREATE TABLE ENUM_API_TYPE (id INTEGER PRIMARY KEY,name TEXT);";
    static void SetUpTestSuite() {
        std::string currPath = Dic::FileUtil::GetCurrPath();
        const u_long index = currPath.find("server");
        currPath = currPath.substr(0, index + std::strlen("server"));
        g_testDbPath = Dic::FileUtil::SplicePath(currPath, "src", "test", "test_data", "test_data_engine.db");
        g_testDataBase.OpenDb(g_testDbPath, true);
        DataBaseManager::Instance().SetDataType(DataType::DB, g_testDbPath);
        DataBaseManager::Instance().CreateTraceConnectionPool("0", g_testDbPath);
    }

    static void TearDownTestSuite() {
        DataBaseManager::Instance().Clear(DatabaseType::TRACE);
        g_testDataBase.CloseDb();
        if (FileUtil::CheckFilePathExist(g_testDbPath) && !FileUtil::RemoveFile(g_testDbPath)) {
            printf("Remove error: %s", strerror(errno));
        }
    }

    void SetUp() override { TrackInfoManager::Instance().Reset(); }

    void TearDown() override { TrackInfoManager::Instance().Reset(); }

  protected:
    class SliceRepoMock : public Dic::Module::Timeline::HcclRepo {
      public:
        void QuerySimpleSliceWithOutNameByTrackId(
            const SliceQuery &sliceQuery, std::vector<SliceDomain> &sliceVec) override {
            SliceDomain sliceDomain1;
            SliceDomain sliceDomain2;
            SliceDomain sliceDomain3;
            const uint64_t expectStart1 = 10;
            const uint64_t expectId1 = 10;
            const uint64_t expectStart2 = 7;
            const uint64_t expectId2 = 11;
            const uint64_t expectStart3 = 7;
            const uint64_t expectId3 = 12;
            sliceDomain1.timestamp = expectStart1;
            sliceDomain1.id = expectId1;
            sliceDomain2.timestamp = expectStart2;
            sliceDomain2.id = expectId2;
            sliceDomain3.timestamp = expectStart3;
            sliceDomain3.id = expectId3;
            sliceVec.emplace_back(sliceDomain1);
            sliceVec.emplace_back(sliceDomain2);
            sliceVec.emplace_back(sliceDomain3);
        }
    };

    class RespotoryFactoryMock : public Dic::Module::Timeline::RepositoryFactory {
      public:
        std::shared_ptr<IBaseSliceRepo> GetSliceRespo(PROCESS_TYPE) override {
            std::shared_ptr<IBaseSliceRepo> res = std::make_shared<SliceRepoMock>();
            return res;
        }
    };
};

std::string DataEngineTest::g_testDbPath;
std::recursive_mutex DataEngineTest::g_testMutex;
Module::Database DataEngineTest::g_testDataBase(g_testMutex);

TEST_F(DataEngineTest, QueryAllThreadInfoNormalTest) {
    std::unordered_map<uint64_t, std::pair<std::string, std::string>> threadInfo;
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        ThreadQuery threadQuery;
        dataEngine.QueryAllThreadInfo(threadQuery, threadInfo);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        threadQuery.metaType = PROCESS_TYPE::CANN_API;
        dataEngine.QueryAllThreadInfo(threadQuery, threadInfo);
        threadQuery.metaType = PROCESS_TYPE::MS_TX;
        dataEngine.QueryAllThreadInfo(threadQuery, threadInfo);
        threadQuery.metaType = PROCESS_TYPE::API;
        dataEngine.QueryAllThreadInfo(threadQuery, threadInfo);
        threadQuery.metaType = PROCESS_TYPE::OVERLAP_ANALYSIS;
        dataEngine.QueryAllThreadInfo(threadQuery, threadInfo);
        threadQuery.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
        dataEngine.QueryAllThreadInfo(threadQuery, threadInfo);
        threadQuery.metaType = PROCESS_TYPE::HCCL;
        dataEngine.QueryAllThreadInfo(threadQuery, threadInfo);
        threadQuery.metaType = PROCESS_TYPE::NONE;
        dataEngine.QueryAllThreadInfo(threadQuery, threadInfo);
        threadQuery.metaType = PROCESS_TYPE::TEXT;
        dataEngine.QueryAllThreadInfo(threadQuery, threadInfo);
    });
}

TEST_F(DataEngineTest, QuerySimpleSliceWithOutNameByTrackIdTestWithOutFactory) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        SliceQuery sliceQuery;
        std::vector<SliceDomain> sliceVec;
        dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        sliceQuery.metaType = PROCESS_TYPE::CANN_API;
        dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::MS_TX;
        dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::API;
        dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::OVERLAP_ANALYSIS;
        dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
        dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::HCCL;
        dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::NONE;
        dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::TEXT;
        dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::PYTHON_GC;
        dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
    });
}

TEST_F(DataEngineTest, QueryPythonFunctionCountByTrackIdTestWithOutFactory) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        SliceQuery sliceQuery;
        dataEngine.QueryPythonFunctionCountByTrackId(sliceQuery);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        sliceQuery.metaType = PROCESS_TYPE::CANN_API;
        dataEngine.QueryPythonFunctionCountByTrackId(sliceQuery);
        sliceQuery.metaType = PROCESS_TYPE::MS_TX;
        dataEngine.QueryPythonFunctionCountByTrackId(sliceQuery);
        sliceQuery.metaType = PROCESS_TYPE::API;
        dataEngine.QueryPythonFunctionCountByTrackId(sliceQuery);
        sliceQuery.metaType = PROCESS_TYPE::OVERLAP_ANALYSIS;
        dataEngine.QueryPythonFunctionCountByTrackId(sliceQuery);
        sliceQuery.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
        dataEngine.QueryPythonFunctionCountByTrackId(sliceQuery);
        sliceQuery.metaType = PROCESS_TYPE::HCCL;
        dataEngine.QueryPythonFunctionCountByTrackId(sliceQuery);
        sliceQuery.metaType = PROCESS_TYPE::NONE;
        dataEngine.QueryPythonFunctionCountByTrackId(sliceQuery);
        sliceQuery.metaType = PROCESS_TYPE::TEXT;
        dataEngine.QueryPythonFunctionCountByTrackId(sliceQuery);
    });
}

TEST_F(DataEngineTest, QuerySliceIdsByCatTestWithOutFactory) {
    g_testDataBase.ExecSql(pytorch_api);
    g_testDataBase.ExecSql(enum_api_type);
    const std::string apiData =
        "INSERT INTO \"PYTORCH_API\" (\"startNs\", \"endNs\", \"globalTid\", \"connectionId\", \"name\", "
        "\"sequenceNumber\", \"fwdThreadId\", \"inputDtypes\", \"inputShapes\", \"callchainId\", \"type\", \"depth\") "
        "VALUES ('1737098043288321660', '1737098043288323230', 59601261180469, NULL, 268435513, NULL, NULL, NULL, "
        "NULL, NULL, 50003, 10);";
    const std::string enumData = R"(INSERT INTO "ENUM_API_TYPE" ("id", "name") VALUES (50003, 'trace');)";
    g_testDataBase.ExecSql(apiData);
    g_testDataBase.ExecSql(enumData);
    DataBaseManager::Instance().SetDbPathMapping("0", g_testDbPath, "");
    Dic::Module::Timeline::DataEngine dataEngine;
    SliceQuery sliceQuery;
    std::vector<uint64_t> sliceIds;
    EXPECT_NO_THROW({
        dataEngine.QuerySliceIdsByCat(sliceQuery, sliceIds);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        sliceQuery.metaType = PROCESS_TYPE::CANN_API;
        dataEngine.QuerySliceIdsByCat(sliceQuery, sliceIds);
        sliceQuery.metaType = PROCESS_TYPE::MS_TX;
        dataEngine.QuerySliceIdsByCat(sliceQuery, sliceIds);
        sliceQuery.metaType = PROCESS_TYPE::OVERLAP_ANALYSIS;
        dataEngine.QuerySliceIdsByCat(sliceQuery, sliceIds);
        sliceQuery.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
        dataEngine.QuerySliceIdsByCat(sliceQuery, sliceIds);
        sliceQuery.metaType = PROCESS_TYPE::HCCL;
        dataEngine.QuerySliceIdsByCat(sliceQuery, sliceIds);
        sliceQuery.metaType = PROCESS_TYPE::NONE;
        dataEngine.QuerySliceIdsByCat(sliceQuery, sliceIds);
        sliceQuery.metaType = PROCESS_TYPE::TEXT;
        dataEngine.QuerySliceIdsByCat(sliceQuery, sliceIds);
    });
    sliceQuery.metaType = PROCESS_TYPE::API;
    sliceQuery.rankId = "0";
    sliceQuery.trackId = TrackInfoManager::Instance().GetTrackId("0", "59601261180469", "0");
    sliceQuery.endTime = UINT64_MAX;
    dataEngine.QuerySliceIdsByCat(sliceQuery, sliceIds);
    EXPECT_EQ(sliceIds.size(), 1);
}

TEST_F(DataEngineTest, QueryCompeteSliceVecByTimeRangeAndTrackIdTestWithOutFactory) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        SliceQuery sliceQuery;
        std::vector<CompeteSliceDomain> sliceVec;
        dataEngine.QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, sliceVec);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        sliceQuery.metaType = PROCESS_TYPE::CANN_API;
        dataEngine.QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::MS_TX;
        dataEngine.QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::API;
        dataEngine.QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::OVERLAP_ANALYSIS;
        dataEngine.QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
        dataEngine.QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::HCCL;
        dataEngine.QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::NONE;
        dataEngine.QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::TEXT;
        dataEngine.QueryCompeteSliceVecByTimeRangeAndTrackId(sliceQuery, sliceVec);
    });
}

TEST_F(DataEngineTest, QueryFlowPointByTimeRangeTestWithOutFactory) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        FlowQuery sliceQuery;
        std::vector<FlowPoint> sliceVec;
        dataEngine.QueryFlowPointByTimeRange(sliceQuery, sliceVec);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        sliceQuery.metaType = PROCESS_TYPE::CANN_API;
        dataEngine.QueryFlowPointByTimeRange(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::MS_TX;
        dataEngine.QueryFlowPointByTimeRange(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::API;
        dataEngine.QueryFlowPointByTimeRange(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::OVERLAP_ANALYSIS;
        dataEngine.QueryFlowPointByTimeRange(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
        dataEngine.QueryFlowPointByTimeRange(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::HCCL;
        dataEngine.QueryFlowPointByTimeRange(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::NONE;
        dataEngine.QueryFlowPointByTimeRange(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::TEXT;
        dataEngine.QueryFlowPointByTimeRange(sliceQuery, sliceVec);
    });
}

TEST_F(DataEngineTest, QueryFlowPointByFlowIdTestWithOutFactory) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        FlowQuery sliceQuery;
        std::vector<FlowPoint> sliceVec;
        dataEngine.QueryFlowPointByFlowId(sliceQuery, sliceVec);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        sliceQuery.metaType = PROCESS_TYPE::CANN_API;
        dataEngine.QueryFlowPointByFlowId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::MS_TX;
        dataEngine.QueryFlowPointByFlowId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::API;
        dataEngine.QueryFlowPointByFlowId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::OVERLAP_ANALYSIS;
        dataEngine.QueryFlowPointByFlowId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
        dataEngine.QueryFlowPointByFlowId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::HCCL;
        dataEngine.QueryFlowPointByFlowId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::NONE;
        dataEngine.QueryFlowPointByFlowId(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::TEXT;
        dataEngine.QueryFlowPointByFlowId(sliceQuery, sliceVec);
    });
}

TEST_F(DataEngineTest, QueryCompeteSliceByIdsTestWithOutFactory) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        SliceQuery sliceQuery;
        std::vector<uint64_t> sliceIds;
        std::vector<CompeteSliceDomain> sliceVec;
        dataEngine.QueryCompeteSliceByIds(sliceQuery, sliceIds, sliceVec);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        sliceQuery.metaType = PROCESS_TYPE::CANN_API;
        dataEngine.QueryCompeteSliceByIds(sliceQuery, sliceIds, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::MS_TX;
        dataEngine.QueryCompeteSliceByIds(sliceQuery, sliceIds, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::API;
        dataEngine.QueryCompeteSliceByIds(sliceQuery, sliceIds, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::OVERLAP_ANALYSIS;
        dataEngine.QueryCompeteSliceByIds(sliceQuery, sliceIds, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
        dataEngine.QueryCompeteSliceByIds(sliceQuery, sliceIds, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::HCCL;
        dataEngine.QueryCompeteSliceByIds(sliceQuery, sliceIds, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::NONE;
        dataEngine.QueryCompeteSliceByIds(sliceQuery, sliceIds, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::TEXT;
        dataEngine.QueryCompeteSliceByIds(sliceQuery, sliceIds, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::PYTHON_GC;
        dataEngine.QueryCompeteSliceByIds(sliceQuery, sliceIds, sliceVec);
    });
}

TEST_F(DataEngineTest, QuerySliceDetailInfoTestWithOutFactory) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        SliceQuery sliceQuery;
        CompeteSliceDomain sliceVec;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        sliceQuery.metaType = PROCESS_TYPE::CANN_API;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::MS_TX;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::API;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::OVERLAP_ANALYSIS;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::HCCL;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::NONE;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::TEXT;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
    });
}

TEST_F(DataEngineTest, QuerySliceDetailInfoFailTest) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        SliceQuery sliceQuery;
        sliceQuery.sliceId = "\n\t\bAA\v";
        CompeteSliceDomain sliceVec;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        sliceQuery.metaType = PROCESS_TYPE::API;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
        sliceQuery.metaType = PROCESS_TYPE::OVERLAP_ANALYSIS;
        dataEngine.QuerySliceDetailInfo(sliceQuery, sliceVec);
    });
}

TEST_F(DataEngineTest, QueryAllFlagSliceTestWithOutFactory) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        SliceQuery sliceQuery;
        std::vector<CompeteSliceDomain> sliceVec;
        dataEngine.QueryAllFlagSlice(sliceQuery, sliceVec);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        dataEngine.QueryAllFlagSlice(sliceQuery, sliceVec);
    });
}

TEST_F(DataEngineTest, QueryFlowPointByCategoryTestWithOutFactory) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        FlowQuery sliceQuery;
        std::vector<FlowPoint> sliceVec;
        dataEngine.QueryFlowPointByCategory(sliceQuery, sliceVec);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        dataEngine.QueryFlowPointByCategory(sliceQuery, sliceVec);
    });
}

TEST_F(DataEngineTest, QuerySliceByTimepointAndNameTestWithOutFactory) {
    EXPECT_NO_THROW({
        Dic::Module::Timeline::DataEngine dataEngine;
        SliceQuery sliceQuery;
        CompeteSliceDomain sliceDomain;
        dataEngine.QuerySliceByTimepointAndName(sliceQuery, sliceDomain);
        std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
            std::make_shared<Dic::Module::Timeline::RepositoryFactory>();
        dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
        sliceQuery.metaType = PROCESS_TYPE::ASCEND_HARDWARE;
        dataEngine.QuerySliceByTimepointAndName(sliceQuery, sliceDomain);
        sliceQuery.metaType = PROCESS_TYPE::API;
        dataEngine.QuerySliceByTimepointAndName(sliceQuery, sliceDomain);
    });
}

TEST_F(DataEngineTest, QuerySimpleSliceWithOutNameByTrackId) {
    Dic::Module::Timeline::DataEngine dataEngine;
    std::shared_ptr<Dic::Module::Timeline::RepositoryFactoryInterface> repositoryFactoryInterface =
        std::make_shared<RespotoryFactoryMock>();
    dataEngine.SetRepositoryFactory(repositoryFactoryInterface);
    const SliceQuery sliceQuery;
    std::vector<SliceDomain> sliceVec;
    dataEngine.QuerySimpleSliceWithOutNameByTrackId(sliceQuery, sliceVec);
    const uint64_t expectSize = 3;
    const uint64_t first = 0;
    const uint64_t third = 2;
    EXPECT_EQ(sliceVec.size(), expectSize);
    const uint64_t firstTime = 7;
    EXPECT_EQ(sliceVec[first].timestamp, firstTime);
    const uint64_t firstId = 11;
    EXPECT_EQ(sliceVec[first].id, firstId);
    const uint64_t thirdTime = 10;
    EXPECT_EQ(sliceVec[third].timestamp, thirdTime);
    const uint64_t thirdId = 10;
    EXPECT_EQ(sliceVec[third].id, thirdId);
}
