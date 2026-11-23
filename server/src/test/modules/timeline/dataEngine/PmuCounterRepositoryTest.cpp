/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy at http://license.coscl.org.cn/MulanPSL2
 */
#include <gtest/gtest.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <set>
#include "PmuCounterCalculator.h"
#include "PmuCounterRepository.h"

using namespace Dic::Module;
using namespace Dic::Module::Timeline;

class PmuCounterRepositoryTest : public ::testing::Test {
  protected:
    std::recursive_mutex mutex;
    Database database{mutex};
    PmuCounterRepository repository{database};

    void SetUp() override {
        const std::string name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        ASSERT_TRUE(database.OpenSharedMemoryDb("file:msinsight_overlap_pmu_" + name + "?mode=memory&cache=shared"));
    }

    void CreateSchema(bool rawValues = true, bool unit = true) {
        ASSERT_TRUE(
            database.ExecSql("CREATE TABLE HOST_PMU_EVENTS(eventId INTEGER PRIMARY KEY,name TEXT,source INTEGER" +
                std::string(unit ? ",unit TEXT" : "") + ")"));
        ASSERT_TRUE(database.ExecSql("CREATE TABLE HOST_PMU_COUNTER_INFO(counterId INTEGER PRIMARY KEY,eventId INTEGER,"
                                     "pid INTEGER,cpu INTEGER,intervalNs NUMERIC)"));
        ASSERT_TRUE(database.ExecSql("CREATE TABLE HOST_PMU_COUNTER_DATA(counterId INTEGER,timestamp NUMERIC,"
                                     "countValue REAL,valid INTEGER" +
            std::string(rawValues ? ",rawValue INTEGER,timeEnabled INTEGER,timeRunning INTEGER" : "") + ")"));
        ASSERT_TRUE(
            database.ExecSql("CREATE INDEX PmuSampleCounterTsIndex ON HOST_PMU_COUNTER_DATA(counterId,timestamp)"));
        ASSERT_TRUE(database.ExecSql(
            std::string("INSERT INTO HOST_PMU_EVENTS VALUES(1,'cycles',2") + (unit ? ",'count'" : "") + ")"));
        ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_INFO VALUES(1,1,-1,0,200000000)"));
    }

    std::vector<PmuCounterSample> Read(uint64_t start = 0, uint64_t end = 1000) {
        std::vector<PmuCounterSample> samples;
        EXPECT_TRUE(repository.ReadWindowWithNeighbors(
            1, start, end, repository.DetectCapabilities(), [&](const auto &sample) { samples.push_back(sample); }));
        return samples;
    }
};

TEST_F(PmuCounterRepositoryTest, DistinguishesNotCollectedAndIncompleteSchema) {
    auto capabilities = repository.DetectCapabilities();
    EXPECT_FALSE(capabilities.available);
    EXPECT_EQ(capabilities.reason, "pmu_not_collected");
    ASSERT_TRUE(database.ExecSql("CREATE TABLE HOST_PMU_EVENTS(eventId INTEGER)"));
    capabilities = repository.DetectCapabilities();
    EXPECT_FALSE(capabilities.available);
    EXPECT_EQ(capabilities.reason, "incomplete_pmu_schema");
}

TEST_F(PmuCounterRepositoryTest, EnumeratesActualCpuAndEmptyCountersWithoutInventingRows) {
    CreateSchema();
    ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_INFO VALUES(2,1,-1,17,200000000)"));
    ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_INFO VALUES(3,999,-1,22,200000000)"));
    ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_INFO VALUES(4,1,42,23,200000000)"));
    ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_INFO VALUES(5,1,-1,-1,200000000)"));
    ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_DATA VALUES(1,100,10,1,10,100,100)"));
    const auto capabilities = repository.DetectCapabilities();
    EXPECT_TRUE(capabilities.hasRawValues);
    std::vector<PmuCounterInfo> counters;
    ASSERT_TRUE(repository.ListCounters(counters, capabilities));
    ASSERT_EQ(counters.size(), 2u);
    EXPECT_EQ(counters[0].cpu, 0);
    EXPECT_TRUE(counters[0].hasSamples);
    EXPECT_EQ(counters[1].cpu, 17);
    EXPECT_FALSE(counters[1].hasSamples);
    EXPECT_EQ(counters[1].unit, "count");
}

