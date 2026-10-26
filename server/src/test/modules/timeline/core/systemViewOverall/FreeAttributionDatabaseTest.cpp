/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <set>
#include <sstream>
#include <sqlite3.h>
#include "DataBaseManager.h"
#include "Database.h"
#include "DbTraceDataBase.h"
#include "FreeAttributionService.h"
#include "RankOverlapStore.h"
#include "TraceTime.h"

namespace Dic::Module::Timeline {
namespace {
const char *TEXT_SCHEMA = R"SQL(
CREATE TABLE process(pid TEXT PRIMARY KEY,process_name TEXT);
CREATE TABLE thread(track_id INTEGER PRIMARY KEY,pid TEXT,tid TEXT,thread_name TEXT);
CREATE TABLE slice(id INTEGER PRIMARY KEY,name TEXT,timestamp INTEGER,end_time INTEGER,
  duration INTEGER,depth INTEGER,track_id INTEGER,cat TEXT);
CREATE TABLE flow(id INTEGER PRIMARY KEY AUTOINCREMENT,flow_id TEXT,cat TEXT,
  track_id INTEGER,timestamp INTEGER,type TEXT);
INSERT INTO process VALUES('1','Python'),('32','Ascend Hardware'),('64','Communication'),('96','Overlap Analysis');
INSERT INTO thread VALUES(1,'1','10','Main'),(2,'32','2','Stream 2'),(3,'64','3','Group 1'),
  (4,'96','4','Free'),(5,'96','5','Computing'),(6,'96','6','Communication');
)SQL";

const char *DB_SCHEMA = R"SQL(
CREATE TABLE OVERLAP_ANALYSIS(id INTEGER PRIMARY KEY,deviceId INTEGER,startNs INTEGER,endNs INTEGER,type INTEGER);
CREATE TABLE TASK(globalTaskId INTEGER,deviceId INTEGER,startNs INTEGER,endNs INTEGER,
  streamId INTEGER,depth INTEGER,connectionId INTEGER,taskType INTEGER);
CREATE TABLE COMPUTE_TASK_INFO(globalTaskId INTEGER,name INTEGER);
CREATE TABLE STRING_IDS(id INTEGER PRIMARY KEY,value TEXT);
CREATE TABLE PYTORCH_API(name INTEGER,startNs INTEGER,endNs INTEGER,globalTid INTEGER,
  type INTEGER,depth INTEGER,connectionId INTEGER);
CREATE TABLE CONNECTION_IDS(id INTEGER,connectionId INTEGER);
CREATE TABLE COMMUNICATION_OP(opId INTEGER,opName INTEGER,groupName INTEGER,startNs INTEGER,
  endNs INTEGER,connectionId INTEGER,deviceId INTEGER);
INSERT INTO STRING_IDS VALUES(1,'aten::add'),(2,'AddKernel'),(3,'hccl_all_reduce'),(4,'group1');
)SQL";

const char *TEXT_COMPUTE = R"SQL(
INSERT INTO slice VALUES(1,'aten::add',110,150,40,0,1,''),
  (2,'Free',200,280,80,0,4,''),(3,'Computing',280,300,20,0,5,''),
  (4,'AddKernel',280,300,20,0,2,'');
INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES
  ('torch-link','async_npu',1,120,'s'),('torch-link','async_npu',2,280,'f');
)SQL";

const char *DB_COMPUTE = R"SQL(
INSERT INTO OVERLAP_ANALYSIS VALUES(1,0,200,280,3),(2,0,280,300,0);
INSERT INTO TASK VALUES(1,0,280,300,2,0,42,2);
INSERT INTO COMPUTE_TASK_INFO VALUES(1,2);
INSERT INTO PYTORCH_API VALUES(1,110,150,4294967306,50001,0,7);
INSERT INTO CONNECTION_IDS VALUES(7,42);
)SQL";

