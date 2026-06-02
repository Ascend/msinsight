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
#include "PythonApiRepo.h"
#include "TrackInfoManager.h"
#include "../../../DatabaseTestCaseMockUtil.h"
#include "TableDefaultMock.h"
using namespace Dic::Module::Timeline;
using namespace Dic::TimeLine::Table::Default::Mock;
using namespace Dic::Global::PROFILER::MockUtil;
class PythonApiRepoTest : public ::testing::Test {
  protected:
    class PythonApiRepoRepoMock : public PythonApiRepo {
      public:
        void SetMock(PytorchApiDependency &dependency) {
            pytorchApiTable = std::move(dependency.pytorchApiTableMock);
            pytorchCallchainsTable = std::move(dependency.pytorchCallchainsTableMock);
            stringIdsTable = std::move(dependency.stringIdsTableMock);
        }
    };
    const std::string pythonApiSql =
        "CREATE TABLE PYTORCH_API (startNs TEXT, endNs TEXT, globalTid INTEGER, connectionId INTEGER, name INTEGER, "
        "sequenceNumber INTEGER, fwdThreadId INTEGER, inputDtypes INTEGER, inputShapes INTEGER, callchainId INTEGER, "
        "depth integer);";
    const std::string pythonApiWithTypeSql =
        "CREATE TABLE PYTORCH_API (startNs TEXT, endNs TEXT, globalTid INTEGER, connectionId INTEGER, name INTEGER, "
        "sequenceNumber INTEGER, fwdThreadId INTEGER, inputDtypes INTEGER, inputShapes INTEGER, callchainId INTEGER, "
        "type INTEGER, depth integer);";
    const std::string stringIdsSql = "CREATE TABLE STRING_IDS (id INTEGER PRIMARY KEY,value TEXT);";
    const std::string chainSql = "CREATE TABLE PYTORCH_CALLCHAINS (id INTEGER, stack INTEGER, stackDepth INTEGER);";
    void SetUp() override { TrackInfoManager::Instance().Reset(); }

    void TearDown() override { TrackInfoManager::Instance().Reset(); }

    void TestQuerySliceDetailInfoNormalPrepare(PytorchApiDependency &dependency) {
        sqlite3 *db = nullptr;
        DatabaseTestCaseMockUtil::OpenDB(db);
        DatabaseTestCaseMockUtil::CreateTable(db, pythonApiSql);
        DatabaseTestCaseMockUtil::CreateTable(db, stringIdsSql);
        DatabaseTestCaseMockUtil::CreateTable(db, chainSql);
        std::string pythonApiInsert =
            "INSERT INTO \"main\".\"PYTORCH_API\" (\"startNs\", \"endNs\", \"globalTid\", \"connectionId\", \"name\", "
            "\"sequenceNumber\", \"fwdThreadId\", \"inputDtypes\", \"inputShapes\", \"callchainId\", \"depth\") VALUES "
            "('1718180919237611490', '1718180919237618540', 8785587534252168, 820, 268435456, 1, 2, 3, 4, "
            "5, 4);";
        std::string chainInsert =
            "INSERT INTO \"main\".\"PYTORCH_CALLCHAINS\" (\"id\", \"stack\", \"stackDepth\") "
            "VALUES (5, 268436792, 0);\n"
            "INSERT INTO \"main\".\"PYTORCH_CALLCHAINS\" (\"id\", \"stack\", \"stackDepth\") VALUES (5, 268436793, 1);";
        std::string stringInsert =
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (268435456, 'qqq');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (3, 'aaa');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (4, 'nnn');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (268436792, 'bbb');\n"
            "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (268436793, 'ggg');";
        DatabaseTestCaseMockUtil::InsertData(db, pythonApiInsert);
        DatabaseTestCaseMockUtil::InsertData(db, chainInsert);
        DatabaseTestCaseMockUtil::InsertData(db, stringInsert);
        dependency.stringIdsTableMock = std::make_unique<StringIdsTableMock>();
        dependency.stringIdsTableMock->SetDb(db);
        dependency.pytorchApiTableMock = std::make_unique<PytorchApiTableMock>();
        dependency.pytorchApiTableMock->SetDb(db);
        dependency.pytorchCallchainsTableMock = std::make_unique<PytorchCallchainsTableMock>();
        dependency.pytorchCallchainsTableMock->SetDb(db);
    }
};

