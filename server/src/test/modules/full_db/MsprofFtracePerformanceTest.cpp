/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#include <gtest/gtest.h>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif
#include "MsprofFtraceImportService.h"
#include "MsprofFtraceReader.h"
#include "DataBaseManager.h"
#include "DataEngine.h"
#include "RepositoryFactory.h"
#define private public
#include "RenderEngine.h"
#undef private
#include "TextTraceDatabase.h"
#include "document.h"
#include "prettywriter.h"
#include "stringbuffer.h"
#include "sqlite3.h"

namespace Dic::Module::FullDb {
namespace {
constexpr uint64_t BENCHMARK_EVENT_COUNT = 1000000;
constexpr uint64_t BENCHMARK_EPOCH_NS = 1793654244074239046;
constexpr uint64_t BENCHMARK_STEP_NS = 1000;
constexpr uint64_t CHECKPOINT_ID = BENCHMARK_EVENT_COUNT / 2;
constexpr uint64_t SCHEDULING_EVENT_COUNT = 200000;
constexpr uint64_t SCHEDULING_CPU_COUNT = 8;
using BenchClock = std::chrono::steady_clock;
using BenchDb = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using BenchStmt = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

uint64_t Milliseconds(BenchClock::time_point start) {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(BenchClock::now() - start).count());
}

uint64_t PeakRssBytes() {
#ifdef _WIN32
    using QueryMemory = BOOL(WINAPI *)(HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
    auto query =
        reinterpret_cast<QueryMemory>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "K32GetProcessMemoryInfo"));
    PROCESS_MEMORY_COUNTERS counters{};
    if (query != nullptr && query(GetCurrentProcess(), &counters, sizeof(counters))) {
        return static_cast<uint64_t>(counters.PeakWorkingSetSize);
    }
    return 0;
#else
    rusage usage{};
    return getrusage(RUSAGE_SELF, &usage) == 0 ? static_cast<uint64_t>(usage.ru_maxrss) * 1024 : 0;
#endif
}