class FreeAttributionDatabaseTest : public testing::TestWithParam<DataType> {
  protected:
    void SetUp() override {
        RankOverlapStore::Instance().Clear();
        DataBaseManager::Instance().Clear();
        TraceTime::Instance().Reset();
        TraceTime::Instance().UpdateTime(100, 1000);
        std::error_code error;
        directory = std::filesystem::temp_directory_path(error) /
            ("free-attribution-ut-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_FALSE(error) << error.message();
        ASSERT_TRUE(std::filesystem::create_directory(directory, error));
        ASSERT_FALSE(error) << error.message();
    }

    void TearDown() override {
        RankOverlapStore::Instance().Clear();
        DataBaseManager::Instance().Clear();
        TraceTime::Instance().Reset();
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        EXPECT_FALSE(error) << error.message();
    }

    void Exec(const std::string &path, const std::string &sql) {
        sqlite3 *database = nullptr;
        ASSERT_EQ(sqlite3_open(path.c_str(), &database), SQLITE_OK);
        char *message = nullptr;
        const auto status = sqlite3_exec(database, sql.c_str(), nullptr, nullptr, &message);
        const std::string detail = message ? message : "";
        sqlite3_free(message);
        sqlite3_close(database);
        ASSERT_EQ(status, SQLITE_OK) << detail << " SQL=" << sql;
    }

    std::string Source(const std::string &name, const std::string &rank = "free-test-rank") {
        const auto path = (directory / (name + ".db")).string();
        Exec(path, GetParam() == DataType::TEXT ? TEXT_SCHEMA : DB_SCHEMA);
        Exec(path, "PRAGMA user_version=" + Database::GetCompileDataBaseVersion() + ";");
        auto &manager = DataBaseManager::Instance();
        manager.SetDataType(GetParam(), path);
        EXPECT_TRUE(manager.CreateTraceConnectionPool(rank, path));
        manager.UpdateRankIdToDeviceId(path, rank, "0");
        return path;
    }

    void Compute(const std::string &path) { Exec(path, GetParam() == DataType::TEXT ? TEXT_COMPUTE : DB_COMPUTE); }

    void ExpectHostLane(const FreeAttributionTarget &target) {
        auto database = std::dynamic_pointer_cast<FullDb::DbTraceDataBase>(
            DataBaseManager::Instance().GetTraceDatabaseByFileId(target.dbPath));
        ASSERT_NE(database, nullptr);
        std::vector<std::unique_ptr<Protocol::UnitTrack>> metadata;
        ASSERT_TRUE(database->QueryHostMetadata("free-test-rank", metadata));
        bool matched = false;
        std::function<void(const Protocol::UnitTrack &)> findLane = [&](const Protocol::UnitTrack &unit) {
            const auto &lane = unit.metaData;
            if (unit.type == "thread" && lane.cardId == target.cardId && lane.processId == target.pid &&
                lane.threadId == target.tid && lane.metaType == target.metaType) {
                matched = true;
            }
            for (const auto &child : unit.children) {
                findLane(*child);
            }
        };
        for (const auto &unit : metadata) {
            findLane(*unit);
        }
        EXPECT_TRUE(matched) << "The API target must match metadata produced for the selected rank.";
    }

    FreeAttributionParams Query(const std::string &path) {
        FreeAttributionParams params;
        params.rankId = "free-test-rank";
        params.dbPath = path;
        return params;
    }

    std::vector<FreeAttributionRecord> Details(FreeAttributionParams params, FreeAttributionSummary &summary) {
        std::string error;
        EXPECT_TRUE(service.QuerySummary(params, summary, error)) << error;
        std::vector<FreeAttributionRecord> records;
        for (const auto &group : summary.groups) {
            params.groupId = group.groupId;
            FreeAttributionDetails details;
            EXPECT_TRUE(service.QueryDetails(params, details, error)) << error;
            records.insert(records.end(), details.records.begin(), details.records.end());
        }
        return records;
    }

    FreeAttributionService service;
    std::filesystem::path directory;
};

TEST_P(FreeAttributionDatabaseTest, FindsComputingApiOutsideRangeAndPreservesJumpCoordinates) {
    const auto path = Source("compute");
    Compute(path);
    auto params = Query(path);
    params.startTime = 150;
    params.endTime = 160;
    FreeAttributionSummary summary;
    const auto records = Details(params, summary);
    ASSERT_EQ(summary.groups.size(), 1U);
    EXPECT_EQ(summary.groups[0].apiName, "aten::add");
    EXPECT_EQ(summary.totalFreeDuration, 80U);
    EXPECT_EQ(summary.totalFreeCount, 1U);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "matched");
    ASSERT_TRUE(records[0].hostApiTarget.has_value());
    EXPECT_EQ(records[0].hostApiTarget->timestamp, 10U);
    EXPECT_EQ(records[0].hostApiTarget->dbPath, path);
    EXPECT_EQ(records[0].hostApiTarget->metaType, GetParam() == DataType::TEXT ? "TEXT" : "PYTORCH_API");
    EXPECT_EQ(records[0].hostApiTarget->cardId, params.rankId);
    EXPECT_EQ(records[0].hostApiTarget->pid, GetParam() == DataType::TEXT ? "1" : "4294967306");
    EXPECT_EQ(records[0].hostApiTarget->tid, GetParam() == DataType::TEXT ? "10" : "pytorch");
    if (GetParam() == DataType::DB) {
        ExpectHostLane(*records[0].hostApiTarget);
    }
    EXPECT_EQ(records[0].freeTarget.timestamp, 100U);
    EXPECT_EQ(records[0].freeTarget.duration, 80U);
    EXPECT_FALSE(records[0].freeTarget.id.empty());
    EXPECT_EQ(records[0].freeTarget.metaType, GetParam() == DataType::TEXT ? "TEXT" : "OVERLAP_ANALYSIS");
    EXPECT_EQ(records[0].freeTarget.pid, GetParam() == DataType::TEXT ? "96" : "OVERLAP_ANALYSIS");
    EXPECT_EQ(records[0].freeTarget.tid, GetParam() == DataType::TEXT ? "4" : "3");
}

TEST_P(FreeAttributionDatabaseTest, FindsCommunicationOperatorRatherThanItsInternalTask) {
    const auto path = Source("communication");
    if (GetParam() == DataType::TEXT) {
        Exec(path, R"SQL(
INSERT INTO slice VALUES(1,'aten::all_reduce',110,150,40,0,1,''),
  (2,'Free',200,280,80,0,4,''),(3,'Communication',280,300,20,0,6,''),
  (4,'hccl_all_reduce',280,300,20,0,3,'');
INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES
  ('comm','async_npu',1,120,'s'),('comm','async_npu',3,280,'f');
INSERT INTO slice VALUES(5,'SDMA',280,285,5,0,2,'');
)SQL");
    } else {
        Exec(path, R"SQL(
INSERT INTO STRING_IDS VALUES(5,'aten::all_reduce');
INSERT INTO OVERLAP_ANALYSIS VALUES(1,0,200,280,3),(2,0,280,300,1);
INSERT INTO COMMUNICATION_OP VALUES(1,3,4,280,300,42,0);
INSERT INTO PYTORCH_API VALUES(5,110,150,4294967306,50001,0,7);
INSERT INTO CONNECTION_IDS VALUES(7,42);
)SQL");
    }
    FreeAttributionSummary summary;
    const auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "matched");
    EXPECT_EQ(records[0].nextBehaviorType, "Communication");
    ASSERT_TRUE(records[0].hostApiTarget.has_value());
    EXPECT_EQ(records[0].hostApiTarget->name, "aten::all_reduce");
}

TEST_P(FreeAttributionDatabaseTest, RequiresRawStartTimestampEquality) {
    const auto path = Source("precision");
    Compute(path);
    Exec(path,
        GetParam() == DataType::TEXT ? "UPDATE slice SET timestamp=281 WHERE id=4;" : "UPDATE TASK SET startNs=281;");
    FreeAttributionSummary summary;
    const auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "unresolved");
    EXPECT_EQ(records[0].reason, "noTargetOperator");
    EXPECT_EQ(summary.totalFreeDuration, 80U);
}

