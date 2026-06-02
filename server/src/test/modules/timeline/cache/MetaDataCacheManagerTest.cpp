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

#include <gtest/gtest.h>
#include "MetaDataCacheManager.h"

using namespace Dic::Module::Timeline;
class MetaDataCacheManagerTest : public ::testing::Test {};

TEST_F(MetaDataCacheManagerTest, test_MetaDataCacheManager_success) {
    MetaDataCacheManager::Instance().Clear();
    std::vector<ParallelGroupInfo> groupInfoList;
    groupInfoList.push_back({"10.174.216.241%enp189s0f1_55000_0_1738895486152654", "default_group", {"1", "2"}});
    groupInfoList.push_back({"", "group", {"1", "2"}});
    MetaDataCacheManager::Instance().AddParallelGroupInfo(groupInfoList);
    EXPECT_EQ(MetaDataCacheManager::Instance().GetParallelGroupInfo("").has_value(), false);
    EXPECT_EQ(MetaDataCacheManager::Instance().GetParallelGroupInfo("test").has_value(), false);
    auto infoOpt =
        MetaDataCacheManager::Instance().GetParallelGroupInfo("10.174.216.241%enp189s0f1_55000_0_1738895486152654");
    EXPECT_EQ(infoOpt.has_value(), true);
    EXPECT_EQ(infoOpt.value().groupName, "default_group");
    MetaDataCacheManager::Instance().Clear();
}
