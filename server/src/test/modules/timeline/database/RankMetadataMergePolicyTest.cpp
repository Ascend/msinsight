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