TEST_P(FreeAttributionDatabaseTest, ExplicitDbConnectionFindsApiStartingAfterDeviceOperator) {
    if (GetParam() != DataType::DB) {
        GTEST_SKIP() << "This fixture exercises explicit DB connection mappings.";
    }
    TraceTime::Instance().UpdateTime(100, 2000);
    const auto path = Source("late-mapped-api");
    Compute(path);
    Exec(path, "UPDATE PYTORCH_API SET startNs=1220,endNs=1260;");
    for (const bool communication : {false, true}) {
        SCOPED_TRACE(communication);
        if (communication) {
            Exec(path, R"SQL(
DELETE FROM TASK;
DELETE FROM COMPUTE_TASK_INFO;
UPDATE OVERLAP_ANALYSIS SET type=1 WHERE id=2;
INSERT INTO COMMUNICATION_OP VALUES(1,3,4,280,300,42,0);
)SQL");
        }
        FreeAttributionSummary summary;
        const auto records = Details(Query(path), summary);
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records[0].attributionStatus, "matched");
        EXPECT_EQ(records[0].nextBehaviorType, communication ? "Communication" : "Computing");
        ASSERT_TRUE(records[0].hostApiTarget.has_value());
        EXPECT_EQ(records[0].hostApiTarget->name, "aten::add");
        EXPECT_EQ(records[0].hostApiTarget->timestamp, 1120U);
        EXPECT_EQ(records[0].hostApiTarget->duration, 40U);
        EXPECT_EQ(records[0].hostApiTarget->dbPath, path);
        EXPECT_EQ(summary.totalFreeCount, 1U);
        EXPECT_EQ(summary.totalFreeDuration, 80U);
    }
}

TEST_P(FreeAttributionDatabaseTest, ReusedDbConnectionKeepsApisBeforeAndAfterDeviceOperator) {
    if (GetParam() != DataType::DB) {
        GTEST_SKIP() << "This fixture exercises explicit DB connection mappings.";
    }
    TraceTime::Instance().UpdateTime(100, 2000);
    const auto path = Source("reused-mapped-api");
    Compute(path);
    Exec(path, R"SQL(
INSERT INTO STRING_IDS VALUES(5,'aten::late');
INSERT INTO PYTORCH_API VALUES(5,1220,1260,4294967306,50001,0,8);
INSERT INTO CONNECTION_IDS VALUES(8,42);
)SQL");
    for (const bool communication : {false, true}) {
        SCOPED_TRACE(communication);
        if (communication) {
            Exec(path, R"SQL(
DELETE FROM TASK;
DELETE FROM COMPUTE_TASK_INFO;
UPDATE OVERLAP_ANALYSIS SET type=1 WHERE id=2;
INSERT INTO COMMUNICATION_OP VALUES(1,3,4,280,300,42,0);
)SQL");
        }
        FreeAttributionSummary summary;
        const auto records = Details(Query(path), summary);
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records[0].attributionStatus, "ambiguous");
        EXPECT_EQ(records[0].nextBehaviorType, communication ? "Communication" : "Computing");
        EXPECT_FALSE(records[0].hostApiTarget.has_value());
        ASSERT_EQ(records[0].candidateApiTargets.size(), 2U);
        std::set<uint64_t> apiStarts;
        for (const auto &candidate : records[0].candidateApiTargets) {
            apiStarts.insert(candidate.timestamp);
            EXPECT_EQ(candidate.dbPath, path);
        }
        EXPECT_EQ(apiStarts, (std::set<uint64_t>{10, 1120}));
        EXPECT_EQ(summary.totalFreeCount, 1U);
        EXPECT_EQ(summary.totalFreeDuration, 80U);
    }
}

TEST_P(FreeAttributionDatabaseTest, DistinctCoStartingApiInstancesStayAmbiguous) {
    const auto path = Source("parallel");
    Compute(path);
    Exec(path,
        GetParam() == DataType::TEXT ? R"SQL(
INSERT INTO slice VALUES(5,'aten::other',160,190,30,0,1,'');
INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES
  ('second','async_npu',1,170,'s'),('second','async_npu',2,280,'f');
)SQL"
                                     : R"SQL(
INSERT INTO STRING_IDS VALUES(5,'aten::other');
INSERT INTO PYTORCH_API VALUES(5,160,190,4294967306,50001,0,8);
INSERT INTO CONNECTION_IDS VALUES(8,42);
)SQL");
    FreeAttributionSummary summary;
    const auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "ambiguous");
    EXPECT_EQ(records[0].candidateApiTargets.size(), 2U);
    EXPECT_EQ(summary.totalFreeCount, 1U);
    EXPECT_EQ(summary.totalFreeDuration, 80U);
}

TEST_P(FreeAttributionDatabaseTest, CoStartingBehaviorsShareApiAndKeepMissingBranchIncomplete) {
    const auto path = Source("co-starting-behaviors");
    Compute(path);
    Exec(path,
        GetParam() == DataType::TEXT ? R"SQL(
INSERT INTO slice VALUES(5,'Communication',280,300,20,0,6,''),
  (6,'hccl_all_reduce',280,300,20,0,3,'');
INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES
  ('comm','async_npu',1,120,'s'),('comm','async_npu',3,280,'f');
)SQL"
                                     : R"SQL(
INSERT INTO OVERLAP_ANALYSIS VALUES(3,0,280,300,1);
INSERT INTO COMMUNICATION_OP VALUES(1,3,4,280,300,42,0);
)SQL");
    FreeAttributionSummary summary;
    auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "matched");
    ASSERT_TRUE(records[0].hostApiTarget.has_value());
    EXPECT_EQ(records[0].hostApiTarget->name, "aten::add");
    EXPECT_EQ(summary.totalFreeCount, 1U);
    EXPECT_EQ(summary.totalFreeDuration, 80U);
    Exec(path,
        GetParam() == DataType::TEXT ? "DELETE FROM flow WHERE flow_id='comm';"
                                     : "UPDATE COMMUNICATION_OP SET connectionId=43;");
    records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "incomplete");
    EXPECT_FALSE(records[0].hostApiTarget.has_value());
    ASSERT_EQ(records[0].candidateApiTargets.size(), 1U);
    EXPECT_EQ(records[0].candidateApiTargets[0].name, "aten::add");
    EXPECT_EQ(summary.totalFreeCount, 1U);
    EXPECT_EQ(summary.totalFreeDuration, 80U);
}