void BenchExec(sqlite3 *db, const std::string &sql) {
    if (sqlite3_exec(db, sql.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
}

BenchStmt BenchPrepare(sqlite3 *db, const std::string &sql) {
    sqlite3_stmt *statement = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &statement, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    return BenchStmt(statement, sqlite3_finalize);
}

void StepAndReset(sqlite3 *db, sqlite3_stmt *statement) {
    if (sqlite3_step(statement) != SQLITE_DONE) {
        throw std::runtime_error(sqlite3_errmsg(db));
    }
    sqlite3_reset(statement);
    sqlite3_clear_bindings(statement);
}

void GenerateBenchmarkSource(const std::filesystem::path &path) {
    sqlite3 *raw = nullptr;
    if (sqlite3_open(path.u8string().c_str(), &raw) != SQLITE_OK) {
        if (raw != nullptr) {
            sqlite3_close(raw);
        }
        throw std::runtime_error("Cannot create private ftrace benchmark fixture");
    }
    BenchDb db(raw, sqlite3_close);
    BenchExec(raw, R"(
PRAGMA synchronous=OFF;
PRAGMA journal_mode=OFF;
CREATE TABLE META_DATA(name TEXT,value TEXT);
INSERT INTO META_DATA VALUES('SCHEMA_VERSION','1.2.0'),('ftrace','benchmark explicit events'),
 ('ftrace_events_complete','1');
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
INSERT INTO STRING_IDS VALUES(10,'sched'),(1,'sched_wakeup'),(2,'sched_wakeup_new'),(3,'common_pid'),(4,'pid');
INSERT INTO GENERIC_EVENT_SOURCES VALUES(9,10,3,1,NULL);
INSERT INTO GENERIC_EVENT_TYPES VALUES(55,9,1,NULL,NULL),(9999,9,2,NULL,NULL);
INSERT INTO GENERIC_EVENT_TYPE_FIELDS VALUES(55,0,3,4,4,0,NULL,NULL),(55,7,4,8,8,1,NULL,NULL),
 (9999,0,3,4,4,0,NULL,NULL),(9999,7,4,8,8,1,NULL,NULL);
BEGIN;
)");
    auto event = BenchPrepare(raw, "INSERT INTO GENERIC_EVENTS VALUES(?,?,?,?,NULL,?,0)");
    auto data = BenchPrepare(raw, "INSERT INTO GENERIC_EVENT_DATA VALUES(?,?,?, ?,0,0,0)");
    for (uint64_t id = 1; id <= BENCHMARK_EVENT_COUNT; ++id) {
        sqlite3_bind_int64(event.get(), 1, static_cast<sqlite3_int64>(id));
        sqlite3_bind_int64(event.get(), 2, static_cast<sqlite3_int64>(5000 + id * BENCHMARK_STEP_NS));
        sqlite3_bind_int64(event.get(), 3, static_cast<sqlite3_int64>(BENCHMARK_EPOCH_NS + id * BENCHMARK_STEP_NS));
        sqlite3_bind_int64(event.get(), 4, id == CHECKPOINT_ID ? 9999 : 55);
        sqlite3_bind_int(event.get(), 5, static_cast<int>(id % 8));
        StepAndReset(raw, event.get());
        sqlite3_bind_int64(data.get(), 1, static_cast<sqlite3_int64>(id));
        sqlite3_bind_int(data.get(), 2, 0);
        sqlite3_bind_int(data.get(), 3, 42);
        sqlite3_bind_int(data.get(), 4, 0);
        StepAndReset(raw, data.get());
        sqlite3_bind_int64(data.get(), 1, static_cast<sqlite3_int64>(id));
        sqlite3_bind_int(data.get(), 2, 7);
        sqlite3_bind_int(data.get(), 3, 0);
        sqlite3_bind_int64(data.get(), 4, static_cast<sqlite3_int64>(id));
        StepAndReset(raw, data.get());
    }
    BenchExec(raw, "COMMIT; CREATE INDEX GenericEventTsIndex ON GENERIC_EVENTS(timestamp)");
}

void GenerateSchedulingBenchmarkSource(const std::filesystem::path &path) {
    sqlite3 *raw = nullptr;
    if (sqlite3_open(path.u8string().c_str(), &raw) != SQLITE_OK) {
        if (raw != nullptr) {
            sqlite3_close(raw);
        }
        throw std::runtime_error("Cannot create private scheduling benchmark fixture");
    }
    BenchDb db(raw, sqlite3_close);
    BenchExec(raw, R"(
PRAGMA synchronous=OFF;
PRAGMA journal_mode=OFF;
CREATE TABLE META_DATA(name TEXT,value TEXT);
INSERT INTO META_DATA VALUES('SCHEMA_VERSION','1.2.0'),('ftrace','sched/sched_switch'),
 ('ftrace_events_complete','1');
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
INSERT INTO STRING_IDS VALUES(100,'sched'),(101,'sched_switch'),(102,'prev_comm'),(103,'prev_pid'),
 (104,'prev_prio'),(105,'prev_state'),(106,'next_comm'),(107,'next_pid'),(108,'next_prio');
INSERT INTO GENERIC_EVENT_SOURCES VALUES(31,100,3,1,NULL);
INSERT INTO GENERIC_EVENT_TYPES VALUES(5047,31,101,NULL,NULL);
INSERT INTO GENERIC_EVENT_TYPE_FIELDS VALUES(5047,0,102,8,16,0,NULL,NULL),
 (5047,1,103,24,4,0,NULL,NULL),(5047,2,104,28,4,0,NULL,NULL),
 (5047,3,105,32,8,0,NULL,NULL),(5047,4,106,40,16,0,NULL,NULL),
 (5047,5,107,56,4,0,NULL,NULL),(5047,6,108,60,4,0,NULL,NULL);
BEGIN;
)");
    auto names = BenchPrepare(raw, "INSERT INTO STRING_IDS VALUES(?,?)");
    for (uint64_t task = 0; task < SCHEDULING_CPU_COUNT * 2; ++task) {
        const auto name = "sched_worker_" + std::to_string(task);
        sqlite3_bind_int64(names.get(), 1, static_cast<sqlite3_int64>(task + 1000));
        sqlite3_bind_text(names.get(), 2, name.c_str(), -1, SQLITE_TRANSIENT);
        StepAndReset(raw, names.get());
    }
    auto event = BenchPrepare(raw, "INSERT INTO GENERIC_EVENTS VALUES(?,?,?,5047,NULL,?,0)");
    auto data = BenchPrepare(raw, "INSERT INTO GENERIC_EVENT_DATA VALUES(?,?,?,0,0,0,?)");
    for (uint64_t index = 0; index < SCHEDULING_EVENT_COUNT; ++index) {
        const uint64_t cpu = index % SCHEDULING_CPU_COUNT;
        const uint64_t previous = cpu * 2 + (index / SCHEDULING_CPU_COUNT) % 2;
        const uint64_t next = cpu * 2 + (1 + index / SCHEDULING_CPU_COUNT) % 2;
        sqlite3_bind_int64(event.get(), 1, static_cast<sqlite3_int64>(index + 1));
        sqlite3_bind_int64(event.get(), 2, static_cast<sqlite3_int64>(5000 + index * BENCHMARK_STEP_NS));
        sqlite3_bind_int64(event.get(), 3, static_cast<sqlite3_int64>(BENCHMARK_EPOCH_NS + index * BENCHMARK_STEP_NS));
        sqlite3_bind_int64(event.get(), 4, static_cast<sqlite3_int64>(cpu));
        StepAndReset(raw, event.get());
        for (uint64_t field = 0; field < 7; ++field) {
            sqlite3_bind_int64(data.get(), 1, static_cast<sqlite3_int64>(index + 1));
            sqlite3_bind_int64(data.get(), 2, static_cast<sqlite3_int64>(field));
            sqlite3_bind_int64(data.get(), 3, 0);
            sqlite3_bind_int64(data.get(), 4, 0);
            if (field == 0 || field == 4) {
                sqlite3_bind_int64(data.get(), 4, static_cast<sqlite3_int64>(1000 + (field == 0 ? previous : next)));
            } else {
                const uint64_t value = field == 1 ? 1000 + previous : field == 5 ? 1000 + next : field == 3 ? 0 : 120;
                sqlite3_bind_int64(data.get(), 3, static_cast<sqlite3_int64>(value));
            }
            StepAndReset(raw, data.get());
        }
    }
    BenchExec(raw, "COMMIT; CREATE INDEX GenericEventTsIndex ON GENERIC_EVENTS(timestamp)");
}

struct BenchmarkResourceCleanup {
    std::string resourceId;
    ~BenchmarkResourceCleanup() { MsprofFtraceImportService::Instance().Remove(resourceId); }
};

struct BenchmarkRenderAssembly {
    std::shared_ptr<Timeline::DataEngineInterface> previous;
    BenchmarkRenderAssembly() {
        previous = Timeline::RenderEngine::Instance()->dataEngine;
        auto engine = std::make_shared<Timeline::DataEngine>();
        engine->SetRepositoryFactory(std::make_shared<Timeline::RepositoryFactory>());
        Timeline::RenderEngine::Instance()->SetDataEngineInterface(engine);
    }
    ~BenchmarkRenderAssembly() { Timeline::RenderEngine::Instance()->SetDataEngineInterface(previous); }
};
}

// Explicitly opt in with --gtest_also_run_disabled_tests and this test's exact filter.
TEST(MsprofFtracePerformanceTest, DISABLED_OneMillionEventsReaderStreaming) {
    namespace fs = std::filesystem;
    auto base = fs::temp_directory_path();
#ifdef _WIN32
    if (fs::exists("D:/001_TEST_DATA")) {
        base = "D:/001_TEST_DATA";
    }
#endif
    const auto directory =
        base / ("msprof_ftrace_reader_benchmark_" + std::to_string(BenchClock::now().time_since_epoch().count()));
    ASSERT_TRUE(fs::create_directory(directory));
    const auto source = directory / "million_events.db";
    const auto report = directory / "benchmark.json";
    rapidjson::Document metrics(rapidjson::kObjectType);
    auto &allocator = metrics.GetAllocator();
    auto number = [&](const char *name, uint64_t value) {
        metrics.AddMember(rapidjson::Value(name, allocator), rapidjson::Value().SetUint64(value), allocator);
    };
    const auto generationStart = BenchClock::now();
    ASSERT_NO_THROW(GenerateBenchmarkSource(source));
    number("fixture_generation_ms", Milliseconds(generationStart));
    number("event_count", BENCHMARK_EVENT_COUNT);
    number("data_field_count", BENCHMARK_EVENT_COUNT * 2);
    number("source_bytes", fs::file_size(source));
    number("peak_rss_before_read_bytes", PeakRssBytes());
    metrics.AddMember("source_path", rapidjson::Value(source.u8string().c_str(), allocator), allocator);
    metrics.AddMember("scope", "reader_streaming_without_display_materialization", allocator);

    // Stream supported wakeups without materializing any display cache.
    const auto inspection = MsprofFtraceReader::Inspect(source.u8string());
    ASSERT_TRUE(inspection.IsReady()) << inspection.error;
    uint64_t count = 0;
    uint64_t payloadSum = 0;
    uint64_t checkpointTimestamp = 0;
    uint64_t firstTimestamp = 0;
    uint64_t lastTimestamp = 0;
    bool valuesValid = true;
    std::string error;
    const auto readStart = BenchClock::now();
    ASSERT_TRUE(MsprofFtraceReader::Read(
        source.u8string(),
        [&](const MsprofFtraceEvent &event) {
            const auto payload = event.fields.find("pid");
            if (payload == event.fields.end() || !std::holds_alternative<uint64_t>(payload->second) ||
                event.eventId != count + 1 || std::get<uint64_t>(payload->second) != event.eventId) {
                valuesValid = false;
                return false;
            }
            if (count == 0) {
                firstTimestamp = event.timestampNs;
            }
            lastTimestamp = event.timestampNs;
            ++count;
            payloadSum += std::get<uint64_t>(payload->second);
            if (event.eventId == CHECKPOINT_ID) {
                checkpointTimestamp = event.timestampNs;
            }
            return true;
        },
        error))
        << error;
    EXPECT_TRUE(valuesValid);
    EXPECT_EQ(count, BENCHMARK_EVENT_COUNT);
    EXPECT_EQ(firstTimestamp, BENCHMARK_EPOCH_NS + BENCHMARK_STEP_NS);
    EXPECT_EQ(lastTimestamp, BENCHMARK_EPOCH_NS + BENCHMARK_EVENT_COUNT * BENCHMARK_STEP_NS);
    EXPECT_EQ(payloadSum, BENCHMARK_EVENT_COUNT * (BENCHMARK_EVENT_COUNT + 1) / 2);
    EXPECT_EQ(checkpointTimestamp, BENCHMARK_EPOCH_NS + CHECKPOINT_ID * BENCHMARK_STEP_NS);
    number("reader_ms", Milliseconds(readStart));
    number("peak_rss_after_read_bytes", PeakRssBytes());
    number("decoded_payload_sum", payloadSum);
    metrics.AddMember("source_retained", true, allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    metrics.Accept(writer);
    std::ofstream output(report, std::ios::binary);
    output << buffer.GetString() << '\n';
    ASSERT_TRUE(output.good());
    std::cout << "Msprof ftrace reader benchmark report: " << report.u8string() << std::endl;
    std::cout << buffer.GetString() << std::endl;
    RecordProperty("benchmark_report", report.u8string());
}

TEST(MsprofFtracePerformanceTest, DISABLED_200ThousandSchedulingEvents) {
    namespace fs = std::filesystem;
    auto base = fs::temp_directory_path();
#ifdef _WIN32
    if (fs::exists("D:/001_TEST_DATA")) {
        base = "D:/001_TEST_DATA";
    }
#endif
    const auto directory =
        base / ("msprof_ftrace_sched_benchmark_" + std::to_string(BenchClock::now().time_since_epoch().count()));
    ASSERT_TRUE(fs::create_directory(directory));
    const auto source = directory / "scheduling_200k.db";
    const auto report = directory / "benchmark.json";
    rapidjson::Document metrics(rapidjson::kObjectType);
    auto &allocator = metrics.GetAllocator();
    auto number = [&](const char *name, uint64_t value) {
        metrics.AddMember(rapidjson::Value(name, allocator), rapidjson::Value().SetUint64(value), allocator);
    };
    auto text = [&](const char *name, const std::string &value) {
        metrics.AddMember(rapidjson::Value(name, allocator), rapidjson::Value(value.c_str(), allocator), allocator);
    };
    const auto generationStart = BenchClock::now();
    ASSERT_NO_THROW(GenerateSchedulingBenchmarkSource(source));
    number("fixture_generation_ms", Milliseconds(generationStart));
    number("event_count", SCHEDULING_EVENT_COUNT);
    number("data_field_count", SCHEDULING_EVENT_COUNT * 7);
    number("source_bytes", fs::file_size(source));
    number("peak_rss_before_import_bytes", PeakRssBytes());
    text("source_path", source.u8string());
    auto &service = MsprofFtraceImportService::Instance();
    BenchmarkRenderAssembly assembly;
    const auto resourceId = MsprofFtraceImportService::ResourceId(source.u8string());
    BenchmarkResourceCleanup cleanup{resourceId};
    std::string error;
    const auto importStart = BenchClock::now();
    ASSERT_TRUE(service.Import(source.u8string(), error, false)) << error;
    number("cold_import_ms", Milliseconds(importStart));
    number("peak_rss_after_import_bytes", PeakRssBytes());
    const auto resource = service.GetResource(resourceId);
    ASSERT_TRUE(resource.has_value());
    ASSERT_TRUE(resource->ready);
    EXPECT_TRUE(resource->inspection.IsReady());
    text("cache_path", resource->cachePath);
    number("cache_bytes", fs::file_size(resource->cachePath));
    const uint64_t expectedRunning =
        (SCHEDULING_EVENT_COUNT - SCHEDULING_CPU_COUNT) * SCHEDULING_CPU_COUNT * BENCHMARK_STEP_NS;
    number("expected_running_ns", expectedRunning);
    const auto queryStart = BenchClock::now();
    {
        auto database = std::dynamic_pointer_cast<Timeline::TextTraceDatabase>(
            Timeline::DataBaseManager::Instance().GetTraceDatabaseByFileId(resourceId));
        ASSERT_NE(database, nullptr);
        auto summaryStatement = database->CreatPreparedStatement(
            "SELECT SUM(running_ns) AS running,SUM(runnable_ns) AS runnable,SUM(sleeping_ns) AS sleeping,"
            "SUM(cs_count) AS switches,SUM(cs_involuntary_count) AS preempted,COUNT(*) AS tasks,"
            "COUNT(DISTINCT cpu_id) AS cpus FROM trace_task_summary");
        ASSERT_NE(summaryStatement, nullptr);
        auto summary = summaryStatement->ExecuteQuery();
        ASSERT_NE(summary, nullptr);
        ASSERT_TRUE(summary->Next());
        EXPECT_EQ(summary->GetUint64("running"), expectedRunning);
        EXPECT_EQ(summary->GetUint64("runnable"), expectedRunning);
        EXPECT_EQ(summary->GetUint64("sleeping"), 0u);
        EXPECT_EQ(summary->GetUint64("switches"), SCHEDULING_EVENT_COUNT);
        EXPECT_EQ(summary->GetUint64("preempted"), 0u);
        EXPECT_EQ(summary->GetUint64("tasks"), SCHEDULING_CPU_COUNT * 2);
        EXPECT_EQ(summary->GetUint64("cpus"), SCHEDULING_CPU_COUNT);
        number("statistics_running_ns", summary->GetUint64("running"));
        number("statistics_context_switches", summary->GetUint64("switches"));
        auto dimensionsStatement = database->CreatPreparedStatement(
            "SELECT t.pid,COUNT(*) AS count,SUM(s.duration) AS duration FROM slice s "
            "JOIN thread t USING(track_id) WHERE s.name='Running' OR t.pid='CPU Scheduling' "
            "GROUP BY t.pid,s.name");
        ASSERT_NE(dimensionsStatement, nullptr);
        auto dimensions = dimensionsStatement->ExecuteQuery();
        ASSERT_NE(dimensions, nullptr);
        bool taskRunning = false;
        while (dimensions->Next()) {
            if (dimensions->GetString("pid") == "Process Scheduling") {
                taskRunning = true;
                EXPECT_EQ(dimensions->GetUint64("duration"), expectedRunning);
                EXPECT_EQ(dimensions->GetUint64("count"), SCHEDULING_EVENT_COUNT - SCHEDULING_CPU_COUNT);
            }
        }
        EXPECT_TRUE(taskRunning);
        auto cpuStatement = database->CreatPreparedStatement(
            "SELECT COUNT(*) AS count,SUM(duration) AS duration FROM slice WHERE duration>0 "
            "AND track_id IN (SELECT track_id FROM thread WHERE pid='CPU Scheduling')");
        ASSERT_NE(cpuStatement, nullptr);
        auto cpu = cpuStatement->ExecuteQuery();
        ASSERT_NE(cpu, nullptr);
        ASSERT_TRUE(cpu->Next());
        EXPECT_EQ(cpu->GetUint64("duration"), expectedRunning);
        EXPECT_EQ(cpu->GetUint64("count"), SCHEDULING_EVENT_COUNT - SCHEDULING_CPU_COUNT);
        auto totalStatement = database->CreatPreparedStatement("SELECT COUNT(*) AS count FROM slice");
        ASSERT_NE(totalStatement, nullptr);
        auto total = totalStatement->ExecuteQuery();
        ASSERT_NE(total, nullptr);
        ASSERT_TRUE(total->Next());
        EXPECT_EQ(total->GetUint64("count"), SCHEDULING_EVENT_COUNT * 4 - SCHEDULING_CPU_COUNT * 3);
        number("derived_slice_count", total->GetUint64("count"));
        EXPECT_EQ(service.GetCapabilities(resourceId), (std::vector<std::string>{"scheduling"}));
    }
    number("statistics_validation_ms", Milliseconds(queryStart));
    number("peak_rss_final_bytes", PeakRssBytes());
    ASSERT_TRUE(service.Remove(resourceId));
    EXPECT_FALSE(fs::exists(resource->cachePath));
    metrics.AddMember("cache_retained", false, allocator);
    metrics.AddMember("source_retained", true, allocator);
    rapidjson::StringBuffer buffer;
    rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
    metrics.Accept(writer);
    std::ofstream output(report, std::ios::binary);
    output << buffer.GetString() << '\n';
    ASSERT_TRUE(output.good());
    std::cout << "Msprof ftrace scheduling benchmark report: " << report.u8string() << std::endl;
    std::cout << buffer.GetString() << std::endl;
    RecordProperty("benchmark_report", report.u8string());
}
}