TEST_F(PmuCounterRepositoryTest, DistinguishesAbsentCounterFromCounterWithNoSamples) {
    CreateSchema();
    std::optional<PmuCounterInfo> counter;
    const auto capabilities = repository.DetectCapabilities();
    ASSERT_TRUE(repository.GetCounter(1, counter, capabilities));
    ASSERT_TRUE(counter);
    EXPECT_FALSE(counter->hasSamples);
    ASSERT_TRUE(repository.GetCounter(99, counter, capabilities));
    EXPECT_FALSE(counter);
}

TEST_F(PmuCounterRepositoryTest, WindowIncludesWholeNeighborTimestampGroups) {
    CreateSchema();
    ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_DATA VALUES"
                                 "(1,50,1,1,1,1,1),(1,100,10,1,10,10,10),(1,100,11,1,11,10,10),"
                                 "(1,300,30,1,30,30,30),(1,300,31,1,31,30,30),(1,400,40,1,40,40,40)"));
    auto samples = Read(150, 250);
    ASSERT_EQ(samples.size(), 4u);
    EXPECT_EQ(samples.front().timestamp, 100);
    EXPECT_EQ(samples.back().timestamp, 300);
    EXPECT_EQ(samples[0].rawValue, 10);
    EXPECT_EQ(samples[1].rawValue, 11);
}

TEST_F(PmuCounterRepositoryTest, PreservesInvalidPointsAndNullsRatherThanConvertingToZero) {
    CreateSchema();
    ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_DATA VALUES"
                                 "(1,100,10,1,10,10,10),(1,200,NULL,0,NULL,NULL,NULL),"
                                 "(1,300,30,1,NULL,30,30),(1,400,40,1,1.25,40,40)"));
    const auto samples = Read();
    ASSERT_EQ(samples.size(), 4u);
    EXPECT_EQ(samples[1].valid, 0);
    EXPECT_FALSE(samples[1].rawValue);
    EXPECT_EQ(samples[2].valid, 1);
    EXPECT_FALSE(samples[2].rawValue);
    EXPECT_FALSE(samples[3].rawValue);
}

TEST_F(PmuCounterRepositoryTest, KeepsIntegerPrecisionAndOperatesInQueryOnlyMode) {
    CreateSchema();
    ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_DATA VALUES"
                                 "(1,1795056377766382850,9007199254740993,1,9007199254740993,100,100)"));
    ASSERT_TRUE(database.ExecSql("PRAGMA query_only=ON"));
    auto samples = Read(1795056377766382850ULL, 1795056377766382850ULL);
    ASSERT_EQ(samples.size(), 1u);
    EXPECT_EQ(samples[0].timestamp, 1795056377766382850LL);
    EXPECT_EQ(samples[0].rawValue, 9007199254740993LL);
}

TEST_F(PmuCounterRepositoryTest, LegacyWithoutOriginalCountersIsUnsupported) {
    CreateSchema(false, false);
    ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_DATA VALUES(1,100,10,1)"));
    const auto capabilities = repository.DetectCapabilities();
    EXPECT_TRUE(capabilities.available);
    EXPECT_FALSE(capabilities.hasRawValues);
    EXPECT_FALSE(capabilities.hasUnit);
    std::vector<PmuCounterInfo> counters;
    ASSERT_TRUE(repository.ListCounters(counters, capabilities));
    ASSERT_EQ(counters.size(), 1u);
    EXPECT_EQ(counters[0].unit, "unknown");
    const auto samples = Read();
    EXPECT_TRUE(samples.empty());
    const auto calculated = PmuCounterCalculator::Calculate(samples, counters.front(), capabilities, {});
    EXPECT_EQ(calculated.status, "unsupported");
}