TEST_P(FreeAttributionDatabaseTest, DoesNotAssociateOtherDeviceAtSameTimestamp) {
    const auto path = Source("device-isolation");
    Compute(path);
    Exec(path,
        GetParam() == DataType::TEXT ? R"SQL(
INSERT INTO process VALUES('33','Ascend Hardware');
INSERT INTO thread VALUES(7,'33','7','Stream 7');
UPDATE slice SET track_id=7 WHERE id=4;
UPDATE flow SET track_id=7 WHERE type='f';
)SQL"
                                     : "UPDATE TASK SET deviceId=1;");
    FreeAttributionSummary summary;
    const auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "unresolved");
    EXPECT_EQ(records[0].reason, "noTargetOperator");
}

TEST_P(FreeAttributionDatabaseTest, RequeriesChangedSourceInsteadOfReturningStaleCachedApiName) {
    const auto path = Source("cache");
    Compute(path);
    FreeAttributionSummary first;
    ASSERT_EQ(Details(Query(path), first).size(), 1U);
    ASSERT_EQ(first.groups.size(), 1U);
    EXPECT_EQ(first.groups[0].apiName, "aten::add");
    Exec(path,
        GetParam() == DataType::TEXT ? "UPDATE slice SET name='aten::renamed' WHERE id=1;"
                                     : "UPDATE STRING_IDS SET value='aten::renamed' WHERE id=1;");
    FreeAttributionSummary changed;
    ASSERT_EQ(Details(Query(path), changed).size(), 1U);
    ASSERT_EQ(changed.groups.size(), 1U);
    EXPECT_EQ(changed.groups[0].apiName, "aten::renamed");
}

TEST_P(FreeAttributionDatabaseTest, ReimportedSamePathUsesNewPoolEvenWithIdenticalFileFingerprint) {
    const auto path = Source("reimport");
    Compute(path);
    FreeAttributionSummary first;
    ASSERT_EQ(Details(Query(path), first).size(), 1U);
    const auto originalTime = std::filesystem::last_write_time(path);
    const auto originalSize = std::filesystem::file_size(path);
    auto &manager = DataBaseManager::Instance();
    manager.ReleaseDatabaseByFileId(path);
    Exec(path,
        GetParam() == DataType::TEXT ? "UPDATE slice SET name='aten::sub' WHERE id=1;"
                                     : "UPDATE STRING_IDS SET value='aten::sub' WHERE id=1;");
    ASSERT_EQ(std::filesystem::file_size(path), originalSize);
    std::filesystem::last_write_time(path, originalTime);
    manager.SetDataType(GetParam(), path);
    ASSERT_TRUE(manager.CreateTraceConnectionPool("free-test-rank", path));
    manager.UpdateRankIdToDeviceId(path, "free-test-rank", "0");
    FreeAttributionSummary changed;
    ASSERT_EQ(Details(Query(path), changed).size(), 1U);
    ASSERT_EQ(changed.groups.size(), 1U);
    EXPECT_EQ(changed.groups[0].apiName, "aten::sub");
}

TEST_P(FreeAttributionDatabaseTest, MissingFlowPreservesFreeAndReportsReason) {
    const auto path = Source("missing-flow");
    Compute(path);
    Exec(path, GetParam() == DataType::TEXT ? "DELETE FROM flow;" : "DELETE FROM CONNECTION_IDS;");
    FreeAttributionSummary summary;
    const auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "unresolved");
    EXPECT_FALSE(records[0].hostApiTarget.has_value());
    EXPECT_FALSE(records[0].reason.empty());
    EXPECT_FALSE(records[0].freeTarget.id.empty());
    EXPECT_EQ(summary.totalFreeCount, 1U);
}

TEST_P(FreeAttributionDatabaseTest, PythonStackIsNotReportedAsPytorchHostApi) {
    const auto path = Source("python-stack");
    Compute(path);
    Exec(path,
        GetParam() == DataType::TEXT ? "UPDATE slice SET cat='python_function' WHERE id=1;"
                                     : "UPDATE PYTORCH_API SET type=50003;");
    FreeAttributionSummary summary;
    const auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "unresolved");
    EXPECT_FALSE(records[0].hostApiTarget.has_value());
    EXPECT_EQ(summary.totalFreeDuration, 80U);
}

TEST_P(FreeAttributionDatabaseTest, ValidApiFlowIgnoresIndependentCannDriverBranch) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow branches.";
    }
    const auto path = Source("partial-flow");
    Compute(path);
    Exec(path, R"SQL(
INSERT INTO process VALUES('128','CANN');
INSERT INTO thread VALUES(7,'128','7','Runtime');
INSERT INTO slice VALUES(5,'LaunchWithoutPytorch',160,190,30,0,7,'');
INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES
  ('partial','HostToDevice',7,170,'s'),('partial','HostToDevice',2,280,'f');
)SQL");
    FreeAttributionSummary summary;
    const auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "matched");
    ASSERT_TRUE(records[0].hostApiTarget.has_value());
    EXPECT_EQ(records[0].hostApiTarget->name, "aten::add");
    EXPECT_EQ(summary.totalFreeCount, 1U);
    EXPECT_EQ(summary.totalFreeDuration, 80U);
}

TEST_P(FreeAttributionDatabaseTest, OrphanFlowEndpointDoesNotHideBehindCompleteApiFlow) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow points.";
    }
    const auto path = Source("orphan-flow");
    Compute(path);
    Exec(path,
        "INSERT INTO flow(flow_id,cat,track_id,timestamp,type) "
        "VALUES('orphan','async_npu',2,280,'f');");
    FreeAttributionSummary summary;
    const auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "incomplete");
    EXPECT_FALSE(records[0].hostApiTarget.has_value());
    ASSERT_EQ(records[0].candidateApiTargets.size(), 1U);
    EXPECT_EQ(records[0].candidateApiTargets[0].name, "aten::add");
    EXPECT_EQ(summary.totalFreeCount, 1U);
}

