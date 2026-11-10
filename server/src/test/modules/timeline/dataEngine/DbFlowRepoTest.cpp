/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include <limits>
#include "TimelineMemoryDatabaseFixture.h"
#include "DbFlowRepo.h"
#include "DbTraceDataBase.h"
#include "ConstantDefs.h"
#include "FlowAnalyzer.h"
#include "HardWareRepo.h"
using namespace Dic::Module::Timeline;
using namespace Dic::Module::Timeline::Test;
namespace {
class HardwareLaneQuery : public HardWareRepo {
  public:
    using HardWareRepo::PrepareStmtForQuerySimpleSliceWithOutNameByTrackId;
};
class DbFlowRepoTest : public TimelineMemoryDatabaseTestBase {
  protected:
    bool InitGraphCategories(bool refresh = false) {
        const auto db = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(database);
        return db != nullptr && db->InitHostToDeviceGraphConnectionCats(refresh);
    }
    // 一条已有 Host 分类及一对有效模型端点；这里只准备数据，不预先生成 Graph 缓存。
    bool SeedGraphCategoryData() {
        return database->ExecSql("ALTER TABLE TASK ADD COLUMN globalPid INTEGER;"
                                 "CREATE TABLE connectionCats(connectionId INTEGER,cat TEXT);"
                                 "INSERT INTO connectionCats VALUES(8,'HostToDevice');"
                                 "INSERT INTO STRING_IDS VALUES(13,'MODEL_EXECUTE');"
                                 "INSERT INTO CANN_API VALUES(110,120,4294967298,20000,7,1,2);"
                                 "INSERT INTO TASK VALUES(130,150,7,46,0,1,13,4,1);");
    }
    uint64_t CountCategory(const std::string &category) {
        auto stmt = database->CreatPreparedStatement("SELECT count(*) AS total FROM connectionCats WHERE cat=?");
        if (stmt == nullptr) {
            ADD_FAILURE() << "Failed to query category cache";
            return UINT64_MAX;
        }
        auto result = stmt->ExecuteQuery(category);
        if (result == nullptr || !result->Next()) {
            ADD_FAILURE() << "Failed to read category count";
            return UINT64_MAX;
        }
        return result->GetUint64("total");
    }
    std::vector<FlowPoint> Query(const std::string &category) {
        DbFlowRepo repo;
        FlowQuery query;
        query.fileId = rank;
        query.cat = category;
        query.minTimestamp = 100;
        std::vector<FlowPoint> result;
        repo.QueryFlowPointByCategory(query, result);
        return result;
    }
    void AssertPair(const std::vector<FlowPoint> &points) {
        ASSERT_EQ(points.size(), 2U);
        const auto start = std::find_if(points.begin(), points.end(), [](const auto &p) { return p.type == "s"; });
        const auto end = std::find_if(points.begin(), points.end(), [](const auto &p) { return p.type == "f"; });
        ASSERT_NE(start, points.end());
        ASSERT_NE(end, points.end());
        EXPECT_EQ(start->flowId, "7");
        EXPECT_EQ(end->flowId, "7");
        EXPECT_EQ(start->timestamp, 10U);
        EXPECT_EQ(end->timestamp, 30U);
        EXPECT_EQ(start->depth, 2U);
        EXPECT_EQ(end->depth, 4U);
        EXPECT_TRUE(std::is_sorted(points.begin(), points.end()));
    }
};
TEST_F(DbFlowRepoTest, HostToDeviceUsesBothRepositoriesAndSortsPoints) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO CANN_API VALUES(110,120,1,2,7,1,2);"
                                  "INSERT INTO TASK VALUES(130,150,7,3,0,1,1,4);"));
    AssertPair(Query("HostToDevice"));
}
TEST_F(DbFlowRepoTest, MstxPropagatesConnectionAndDomainToDevicePair) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO MSTX_EVENTS VALUES(110,120,1,9,7,1,2);"
                                  "INSERT INTO TASK VALUES(130,150,7,3,0,1,1,4);"));
    AssertPair(Query("MsTx"));
}
TEST_F(DbFlowRepoTest, AsyncNpuConnectsPythonOperatorAndDeviceTask) {
    ASSERT_TRUE(
        database->ExecSql("INSERT INTO STRING_IDS VALUES(1,'aten::add'); INSERT INTO CONNECTION_IDS VALUES(1,7);"
                          "INSERT INTO PYTORCH_API VALUES(110,120,1,1,1,1,2);"
                          "INSERT INTO TASK VALUES(130,150,7,3,0,1,1,4);"));
    AssertPair(Query("async_npu"));
}
TEST_F(DbFlowRepoTest, QueueAndFwdbwdUseDistinctNamesAndPreserveDepth) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO STRING_IDS VALUES(1,'Enqueue'),(2,'Dequeue'),(3,'aten::add');"
                                  "INSERT INTO CONNECTION_IDS VALUES(1,7),(2,7);"
                                  "INSERT INTO PYTORCH_API VALUES(110,120,1,1,1,1,2),(130,140,1,1,2,2,4);"));
    AssertPair(Query("async_task_queue"));
    EXPECT_TRUE(Query("fwdbwd").empty());
    ASSERT_TRUE(database->ExecSql("UPDATE PYTORCH_API SET name=3;"));
    AssertPair(Query("fwdbwd"));
    EXPECT_TRUE(Query("async_task_queue").empty());
}
TEST_F(DbFlowRepoTest, EmptyAndUnknownCategoriesDoNotInventPoints) {
    for (const auto *category :
        {"async_task_queue", "fwdbwd", "async_npu", "HostToDevice", "MsTx", "HostToDevice(Graph)", "unknown"}) {
        EXPECT_TRUE(Query(category).empty()) << category;
    }
    DbFlowRepo repo;
    FlowQuery query;
    query.cat = "unknown";
    std::vector<FlowPoint> points(1);
    points[0].id = 99;
    repo.QueryFlowPointByCategory(query, points);
    EXPECT_EQ(points.size(), 1U);
    EXPECT_EQ(points[0].id, 99U);
}

