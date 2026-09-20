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
#include <algorithm>
#include <array>
#include "MemcpyDetailTestDatabase.h"
#include "MemcpyOverallDatabaseAccesser.h"
#include "TraceTime.h"

namespace Dic::Module::Timeline {
namespace {
class MemcpyDetailSecurityTest : public testing::TestWithParam<DataType> {
  protected:
    void SetUp() override {
        TraceTime::Instance().Reset();
        TraceTime::Instance().UpdateTime(100, 1000);
        database = std::make_shared<MemcpyDetailTestDatabase>(mutex);
        ASSERT_TRUE(database->OpenMemory());
        fileId = GetParam() == DataType::DB ? "memcpy-security-db" : "memcpy-security-text";
        DataBaseManager::Instance().SetDataType(GetParam(), fileId);
        const char *schema = GetParam() == DataType::DB ? R"SQL(
            CREATE TABLE TASK(globalTaskId INTEGER, streamId INTEGER, taskType INTEGER, startNs INTEGER, endNs INTEGER);
            CREATE TABLE MEMCPY_INFO(globalTaskId INTEGER, memcpyOperation INTEGER, size INTEGER);
            CREATE TABLE ENUM_MEMCPY_OPERATION(id INTEGER, name TEXT);
            CREATE TABLE STRING_IDS(id INTEGER, value TEXT);
            INSERT INTO ENUM_MEMCPY_OPERATION VALUES(1,'host to device');
            INSERT INTO STRING_IDS VALUES(1,'MEMCPY_ASYNC');
            INSERT INTO TASK VALUES(1,7,1,120,150),(2,7,1,110,170),(3,7,1,130,140);
            INSERT INTO MEMCPY_INFO VALUES(1,1,900),(2,1,300),(3,1,600);
        )SQL"
                                                        : R"SQL(
            CREATE TABLE slice(id INTEGER, name TEXT, args TEXT, timestamp INTEGER, duration INTEGER, end_time INTEGER, track_id INTEGER);
            CREATE TABLE thread(track_id INTEGER, tid TEXT);
            INSERT INTO thread VALUES(1,'7');
            INSERT INTO slice VALUES
              (1,'MEMCPY_ASYNC','{"operation":"host to device","size(B)":900}',120,30,150,1),
              (2,'MEMCPY_ASYNC','{"operation":"host to device","size(B)":300}',110,60,170,1),
              (3,'MEMCPY_ASYNC','{"operation":"host to device","size(B)":600}',130,10,140,1);
        )SQL";
        ASSERT_EQ(sqlite3_exec(database->Handle(), schema, nullptr, nullptr, nullptr), SQLITE_OK);
        accessor = std::make_unique<MemcpyOverallDatabaseAccesser>(database, fileId);
        ASSERT_EQ(sqlite3_trace_v2(database->Handle(), SQLITE_TRACE_STMT, TraceStatement, &statementCount), SQLITE_OK);
    }

    void TearDown() override {
        accessor.reset();
        database.reset();
        TraceTime::Instance().Reset();
    }

    static int TraceStatement(unsigned, void *context, void *, void *) {
        ++*static_cast<size_t *>(context);
        return 0;
    }

    bool Query(const std::string &field, const std::string &direction, std::vector<MemcpyDetailRecord> &records,
        uint64_t &total, uint32_t page = 1, uint32_t size = 10) {
        return accessor->GetMemcpyDetailRecordsPaged(
            0, 0, "7", "host to device", page, size, Protocol::OrderParam{field, direction}, records, total);
    }

    static std::vector<std::string> Ids(const std::vector<MemcpyDetailRecord> &records) {
        std::vector<std::string> result;
        for (const auto &record : records) {
            result.push_back(record.id);
        }
        return result;
    }

    void ExpectFixedDbOrder(const std::string &field, const std::string &direction) const {
        if (GetParam() != DataType::DB) {
            return;
        }
        ASSERT_FALSE(database->preparedSql.empty());
        const auto &sql = database->preparedSql.back();
        const auto orderPos = sql.find("ORDER BY");
        ASSERT_NE(orderPos, std::string::npos);
        EXPECT_EQ(sql.substr(orderPos), "ORDER BY " + field + " " + direction + " LIMIT ? OFFSET ?");
    }