TEST_P(FreeAttributionDatabaseTest, SameTimeStartAndEndIgnoreInsertionOrder) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow phases.";
    }
    const auto path = Source("same-time-start-end");
    Compute(path);
    Exec(path, "UPDATE slice SET end_time=290,duration=180 WHERE id=1;");
    for (const bool reverse : {false, true}) {
        SCOPED_TRACE(reverse);
        Exec(path, "DELETE FROM flow;");
        const std::string start = "('same-time','async_npu',1,280,'s')";
        const std::string end = "('same-time','async_npu',2,280,'f')";
        Exec(path,
            "INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES " +
                (reverse ? end + "," + start : start + "," + end) + ";");
        FreeAttributionSummary summary;
        const auto records = Details(Query(path), summary);
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records[0].attributionStatus, "matched");
        ASSERT_TRUE(records[0].hostApiTarget.has_value());
        EXPECT_EQ(records[0].hostApiTarget->id, "1");
        EXPECT_EQ(summary.totalFreeCount, 1U);
        EXPECT_EQ(summary.totalFreeDuration, 80U);
    }
}

TEST_P(FreeAttributionDatabaseTest, ExplicitApiPairAllowsReversedHostDeviceTimestamps) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow phases.";
    }
    const auto path = Source("reversed-host-device-time");
    Compute(path);
    Exec(path, "UPDATE slice SET timestamp=300,end_time=330,duration=30 WHERE id=1;");
    for (const bool reverse : {false, true}) {
        SCOPED_TRACE(reverse);
        Exec(path, "DELETE FROM flow;");
        const std::string start = "('inverted','async_npu',1,310,'s')";
        const std::string end = "('inverted','async_npu',2,280,'f')";
        Exec(path,
            "INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES " +
                (reverse ? end + "," + start : start + "," + end) + ";");
        FreeAttributionSummary summary;
        const auto records = Details(Query(path), summary);
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records[0].attributionStatus, "matched");
        ASSERT_TRUE(records[0].hostApiTarget.has_value());
        EXPECT_EQ(records[0].hostApiTarget->id, "1");
        EXPECT_EQ(records[0].hostApiTarget->timestamp, 200U);
        EXPECT_EQ(summary.totalFreeCount, 1U);
        EXPECT_EQ(summary.totalFreeDuration, 80U);
    }
}

TEST_P(FreeAttributionDatabaseTest, CannOriginIsNotReplacedByPythonRelay) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow phases.";
    }
    const auto path = Source("same-time-relay-end");
    Compute(path);
    Exec(path, R"SQL(
UPDATE slice SET timestamp=250,end_time=290,duration=40 WHERE id=1;
INSERT INTO process VALUES('128','CANN');
INSERT INTO thread VALUES(7,'128','7','Runtime');
INSERT INTO slice VALUES(5,'Launch',110,290,180,0,7,'');
)SQL");
    for (const auto startTime : {120, 280}) {
        for (const bool reverse : {false, true}) {
            SCOPED_TRACE(startTime);
            SCOPED_TRACE(reverse);
            Exec(path, "DELETE FROM flow;");
            const std::string start = "('relay','async_npu',7," + std::to_string(startTime) + ",'s')";
            const std::string relay = "('relay','async_npu',1,280,'t')";
            const std::string end = "('relay','async_npu',2,280,'f')";
            Exec(path,
                "INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES " +
                    (reverse ? end + "," + relay + "," + start : start + "," + relay + "," + end) + ";");
            FreeAttributionSummary summary;
            const auto records = Details(Query(path), summary);
            ASSERT_EQ(records.size(), 1U);
            EXPECT_EQ(records[0].attributionStatus, "unresolved");
            EXPECT_FALSE(records[0].hostApiTarget.has_value());
            EXPECT_TRUE(records[0].candidateApiTargets.empty());
            EXPECT_EQ(summary.totalFreeDuration, 80U);
        }
    }
}

TEST_P(FreeAttributionDatabaseTest, PytorchOriginRemainsApiAcrossCannRelay) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow phases.";
    }
    const auto path = Source("pytorch-origin-cann-relay");
    Compute(path);
    Exec(path, R"SQL(
INSERT INTO process VALUES('128','CANN');
INSERT INTO thread VALUES(7,'128','7','Runtime');
INSERT INTO slice VALUES(5,'Launch',250,290,40,0,7,'');
DELETE FROM flow;
)SQL");
    for (const bool reverse : {false, true}) {
        SCOPED_TRACE(reverse);
        Exec(path, "DELETE FROM flow;");
        const std::string relay = "('relay','async_npu',7,280,'t')";
        const std::string end = "('relay','async_npu',2,280,'f')";
        Exec(path,
            "INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES "
            "('relay','async_npu',1,120,'s')," +
                (reverse ? end + "," + relay : relay + "," + end) + ";");
        FreeAttributionSummary summary;
        const auto records = Details(Query(path), summary);
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records[0].attributionStatus, "matched");
        ASSERT_TRUE(records[0].hostApiTarget.has_value());
        EXPECT_EQ(records[0].hostApiTarget->id, "1");
        EXPECT_EQ(summary.totalFreeDuration, 80U);
    }
}

TEST_P(FreeAttributionDatabaseTest, SimultaneousRelaysPreserveIncompleteOrdering) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow phases.";
    }
    const auto path = Source("same-time-relays");
    Compute(path);
    Exec(path, R"SQL(
UPDATE slice SET timestamp=250,end_time=290,duration=40 WHERE id=1;
INSERT INTO process VALUES('128','CANN');
INSERT INTO thread VALUES(7,'128','7','Runtime'),(8,'1','11','Other Python'),(9,'128','9','Other Runtime');
INSERT INTO slice VALUES(5,'Launch',250,290,40,0,7,''),
  (6,'aten::other',250,290,40,0,8,''),(7,'OtherLaunch',250,290,40,0,9,'');
)SQL");
    for (const bool reverse : {false, true}) {
        SCOPED_TRACE(reverse);
        Exec(path, "DELETE FROM flow;");
        const std::string first = "('relays','async_npu',7,280,'t')";
        const std::string second = "('relays','async_npu',9,280,'t')";
        Exec(path,
            "INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES "
            "('relays','async_npu',1,280,'s'),('relays','async_npu',8,280,'s')," +
                (reverse ? second + "," + first : first + "," + second) + ",('relays','async_npu',2,280,'f');");
        FreeAttributionSummary summary;
        const auto records = Details(Query(path), summary);
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records[0].attributionStatus, "incomplete");
        EXPECT_FALSE(records[0].hostApiTarget.has_value());
        std::set<std::string> ids;
        for (const auto &candidate : records[0].candidateApiTargets) {
            ids.insert(candidate.id);
        }
        EXPECT_EQ(ids, (std::set<std::string>{"1", "6"}));
        EXPECT_EQ(summary.totalFreeCount, 1U);
        EXPECT_EQ(summary.totalFreeDuration, 80U);
    }
}

