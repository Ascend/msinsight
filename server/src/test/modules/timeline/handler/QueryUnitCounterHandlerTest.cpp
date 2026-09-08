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
#include <filesystem>
#include <gtest/gtest.h>
#include <sqlite3.h>

#include "QueryUnitCounterHandler.h"
#include "DataBaseManager.h"
#include "HandlerTest.cpp"

namespace Dic::Protocol {
using namespace Dic::Module::Timeline;
}

namespace testfs = std::filesystem;

class QueryUnitCounterHandlerTest : public HandlerTest {
  protected:
    void SetUp() override {
        Dic::Protocol::DataBaseManager::Instance().Clear();
        databasePath_ = (testfs::temp_directory_path() / "query-unit-counter-routing.db").string();
        testfs::remove(databasePath_);

        sqlite3 *database = nullptr;
        ASSERT_EQ(sqlite3_open(databasePath_.c_str(), &database), SQLITE_OK);
        ASSERT_EQ(sqlite3_exec(database,
                      "CREATE TABLE NUMA_TITLES_NAMES (name TEXT, description TEXT, summary_flag INTEGER, "
                      "measurement_unit TEXT, unique_id INTEGER);"
                      "CREATE TABLE NUMA_LEVELS_HIERARCHY_NAMES (title0_id INTEGER, title1_id INTEGER, "
                      "title2_id INTEGER);"
                      "CREATE TABLE NUMA_METRICS (ts INTEGER, value REAL, levels_id INTEGER);"
                      "INSERT INTO NUMA_TITLES_NAMES VALUES ('Metric', 'desc', 1, 'Ratio', 1);"
                      "INSERT INTO NUMA_LEVELS_HIERARCHY_NAMES VALUES (1, 0, 0);"
                      "INSERT INTO NUMA_METRICS VALUES (100, 50.0, 1);",
                      nullptr, nullptr, nullptr),
            SQLITE_OK);
        ASSERT_EQ(sqlite3_close(database), SQLITE_OK);
        auto &manager = Dic::Protocol::DataBaseManager::Instance();
        manager.SetFileType(Dic::Protocol::FileType::MS_PROF, databasePath_);
        manager.SetRankIdFileIdMapping("trace-rank", databasePath_);
        auto platform = manager.CreatePlatformDataBase("trace-rank#platform", databasePath_);
        ASSERT_NE(platform, nullptr);
        ASSERT_TRUE(platform->OpenDb(databasePath_, false));
        manager.CreatePlatformDataBase("standalone-platform-rank", databasePath_);
    }

    void TearDown() override {
        Dic::Protocol::DataBaseManager::Instance().Clear();
        testfs::remove(databasePath_);
    }

    bool QueryCounter(const std::string &rankId) {
        Dic::Module::Timeline::QueryUnitCounterHandler handler;
        auto requestPtr = std::make_unique<Dic::Protocol::UnitCounterRequest>();
        requestPtr->params.rankId = rankId;
        requestPtr->params.threadName = "Metric";
        requestPtr->params.threadId = "1";
        return handler.HandleRequest(std::move(requestPtr));
    }

    std::string databasePath_;
};

TEST_F(HandlerTest, QueryUnitCounterHandlerTestNormal) {
    Dic::Module::Timeline::QueryUnitCounterHandler handler;
    std::unique_ptr<Dic::Protocol::Request> requestPtr = std::make_unique<Dic::Protocol::UnitCounterRequest>();
    handler.HandleRequest(std::move(requestPtr));
}

TEST_F(QueryUnitCounterHandlerTest, RoutesOnlyEmbeddedRankToSharedPlatformDatabase) {
    EXPECT_FALSE(QueryCounter("trace-rank"));
    EXPECT_TRUE(QueryCounter("trace-rank#platform"));
}

TEST_F(QueryUnitCounterHandlerTest, DoesNotRouteStandalonePlatformRank) {
    Dic::Protocol::DataBaseManager::Instance().SetFileType(Dic::Protocol::FileType::PLATFORM, databasePath_);
    EXPECT_FALSE(QueryCounter("standalone-platform-rank"));
}
