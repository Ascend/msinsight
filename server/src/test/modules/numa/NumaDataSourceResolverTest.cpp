/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#include <filesystem>
#include <string>
#include <system_error>

#include <gtest/gtest.h>
#include <sqlite3.h>

#include "DataBaseManager.h"
#include "NumaDataSourceResolver.h"
#include "NumaProtocol.h"
#include "QueryNumaOverviewHandler.h"

namespace Dic::Module::Numa {
namespace {
namespace fs = std::filesystem;

class NumaDataSourceResolverTest : public ::testing::Test {
  protected:
    void SetUp() override {
        root_ = fs::temp_directory_path() / "numa-data-source-resolver";
        fs::remove_all(root_);
        fs::create_directories(root_);
        Timeline::DataBaseManager::Instance().Clear();
    }

    void TearDown() override {
        Timeline::DataBaseManager::Instance().Clear();
        std::error_code error;
        fs::remove_all(root_, error);
    }

    static void CreateMergedDatabase(const fs::path &path, bool complete = true, bool withMetric = false) {
        fs::create_directories(path.parent_path());
        sqlite3 *database = nullptr;
        ASSERT_EQ(sqlite3_open(path.string().c_str(), &database), SQLITE_OK);
        std::string sql =
            "CREATE TABLE NUMA_TITLES_NAMES (name TEXT, description TEXT, summary_flag INTEGER, "
            "measurement_unit TEXT, unique_id INTEGER);"
            "CREATE TABLE NUMA_LEVELS_HIERARCHY_NAMES (title0_id INTEGER, title1_id INTEGER, title2_id INTEGER);";
        if (complete) {
            sql += "CREATE TABLE NUMA_METRICS (ts INTEGER, value REAL, levels_id INTEGER);"
                   "CREATE TABLE NUMA_SCALING_VALUES (id INTEGER PRIMARY KEY, level_id INTEGER, max_value REAL);";
        }
        if (withMetric) {
            sql += "INSERT INTO NUMA_TITLES_NAMES VALUES ('Socket 0', '', 0, 'GB/s', 1);"
                   "INSERT INTO NUMA_LEVELS_HIERARCHY_NAMES VALUES (1, 0, 0);"
                   "INSERT INTO NUMA_METRICS VALUES (100, 1.5, 1);"
                   "INSERT INTO NUMA_SCALING_VALUES VALUES (1, 1, 10.0);";
        }
        ASSERT_EQ(sqlite3_exec(database, sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
        ASSERT_EQ(sqlite3_close(database), SQLITE_OK);
    }

    static void CreateLegacyPlatformDatabase(const fs::path &path) {
        fs::create_directories(path.parent_path());
        sqlite3 *database = nullptr;
        ASSERT_EQ(sqlite3_open(path.string().c_str(), &database), SQLITE_OK);
        ASSERT_EQ(sqlite3_exec(database,
                      "CREATE TABLE p_titles_names (id INTEGER PRIMARY KEY);"
                      "CREATE TABLE p_levels_hierarchy_names (id INTEGER PRIMARY KEY);"
                      "CREATE TABLE p_metrics (id INTEGER PRIMARY KEY);"
                      "CREATE TABLE p_scaling_values (id INTEGER PRIMARY KEY);",
                      nullptr, nullptr, nullptr),
            SQLITE_OK);
        ASSERT_EQ(sqlite3_close(database), SQLITE_OK);
    }

    static void RegisterInsight(const std::string &rankId, const fs::path &path) {
        auto &manager = Timeline::DataBaseManager::Instance();
        manager.SetDataType(Timeline::DataType::DB, path.string());
        ASSERT_TRUE(manager.CreateTraceConnectionPool(rankId, path.string()));
    }

    static NumaDataSourceContext Context(const fs::path &fileId) { return {"rank0", fileId.string()}; }

    fs::path root_;
};

TEST_F(NumaDataSourceResolverTest, ResolvesMergedInsightDatabaseByRank) {
    const fs::path insight = root_ / "ascend_pytorch_profiler.db";
    CreateMergedDatabase(insight);
    RegisterInsight("rank0", insight);

    const auto database = NumaDataSourceResolver().Resolve(Context(insight));
    ASSERT_NE(database, nullptr);
    EXPECT_EQ(database->GetDbPath(), insight.string());
}

TEST_F(NumaDataSourceResolverTest, ResolvesMergedInsightDatabaseByFileId) {
    const fs::path insight = root_ / "ascend_pytorch_profiler.db";
    CreateMergedDatabase(insight);
    RegisterInsight("registered-rank", insight);
    auto context = Context(insight);
    context.rankId = "missing-rank";

    const auto database = NumaDataSourceResolver().Resolve(context);
    ASSERT_NE(database, nullptr);
    EXPECT_EQ(database->GetDbPath(), insight.string());
}

TEST_F(NumaDataSourceResolverTest, RejectsIncompleteOrLegacySchema) {
    const fs::path incomplete = root_ / "incomplete.db";
    CreateMergedDatabase(incomplete, false);
    RegisterInsight("rank0", incomplete);
    EXPECT_EQ(NumaDataSourceResolver().Resolve(Context(incomplete)), nullptr);

    Timeline::DataBaseManager::Instance().Clear();
    const fs::path legacy = root_ / "legacy.db";
    CreateLegacyPlatformDatabase(legacy);
    RegisterInsight("rank0", legacy);
    EXPECT_EQ(NumaDataSourceResolver().Resolve(Context(legacy)), nullptr);
}

TEST_F(NumaDataSourceResolverTest, DoesNotFallBackToStandalonePlatformDatabase) {
    const fs::path insight = root_ / "ascend_pytorch_profiler.db";
    CreateMergedDatabase(insight, false);
    RegisterInsight("rank0", insight);
    CreateLegacyPlatformDatabase(root_ / "results" / "PLATFORM_20260730095209226" / "platform.db");

    EXPECT_EQ(NumaDataSourceResolver().Resolve(Context(insight)), nullptr);
}

TEST_F(NumaDataSourceResolverTest, OverviewHandlerUsesMergedInsightDatabase) {
    const fs::path insight = root_ / "ascend_pytorch_profiler.db";
    CreateMergedDatabase(insight, true, true);
    RegisterInsight("rank0", insight);
    auto request = std::make_unique<Protocol::NumaOverviewRequest>();
    request->rankId = "rank0";
    request->fileId = insight.string();
    request->projectName = root_.string();

    EXPECT_TRUE(QueryNumaOverviewHandler().HandleRequest(std::move(request)));
}
} // namespace
} // namespace Dic::Module::Numa