    std::recursive_mutex mutex;
    std::shared_ptr<MemcpyDetailTestDatabase> database;
    std::unique_ptr<MemcpyOverallDatabaseAccesser> accessor;
    std::string fileId;
    size_t statementCount = 0;
};

TEST_P(MemcpyDetailSecurityTest, SupportedFieldsAndDirectionSpellingsReturnCorrectRows) {
    const std::array<std::pair<std::string, std::vector<std::string>>, 3> orders = {
        {{"startTime", {"2", "1", "3"}}, {"duration", {"3", "1", "2"}}, {"size", {"2", "3", "1"}}}};
    for (const auto &[field, ascending] : orders) {
        for (const auto &direction : {"ascend", "descend", "Ascend", "Descend"}) {
            SCOPED_TRACE(field + " " + direction);
            auto expected = ascending;
            const bool isAscending = std::string(direction) == "ascend" || std::string(direction) == "Ascend";
            if (!isAscending) {
                std::reverse(expected.begin(), expected.end());
            }
            std::vector<MemcpyDetailRecord> records;
            uint64_t total = 0;
            const auto previousStatements = statementCount;
            ASSERT_TRUE(Query(field, direction, records, total));
            EXPECT_EQ(total, 3);
            EXPECT_EQ(Ids(records), expected);
            EXPECT_GT(statementCount, previousStatements);
            ExpectFixedDbOrder(field, isAscending ? "ASC" : "DESC");
        }
    }
}

TEST_P(MemcpyDetailSecurityTest, InvalidFieldsExecuteSafeTimeSortWithoutUsingInputAsSql) {
    const std::vector<std::string> invalidFields = {"CASE WHEN 1=1 THEN -startTime ELSE startTime END",
        "(SELECT -filtered.startTime)", "startTime,duration", "startTime--", "startTime;SELECT 1", "unknown",
        "StartTime", std::string("startTime\0, duration", 20), std::string("size\0ignored", 12)};
    for (const auto &field : invalidFields) {
        for (const auto &direction : {"ascend", "descend"}) {
            SCOPED_TRACE(field + " " + direction);
            std::vector<MemcpyDetailRecord> records;
            uint64_t total = 0;
            const auto previousStatements = statementCount;
            const auto previousPrepares = database->prepareCount;
            ASSERT_TRUE(Query(field, direction, records, total));
            EXPECT_EQ(total, 3);
            const bool isAscending = std::string(direction) == "ascend";
            EXPECT_EQ(Ids(records),
                (isAscending ? std::vector<std::string>{"2", "1", "3"} : std::vector<std::string>{"3", "1", "2"}));
            EXPECT_GT(database->prepareCount, previousPrepares);
            EXPECT_GT(statementCount, previousStatements);
            ExpectFixedDbOrder("startTime", isAscending ? "ASC" : "DESC");
            for (const auto &sql : database->preparedSql) {
                EXPECT_EQ(sql.find(field), std::string::npos);
            }
        }
    }
}

TEST_P(MemcpyDetailSecurityTest, UnknownDirectionsPreserveDescendingFallback) {
    for (const auto &direction : {"ASC", "DESC", "ascend;SELECT 1", "ascend--", "random"}) {
        SCOPED_TRACE(direction);
        std::vector<MemcpyDetailRecord> records;
        uint64_t total = 0;
        const auto previousStatements = statementCount;
        ASSERT_TRUE(Query("startTime", direction, records, total));
        EXPECT_EQ(total, 3);
        EXPECT_EQ(Ids(records), (std::vector<std::string>{"3", "1", "2"}));
        EXPECT_GT(statementCount, previousStatements);
        ExpectFixedDbOrder("startTime", "DESC");
    }
}