TEST_F(DbFlowRepoTest, ModelExecuteExcludesNotifyWaitAndOtherProcessesAndInvalidConnections) {
    ASSERT_TRUE(database->ExecSql("ALTER TABLE TASK ADD COLUMN globalPid INTEGER;"
                                  "INSERT INTO STRING_IDS VALUES(13,'MODEL_EXECUTE'),(14,'NOTIFY_WAIT');"
                                  "INSERT INTO CANN_API VALUES(110,120,4294967298,20000,7,1,2),"
                                  "(110,120,4294967298,20000,-1,1,2),(110,120,4294967298,20000,4294967295,1,2);"
                                  "INSERT INTO TASK VALUES(130,150,7,46,0,1,13,4,1),"
                                  "(135,155,7,46,0,2,14,0,1),(140,160,7,47,0,3,13,0,2),"
                                  "(140,160,-1,46,0,4,13,0,1),(140,160,4294967295,46,0,5,13,0,1),"
                                  "(90,95,7,46,0,6,13,0,1);"));
    const auto points = Query("HostToDevice(Graph)");
    AssertPair(points);
    ASSERT_EQ(points.size(), 2U);
    for (const auto &point : points) {
        EXPECT_EQ(point.id, point.type == "s" ? 7U : 1U);
    }
}

TEST_F(DbFlowRepoTest, ModelExecuteCategoryDoesNotRequireRebuildingExistingConnectionCache) {
    ASSERT_TRUE(SeedGraphCategoryData());
    ASSERT_TRUE(database->ExecSql("INSERT INTO STRING_IDS VALUES(14,'NOTIFY_WAIT');"
                                  "INSERT INTO TASK VALUES(135,155,7,46,0,2,14,0,1);"));
    std::vector<std::string> categories;
    // 旧库由解析初始化补建 Graph，查询接口只读取缓存。
    ASSERT_TRUE(InitGraphCategories());
    ASSERT_TRUE(database->QueryFlowCategoryList(categories, rank));
    EXPECT_EQ(categories, (std::vector<std::string>{"HostToDevice", "HostToDevice(Graph)"}));
    ASSERT_TRUE(database->ExecSql("DELETE FROM TASK WHERE taskType=13;"));
    ASSERT_TRUE(InitGraphCategories(true));
    categories.clear();
    ASSERT_TRUE(database->QueryFlowCategoryList(categories, rank));
    EXPECT_EQ(categories, (std::vector<std::string>{"HostToDevice"}));
    EXPECT_TRUE(Query("HostToDevice(Graph)").empty());
}

