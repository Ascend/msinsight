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
#include "SystemViewOverallCacheManager.h"

using namespace Dic::Module::Timeline;
class SystemViewOverallCacheManagerTest : public testing::Test {
  protected:
    void SetUp() override { SystemViewOverallCacheManager::Instance().ClearAll(); }
    void TearDown() override { SystemViewOverallCacheManager::Instance().ClearAll(); }
};

TEST_F(SystemViewOverallCacheManagerTest, StoresIndependentCopiesAndReplacesOneKey) {
    auto &cache = SystemViewOverallCacheManager::Instance();
    Dic::Protocol::SystemViewOverallRes row;
    row.name = "compute";
    row.totalTime = 42.5;
    row.children.push_back(row);
    cache.SetOverallData("rank0", {row});
    cache.SetOverallData("rank1", {row});
    row.name = "mutated";
    auto found = cache.GetOverallData("rank0");
    ASSERT_EQ(found.size(), 1U);
    EXPECT_EQ(found[0].name, "compute");
    EXPECT_DOUBLE_EQ(found[0].totalTime, 42.5);
    ASSERT_EQ(found[0].children.size(), 1U);
    found[0].children.clear();
    EXPECT_EQ(cache.GetOverallData("rank0")[0].children.size(), 1U);
    cache.SetOverallData("rank0", {});
    EXPECT_TRUE(cache.GetOverallData("rank0").empty());
    EXPECT_EQ(cache.GetOverallData("rank1").size(), 1U);
}

TEST_F(SystemViewOverallCacheManagerTest, MissingKeysAndRepeatedClearAreEmpty) {
    auto &cache = SystemViewOverallCacheManager::Instance();
    EXPECT_TRUE(cache.GetOverallData("missing").empty());
    cache.SetOverallData("", {Dic::Protocol::SystemViewOverallRes{}});
    EXPECT_EQ(cache.GetOverallData("").size(), 1U);
    cache.ClearAll();
    cache.ClearAll();
    EXPECT_TRUE(cache.GetOverallData("").empty());
}
