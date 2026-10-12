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
#include "TimelineMemoryDatabaseFixture.h"
#include "NpuInfoRepo.h"
using namespace Dic::Module::Timeline;
using namespace Dic::Module::Timeline::Test;
namespace {
class NpuInfoRepoBoundaryTest : public TimelineMemoryDatabaseTestBase {};
TEST_F(NpuInfoRepoBoundaryTest, RealTableReturnsUniqueDeviceIdsAndKeepsZero) {
    ASSERT_TRUE(database->ExecSql("INSERT INTO NPU_INFO VALUES(0,'zero'),(2,'two'),(2,'duplicate');"));
    NpuInfoRepo repo;
    auto ids = repo.QueryDeviceIdByFileId(rank);
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids, (std::vector<uint64_t>{0, 2}));
    repo.SetNpuInfoTable(nullptr);
    EXPECT_EQ(repo.QueryDeviceIdByFileId(rank).size(), 2U);
}
TEST_F(NpuInfoRepoBoundaryTest, EmptyMissingDatabaseAndMissingSchemaReturnEmpty) {
    NpuInfoRepo repo;
    EXPECT_TRUE(repo.QueryDeviceIdByFileId(rank).empty());
    EXPECT_TRUE(repo.QueryDeviceIdByFileId(rank + "missing").empty());
    ASSERT_TRUE(database->ExecSql("DROP TABLE NPU_INFO;"));
    EXPECT_TRUE(repo.QueryDeviceIdByFileId(rank).empty());
}
}