TEST_P(FreeAttributionDatabaseTest, ReusedFlowIdAtSameTimeBoundaryKeepsInstancesSeparate) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow phases.";
    }
    const auto path = Source("reused-flow-boundary");
    Compute(path);
    Exec(path, R"SQL(
INSERT INTO slice VALUES(5,'aten::other',280,320,40,0,1,''),
  (6,'Free',300,380,80,0,4,''),(7,'Computing',380,400,20,0,5,''),
  (8,'OtherKernel',380,400,20,0,2,'');
)SQL");
    for (const bool reverse : {false, true}) {
        SCOPED_TRACE(reverse);
        Exec(path, "DELETE FROM flow;");
        const std::string end = "('reused','async_npu',2,280,'f')";
        const std::string start = "('reused','async_npu',1,280,'s')";
        Exec(path,
            "INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES "
            "('reused','async_npu',1,120,'s')," +
                (reverse ? start + "," + end : end + "," + start) + ",('reused','async_npu',2,380,'f');");
        FreeAttributionSummary summary;
        const auto records = Details(Query(path), summary);
        ASSERT_EQ(records.size(), 2U);
        for (const auto &record : records) {
            EXPECT_EQ(record.attributionStatus, "matched");
            ASSERT_TRUE(record.hostApiTarget.has_value());
            EXPECT_EQ(record.hostApiTarget->id, record.freeTarget.timestamp == 100 ? "1" : "5");
        }
        EXPECT_EQ(summary.totalFreeCount, 2U);
        EXPECT_EQ(summary.totalFreeDuration, 160U);
    }
}

TEST_P(FreeAttributionDatabaseTest, AmbiguousReuseBoundaryKeepsBothGenerationsIncomplete) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow phases.";
    }
    const auto path = Source("uncertain-reuse-boundary");
    Compute(path);
    Exec(path, R"SQL(
INSERT INTO slice VALUES(5,'aten::other',280,290,10,0,1,'');
INSERT INTO thread VALUES(7,'32','7','Stream 7');
INSERT INTO slice VALUES(6,'OtherKernel',280,300,20,0,7,'');
)SQL");
    for (const bool priorEnded : {false, true}) {
        SCOPED_TRACE(priorEnded);
        Exec(path, "DELETE FROM flow;");
        Exec(path,
            priorEnded ? "UPDATE slice SET timestamp=180,end_time=190,duration=10 WHERE id=6;"
                       : "UPDATE slice SET timestamp=280,end_time=300,duration=20 WHERE id=6;");
        // Two simultaneous finals can straddle the old and new instances. A
        // previous final also allows an old fan-out beside the new start/final.
        Exec(path,
            "INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES "
            "('uncertain','async_npu',1,120,'s'),"
            "('uncertain','async_npu',7," +
                std::string(priorEnded ? "180" : "280") +
                ",'f'),"
                "('uncertain','async_npu',1,280,'s'),('uncertain','async_npu',2,280,'f');");
        FreeAttributionSummary summary;
        const auto records = Details(Query(path), summary);
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records[0].attributionStatus, "incomplete");
        EXPECT_FALSE(records[0].hostApiTarget.has_value());
        std::set<std::string> ids;
        for (const auto &candidate : records[0].candidateApiTargets) {
            ids.insert(candidate.id);
        }
        EXPECT_EQ(ids, (std::set<std::string>{"1", "5"}));
        EXPECT_EQ(summary.totalFreeCount, 1U);
        EXPECT_EQ(summary.totalFreeDuration, 80U);
    }
}

TEST_P(FreeAttributionDatabaseTest, ConsecutiveStartsKeepAllApiCandidates) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow phases.";
    }
    const auto path = Source("consecutive-starts");
    Compute(path);
    Exec(path, R"SQL(
INSERT INTO slice VALUES(5,'aten::other',160,190,30,0,1,'');
INSERT INTO process VALUES('128','CANN');
INSERT INTO thread VALUES(7,'128','7','Runtime');
INSERT INTO slice VALUES(6,'Launch',160,190,30,0,7,'');
)SQL");
    for (const auto order : {0, 1, 2}) {
        SCOPED_TRACE(order);
        Exec(path, "DELETE FROM flow;");
        const std::string start = "('multiple-starts','async_npu',1,170,'s')";
        const std::string relay = "('multiple-starts','async_npu',7,170,'t')";
        const auto middle = order == 0 ? start : (order == 1 ? start + "," + relay : relay + "," + start);
        Exec(path,
            "INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES "
            "('multiple-starts','async_npu',1,120,'s')," +
                middle + ",('multiple-starts','async_npu',2,280,'f');");
        FreeAttributionSummary summary;
        const auto records = Details(Query(path), summary);
        ASSERT_EQ(records.size(), 1U);
        EXPECT_EQ(records[0].attributionStatus, "ambiguous");
        EXPECT_FALSE(records[0].hostApiTarget.has_value());
        std::set<std::string> ids;
        for (const auto &candidate : records[0].candidateApiTargets) {
            ids.insert(candidate.id);
        }
        EXPECT_EQ(ids, (std::set<std::string>{"1", "5"}));
        EXPECT_EQ(summary.totalFreeCount, 1U);
    }
}

