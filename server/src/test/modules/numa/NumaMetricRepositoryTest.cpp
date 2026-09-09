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

#include "Database.h"
#include "NumaMetricRepository.h"
#include "../../DatabaseTestCaseMockUtil.h"

using namespace Dic::Global::PROFILER::MockUtil;

namespace Dic::Module::Numa {
namespace {
class MockNumaDatabase : public Database {
  public:
    explicit MockNumaDatabase(std::recursive_mutex &mutex) : Database(mutex) {}
    void Use(sqlite3 *database) {
        isOpen = true;
        db = database;
        path = ":memory:";
    }
};

class NumaMetricRepositoryTest : public ::testing::Test {
  protected:
    void SetUp() override {
        DatabaseTestCaseMockUtil::OpenDB(sqlite);
        Sql("CREATE TABLE NUMA_TITLES_NAMES (name TEXT, description TEXT, summary_flag INTEGER, "
            "measurement_unit TEXT, unique_id INTEGER);");
        Sql("CREATE TABLE NUMA_LEVELS_HIERARCHY_NAMES (title0_id INTEGER, title1_id INTEGER, title2_id INTEGER);");
        Sql("CREATE TABLE NUMA_METRICS (ts INTEGER, value REAL, levels_id INTEGER);");
        database = std::make_shared<MockNumaDatabase>(mutex);
        database->Use(sqlite);
    }

    void Sql(const std::string &statement) { DatabaseTestCaseMockUtil::InsertData(sqlite, statement); }
    void Title(int id, const std::string &name, const std::string &description = "", const std::string &unit = "") {
        Sql("INSERT INTO NUMA_TITLES_NAMES VALUES ('" + name + "', '" + description + "', 0, '" + unit + "', " +
            std::to_string(id) + ");");
    }
    void Level(int id, int title0, int title1, int title2) {
        static_cast<void>(id);
        Sql("INSERT INTO NUMA_LEVELS_HIERARCHY_NAMES VALUES (" + std::to_string(title0) + ", " +
            std::to_string(title1) + ", " + std::to_string(title2) + ");");
    }
    void Metric(int id, uint64_t timestamp, double value, int level) {
        static_cast<void>(id);
        Sql("INSERT INTO NUMA_METRICS VALUES (" + std::to_string(timestamp) + ", " + std::to_string(value) + ", " +
            std::to_string(level) + ");");
    }
    std::vector<NumaMetricRecord> Query(uint64_t start = 0, uint64_t end = 0) {
        std::vector<NumaMetricRecord> records;
        EXPECT_TRUE(NumaMetricRepository(database).QueryMetricRecords(start, end, records));
        return records;
    }

    std::recursive_mutex mutex;
    sqlite3 *sqlite = nullptr;
    std::shared_ptr<MockNumaDatabase> database;
};

TEST_F(NumaMetricRepositoryTest, ExcludesImpactMetricsWithoutHidingOtherRatios) {
    Title(1, "Socket 0");
    Title(2, "External Traffic Impact", "Ratio description", "%");
    Title(3, "NUMA Node 0");
    Title(4, "DRAM Read Bandwidth", "Traffic description", "GB/s");
    Title(5, "Cache Hit Ratio", "Visible ratio description", "%");
    Title(6, "Total External Traffic Impact", "Hidden total ratio", "%");
    Level(1, 1, 2, 0);
    Level(2, 1, 3, 4);
    Level(3, 1, 5, 0);
    Level(4, 6, 0, 0);
    Metric(1, 100, 40, 1);
    Metric(2, 200, 60, 1);
    Metric(3, 100, 2.5, 2);
    Metric(4, 200, 3.5, 2);
    Metric(5, 100, 20, 3);
    Metric(6, 200, 40, 3);
    Metric(7, 100, 30, 4);
    Metric(8, 200, 50, 4);

    const auto records = Query();
    ASSERT_EQ(records.size(), 2U);
    EXPECT_EQ(records[0].metricName, "DRAM Read Bandwidth");
    EXPECT_DOUBLE_EQ(records[0].value, 6.0);
    EXPECT_EQ(records[1].metricName, "Cache Hit Ratio");
    EXPECT_DOUBLE_EQ(records[1].value, 30.0);
}

TEST_F(NumaMetricRepositoryTest, FiltersSamplesByClosedTimeRange) {
    Title(1, "Socket 0");
    Title(2, "NUMA Node 0");
    Title(3, "DRAM Read Bandwidth", "", "GB/s");
    Level(1, 1, 2, 3);
    Metric(1, 1000, 2.5, 1);
    Metric(2, 1100, 3.5, 1);
    Metric(3, 1200, 4.5, 1);

    const auto records = Query(100, 200);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_DOUBLE_EQ(records[0].value, 8.0);
    EXPECT_EQ(records[0].sampleCount, 2U);
    EXPECT_EQ(records[0].minTimestamp, 100U);
    EXPECT_EQ(records[0].maxTimestamp, 200U);
}

TEST_F(NumaMetricRepositoryTest, UsesCrossSocketParentMetadata) {
    Title(1, "Socket 1");
    Title(2, "Cross Socket Bandwidth", "Cross socket traffic.", "GB/s");
    Title(3, "Outgoing to Socket 0");
    Level(1, 1, 2, 3);
    Metric(1, 100, 0.25, 1);
    Metric(2, 200, 0.75, 1);

    const auto records = Query();
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].socketName, "Socket 1");
    EXPECT_EQ(records[0].numaName, "Cross Socket Bandwidth");
    EXPECT_EQ(records[0].metricName, "Outgoing to Socket 0");
    EXPECT_EQ(records[0].description, "Cross socket traffic.");
    EXPECT_EQ(records[0].measurementUnit, "GB/s");
    EXPECT_DOUBLE_EQ(records[0].value, 1.0);
}
} // namespace
} // namespace Dic::Module::Numa