/**
 * 测试根据id查询算子详情,正常情况
 */
TEST_F(PythonApiRepoTest, TestQuerySliceDetailInfoNormal) {
    class PythonApiRepoRepoMock : public PythonApiRepo {
      public:
        void SetMock(PytorchApiDependency &dependency) {
            pytorchApiTable = std::move(dependency.pytorchApiTableMock);
            pytorchCallchainsTable = std::move(dependency.pytorchCallchainsTableMock);
            stringIdsTable = std::move(dependency.stringIdsTableMock);
        }
    };
    PytorchApiDependency dependency;
    TestQuerySliceDetailInfoNormalPrepare(dependency);
    PythonApiRepoRepoMock repo;
    repo.SetMock(dependency);
    SliceQuery query;
    query.sliceId = "1";
    query.rankId = "hhh";
    CompeteSliceDomain slice;
    bool result = repo.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, true);
    const uint64_t expectStart = 1718180919237611490;
    const uint64_t expectEnd = 1718180919237618540;
    EXPECT_EQ(slice.name, "qqq");
    EXPECT_EQ(slice.timestamp, expectStart);
    EXPECT_EQ(slice.endTime, expectEnd);
    const std::string expectArgs = "{\"sequenceNumber\":\"1\",\"fwdThreadId\":\"2\",\"connectionId\":\"820\","
                                   "\"inputShapes\":\"nnn\",\"inputDtypes\":\"aaa\",\"Call stack\":\"bbb;\\nggg;\\n\"}";
    EXPECT_EQ(slice.args, expectArgs);
}

/**
 * 测试根据id查询算子详情,算子不存在的情况
 */
TEST_F(PythonApiRepoTest, TestQuerySliceDetailInfoWhenSliceNotExistThenReturnFalse) {
    PythonApiRepo repo;
    SliceQuery query;
    query.sliceId = "1";
    query.rankId = "hhh";
    CompeteSliceDomain slice;
    bool result = repo.QuerySliceDetailInfo(query, slice);
    EXPECT_EQ(result, false);
}

/**
 * 根据时间点查询算子，名字不存在
 */
TEST_F(PythonApiRepoTest, TestQuerySliceByTimepointAndNameWhenNameNotExistThenReturnFalse) {
    PytorchApiDependency dependency;
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, stringIdsSql);
    dependency.stringIdsTableMock->SetDb(db);
    PythonApiRepoRepoMock repo;
    repo.SetMock(dependency);
    SliceQuery sliceQuery;
    CompeteSliceDomain competeSliceDomain;
    bool result = repo.QuerySliceByTimepointAndName(sliceQuery, competeSliceDomain);
    EXPECT_EQ(result, false);
}

/**
 * 根据时间点查询算子，名字存在，但没有算子信息
 */
TEST_F(PythonApiRepoTest, TestQuerySliceByTimepointAndNameWhenNameExistAndPytorchNotExistThenReturnFalse) {
    PytorchApiDependency dependency;
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, stringIdsSql);
    const std::string strIdsData =
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (7, 'aclnnCast_CastAiCore_Cast');";
    DatabaseTestCaseMockUtil::InsertData(db, strIdsData);
    DatabaseTestCaseMockUtil::CreateTable(db, pythonApiWithTypeSql);
    dependency.stringIdsTableMock->SetDb(db);
    dependency.pytorchApiTableMock->SetDb(db);
    PythonApiRepoRepoMock repo;
    repo.SetMock(dependency);
    SliceQuery sliceQuery;
    CompeteSliceDomain competeSliceDomain;
    sliceQuery.name = "aclnnCast_CastAiCore_Cast";
    bool result = repo.QuerySliceByTimepointAndName(sliceQuery, competeSliceDomain);
    EXPECT_EQ(result, false);
}

/**
 * 根据时间点查询算子，名字存在，也有算子信息
 */