TEST_F(DbFlowRepoTest, ModelExecuteDoesNotEmitOrphanPointsForMismatchedHostProcess) {
    ASSERT_TRUE(database->ExecSql("ALTER TABLE TASK ADD COLUMN globalPid INTEGER;"
                                  "INSERT INTO STRING_IDS VALUES(13,'MODEL_EXECUTE');"
                                  "INSERT INTO CANN_API VALUES(110,120,4294967298,20000,7,1,2);"
                                  "INSERT INTO TASK VALUES(130,150,7,46,0,1,13,4,2);"));
    EXPECT_TRUE(Query("HostToDevice(Graph)").empty());
    ASSERT_TRUE(database->ExecSql("UPDATE TASK SET globalPid=1; DELETE FROM CANN_API;"));
    EXPECT_TRUE(Query("HostToDevice(Graph)").empty());
}

TEST_F(DbFlowRepoTest, GraphEndpointsUseActualHardwareLanesWithMstxDomains) {
    ASSERT_TRUE(SeedGraphCategoryData());
    auto &tracks = TrackInfoManager::Instance();
    tracks.UpdateDeviceToRankIdMap(database->QueryHostInfo() + "0", "0");
    const std::vector<std::pair<std::string, std::vector<std::string>>> cases{{"", {"46"}}, {"(0)", {"46_0"}},
        {"(9)", {"46_9"}}, {"(0),(9),(9)", {"46_0", "46_9"}}, {"(NULL)", {}}, {"(NULL),(0)", {"46_0"}}};
    for (const std::string id : {"7", "-2"}) {
        ASSERT_TRUE(
            database->ExecSql("UPDATE CANN_API SET connectionId=" + id + "; UPDATE TASK SET connectionId=" + id));
        for (const auto &testCase : cases) {
            SCOPED_TRACE(id + ":" + testCase.first);
            ASSERT_TRUE(database->ExecSql("DELETE FROM MSTX_EVENTS;"
                                          "INSERT INTO MSTX_EVENTS(connectionId,domainId) VALUES(99,55);"));
            if (!testCase.first.empty()) {
                ASSERT_TRUE(database->ExecSql("WITH domains(id) AS (VALUES " + testCase.first +
                    ") INSERT INTO MSTX_EVENTS(connectionId,domainId) SELECT " + id + ",id FROM domains"));
            }
            const auto points = Query("HostToDevice(Graph)");
            ASSERT_EQ(points.size(), 1 + testCase.second.size());
            std::vector<std::string> actualLanes;
            for (const auto &point : points) {
                EXPECT_EQ(point.flowId, id);
                if (point.type != "f") {
                    continue;
                }
                EXPECT_EQ(point.id, 1U);
                TrackInfo track;
                ASSERT_TRUE(tracks.GetTrackInfo(point.trackId, track, point.rankId));
                actualLanes.emplace_back(track.threadId);
                // 不只比较构造出来的名字：用真实 Hardware 查询确认该泳道上确有原任务。
                SliceQuery slice;
                slice.startTime = 0;
                slice.endTime = 200;
                auto stmt =
                    HardwareLaneQuery::PrepareStmtForQuerySimpleSliceWithOutNameByTrackId(track, database, slice);
                ASSERT_NE(stmt, nullptr);
                auto result = stmt->ExecuteQuery();
                ASSERT_NE(result, nullptr);
                ASSERT_TRUE(result->Next());
                do {
                    EXPECT_EQ(result->GetUint64("id"), point.id);
                } while (result->Next());
            }
            std::sort(actualLanes.begin(), actualLanes.end());
            EXPECT_EQ(actualLanes, testCase.second);
        }
    }
}