TEST_P(FreeAttributionDatabaseTest, DistinctTimeFanOutFinalsKeepTheSameApi) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow phases.";
    }
    const auto path = Source("fan-out-finals");
    Compute(path);
    Exec(path, R"SQL(
INSERT INTO slice VALUES(5,'Free',300,380,80,0,4,''),
  (6,'Computing',380,400,20,0,5,''),(7,'OtherKernel',380,400,20,0,2,'');
INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES('torch-link','async_npu',2,380,'f');
)SQL");
    FreeAttributionSummary summary;
    const auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 2U);
    for (const auto &record : records) {
        EXPECT_EQ(record.attributionStatus, "matched");
        ASSERT_TRUE(record.hostApiTarget.has_value());
        EXPECT_EQ(record.hostApiTarget->id, "1");
    }
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 160U);
}

TEST_P(FreeAttributionDatabaseTest, IndependentCannCyclesDoNotInvalidatePytorchApiFlow) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "This fixture exercises explicit TEXT flow point cycles.";
    }
    const auto path = Source("cyclic-flow");
    Compute(path);
    Exec(path, R"SQL(
INSERT INTO process VALUES('128','CANN');
INSERT INTO thread VALUES(7,'128','7','Runtime A'),(8,'128','8','Runtime B');
INSERT INTO slice VALUES(5,'Runtime A',160,170,10,0,7,''),(6,'Runtime B',180,190,10,0,8,'');
INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES
  ('cycle-a','HostToDevice',7,165,'s'),('cycle-a','HostToDevice',8,185,'f'),
  ('cycle-b','HostToDevice',8,186,'s'),('cycle-b','HostToDevice',7,166,'f'),
  ('cycle-device','HostToDevice',8,187,'s'),('cycle-device','HostToDevice',2,280,'f');
)SQL");
    FreeAttributionSummary summary;
    const auto records = Details(Query(path), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "matched");
    ASSERT_TRUE(records[0].hostApiTarget.has_value());
    EXPECT_EQ(records[0].hostApiTarget->name, "aten::add");
    EXPECT_EQ(summary.totalFreeDuration, 80U);
}

TEST_P(FreeAttributionDatabaseTest, DerivedOverlapFindsApiInNonRepresentativeSource) {
    if (GetParam() != DataType::DB) {
        GTEST_SKIP() << "Rank-derived overlap applies to the DB import format.";
    }
    const auto first = Source("first-source");
    const auto second = Source("second-source");
    Compute(second);
    auto &manager = DataBaseManager::Instance();
    manager.SetRepresentativeSource("free-test-rank", first);
    const auto derived = RankOverlapStore::Instance().Publish("free-test-rank", "0", {{200, 280, 3}, {280, 300, 0}});
    ASSERT_FALSE(derived.empty());
    FreeAttributionSummary summary;
    const auto records = Details(Query(first), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "matched");
    ASSERT_TRUE(records[0].hostApiTarget.has_value());
    EXPECT_EQ(records[0].hostApiTarget->cardId, "free-test-rank");
    EXPECT_EQ(records[0].hostApiTarget->dbPath, second);
    ExpectHostLane(*records[0].hostApiTarget);
    EXPECT_EQ(records[0].freeTarget.dbPath, derived);
    EXPECT_EQ(summary.totalFreeCount, 1U);
}

TEST_P(FreeAttributionDatabaseTest, KeepsHostSourcesDistinctWithTheSameRankAndThread) {
    if (GetParam() != DataType::DB) {
        GTEST_SKIP() << "Rank-derived overlap applies to the DB import format.";
    }
    const auto first = Source("same-thread-first-source");
    const auto second = Source("same-thread-second-source");
    Compute(first);
    Compute(second);
    DataBaseManager::Instance().SetRepresentativeSource("free-test-rank", first);
    ASSERT_FALSE(RankOverlapStore::Instance().Publish("free-test-rank", "0", {{200, 280, 3}, {280, 300, 0}}).empty());
    FreeAttributionSummary summary;
    const auto records = Details(Query(first), summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].attributionStatus, "ambiguous");
    ASSERT_EQ(records[0].candidateApiTargets.size(), 2U);
    std::set<std::string> sourcePaths;
    for (const auto &target : records[0].candidateApiTargets) {
        EXPECT_EQ(target.cardId, "free-test-rank");
        EXPECT_EQ(target.pid, "4294967306");
        EXPECT_EQ(target.tid, "pytorch");
        sourcePaths.insert(target.dbPath);
        ExpectHostLane(target);
    }
    EXPECT_EQ(sourcePaths, (std::set<std::string>{first, second}));
}

TEST_P(FreeAttributionDatabaseTest, ReportsBatchAndCachedQueriesForTenThousandFreeEvents) {
    if (GetParam() != DataType::TEXT) {
        GTEST_SKIP() << "The scale fixture exercises TEXT flow endpoint indexing.";
    }
    const auto path = Source("scale");
    constexpr uint64_t count = 10000;
    std::ostringstream slices;
    std::ostringstream flows;
    slices << "BEGIN; INSERT INTO slice VALUES ";
    flows << "; INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES ";
    for (uint64_t index = 0; index < count; ++index) {
        const auto start = 200 + index * 40;
        const auto id = index * 4 + 1;
        if (index != 0) {
            slices << ',';
            flows << ',';
        }
        slices << '(' << id << ",'aten::add'," << start - 10 << ',' << start - 5 << ",5,0,1,'')," << '(' << id + 1
               << ",'Free'," << start << ',' << start + 20 << ",20,0,4,'')," << '(' << id + 2 << ",'Computing',"
               << start + 20 << ',' << start + 40 << ",20,0,5,'')," << '(' << id + 3 << ",'AddKernel'," << start + 20
               << ',' << start + 40 << ",20,0,2,'')";
        flows << "('flow" << index << "','async_npu',1," << start - 9 << ",'s'),"
              << "('flow" << index << "','async_npu',2," << start + 20 << ",'f')";
    }
    Exec(path, slices.str() + flows.str() + "; COMMIT;");
    auto params = Query(path);
    FreeAttributionSummary first;
    FreeAttributionSummary cached;
    FreeAttributionSummary ranged;
    std::string error;
    const auto firstStart = std::chrono::steady_clock::now();
    ASSERT_TRUE(service.QuerySummary(params, first, error)) << error;
    const auto cacheStart = std::chrono::steady_clock::now();
    ASSERT_TRUE(service.QuerySummary(params, cached, error)) << error;
    const auto rangeStart = std::chrono::steady_clock::now();
    params.startTime = 200 + (count / 2) * 40 - 100 + 1;
    params.endTime = params.startTime + 1;
    ASSERT_TRUE(service.QuerySummary(params, ranged, error)) << error;
    const auto end = std::chrono::steady_clock::now();
    EXPECT_EQ(first.totalFreeCount, count);
    EXPECT_EQ(first.totalFreeDuration, count * 20);
    ASSERT_EQ(first.groups.size(), 1U);
    EXPECT_EQ(first.groups[0].apiName, "aten::add");
    EXPECT_EQ(cached.totalFreeCount, count);
    EXPECT_EQ(ranged.totalFreeCount, 1U);
    EXPECT_EQ(ranged.totalFreeDuration, 20U);
    RecordProperty("freeEvents", std::to_string(count));
    RecordProperty("firstQueryMs",
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(cacheStart - firstStart).count()));
    RecordProperty("cachedQueryMs",
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(rangeStart - cacheStart).count()));
    RecordProperty("rangeQueryMs",
        std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(end - rangeStart).count()));
}