TEST_P(MemcpyDetailSecurityTest, ClearedAndMissingSortingPreserveExistingBehavior) {
    for (const auto &field : {"", "startTime", "timestamp", "duration", "size", "unknown"}) {
        SCOPED_TRACE(field);
        std::vector<MemcpyDetailRecord> records;
        uint64_t total = 0;
        const auto previousStatements = statementCount;
        ASSERT_TRUE(Query(field, "", records, total));
        EXPECT_EQ(total, 3);
        EXPECT_GT(statementCount, previousStatements);
        if (GetParam() == DataType::TEXT) {
            const std::string selectedField = field;
            const std::vector<std::string> expected = selectedField == "duration"
                ? std::vector<std::string>{"2", "1", "3"}
                : selectedField == "size" ? std::vector<std::string>{"1", "3", "2"}
                                          : std::vector<std::string>{"3", "1", "2"};
            EXPECT_EQ(Ids(records), expected);
        }
        auto ids = Ids(records);
        std::sort(ids.begin(), ids.end());
        EXPECT_EQ(ids, (std::vector<std::string>{"1", "2", "3"}));
        for (const auto &sql : database->preparedSql) {
            EXPECT_EQ(sql.find("ORDER BY"), std::string::npos);
        }
    }
}

TEST_P(MemcpyDetailSecurityTest, PaginationFollowsRequestedOrdering) {
    std::vector<MemcpyDetailRecord> first, second;
    uint64_t total = 0;
    ASSERT_TRUE(Query("size", "ascend", first, total, 1, 2));
    EXPECT_EQ(total, 3);
    ASSERT_TRUE(Query("size", "ascend", second, total, 2, 2));
    EXPECT_EQ(Ids(first), (std::vector<std::string>{"2", "3"}));
    EXPECT_EQ(Ids(second), (std::vector<std::string>{"1"}));
}

TEST_P(MemcpyDetailSecurityTest, StartTimeAndTimestampAliasesReturnEquivalentRows) {
    for (const auto &direction : {"ascend", "descend", "Ascend", "Descend", ""}) {
        SCOPED_TRACE(direction);
        std::vector<MemcpyDetailRecord> startTimeRecords, timestampRecords;
        uint64_t startTimeTotal = 0;
        uint64_t timestampTotal = 0;
        const auto previousStatements = statementCount;
        ASSERT_TRUE(Query("startTime", direction, startTimeRecords, startTimeTotal));
        EXPECT_GT(statementCount, previousStatements);
        const auto statementsAfterStartTime = statementCount;
        ASSERT_TRUE(Query("timestamp", direction, timestampRecords, timestampTotal));
        EXPECT_GT(statementCount, statementsAfterStartTime);
        EXPECT_EQ(startTimeTotal, 3);
        EXPECT_EQ(timestampTotal, startTimeTotal);
        auto startTimeIds = Ids(startTimeRecords);
        auto timestampIds = Ids(timestampRecords);
        if (std::string(direction).empty() && GetParam() == DataType::DB) {
            // With no ORDER BY, compare row membership without promising a SQLite row order.
            std::sort(startTimeIds.begin(), startTimeIds.end());
            std::sort(timestampIds.begin(), timestampIds.end());
            EXPECT_EQ(startTimeIds, (std::vector<std::string>{"1", "2", "3"}));
            EXPECT_EQ(database->preparedSql.back().find("ORDER BY"), std::string::npos);
        } else {
            const bool isAscending = std::string(direction) == "ascend" || std::string(direction) == "Ascend";
            EXPECT_EQ(startTimeIds,
                (isAscending ? std::vector<std::string>{"2", "1", "3"} : std::vector<std::string>{"3", "1", "2"}));
            ExpectFixedDbOrder("startTime", isAscending ? "ASC" : "DESC");
        }
        EXPECT_EQ(timestampIds, startTimeIds);
    }
}

INSTANTIATE_TEST_SUITE_P(InMemorySqlite, MemcpyDetailSecurityTest, testing::Values(DataType::DB, DataType::TEXT),
    [](const testing::TestParamInfo<DataType> &info) { return info.param == DataType::DB ? "DB" : "TEXT"; });
}
}