TEST_F(DbFlowRepoTest, FullGraphKeepsRepeatedModelTasksOnTheSameLane) {
    ASSERT_TRUE(SeedGraphCategoryData());
    TrackInfoManager::Instance().UpdateDeviceToRankIdMap(database->QueryHostInfo() + "0", "0");
    ASSERT_TRUE(database->ExecSql("INSERT INTO TASK VALUES(140,160,7,46,0,2,13,4,1);"));
    const auto points = Query("HostToDevice(Graph)");
    ASSERT_EQ(points.size(), 3U);
    std::vector<uint64_t> targets;
    for (const auto &point : points) {
        if (point.type == "f") {
            targets.emplace_back(point.id);
        }
    }
    std::sort(targets.begin(), targets.end());
    EXPECT_EQ(targets, (std::vector<uint64_t>{1, 2}));
}

TEST_F(DbFlowRepoTest, GraphCacheFailureDoesNotHideExistingCategories) {
    ASSERT_TRUE(SeedGraphCategoryData());
    ASSERT_TRUE(database->ExecSql("INSERT INTO connectionCats VALUES(9,'async_npu'),(10,'fwdbwd');"
                                  "DROP TABLE CANN_API;"));
    std::vector<std::string> categories;
    EXPECT_FALSE(InitGraphCategories());
    EXPECT_FALSE(database->CheckValueFromStatusInfoTable(Dic::GRAPH_CONNECTION_CATEGORY, Dic::FINISH_STATUS));
    ASSERT_TRUE(database->QueryFlowCategoryList(categories, rank));
    EXPECT_EQ(categories, (std::vector<std::string>{"HostToDevice", "async_npu", "fwdbwd"}));

    // 已有缓存中的类别也不能因源表缺失而被删除或重复追加。
    ASSERT_TRUE(database->ExecSql("INSERT INTO connectionCats VALUES(4,'HostToDevice(Graph)');"));
    categories.clear();
    ASSERT_TRUE(database->QueryFlowCategoryList(categories, rank));
    EXPECT_EQ(categories, (std::vector<std::string>{"HostToDevice", "HostToDevice(Graph)", "async_npu", "fwdbwd"}));
}

TEST_F(DbFlowRepoTest, GraphCategoriesAreBuiltByTheExistingConnectionInitialization) {
    ASSERT_TRUE(database->ExecSql("ALTER TABLE TASK ADD COLUMN globalPid INTEGER;"
                                  "INSERT INTO STRING_IDS VALUES(13,'MODEL_EXECUTE'),(14,'NOTIFY_WAIT');"
                                  "INSERT INTO CANN_API VALUES(110,120,4294967298,20000,7,1,2),"
                                  "(110,120,4294967298,20000,-1,1,2),(110,120,4294967298,20000,4294967295,1,2),"
                                  "(110,120,4294967298,20000,9,1,2),(110,120,4294967298,20000,11,1,2);"
                                  "INSERT INTO TASK VALUES(130,150,7,46,0,1,13,4,1),"
                                  "(135,155,7,47,0,2,13,0,1),(140,160,7,48,0,3,13,0,2),"
                                  "(140,160,7,46,0,4,14,0,1),(140,160,-1,46,0,5,13,0,1),"
                                  "(140,160,4294967295,46,0,6,13,0,1),"
                                  "(140,160,9,46,0,7,13,0,2),(140,160,11,46,0,8,14,0,1);"));
    const auto db = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(database);
    ASSERT_NE(db, nullptr);
    ASSERT_TRUE(db->InitConnectionCats());
    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(Dic::GRAPH_CONNECTION_CATEGORY, Dic::FINISH_STATUS));
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 1U);
    EXPECT_EQ(CountCategory("HostToDevice"), 0U);
    ASSERT_TRUE(InitGraphCategories());
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 1U);
}