TEST_P(FreeAttributionDatabaseTest, ExplicitRangeModesRemainDistinctAcrossCachedQueries) {
    const auto path = Source("range-modes");
    Compute(path);
    Exec(path,
        GetParam() == DataType::TEXT ? R"SQL(
INSERT INTO slice VALUES(5,'aten::add',100,103,3,0,1,''),
  (6,'Free',100,105,5,0,4,''),(7,'Computing',105,108,3,0,5,''),
  (8,'AddKernel',105,108,3,0,2,'');
INSERT INTO flow(flow_id,cat,track_id,timestamp,type) VALUES
  ('early-link','async_npu',1,101,'s'),('early-link','async_npu',2,105,'f');
)SQL"
                                     : R"SQL(
INSERT INTO OVERLAP_ANALYSIS VALUES(3,0,100,105,3),(4,0,105,108,0);
INSERT INTO TASK VALUES(2,0,105,108,2,0,43,2);
INSERT INTO COMPUTE_TASK_INFO VALUES(2,2);
INSERT INTO PYTORCH_API VALUES(1,100,103,4294967306,50001,0,8);
INSERT INTO CONNECTION_IDS VALUES(8,43);
)SQL");
    auto params = Query(path);
    FreeAttributionSummary summary;
    auto records = Details(params, summary);
    ASSERT_EQ(records.size(), 2U);
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 85U);
    ASSERT_EQ(summary.groups.size(), 1U);

    // Only query state changes below: every nonempty request can reuse the same
    // source snapshot, while rangeMode must still change its selected Free set.
    params.rangeMode = "range";
    params.startTime = params.endTime = 0;
    records = Details(params, summary);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].freeTarget.timestamp, 0U);
    EXPECT_EQ(summary.totalFreeCount, 1U);
    EXPECT_EQ(summary.totalFreeDuration, 5U);
    ASSERT_EQ(summary.groups.size(), 1U);
    params.groupId = summary.groups[0].groupId;
    FreeAttributionDetails details;
    std::string error;
    ASSERT_TRUE(service.QueryDetails(params, details, error)) << error;
    EXPECT_EQ(details.totalRecords, 1U);

    params.rangeMode = "all";
    params.startTime = 900;
    params.endTime = 901;
    records = Details(params, summary);
    EXPECT_EQ(records.size(), 2U);
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 85U);

    params.rangeMode = "empty";
    params.startTime = params.endTime = 0;
    ASSERT_TRUE(service.QuerySummary(params, summary, error)) << error;
    ASSERT_TRUE(service.QueryDetails(params, details, error)) << error;
    EXPECT_TRUE(summary.groups.empty());
    EXPECT_EQ(summary.totalGroups, 0U);
    EXPECT_EQ(summary.totalFreeCount, 0U);
    EXPECT_EQ(summary.totalFreeDuration, 0U);
    EXPECT_TRUE(details.records.empty());
    EXPECT_EQ(details.totalRecords, 0U);

    params.rangeMode = "range";
    params.startTime = params.endTime = 50;
    records = Details(params, summary);
    EXPECT_TRUE(records.empty());
    EXPECT_EQ(summary.totalFreeCount, 0U);

    params.rangeMode.clear();
    params.startTime = params.endTime = 0;
    records = Details(params, summary);
    EXPECT_EQ(records.size(), 2U);
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 85U);
}

TEST_P(FreeAttributionDatabaseTest, EmptyModeClearsResultsWithoutAnySource) {
    auto params = Query("missing.db");
    params.rangeMode = "empty";
    params.groupId = "matched:aten::add";
    FreeAttributionSummary summary;
    summary.groups.push_back({"matched:aten::add", "matched", "aten::add", 80, 1});
    summary.totalGroups = 1;
    summary.totalFreeCount = 1;
    summary.totalFreeDuration = 80;
    FreeAttributionDetails details;
    details.records.emplace_back();
    details.totalRecords = 1;
    std::string error = "previous query error";
    ASSERT_TRUE(service.QuerySummary(params, summary, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(summary.groups.empty());
    EXPECT_EQ(summary.totalGroups, 0U);
    EXPECT_EQ(summary.totalFreeCount, 0U);
    EXPECT_EQ(summary.totalFreeDuration, 0U);
    error = "previous detail error";
    ASSERT_TRUE(service.QueryDetails(params, details, error));
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(details.records.empty());
    EXPECT_EQ(details.totalRecords, 0U);
}

INSTANTIATE_TEST_SUITE_P(Formats, FreeAttributionDatabaseTest, testing::Values(DataType::TEXT, DataType::DB),
    [](const testing::TestParamInfo<DataType> &info) { return info.param == DataType::TEXT ? "Text" : "Db"; });
} // namespace
} // namespace Dic::Module::Timeline
