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
#include "PythonGcRepo.h"
#include "TrackInfoManager.h"
#include "../../../DatabaseTestCaseMockUtil.h"
#include "TableDefaultMock.h"
using namespace Dic::Module::Timeline;
using namespace Dic::TimeLine::Table::Default::Mock;
using namespace Dic::Global::PROFILER::MockUtil;
class PythonGCRepoTest : public ::testing::Test {
  protected:
    const std::string gcSql = "CREATE TABLE GC_RECORD (startNs INTEGER,endNs INTEGER, globalTid INTEGER);";
    void SetUp() override { TrackInfoManager::Instance().Reset(); }

    void TearDown() override { TrackInfoManager::Instance().Reset(); }
};

TEST_F(PythonGCRepoTest, TestQuerySliceDetailInfoNormal) {
    class PythonGCRepoMock : public PythonGcRepo {
      public:
        void SetMock(PythonGcDependency &dependency) { table = std::move(dependency.tableMock); }
    };
    sqlite3 *db = nullptr;
    ASSERT_EQ(sqlite3_open(":memory:", &db), SQLITE_OK);
    const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> database(db, sqlite3_close);
    ASSERT_EQ(sqlite3_exec(db, gcSql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
    std::string pythonGcInsert = "INSERT INTO GC_RECORD (\"startNs\", \"endNs\", \"globalTid\") VALUES "
                                 "(1000, 1250, 65535);";
    ASSERT_EQ(sqlite3_exec(db, pythonGcInsert.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);

    PythonGcDependency dependency;
    dependency.tableMock = std::make_unique<PythonGCTableMock>();
    dependency.tableMock->SetDb(db);
    PythonGCRepoMock pythonGCRepoMock;
    pythonGCRepoMock.SetMock(dependency);
    SliceQuery query;
    query.sliceId = "1";
    query.rankId = "hhh";
    CompeteSliceDomain slice;
    bool result = pythonGCRepoMock.QuerySliceDetailInfo(query, slice);
    ASSERT_TRUE(result);
    EXPECT_EQ(slice.id, 1u);
    EXPECT_EQ(slice.timestamp, 1000u);
    EXPECT_EQ(slice.endTime, 1250u);
    EXPECT_EQ(slice.name, "PythonGC");

    ASSERT_EQ(sqlite3_exec(db, "DELETE FROM GC_RECORD", nullptr, nullptr, nullptr), SQLITE_OK);
    CompeteSliceDomain missing;
    EXPECT_FALSE(pythonGCRepoMock.QuerySliceDetailInfo(query, missing));
    EXPECT_TRUE(missing.name.empty());
}

/**
 * 测试全量DB的 pythonGcRepo 转化 SliceInterface 的情况
 */
TEST_F(PythonGCRepoTest, TestDynamicCastOfMultiSliceInterface) {
    std::shared_ptr<IBaseSliceRepo> pythonGcRepo = std::make_shared<PythonGcRepo>();
    // 转 IPythonFuncSlice 失败
    const auto pythonFuncRepo = dynamic_cast<IPythonFuncSlice *>(pythonGcRepo.get());
    EXPECT_EQ(pythonFuncRepo, nullptr);
    // 转 IFindSliceByNameList 失败
    const auto findSliceByNameList = dynamic_cast<IFindSliceByNameList *>(pythonGcRepo.get());
    EXPECT_EQ(findSliceByNameList, nullptr);
    // 转 IFindSliceByTimepointAndName 失败
    const auto findSliceByTimepointAndName = dynamic_cast<IFindSliceByTimepointAndName *>(pythonGcRepo.get());
    EXPECT_EQ(findSliceByTimepointAndName, nullptr);
    // 转 ITextSlice 失败
    const auto textSliceRepo = dynamic_cast<ITextSlice *>(pythonGcRepo.get());
    EXPECT_EQ(textSliceRepo, nullptr);
}