TEST_F(PythonApiRepoTest, TestQuerySliceByTimepointAndNameNormal) {
    PytorchApiDependency dependency;
    sqlite3 *db = nullptr;
    DatabaseTestCaseMockUtil::OpenDB(db);
    DatabaseTestCaseMockUtil::CreateTable(db, stringIdsSql);
    const std::string strIdsData =
        "INSERT INTO \"main\".\"STRING_IDS\" (\"id\", \"value\") VALUES (7, 'aclnnCast_CastAiCore_Cast');";
    DatabaseTestCaseMockUtil::InsertData(db, strIdsData);
    DatabaseTestCaseMockUtil::CreateTable(db, pythonApiWithTypeSql);
    const std::string pythonData =
        "INSERT INTO \"main\".\"PYTORCH_API\" (\"startNs\", \"endNs\", \"globalTid\", \"connectionId\", \"name\", "
        "\"sequenceNumber\", \"fwdThreadId\", \"inputDtypes\", \"inputShapes\", \"callchainId\", \"type\", \"depth\") "
        "VALUES ('1724670453434388370', '1724670453434401040', 1584297471746281, 1, 7, NULL, NULL, NULL, NULL, "
        "NULL, 50002, 13);";
    DatabaseTestCaseMockUtil::InsertData(db, pythonData);
    dependency.stringIdsTableMock->SetDb(db);
    dependency.pytorchApiTableMock->SetDb(db);
    PythonApiRepoRepoMock repo;
    repo.SetMock(dependency);
    SliceQuery sliceQuery;
    CompeteSliceDomain competeSliceDomain;
    sliceQuery.name = "aclnnCast_CastAiCore_Cast";
    const uint64_t targetTimepoint = 1724670453434388400;
    sliceQuery.timePoint = targetTimepoint;
    sliceQuery.rankId = "mmmmmmmmmm";
    std::string hostCardId = "lllllllll";
    TrackInfoManager::Instance().UpdateHostCardId(sliceQuery.rankId, hostCardId);
    bool result = repo.QuerySliceByTimepointAndName(sliceQuery, competeSliceDomain);
    const uint64_t one = 1;
    EXPECT_EQ(result, true);
    EXPECT_EQ(competeSliceDomain.id, one);
    const uint64_t expectStart = 1724670453434388370;
    EXPECT_EQ(competeSliceDomain.timestamp, expectStart);
    const uint64_t expectEnd = 1724670453434401040;
    EXPECT_EQ(competeSliceDomain.endTime, expectEnd);
    EXPECT_EQ(competeSliceDomain.pid, "1584297471746281");
    EXPECT_EQ(competeSliceDomain.tid, "pytorch");
    EXPECT_EQ(competeSliceDomain.trackId, one);
    EXPECT_EQ(competeSliceDomain.duration, competeSliceDomain.endTime - competeSliceDomain.timestamp);
    EXPECT_EQ(competeSliceDomain.cardId, sliceQuery.rankId);
}

/**
 * 测试全量DB的 pythonApiRepo 转化 SliceInterface 的情况
 */
TEST_F(PythonApiRepoTest, TestDynamicCastOfMultiSliceInterface) {
    std::shared_ptr<IBaseSliceRepo> pythonApiRepo = std::make_shared<PythonApiRepo>();
    // 转 IPythonFuncSlice 成功
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(pythonApiRepo.get());
    EXPECT_NE(pythonFuncRepo, nullptr);
    // 转 IFindSliceByNameList 失败
    const auto findSliceByNameList = dynamic_cast<IFindSliceByNameList *>(pythonApiRepo.get());
    EXPECT_EQ(findSliceByNameList, nullptr);
    // 转 IFindSliceByTimepointAndName 成功
    const auto findSliceByTimepointAndName = dynamic_cast<IFindSliceByTimepointAndName *>(pythonApiRepo.get());
    EXPECT_NE(findSliceByTimepointAndName, nullptr);
    // 转 ITextSlice 失败
    const auto textSliceRepo = dynamic_cast<ITextSlice *>(pythonApiRepo.get());
    EXPECT_EQ(textSliceRepo, nullptr);
}