TEST_F(PmuCounterRepositoryTest, ReadsOnlyOriginalCountersWithoutDependingOnScaledTotals) {
    CreateSchema();
    ASSERT_TRUE(database.ExecSql("ALTER TABLE HOST_PMU_COUNTER_DATA DROP COLUMN countValue"));
    ASSERT_TRUE(database.ExecSql("INSERT INTO HOST_PMU_COUNTER_DATA VALUES"
                                 "(1,100,1,10,10,10),(1,200,1,17,20,20)"));
    const auto capabilities = repository.DetectCapabilities();
    ASSERT_TRUE(capabilities.available);
    ASSERT_TRUE(capabilities.hasRawValues);
    const auto samples = Read();
    ASSERT_EQ(samples.size(), 2u);
    std::optional<PmuCounterInfo> counter;
    ASSERT_TRUE(repository.GetCounter(1, counter, capabilities));
    ASSERT_TRUE(counter);
    const auto calculated = PmuCounterCalculator::Calculate(samples, *counter, capabilities, {0, 1000, 0, 1024});
    ASSERT_EQ(calculated.segments.size(), 1u);
    EXPECT_DOUBLE_EQ(*calculated.segments.front().value, 7);
}

TEST_F(PmuCounterRepositoryTest, RejectsUnrepresentableOrReversedWindow) {
    CreateSchema();
    const auto capabilities = repository.DetectCapabilities();
    const auto consume = [](const auto &) {};
    EXPECT_FALSE(repository.ReadWindowWithNeighbors(1, 10, 9, capabilities, consume));
    EXPECT_FALSE(repository.ReadWindowWithNeighbors(1, 0, UINT64_MAX, capabilities, consume));
    EXPECT_FALSE(repository.ReadWindowWithNeighbors(-1, 0, 10, capabilities, consume));
}

namespace {
// The optional real-device fixture must never pass through an importer or writable connection.
class ReadOnlyPmuDatabase : public Database {
  public:
    explicit ReadOnlyPmuDatabase(std::recursive_mutex &mutex) : Database(mutex) {}

    bool OpenReadOnly(const std::filesystem::path &file) {
        const int code =
            sqlite3_open_v2(file.u8string().c_str(), &db, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
        if (code != SQLITE_OK) {
            sqlite3_close(db);
            db = nullptr;
            return false;
        }
        isOpen = true;
        path = file.u8string();
        return sqlite3_db_readonly(db, "main") == 1;
    }
};

int64_t ReadScalar(Database &database, const std::string &sql) {
    auto statement = database.CreatPreparedStatement(sql);
    EXPECT_NE(statement, nullptr);
    if (!statement) {
        return -1;
    }
    auto result = statement->ExecuteQuery();
    const bool found = result && result->Next();
    EXPECT_TRUE(found);
    return found ? sqlite3_column_int64(statement->stmt, 0) : -1;
}

void VerifyRealCounter(const std::vector<PmuCounterSample> &samples, const PmuCounterInfo &counter,
    const PmuCounterCapabilities &capabilities, uint64_t origin, uint64_t duration) {
    SCOPED_TRACE("counter=" + std::to_string(counter.counterId) + " event=" + counter.name);
    long double expectedDelta = 0;
    size_t validIntervals = 0;
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto &right = samples[i];
        if (right.valid == 1) {
            ASSERT_TRUE(right.rawValue);
            ASSERT_TRUE(right.timeEnabled);
            ASSERT_TRUE(right.timeRunning);
        }
        if (i == 0) {
            continue;
        }
        const auto &left = samples[i - 1];
        if (left.valid != 1 || right.valid != 1 || !left.rawValue || !right.rawValue || !left.timeEnabled ||
            !right.timeEnabled || !left.timeRunning || !right.timeRunning || right.timestamp <= left.timestamp ||
            *right.rawValue < *left.rawValue || *right.timeEnabled <= *left.timeEnabled ||
            *right.timeRunning <= *left.timeRunning) {
            continue;
        }
        const int64_t rawDelta = *right.rawValue - *left.rawValue;
        const int64_t enabledDelta = *right.timeEnabled - *left.timeEnabled;
        const int64_t runningDelta = *right.timeRunning - *left.timeRunning;
        if (runningDelta > enabledDelta) {
            continue;
        }
        expectedDelta += static_cast<long double>(rawDelta) * enabledDelta / runningDelta;
        ++validIntervals;
    }
    const auto calculated =
        PmuCounterCalculator::Calculate(samples, counter, capabilities, {0, duration, origin, 4096});
    if (validIntervals == 0) {
        return;
    }
    EXPECT_EQ(calculated.status, "ok");
    long double actualDelta = 0;
    for (const auto &segment : calculated.segments) {
        if (segment.value) {
            EXPECT_TRUE(std::isfinite(*segment.value));
            EXPECT_GT(segment.endTime, segment.startTime);
            actualDelta += *segment.value;
        }
    }
    EXPECT_NEAR(static_cast<double>(actualDelta), static_cast<double>(expectedDelta),
        std::abs(static_cast<double>(expectedDelta)) * 1e-12 + 1e-6);
}
}