TEST_F(DbFlowRepoTest, OldCompletedConnectionCacheIsUpgradedWithoutChangingLegacyRows) {
    ASSERT_TRUE(SeedGraphCategoryData());
    ASSERT_TRUE(database->ExecSql("INSERT INTO connectionCats VALUES(9,'async_npu'),(10,'MsTx');"));
    ASSERT_TRUE(database->UpdateValueIntoStatusInfoTable(Dic::CONNECTION_UNIT, Dic::FINISH_STATUS));
    // 原解析单元已经完成时，旧缓存升级入口仍会单独生成 Graph 分类。
    ASSERT_TRUE(InitGraphCategories());
    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(Dic::CONNECTION_UNIT, Dic::FINISH_STATUS));
    EXPECT_EQ(CountCategory("HostToDevice"), 1U);
    EXPECT_EQ(CountCategory("async_npu"), 1U);
    EXPECT_EQ(CountCategory("MsTx"), 1U);
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 1U);
    auto legacyStmt = database->CreatPreparedStatement(
        "SELECT connectionId,cat FROM connectionCats WHERE cat <> 'HostToDevice(Graph)' ORDER BY connectionId");
    ASSERT_NE(legacyStmt, nullptr);
    auto legacyRows = legacyStmt->ExecuteQuery();
    ASSERT_NE(legacyRows, nullptr);
    for (const auto &expected :
        std::vector<std::pair<uint64_t, std::string>>{{8, "HostToDevice"}, {9, "async_npu"}, {10, "MsTx"}}) {
        ASSERT_TRUE(legacyRows->Next());
        EXPECT_EQ(legacyRows->GetUint64("connectionId"), expected.first);
        EXPECT_EQ(legacyRows->GetString("cat"), expected.second);
    }
    EXPECT_FALSE(legacyRows->Next());
    legacyRows.reset();
    legacyStmt.reset();

    ASSERT_TRUE(database->ExecSql("DROP TABLE TASK; DROP TABLE CANN_API; DROP TABLE STRING_IDS;"));
    auto second = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(
        DataBaseManager::Instance().GetTraceDatabaseByRankId(rank));
    ASSERT_NE(second, nullptr);
    EXPECT_NE(second.get(), database.get());
    EXPECT_TRUE(second->InitHostToDeviceGraphConnectionCats());
    std::vector<std::string> categories;
    ASSERT_TRUE(second->QueryFlowCategoryList(categories, rank));
    EXPECT_EQ(categories, (std::vector<std::string>{"HostToDevice", "HostToDevice(Graph)", "MsTx", "async_npu"}));
}

TEST_F(DbFlowRepoTest, NegativeGraphCacheAvoidsRescansUntilConnectionReinitialization) {
    ASSERT_TRUE(SeedGraphCategoryData());
    ASSERT_TRUE(database->ExecSql("DELETE FROM TASK; DELETE FROM CANN_API; DELETE FROM STRING_IDS;"));
    ASSERT_TRUE(InitGraphCategories());
    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(Dic::GRAPH_CONNECTION_CATEGORY, Dic::FINISH_STATUS));
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 0U);
    // 后续源数据变化只有在解析状态失效、重新初始化时才更新类别。
    ASSERT_TRUE(database->ExecSql("INSERT INTO STRING_IDS VALUES(13,'MODEL_EXECUTE');"
                                  "INSERT INTO CANN_API VALUES(110,120,4294967298,20000,7,1,2);"
                                  "INSERT INTO TASK VALUES(130,150,7,46,0,1,13,4,1);"));
    ASSERT_TRUE(InitGraphCategories());
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 0U);
    ASSERT_TRUE(InitGraphCategories(true));
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 1U);
    ASSERT_TRUE(database->ExecSql("DROP TABLE TASK; DROP TABLE CANN_API; DROP TABLE STRING_IDS;"));
    for (int i = 0; i < 3; ++i) {
        std::vector<std::string> categories;
        ASSERT_TRUE(database->QueryFlowCategoryList(categories, rank));
        EXPECT_EQ(categories, (std::vector<std::string>{"HostToDevice", "HostToDevice(Graph)"}));
    }
}

TEST_F(DbFlowRepoTest, UnusedModelTypeIsCachedAsAbsentAcrossConnections) {
    ASSERT_TRUE(SeedGraphCategoryData());
    ASSERT_TRUE(database->ExecSql("DELETE FROM TASK;"));
    ASSERT_TRUE(InitGraphCategories());
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 0U);
    ASSERT_TRUE(database->ExecSql("DROP TABLE TASK; DROP TABLE CANN_API; DROP TABLE STRING_IDS;"));
    auto second = std::dynamic_pointer_cast<Dic::Module::FullDb::DbTraceDataBase>(
        DataBaseManager::Instance().GetTraceDatabaseByRankId(rank));
    ASSERT_NE(second, nullptr);
    EXPECT_TRUE(second->InitHostToDeviceGraphConnectionCats());
    std::vector<std::string> categories;
    ASSERT_TRUE(second->QueryFlowCategoryList(categories, rank));
    EXPECT_EQ(categories, (std::vector<std::string>{"HostToDevice"}));
}

