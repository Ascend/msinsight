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
#include <numeric>
#include "../dataEngine/TimelineMemoryDatabaseFixture.h"
#include "NpuInfoTable.h"
#include "PythonGCTable.h"
using namespace Dic::Module::Timeline;
using namespace Dic::Module::Timeline::Test;
namespace {
class TableBoundaryTest : public TimelineMemoryDatabaseTestBase {};
TEST_F(TableBoundaryTest, NpuAndGcColumnsMapActualSqliteRows) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO NPU_INFO VALUES(0,'Ascend'),(9,'Other');"
                                  "INSERT INTO GC_RECORD(ROWID,startNs,endNs,globalTid) VALUES(3,100,125,7);"));
    NpuInfoTable npu;
    const auto devices = npu.Select(NpuInfoColumn::ID, NpuInfoColumn::NAME)
                             .OrderBy(NpuInfoColumn::ID, TableOrder::DESC)
                             .ExcuteQuery(rank);
    ASSERT_EQ(devices.size(), 2U);
    EXPECT_EQ(devices[0].id, 9U);
    EXPECT_EQ(devices[0].name, "Other");
    EXPECT_EQ(devices[1].name, "Ascend");
    PythonGCTable gc;
    const auto records =
        gc.Select(PythonGCColumn::ID, PythonGCColumn::TIMESTAMP, PythonGCColumn::ENDTIME).ExcuteQuery(rank);
    ASSERT_EQ(records.size(), 1U);
    EXPECT_EQ(records[0].id, 3U);
    EXPECT_EQ(records[0].timestamp, 100U);
    EXPECT_EQ(records[0].endTime, 125U);
}
TEST_F(TableBoundaryTest, EmptyInAndNotInHaveOppositePredicatesAndResetState) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO NPU_INFO VALUES(0,'zero'),(2,'two');"));
    NpuInfoTable table;
    const std::vector<uint64_t> empty;
    EXPECT_TRUE(table.Select(NpuInfoColumn::ID).In(NpuInfoColumn::ID, empty).ExcuteQuery(rank).empty());
    EXPECT_EQ(table.Select(NpuInfoColumn::ID).NotIn(NpuInfoColumn::ID, empty).ExcuteQuery(rank).size(), 2U);
    EXPECT_EQ(table.Select(NpuInfoColumn::NAME).ExcuteQuery(rank).size(), 2U);
}
TEST_F(TableBoundaryTest, InAndNotInHandlePlaceholderAndLiteralBatchBoundaries) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO NPU_INFO VALUES(0,'zero'),(899,'edge'),(900,'next'),(901,'outside');"));
    NpuInfoTable table;
    for (size_t count : {900U, 901U}) {
        std::vector<uint64_t> ids(count);
        std::iota(ids.begin(), ids.end(), 0);
        EXPECT_EQ(table.Select(NpuInfoColumn::ID).In(NpuInfoColumn::ID, ids).ExcuteQuery(rank).size(),
            count == 900 ? 2U : 3U);
        EXPECT_EQ(table.Select(NpuInfoColumn::ID).NotIn(NpuInfoColumn::ID, ids).ExcuteQuery(rank).size(),
            count == 900 ? 2U : 1U);
    }
}
TEST_F(TableBoundaryTest, LargeStringListsEscapeQuotesWithoutChangingQueryMeaning) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO NPU_INFO VALUES(1,'O''Brien'),(2,'safe');"));
    NpuInfoTable table;
    std::vector<std::string> names(901, "absent");
    names[900] = "O'Brien";
    const auto included =
        table.Select(NpuInfoColumn::ID, NpuInfoColumn::NAME).In(NpuInfoColumn::NAME, names).ExcuteQuery(rank);
    ASSERT_EQ(included.size(), 1U);
    EXPECT_EQ(included[0].id, 1U);
    const auto excluded = table.Select(NpuInfoColumn::ID).NotIn(NpuInfoColumn::NAME, names).ExcuteQuery(rank);
    ASSERT_EQ(excluded.size(), 1U);
    EXPECT_EQ(excluded[0].id, 2U);
}
TEST_F(TableBoundaryTest, CombinedPredicatesBindAllSupportedTypesAndCount) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO NPU_INFO VALUES(1,'alpha'),(2,'alpine'),(3,NULL),(4,'beta');"));
    NpuInfoTable table;
    const auto rows = table.Select(NpuInfoColumn::ID)
                          .GreaterEq(NpuInfoColumn::ID, uint32_t{1})
                          .Less(NpuInfoColumn::ID, uint64_t{4})
                          .Like(NpuInfoColumn::NAME, "al%")
                          .NotEq(NpuInfoColumn::ID, uint32_t{1})
                          .ExcuteQuery(rank);
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0].id, 2U);
    EXPECT_EQ(table.IsNullOrNotEq(NpuInfoColumn::NAME, std::string("beta")).Count(rank), 3U);
    EXPECT_EQ(table.Greater(NpuInfoColumn::ID, uint32_t{1}).LessEq(NpuInfoColumn::ID, uint64_t{3}).Count(rank), 2U);
    EXPECT_EQ(table.Eq(NpuInfoColumn::NAME, std::string("alpha")).Count(rank), 1U);
    EXPECT_EQ(table.Count(rank), 4U);
}
TEST_F(TableBoundaryTest, FailedQueryClearsBindingsBeforeNextQuery) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO NPU_INFO VALUES(1,'one');"));
    NpuInfoTable table;
    EXPECT_TRUE(table.Select("missing_column").Eq(NpuInfoColumn::ID, uint32_t{999}).ExcuteQuery(rank).empty());
    const auto rows = table.Select(NpuInfoColumn::ID).ExcuteQuery(rank);
    ASSERT_EQ(rows.size(), 1U);
    EXPECT_EQ(rows[0].id, 1U);
    EXPECT_TRUE(
        table.Select(NpuInfoColumn::ID).Eq(NpuInfoColumn::ID, uint32_t{999}).ExcuteQuery(rank + "missing").empty());
    EXPECT_EQ(table.Count(rank), 1U);
    EXPECT_EQ(table.Count(rank + "missing"), 0U);
}
TEST_F(TableBoundaryTest, GroupByRemovesDuplicateDevicesAndMissingGcTableReturnsEmpty) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO NPU_INFO VALUES(1,'one'),(1,'one'),(2,'two'); DROP TABLE GC_RECORD;"));
    NpuInfoTable table;
    EXPECT_EQ(table.Select(NpuInfoColumn::ID).GroupBy(NpuInfoColumn::ID).ExcuteQuery(rank).size(), 2U);
    PythonGCTable gc;
    EXPECT_TRUE(gc.Select(PythonGCColumn::ID).ExcuteQuery(rank).empty());
}
}