TEST(PmuCounterRealDataTest, ReadsAllCountersThroughProductionRepositoryAndCalculator) {
    const char *fixtureRoot = std::getenv("MSINSIGHT_PMU_FIXTURE_ROOT");
    if (fixtureRoot == nullptr || std::string(fixtureRoot).empty()) {
        GTEST_SKIP() << "Set MSINSIGHT_PMU_FIXTURE_ROOT to the real-device sc_pkg directory.";
    }
    ASSERT_TRUE(std::filesystem::is_directory(fixtureRoot));
    std::vector<std::filesystem::path> files;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(fixtureRoot)) {
        if (entry.is_regular_file() && entry.path().extension() == ".db" &&
            entry.path().filename().string().rfind("msprof_", 0) == 0) {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    ASSERT_FALSE(files.empty());
    for (const auto &file : files) {
        SCOPED_TRACE(file.u8string());
        std::recursive_mutex mutex;
        ReadOnlyPmuDatabase database(mutex);
        ASSERT_TRUE(database.OpenReadOnly(file));
        const auto schemaBefore = ReadScalar(database, "PRAGMA schema_version");
        PmuCounterRepository repository(database);
        const auto capabilities = repository.DetectCapabilities();
        ASSERT_TRUE(capabilities.available);
        ASSERT_TRUE(capabilities.hasRawValues);
        std::vector<PmuCounterInfo> counters;
        ASSERT_TRUE(repository.ListCounters(counters, capabilities));
        EXPECT_EQ(
            counters.size(), static_cast<size_t>(ReadScalar(database, "SELECT COUNT(*) FROM HOST_PMU_COUNTER_INFO")));
        const auto origin =
            static_cast<uint64_t>(ReadScalar(database, "SELECT MIN(timestamp) FROM HOST_PMU_COUNTER_DATA"));
        const auto end =
            static_cast<uint64_t>(ReadScalar(database, "SELECT MAX(timestamp) FROM HOST_PMU_COUNTER_DATA"));
        ASSERT_GT(end, origin);
        std::set<int64_t> cpus;
        size_t sampleCount = 0;
        for (const auto &counter : counters) {
            cpus.insert(counter.cpu);
            std::vector<PmuCounterSample> samples;
            ASSERT_TRUE(repository.ReadWindowWithNeighbors(
                counter.counterId, origin, end, capabilities, [&](const auto &sample) { samples.push_back(sample); }));
            sampleCount += samples.size();
            EXPECT_EQ(samples.size(),
                static_cast<size_t>(ReadScalar(database,
                    "SELECT COUNT(*) FROM HOST_PMU_COUNTER_DATA WHERE counterId=" +
                        std::to_string(counter.counterId))));
            VerifyRealCounter(samples, counter, capabilities, origin, end - origin);
        }
        EXPECT_EQ(cpus.size(),
            static_cast<size_t>(ReadScalar(database, "SELECT COUNT(DISTINCT cpu) FROM HOST_PMU_COUNTER_INFO")));
        EXPECT_EQ(sampleCount, static_cast<size_t>(ReadScalar(database, "SELECT COUNT(*) FROM HOST_PMU_COUNTER_DATA")));
        EXPECT_EQ(ReadScalar(database, "PRAGMA schema_version"), schemaBefore);
        std::cout << "PMU real-data " << file.filename().u8string() << ": counters=" << counters.size()
                  << " CPUs=" << cpus.size() << " samples=" << sampleCount << std::endl;
    }
}