TEST_F(DbFlowRepoTest, FailedGraphRefreshPreservesOldCategoriesAndRetries) {
    ASSERT_TRUE(SeedGraphCategoryData());
    ASSERT_TRUE(InitGraphCategories());
    ASSERT_TRUE(database->ExecSql("DROP TABLE CANN_API;"));
    EXPECT_FALSE(InitGraphCategories(true));
    EXPECT_FALSE(database->CheckValueFromStatusInfoTable(Dic::GRAPH_CONNECTION_CATEGORY, Dic::FINISH_STATUS));
    EXPECT_EQ(CountCategory("HostToDevice"), 1U);
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 1U);
    ASSERT_TRUE(database->ExecSql("CREATE TABLE CANN_API(startNs INTEGER,endNs INTEGER,globalTid INTEGER,"
                                  "type INTEGER,connectionId INTEGER,name INTEGER,depth INTEGER);"
                                  "INSERT INTO CANN_API VALUES(110,120,4294967298,20000,7,1,2);"));
    ASSERT_TRUE(InitGraphCategories());
    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(Dic::GRAPH_CONNECTION_CATEGORY, Dic::FINISH_STATUS));
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 1U);
}

TEST_F(DbFlowRepoTest, GraphCompletionMarkerAndRowsCommitTogether) {
    ASSERT_TRUE(SeedGraphCategoryData());
    ASSERT_TRUE(database->UpdateValueIntoStatusInfoTable(Dic::GRAPH_CONNECTION_CATEGORY, Dic::NOT_FINISH_STATUS));
    ASSERT_TRUE(database->UpdateValueIntoStatusInfoTable(Dic::CONNECTION_UNIT, Dic::FINISH_STATUS));
    const std::string rejectFinishSql = "CREATE TRIGGER reject_graph_finish BEFORE UPDATE ON status_info "
                                        "WHEN NEW.key='" +
        Dic::GRAPH_CONNECTION_CATEGORY +
        "' AND NEW.value='FINISH' "
        "BEGIN SELECT RAISE(ABORT,'cannot persist graph completion'); END;";
    ASSERT_TRUE(database->ExecSql(rejectFinishSql));
    EXPECT_FALSE(InitGraphCategories());
    EXPECT_EQ(CountCategory("HostToDevice"), 1U);
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 0U);
    EXPECT_FALSE(database->CheckValueFromStatusInfoTable(Dic::GRAPH_CONNECTION_CATEGORY, Dic::FINISH_STATUS));
    EXPECT_TRUE(database->CheckValueFromStatusInfoTable(Dic::CONNECTION_UNIT, Dic::FINISH_STATUS));
    ASSERT_TRUE(database->ExecSql("DROP TRIGGER reject_graph_finish;"));
    ASSERT_TRUE(InitGraphCategories());
    EXPECT_EQ(CountCategory("HostToDevice(Graph)"), 1U);
}

