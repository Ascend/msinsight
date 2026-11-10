/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include "MsprofFtraceReader.h"
#include "sqlite3.h"

namespace Dic::Module::FullDb {
namespace {
namespace fs = std::filesystem;
constexpr uint64_t EPOCH_NS = 1787735841201825000ULL;

class MsprofFtraceReaderTest : public testing::Test {
  protected:
    void SetUp() override {
        static std::atomic<unsigned> sequence{0};
        directory_ = fs::temp_directory_path() /
            ("msprof-ftrace-reader-" + std::to_string(sequence++) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(fs::create_directory(directory_));
        path_ = directory_ / "source.db";
        ASSERT_EQ(sqlite3_open(path_.u8string().c_str(), &db_), SQLITE_OK);
        ASSERT_TRUE(Exec(R"(
CREATE TABLE META_DATA(name TEXT,value TEXT);
INSERT INTO META_DATA VALUES('SCHEMA_VERSION','1.2.0'),('ftrace',NULL),('ftrace_events_complete','1');
CREATE TABLE STRING_IDS(id INTEGER PRIMARY KEY,value TEXT);
CREATE TABLE GENERIC_EVENT_SOURCES(sourceId INTEGER PRIMARY KEY,nameId INTEGER,timeSourceId INTEGER,
 sourceGroupId INTEGER,data TEXT);
CREATE TABLE GENERIC_EVENT_TYPES(typeId INTEGER PRIMARY KEY,sourceId INTEGER,nameId INTEGER,ftraceFormat TEXT,data TEXT);
CREATE TABLE GENERIC_EVENT_TYPE_FIELDS(typeId INTEGER,fieldIdx INTEGER,fieldNameId INTEGER,offset INTEGER,
 size INTEGER,type INTEGER,ftracePrefix TEXT,ftraceSuffix TEXT,PRIMARY KEY(typeId,fieldIdx));
CREATE TABLE GENERIC_EVENTS(genericEventId INTEGER PRIMARY KEY,rawTimestamp INTEGER,timestamp INTEGER,
 typeId INTEGER,globalTid INTEGER,cpuId INTEGER,flags INTEGER);
CREATE TABLE GENERIC_EVENT_DATA(genericEventId INTEGER,fieldIdx INTEGER,intVal INTEGER,uintVal INTEGER,
 floatVal REAL,doubleVal REAL,strId INTEGER,PRIMARY KEY(genericEventId,fieldIdx));
INSERT INTO STRING_IDS VALUES(1,'worker_thread_1'),(10,'sched'),(11,'irq'),(12,'other'),
 (20,'sched_switch'),(21,'irq:softirq_entry'),(22,''),(23,'other:event'),
 (100,'common_pid'),(101,'prev_comm'),(102,'vec');
INSERT INTO GENERIC_EVENT_SOURCES VALUES(8,10,3,1,NULL),(15,11,3,1,NULL),(99,12,3,2,NULL);
INSERT INTO GENERIC_EVENT_TYPES VALUES(900,8,20,'format for sched',NULL),
 (901,15,21,'format for irq',NULL),(902,99,23,NULL,NULL);
INSERT INTO GENERIC_EVENT_TYPE_FIELDS VALUES
 (900,6,100,4,4,0,'printed delimiter',NULL),(900,19,101,8,16,0,'not a C type',NULL),
 (901,2,100,4,4,0,NULL,NULL),(901,27,102,8,4,1,NULL,NULL);
CREATE INDEX GenericEventTsIndex ON GENERIC_EVENTS(timestamp);
INSERT INTO GENERIC_EVENTS VALUES
 (1,42,1787735841201825100,900,30064775314,8,1),
 (2,43,1787735841201825100,901,30064775314,8,0),
 (30,41,1787735841201825050,900,NULL,20,0),
 (9,44,1787735841201825200,900,NULL,NULL,0),
 (55,45,1787735841201825250,902,NULL,31,0);
INSERT INTO GENERIC_EVENT_DATA VALUES
 (1,6,4242,0,0,0,0),(1,19,0,0,0,0,1),
 (2,2,4242,0,0,0,0),(2,27,0,3,0,0,0),
 (30,6,123,0,0,0,0),(30,19,0,0,0,0,22),
 (9,6,777,0,0,0,0),(9,19,0,0,0,0,1);
)"));
    }

    void TearDown() override {
        if (db_ != nullptr) {
            sqlite3_close(db_);
            db_ = nullptr;
        }
        std::error_code error;
        fs::remove(path_, error);
        fs::remove(directory_, error);
    }

    bool Exec(const std::string &sql) { return sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, nullptr) == SQLITE_OK; }

    bool RemoveCompositeKeys() {
        return Exec(R"(
ALTER TABLE GENERIC_EVENT_TYPE_FIELDS RENAME TO old_fields;
CREATE TABLE GENERIC_EVENT_TYPE_FIELDS(typeId INTEGER,fieldIdx INTEGER,fieldNameId INTEGER,offset INTEGER,
 size INTEGER,type INTEGER,ftracePrefix TEXT,ftraceSuffix TEXT);
INSERT INTO GENERIC_EVENT_TYPE_FIELDS SELECT * FROM old_fields;
DROP TABLE old_fields;
ALTER TABLE GENERIC_EVENT_DATA RENAME TO old_data;
CREATE TABLE GENERIC_EVENT_DATA(genericEventId INTEGER,fieldIdx INTEGER,intVal INTEGER,uintVal INTEGER,
 floatVal REAL,doubleVal REAL,strId INTEGER);
INSERT INTO GENERIC_EVENT_DATA SELECT * FROM old_data;
DROP TABLE old_data;
)");
    }

    void CloseWriter() {
        ASSERT_EQ(sqlite3_close(db_), SQLITE_OK);
        db_ = nullptr;
    }

    std::string Bytes() const {
        std::ifstream input(path_, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    }

    std::vector<MsprofFtraceEvent> ReadAll(std::string &error) {
        std::vector<MsprofFtraceEvent> events;
        const bool result = MsprofFtraceReader::Read(
            path_.string(),
            [&events](const MsprofFtraceEvent &event) {
                events.push_back(event);
                return true;
            },
            error);
        EXPECT_TRUE(result) << error;
        return events;
    }

    fs::path directory_;
    fs::path path_;
    sqlite3 *db_ = nullptr;
};

TEST_F(MsprofFtraceReaderTest, RecognizesMarkerWithoutInterpretingValueAndReadsEpochNanoseconds) {
    CloseWriter();
    const auto before = Bytes();
    const auto inspection = MsprofFtraceReader::Inspect(path_.string());
    ASSERT_EQ(inspection.status, MsprofFtraceStatus::READY) << inspection.error;
    EXPECT_EQ(inspection.schemaVersion, "1.2.0");
    std::string error;
    const auto events = ReadAll(error);
    ASSERT_EQ(events.size(), 4U);
    EXPECT_EQ(events.front().timestampNs, EPOCH_NS + 50);
    EXPECT_EQ(events.back().timestampNs, EPOCH_NS + 200);
    std::set<std::string> names;
    for (const auto &event : events) {
        names.insert(event.name);
    }
    EXPECT_EQ(names, (std::set<std::string>{"sched:sched_switch", "irq:softirq_entry"}));
    EXPECT_EQ(events[0].eventId, 30U);
    EXPECT_EQ(events[1].eventId, 1U);
    EXPECT_EQ(events[2].eventId, 2U);
    EXPECT_EQ(events[3].eventId, 9U);
    EXPECT_EQ(events[1].timestampNs, EPOCH_NS + 100);
    EXPECT_EQ(events[1].rawTimestamp, 42U);
    EXPECT_EQ(events[1].cpu, 8U);
    EXPECT_EQ(events[1].sourceId, 8U);
    EXPECT_EQ(events[2].sourceId, 15U);
    EXPECT_EQ(events[1].typeId, 900U);
    EXPECT_EQ(events[1].flags, 1U);
    EXPECT_EQ(events[1].globalTid, 30064775314ULL);
    ASSERT_NE(events[1].type, nullptr);
    EXPECT_EQ(events[1].type->format, "format for sched");
    EXPECT_EQ(Bytes(), before);
    EXPECT_FALSE(fs::exists(path_.string() + "-wal"));
    EXPECT_FALSE(fs::exists(path_.string() + "-shm"));
}

TEST_F(MsprofFtraceReaderTest, ReadsZeroFilledScalarsPositiveStringIdsEmptyStringsAndNullCpu) {
    CloseWriter();
    std::string error;
    const auto events = ReadAll(error);
    ASSERT_EQ(events.size(), 4U);
    EXPECT_EQ(std::get<std::string>(events[0].fields.at("prev_comm")), "");
    EXPECT_EQ(std::get<std::string>(events[1].fields.at("prev_comm")), "worker_thread_1");
    EXPECT_EQ(std::get<int64_t>(events[1].fields.at("common_pid")), 4242);
    EXPECT_EQ(std::get<uint64_t>(events[2].fields.at("vec")), 3U);
    EXPECT_FALSE(events[3].cpu.has_value());
}

TEST_F(MsprofFtraceReaderTest, KeepsEventOrderWithAndWithoutAnEligibleTimestampIndex) {
    const std::vector<std::string> indexStatements = {
        "DROP INDEX GenericEventTsIndex",
        "CREATE INDEX \"timestamp \"\" custom\" ON GENERIC_EVENTS(timestamp,genericEventId)",
        "DROP INDEX \"timestamp \"\" custom\";"
        "CREATE INDEX only_switches ON GENERIC_EVENTS(timestamp) WHERE typeId=900;"
        "CREATE INDEX expression_timestamp ON GENERIC_EVENTS(timestamp+0)",
    };
    for (const auto &sql : indexStatements) {
        ASSERT_TRUE(Exec(sql));
        std::string error;
        const auto events = ReadAll(error);
        ASSERT_EQ(events.size(), 4U);
        EXPECT_EQ(events[0].eventId, 30U);
        EXPECT_EQ(events[1].eventId, 1U);
        EXPECT_EQ(events[2].eventId, 2U);
        EXPECT_EQ(events[3].eventId, 9U);
    }
}

TEST_F(MsprofFtraceReaderTest, AcceptsEmptyTextAndJsonFeatureMarkerValues) {
    for (const std::string value : {"", "selected events", "{\"events\":[\"sched/sched_switch\"]}"}) {
        sqlite3_stmt *statement = nullptr;
        ASSERT_EQ(sqlite3_prepare_v2(db_, "UPDATE META_DATA SET value=? WHERE name='ftrace'", -1, &statement, nullptr),
            SQLITE_OK);
        sqlite3_bind_text(statement, 1, value.c_str(), -1, SQLITE_TRANSIENT);
        ASSERT_EQ(sqlite3_step(statement), SQLITE_DONE);
        sqlite3_finalize(statement);
        EXPECT_EQ(MsprofFtraceReader::Inspect(path_.string()).status, MsprofFtraceStatus::READY);
    }
}

TEST_F(MsprofFtraceReaderTest, RequiresAnIndependentCompletionMarker) {
    ASSERT_TRUE(Exec("DELETE FROM META_DATA WHERE name='ftrace_events_complete'"));
    EXPECT_EQ(MsprofFtraceReader::Inspect(path_.string()).status, MsprofFtraceStatus::INCOMPLETE);
    ASSERT_TRUE(Exec("INSERT INTO META_DATA VALUES('ftrace_events_complete','0')"));
    EXPECT_EQ(MsprofFtraceReader::Inspect(path_.string()).status, MsprofFtraceStatus::INCOMPLETE);
    ASSERT_TRUE(Exec("UPDATE META_DATA SET value='1' WHERE name='ftrace_events_complete'"));
    EXPECT_TRUE(MsprofFtraceReader::Inspect(path_.string()).IsReady());
}

TEST_F(MsprofFtraceReaderTest, IgnoresGenericTablesWithoutFeatureMarker) {
    ASSERT_TRUE(Exec("DELETE FROM META_DATA WHERE name='ftrace'"));
    const auto inspection = MsprofFtraceReader::Inspect(path_.string());
    EXPECT_EQ(inspection.status, MsprofFtraceStatus::ABSENT);
    EXPECT_FALSE(inspection.HasFtrace());
}

TEST_F(MsprofFtraceReaderTest, AcceptsFinalized120AndRejectsOtherLegacySchemaProfiles) {
    ASSERT_TRUE(Exec("UPDATE META_DATA SET value='1.2.0' WHERE name='SCHEMA_VERSION'"));
    EXPECT_TRUE(MsprofFtraceReader::Inspect(path_.string()).IsReady());
    for (const auto &version : {"1.2.1", "1.3.0", "1.4.0", "2.0.0"}) {
        ASSERT_TRUE(Exec(std::string("UPDATE META_DATA SET value='") + version + "' WHERE name='SCHEMA_VERSION'"));
        EXPECT_EQ(MsprofFtraceReader::Inspect(path_.string()).status, MsprofFtraceStatus::UNSUPPORTED);
    }
    ASSERT_TRUE(Exec("UPDATE META_DATA SET value='1.2.0' WHERE name='SCHEMA_VERSION';"
                     "ALTER TABLE GENERIC_EVENT_DATA RENAME COLUMN strId TO oldStringValue"));
    const auto inspection = MsprofFtraceReader::Inspect(path_.string());
    EXPECT_EQ(inspection.status, MsprofFtraceStatus::INVALID);
    EXPECT_NE(inspection.error.find("strId"), std::string::npos);
}

TEST_F(MsprofFtraceReaderTest, AcceptsCompletedEmptyCapture) {
    ASSERT_TRUE(Exec("DELETE FROM GENERIC_EVENT_DATA; DELETE FROM GENERIC_EVENTS"));
    const auto inspection = MsprofFtraceReader::Inspect(path_.string());
    ASSERT_TRUE(inspection.IsReady()) << inspection.error;
    CloseWriter();
    std::string error;
    EXPECT_TRUE(ReadAll(error).empty());
}

TEST_F(MsprofFtraceReaderTest, IgnoresUnsupportedEventDefinitionsAndValuesBeforeDecoding) {
    ASSERT_TRUE(Exec(R"(
INSERT INTO STRING_IDS VALUES(300,'power:cpu_idle'),(301,'custom:marker');
INSERT INTO GENERIC_EVENT_TYPES VALUES(903,8,300,'unused format',NULL),(904,8,301,NULL,NULL);
INSERT INTO GENERIC_EVENT_TYPE_FIELDS VALUES(903,0,999999,-1,-1,99,NULL,NULL),
 (904,0,999999,0,8,99,NULL,NULL);
INSERT INTO GENERIC_EVENTS VALUES(60,-1,'not a timestamp',903,'not a tid',-1,-1),
 (61,-1,'not a timestamp',904,NULL,NULL,0);
INSERT INTO GENERIC_EVENT_DATA VALUES(60,0,1,2,NULL,NULL,999999),
 (61,0,NULL,3.5,NULL,NULL,999999),(999999,0,1,NULL,NULL,NULL,NULL);
)"));
    CloseWriter();
    const auto before = Bytes();
    const auto inspection = MsprofFtraceReader::Inspect(path_.string());
    ASSERT_TRUE(inspection.IsReady()) << inspection.error;
    std::string error;
    const auto events = ReadAll(error);
    ASSERT_EQ(events.size(), 4U);
    EXPECT_EQ(events.front().timestampNs, EPOCH_NS + 50);
    EXPECT_EQ(events.back().timestampNs, EPOCH_NS + 200);
    std::set<std::string> names;
    for (const auto &event : events) {
        names.insert(event.name);
    }
    EXPECT_EQ(names, (std::set<std::string>{"sched:sched_switch", "irq:softirq_entry"}));
    EXPECT_EQ(Bytes(), before);
}

TEST_F(MsprofFtraceReaderTest, UnsupportedOnlySourceEmitsNoCallbacks) {
    ASSERT_TRUE(Exec("UPDATE STRING_IDS SET value='power:cpu_idle' WHERE id=20;"
                     "UPDATE STRING_IDS SET value='custom:marker' WHERE id=21;"
                     "UPDATE GENERIC_EVENT_TYPE_FIELDS SET type=999,fieldNameId=999999;"
                     "UPDATE GENERIC_EVENT_DATA SET intVal=1,uintVal=2,strId=999999;"));
    CloseWriter();
    const auto before = Bytes();
    const auto inspection = MsprofFtraceReader::Inspect(path_.string());
    ASSERT_TRUE(inspection.IsReady()) << inspection.error;
    std::string error;
    EXPECT_TRUE(ReadAll(error).empty());
    EXPECT_EQ(Bytes(), before);
}

TEST_F(MsprofFtraceReaderTest, InspectionLeavesEventValidationToTheReadStream) {
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENTS SET timestamp=-1 WHERE genericEventId=30"));
    CloseWriter();
    const auto before = Bytes();
    const auto inspection = MsprofFtraceReader::Inspect(path_.string());
    ASSERT_TRUE(inspection.IsReady()) << inspection.error;
    unsigned callbacks = 0;
    std::string error;
    EXPECT_FALSE(MsprofFtraceReader::Read(
        path_.string(),
        [&callbacks](const auto &) {
            ++callbacks;
            return true;
        },
        error));
    EXPECT_EQ(callbacks, 0U);
    EXPECT_NE(error.find("event header"), std::string::npos);
    EXPECT_EQ(Bytes(), before);
}

TEST_F(MsprofFtraceReaderTest, InspectionStillRejectsInvalidSelectedFieldDefinitions) {
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_TYPE_FIELDS SET type=99 WHERE typeId=900 AND fieldIdx=6"));
    const auto inspection = MsprofFtraceReader::Inspect(path_.string());
    EXPECT_EQ(inspection.status, MsprofFtraceStatus::INVALID);
    EXPECT_NE(inspection.error.find("field definition"), std::string::npos);
}

TEST_F(MsprofFtraceReaderTest, RejectsMissingStringReferencesAndInvalidStringSentinels) {
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET strId=99999 WHERE genericEventId=1 AND fieldIdx=19"));
    std::string error;
    EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
    EXPECT_NE(error.find("STRING_IDS"), std::string::npos);
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET strId=NULL WHERE genericEventId=1 AND fieldIdx=19"));
    EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
    EXPECT_NE(error.find("string sentinel"), std::string::npos);
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET strId=-1 WHERE genericEventId=1 AND fieldIdx=19"));
    EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
    EXPECT_NE(error.find("string sentinel"), std::string::npos);
}

TEST_F(MsprofFtraceReaderTest, RejectsFloatingPointAndNegativeNarrowUnsignedValuesWithoutRounding) {
    ASSERT_TRUE(
        Exec("UPDATE GENERIC_EVENT_DATA SET uintVal=9007199254740992.5 WHERE genericEventId=2 AND fieldIdx=27"));
    // Use a non-integral REAL small enough that SQLite does not convert it back to INTEGER affinity.
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET uintVal=3.5 WHERE genericEventId=2 AND fieldIdx=27"));
    std::string error;
    EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
    EXPECT_NE(error.find("value type"), std::string::npos);
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET uintVal=-1 WHERE genericEventId=2 AND fieldIdx=27"));
    EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
    EXPECT_NE(error.find("unsigned"), std::string::npos);
}

TEST_F(MsprofFtraceReaderTest, PreservesUnsigned64BitIntegerPatternsAndExactDecimalValues) {
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_TYPE_FIELDS SET size=8 WHERE typeId=901 AND fieldIdx=27"));
    const std::vector<std::pair<sqlite3_int64, uint64_t>> cases = {{-1, UINT64_MAX}, {INT64_MIN, uint64_t{1} << 63}};
    const std::vector<std::string> decimals = {"18446744073709551615", "9223372036854775808"};
    for (size_t index = 0; index < cases.size(); ++index) {
        sqlite3_stmt *statement = nullptr;
        ASSERT_EQ(
            sqlite3_prepare_v2(db_, "UPDATE GENERIC_EVENT_DATA SET uintVal=? WHERE genericEventId=2 AND fieldIdx=27",
                -1, &statement, nullptr),
            SQLITE_OK);
        ASSERT_EQ(sqlite3_bind_int64(statement, 1, cases[index].first), SQLITE_OK);
        ASSERT_EQ(sqlite3_step(statement), SQLITE_DONE);
        ASSERT_EQ(sqlite3_finalize(statement), SQLITE_OK);
        std::string error;
        const auto events = ReadAll(error);
        ASSERT_EQ(events.size(), 4U);
        const auto &value = events[2].fields.at("vec");
        ASSERT_TRUE(std::holds_alternative<uint64_t>(value));
        EXPECT_EQ(std::get<uint64_t>(value), cases[index].second);
        EXPECT_EQ(std::to_string(std::get<uint64_t>(value)), decimals[index]);
    }
    for (const auto &invalid : {"3.5", "'not-an-integer'"}) {
        ASSERT_TRUE(Exec(std::string("UPDATE GENERIC_EVENT_DATA SET uintVal=") + invalid +
            " WHERE genericEventId=2 AND fieldIdx=27"));
        std::string error;
        EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
        EXPECT_NE(error.find("value type"), std::string::npos);
    }
}

TEST_F(MsprofFtraceReaderTest, RejectsMissingTrustedNumericValueAndKeepsRealZero) {
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET intVal=NULL WHERE genericEventId=9 AND fieldIdx=6"));
    std::string error;
    EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
    EXPECT_NE(error.find("value type"), std::string::npos);
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET intVal=0 WHERE genericEventId=9 AND fieldIdx=6"));
    CloseWriter();
    const auto events = ReadAll(error);
    ASSERT_EQ(events.size(), 4U);
    EXPECT_EQ(std::get<int64_t>(events.back().fields.at("common_pid")), 0);
    EXPECT_EQ(std::get<std::string>(events.front().fields.at("prev_comm")), "");
}

TEST_F(MsprofFtraceReaderTest, RejectsMissingOrUnregisteredFieldRows) {
    ASSERT_TRUE(Exec("DELETE FROM GENERIC_EVENT_DATA WHERE genericEventId=1 AND fieldIdx=6"));
    std::string error;
    EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
    EXPECT_NE(error.find("Missing msprof ftrace field"), std::string::npos);
    ASSERT_TRUE(Exec("INSERT INTO GENERIC_EVENT_DATA VALUES(1,666,3,0,0,0,0)"));
    EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
    EXPECT_NE(error.find("Unknown msprof ftrace field"), std::string::npos);
}

TEST_F(MsprofFtraceReaderTest, AcceptsCompositeUniqueIndexesAndLogicalKeysWithoutIndexes) {
    ASSERT_TRUE(RemoveCompositeKeys());
    for (unsigned pass = 0; pass < 2; ++pass) {
        if (pass != 0) {
            ASSERT_TRUE(Exec("CREATE UNIQUE INDEX UxFields ON GENERIC_EVENT_TYPE_FIELDS(typeId,fieldIdx);"
                             "CREATE UNIQUE INDEX UxData ON GENERIC_EVENT_DATA(genericEventId,fieldIdx)"));
        }
        const auto inspection = MsprofFtraceReader::Inspect(path_.string());
        ASSERT_TRUE(inspection.IsReady()) << inspection.error;
        std::string error;
        EXPECT_EQ(ReadAll(error).size(), 4U);
    }
}

TEST_F(MsprofFtraceReaderTest, RejectsDuplicateFieldDefinitionsWithoutACompositeConstraint) {
    ASSERT_TRUE(RemoveCompositeKeys());
    ASSERT_TRUE(Exec("INSERT INTO GENERIC_EVENT_TYPE_FIELDS SELECT * FROM GENERIC_EVENT_TYPE_FIELDS "
                     "WHERE typeId=900 AND fieldIdx=6"));
    const auto inspection = MsprofFtraceReader::Inspect(path_.string());
    EXPECT_EQ(inspection.status, MsprofFtraceStatus::INVALID);
    EXPECT_NE(inspection.error.find("Duplicate msprof ftrace field definition"), std::string::npos);
}

TEST_F(MsprofFtraceReaderTest, RejectsDuplicateDataKeysForBothTrustedAndFailedEvents) {
    ASSERT_TRUE(RemoveCompositeKeys());
    ASSERT_TRUE(Exec("INSERT INTO GENERIC_EVENT_DATA SELECT * FROM GENERIC_EVENT_DATA "
                     "WHERE genericEventId=1 AND fieldIdx=6"));
    for (const auto flags : {0, 2}) {
        ASSERT_TRUE(Exec("UPDATE GENERIC_EVENTS SET flags=" + std::to_string(flags) + " WHERE genericEventId=1"));
        std::string error;
        EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
        EXPECT_NE(error.find("Duplicate msprof ftrace field value"), std::string::npos);
    }
}

TEST_F(MsprofFtraceReaderTest, EmitsFailedEventHeadersWithoutDecodingTheirPlaceholderValues) {
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET intVal=NULL,uintVal='invalid',strId=99999 "
                     "WHERE genericEventId=1;"
                     "UPDATE GENERIC_EVENTS SET globalTid='untrusted' WHERE genericEventId=1"));
    for (const auto flags : {2U, 3U, 0x102U}) {
        ASSERT_TRUE(Exec("UPDATE GENERIC_EVENTS SET flags=" + std::to_string(flags) + " WHERE genericEventId=1"));
        std::string error;
        const auto events = ReadAll(error);
        ASSERT_EQ(events.size(), 4U);
        const auto &failed = events[1];
        EXPECT_EQ(failed.eventId, 1U);
        EXPECT_EQ(failed.timestampNs, EPOCH_NS + 100);
        EXPECT_EQ(failed.cpu, 8U);
        EXPECT_EQ(failed.flags, flags);
        EXPECT_EQ(failed.name, "sched:sched_switch");
        EXPECT_TRUE(failed.fields.empty());
        EXPECT_FALSE(failed.globalTid.has_value());
    }
}

TEST_F(MsprofFtraceReaderTest, FailedEventsAllowMissingValuesButStillRejectUnknownFieldKeys) {
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENTS SET flags=2 WHERE genericEventId=1;"
                     "DELETE FROM GENERIC_EVENT_DATA WHERE genericEventId=1"));
    std::string error;
    const auto events = ReadAll(error);
    ASSERT_EQ(events.size(), 4U);
    EXPECT_TRUE(events[1].fields.empty());
    ASSERT_TRUE(Exec("INSERT INTO GENERIC_EVENT_DATA VALUES(1,666,0,0,0,0,0)"));
    EXPECT_FALSE(MsprofFtraceReader::Read(path_.string(), [](const auto &) { return true; }, error));
    EXPECT_NE(error.find("Unknown msprof ftrace field index"), std::string::npos);
}

TEST_F(MsprofFtraceReaderTest, PreservesRealZeroWithCpuFileAndCommonFlagsBits) {
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET intVal=0 WHERE genericEventId=1 AND fieldIdx=6"));
    for (const auto flags : {0U, 1U, 0x100U, 0x101U}) {
        ASSERT_TRUE(Exec("UPDATE GENERIC_EVENTS SET flags=" + std::to_string(flags) + " WHERE genericEventId=1"));
        std::string error;
        const auto events = ReadAll(error);
        ASSERT_EQ(events.size(), 4U);
        EXPECT_EQ(std::get<int64_t>(events[1].fields.at("common_pid")), 0);
        EXPECT_EQ(std::get<std::string>(events[1].fields.at("prev_comm")), "worker_thread_1");
        EXPECT_EQ(events[1].flags, flags);
    }
}

TEST_F(MsprofFtraceReaderTest, ReadsDynamicAndShortStringsWithoutUsingPhysicalSizeForDispatch) {
    ASSERT_TRUE(Exec(R"(
INSERT INTO STRING_IDS VALUES(300,'irq_handler_entry'),(301,'name'),(302,'arch_timer');
INSERT INTO GENERIC_EVENT_TYPES VALUES(903,15,300,'__data_loc char[] name',NULL);
INSERT INTO GENERIC_EVENT_TYPE_FIELDS VALUES(903,5,301,12,4,0,'name=',NULL);
INSERT INTO GENERIC_EVENTS VALUES(66,44,1787735841201825300,903,NULL,8,0);
INSERT INTO GENERIC_EVENT_DATA VALUES(66,5,0,0,0,0,302);
UPDATE GENERIC_EVENT_TYPE_FIELDS SET size=1 WHERE typeId=900 AND fieldIdx=19;
)"));
    CloseWriter();
    std::string error;
    const auto events = ReadAll(error);
    ASSERT_EQ(events.size(), 5U);
    EXPECT_EQ(std::get<std::string>(events[1].fields.at("prev_comm")), "worker_thread_1");
    EXPECT_EQ(std::get<std::string>(events.back().fields.at("name")), "arch_timer");
}

TEST_F(MsprofFtraceReaderTest, ReadsSignedNegativeAndFloatingValuesFromTheirDeclaredColumns) {
    ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET intVal=-7 WHERE genericEventId=1 AND fieldIdx=6"));
    std::string error;
    auto events = ReadAll(error);
    ASSERT_EQ(events.size(), 4U);
    EXPECT_EQ(std::get<int64_t>(events[1].fields.at("common_pid")), -7);
    for (const auto kind : {2U, 3U}) {
        ASSERT_TRUE(Exec(
            "UPDATE GENERIC_EVENT_TYPE_FIELDS SET type=" + std::to_string(kind) + " WHERE typeId=900 AND fieldIdx=6"));
        const std::string column = kind == 2 ? "floatVal" : "doubleVal";
        ASSERT_TRUE(Exec("UPDATE GENERIC_EVENT_DATA SET " + column + "=1.25 WHERE genericEventId=1 AND fieldIdx=6"));
        events = ReadAll(error);
        ASSERT_EQ(events.size(), 4U);
        EXPECT_DOUBLE_EQ(std::get<double>(events[1].fields.at("common_pid")), 1.25);
    }
}

TEST_F(MsprofFtraceReaderTest, HonoursConsumerCancellationAndCancellationBeforeReading) {
    unsigned consumed = 0;
    std::string error;
    EXPECT_FALSE(MsprofFtraceReader::Read(
        path_.string(),
        [&consumed](const auto &) {
            ++consumed;
            return false;
        },
        error));
    EXPECT_EQ(consumed, 1U);
    EXPECT_NE(error.find("cancelled"), std::string::npos);
    consumed = 0;
    EXPECT_FALSE(MsprofFtraceReader::Read(
        path_.string(),
        [&consumed](const auto &) {
            ++consumed;
            return true;
        },
        error, [] { return true; }));
    EXPECT_EQ(consumed, 0U);
    EXPECT_NE(error.find("cancelled"), std::string::npos);
}

TEST_F(MsprofFtraceReaderTest, InterruptsSqliteWorkBeforeTheFirstEventIsEmitted) {
    ASSERT_TRUE(Exec(R"(
WITH RECURSIVE sequence(x) AS (VALUES(1000) UNION ALL SELECT x+1 FROM sequence WHERE x<30000)
INSERT INTO GENERIC_EVENTS SELECT x,42,1787735841201925000+x,900,NULL,8,0 FROM sequence;
)"));
    unsigned callbacks = 0;
    unsigned polls = 0;
    std::string error;
    EXPECT_FALSE(MsprofFtraceReader::Read(
        path_.string(),
        [&callbacks](const auto &) {
            ++callbacks;
            return true;
        },
        error, [&polls] { return ++polls >= 2; }));
    EXPECT_EQ(callbacks, 0U);
    EXPECT_GE(polls, 2U);
    EXPECT_NE(error.find("cancelled"), std::string::npos);
}

}
}
