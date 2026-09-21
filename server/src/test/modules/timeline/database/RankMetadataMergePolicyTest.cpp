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

#include "RankMetadataMergePolicy.h"

using Dic::Module::Timeline::RankMetadataMergePolicy;

TEST(RankMetadataMergePolicyTest, RepresentativeKeepsEveryMetadataType) {
    EXPECT_TRUE(RankMetadataMergePolicy::ShouldInclude("Ascend Hardware", true));
    EXPECT_TRUE(RankMetadataMergePolicy::ShouldInclude("OVERLAP_ANALYSIS", true));
    EXPECT_TRUE(RankMetadataMergePolicy::ShouldInclude("CANN_API", true));
}

TEST(RankMetadataMergePolicyTest, AdditionalSourcesContributeHardwareButExcludeSourceOverlap) {
    EXPECT_TRUE(RankMetadataMergePolicy::ShouldInclude("Ascend Hardware", false));
    EXPECT_FALSE(RankMetadataMergePolicy::ShouldInclude("OVERLAP_ANALYSIS", false));
    EXPECT_TRUE(RankMetadataMergePolicy::ShouldInclude("HCCL", false));
    EXPECT_TRUE(RankMetadataMergePolicy::ShouldInclude("CCU", false));
    EXPECT_TRUE(RankMetadataMergePolicy::ShouldInclude("CANN_API", false));
    EXPECT_TRUE(RankMetadataMergePolicy::ShouldInclude("API", false));
    EXPECT_TRUE(RankMetadataMergePolicy::ShouldInclude("OSRT_API", false));
    EXPECT_TRUE(RankMetadataMergePolicy::ShouldInclude("MS_TX", false));
}