TEST_F(DbFlowRepoTest, GraphSignedConnectionIdsSurviveQueryPairingAndProtocolAssembly) {
    ASSERT_TRUE(database->ExecSql("ALTER TABLE TASK ADD COLUMN globalPid INTEGER;"
                                  "CREATE TABLE connectionCats(connectionId INTEGER,cat TEXT);"
                                  "INSERT INTO STRING_IDS VALUES(13,'MODEL_EXECUTE');"));
    TrackInfoManager::Instance().UpdateDeviceToRankIdMap(database->QueryHostInfo() + "0", "0");
    // 同一组断言覆盖小集合、原绑定数量分界及超限集合；每组均含整数边界与无效哨兵。
    for (const size_t count : {5U, 900U, 901U}) {
        SCOPED_TRACE(count);
        ASSERT_TRUE(database->ExecSql("DELETE FROM TASK; DELETE FROM CANN_API;"
                                      "WITH RECURSIVE ids(n) AS (SELECT 1 UNION ALL SELECT n+1 FROM ids WHERE n<" +
            std::to_string(count) +
            ") "
            "INSERT INTO CANN_API SELECT 110,120,4294967298,20000,CASE n "
            "WHEN 1 THEN -9223372036854775808 WHEN 2 THEN 9223372036854775807 "
            "WHEN 3 THEN 0 WHEN 4 THEN 7 ELSE 3-n END,1,2 FROM ids;"
            "INSERT INTO CANN_API VALUES(110,120,4294967298,20000,-1,1,2),"
            "(110,120,4294967298,20000,4294967295,1,2);"
            "INSERT INTO TASK SELECT 130,150,connectionId,46,0,rowid,13,4,1 FROM CANN_API;"));
        ASSERT_TRUE(InitGraphCategories(true));
        EXPECT_EQ(CountCategory("HostToDevice(Graph)"), count);
        std::vector<std::string> categories;
        ASSERT_TRUE(database->QueryFlowCategoryList(categories, rank));
        EXPECT_EQ(categories, (std::vector<std::string>{"HostToDevice(Graph)"}));
        auto points = Query("HostToDevice(Graph)");
        ASSERT_EQ(points.size(), count * 2);
        FlowAnalyzer::SortByFlowIdAndTimestampASC(points);
        std::vector<std::unique_ptr<Dic::Protocol::UnitSingleFlow>> flows;
        FlowAnalyzer::ComputeUintFlows(points, "HostToDevice(Graph)", flows);
        ASSERT_EQ(flows.size(), count);
        const std::vector<std::string> boundaryIds{std::to_string(std::numeric_limits<int64_t>::min()),
            std::to_string(std::numeric_limits<int64_t>::max()), "0", "7"};
        for (const auto &flow : flows) {
            const auto boundary = std::find(boundaryIds.begin(), boundaryIds.end(), flow->from.id);
            if (boundary != boundaryIds.end()) {
                EXPECT_EQ(flow->to.id, std::to_string(std::distance(boundaryIds.begin(), boundary) + 1));
            } else {
                const auto id = std::stoll(flow->from.id);
                EXPECT_LE(id, -2);
                EXPECT_GE(id, 3 - static_cast<int64_t>(count));
                EXPECT_EQ(flow->to.id, std::to_string(3 - id));
            }
            EXPECT_EQ(flow->cat, "HostToDevice(Graph)");
            EXPECT_EQ(flow->from.timestamp, 10U);
            EXPECT_EQ(flow->to.timestamp, 30U);
        }
    }
}

TEST_F(DbFlowRepoTest, GraphSourceIdRuleDoesNotChangeLegacyCategoryEndpoints) {
    FlowPoint source;
    source.id = 9;
    source.flowId = "7";
    source.type = "s";
    FlowPoint target;
    target.id = 2;
    target.flowId = "7";
    target.type = "f";
    for (const auto *category :
        {"HostToDevice", "async_npu", "MsTx", "fwdbwd", "async_task_queue", "HostToDevice(Graph)"}) {
        std::vector<std::unique_ptr<Dic::Protocol::UnitSingleFlow>> flows;
        FlowAnalyzer::ComputeUintFlows({source, target}, category, flows);
        ASSERT_EQ(flows.size(), 1U);
        EXPECT_EQ(flows.front()->from.id, std::string(category) == "HostToDevice(Graph)" ? "7" : "9");
        EXPECT_EQ(flows.front()->to.id, "2");
    }
    // TEXT 的 flowId 不等于算子 ID，不能因展示类别相同而套用 DB CANN 的身份规则。
    source.resolveDepthAfterSampling = true;
    target.resolveDepthAfterSampling = true;
    std::vector<std::unique_ptr<Dic::Protocol::UnitSingleFlow>> textFlows;
    FlowAnalyzer::ComputeUintFlows({source, target}, "HostToDevice(Graph)", textFlows);
    ASSERT_EQ(textFlows.size(), 1U);
    EXPECT_EQ(textFlows.front()->from.id, "9");
    EXPECT_EQ(textFlows.front()->to.id, "2");
}
}
