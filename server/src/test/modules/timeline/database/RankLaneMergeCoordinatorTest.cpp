#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <vector>
#include "RankLaneMergeCoordinator.h"

using Dic::Module::Timeline::RankLaneMergeCoordinator;

class RankLaneMergeCoordinatorTest : public testing::Test {
  protected:
    void SetUp() override { RankLaneMergeCoordinator::Instance().Reset(); }
};

TEST_F(RankLaneMergeCoordinatorTest, WaitsForAllRegisteredSources) {
    auto &coordinator = RankLaneMergeCoordinator::Instance();
    coordinator.RegisterSource("rank0", "z.db");
    coordinator.RegisterSource("rank0", "a.db");
    coordinator.MarkSourceSucceeded("rank0", "z.db");
    EXPECT_FALSE(coordinator.IsRankReady("rank0"));
    coordinator.MarkSourceFailed("rank0", "a.db", "failed");
    EXPECT_TRUE(coordinator.IsRankReady("rank0"));
    EXPECT_EQ(coordinator.GetRegisteredSourceCount("rank0"), 2U);
}

TEST_F(RankLaneMergeCoordinatorTest, SelectsSmallestSuccessfulNormalizedPath) {
    auto &coordinator = RankLaneMergeCoordinator::Instance();
    coordinator.RegisterSource("rank0", "z.db");
    coordinator.RegisterSource("rank0", "a.db");
    coordinator.MarkSourceSucceeded("rank0", "z.db");
    coordinator.MarkSourceSucceeded("rank0", "a.db");
    auto sources = coordinator.GetSuccessfulSources("rank0");
    ASSERT_EQ(sources.size(), 2U);
    EXPECT_LT(sources[0], sources[1]);
    EXPECT_EQ(coordinator.GetRepresentativeSource("rank0"), sources.front());
}

TEST_F(RankLaneMergeCoordinatorTest, NormalizesUnicodePathWithoutFilesystemConversion) {
    EXPECT_NO_THROW(RankLaneMergeCoordinator::Instance().RegisterSource("rank0", "????/??.db"));
    auto normalized = RankLaneMergeCoordinator::NormalizeSourceFileId("????/??.db");
    EXPECT_NE(normalized.find("????/??.db"), std::string::npos);
}

TEST_F(RankLaneMergeCoordinatorTest, ReturnsOriginalFileIdAfterUsingNormalizedSortKey) {
    const std::string originalFileId = R"(D:\data\profile\ascend_pytorch_profiler.db)";
    auto &coordinator = RankLaneMergeCoordinator::Instance();
    coordinator.RegisterSource("rank0", originalFileId);
    coordinator.MarkSourceSucceeded("rank0", originalFileId);

    EXPECT_EQ(coordinator.GetRepresentativeSource("rank0"), originalFileId);
    ASSERT_EQ(coordinator.GetSuccessfulSources("rank0").size(), 1U);
    EXPECT_EQ(coordinator.GetSuccessfulSources("rank0").front(), originalFileId);
}

TEST_F(RankLaneMergeCoordinatorTest, RegistrationAndTerminalCallbacksAreIdempotent) {
    auto &coordinator = RankLaneMergeCoordinator::Instance();
    coordinator.RegisterSource("rank0", "a.db");
    coordinator.RegisterSource("rank0", "a.db");
    coordinator.MarkSourceSucceeded("rank0", "a.db");
    coordinator.MarkSourceFailed("rank0", "a.db", "late failure");
    EXPECT_EQ(coordinator.GetSuccessfulSources("rank0").size(), 1U);
}

TEST_F(RankLaneMergeCoordinatorTest, EmitsOnceForPartialSuccessAndNeverForTotalFailure) {
    auto &coordinator = RankLaneMergeCoordinator::Instance();
    coordinator.RegisterSource("partial", "a.db");
    coordinator.RegisterSource("partial", "b.db");
    coordinator.MarkSourceSucceeded("partial", "a.db");
    coordinator.MarkSourceFailed("partial", "b.db", "failed");
    EXPECT_TRUE(coordinator.TryMarkRankEventEmitted("partial"));
    EXPECT_FALSE(coordinator.TryMarkRankEventEmitted("partial"));
    coordinator.RegisterSource("failed", "c.db");
    coordinator.MarkSourceFailed("failed", "c.db", "failed");
    EXPECT_FALSE(coordinator.TryMarkRankEventEmitted("failed"));
}

TEST_F(RankLaneMergeCoordinatorTest, ConcurrentEmissionHasOneWinner) {
    auto &coordinator = RankLaneMergeCoordinator::Instance();
    coordinator.RegisterSource("rank0", "a.db");
    coordinator.MarkSourceSucceeded("rank0", "a.db");
    std::atomic<int> winners = 0;
    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([&]() {
            if (coordinator.TryMarkRankEventEmitted("rank0")) {
                ++winners;
            }
        });
    }
    for (auto &thread : threads) {
        thread.join();
    }
    EXPECT_EQ(winners.load(), 1);
}

TEST_F(RankLaneMergeCoordinatorTest, ResetClearsState) {
    auto &coordinator = RankLaneMergeCoordinator::Instance();
    coordinator.RegisterSource("rank0", "a.db");
    coordinator.MarkSourceSucceeded("rank0", "a.db");
    coordinator.Reset();
    EXPECT_FALSE(coordinator.IsRankReady("rank0"));
    EXPECT_TRUE(coordinator.GetSuccessfulSources("rank0").empty());
}

TEST_F(RankLaneMergeCoordinatorTest, FinalizationClaimSuppressesDuplicatesAndCanRetryAfterFailure) {
    auto &coordinator = RankLaneMergeCoordinator::Instance();
    coordinator.RegisterSource("rank0", "a.db");
    coordinator.MarkSourceSucceeded("rank0", "a.db");

    EXPECT_TRUE(coordinator.TryStartRankFinalization("rank0"));
    EXPECT_FALSE(coordinator.TryStartRankFinalization("rank0"));
    coordinator.MarkRankFinalizationFailed("rank0");
    EXPECT_TRUE(coordinator.TryStartRankFinalization("rank0"));
    coordinator.MarkRankEventEmitted("rank0");
    EXPECT_FALSE(coordinator.TryStartRankFinalization("rank0"));
}

TEST_F(RankLaneMergeCoordinatorTest, RemovingSourceReopensRankLifecycle) {
    auto &coordinator = RankLaneMergeCoordinator::Instance();
    coordinator.RegisterSource("rank0", "a.db");
    coordinator.RegisterSource("rank0", "b.db");
    coordinator.MarkSourceSucceeded("rank0", "a.db");
    coordinator.MarkSourceSucceeded("rank0", "b.db");
    ASSERT_TRUE(coordinator.TryStartRankFinalization("rank0"));
    coordinator.MarkRankEventEmitted("rank0");

    coordinator.RemoveSource("rank0", "a.db");
    EXPECT_TRUE(coordinator.TryStartRankFinalization("rank0"));
    coordinator.MarkRankEventEmitted("rank0");
    coordinator.RegisterSource("rank0", "c.db");
    EXPECT_FALSE(coordinator.TryStartRankFinalization("rank0"));
    coordinator.MarkSourceSucceeded("rank0", "c.db");
    EXPECT_TRUE(coordinator.TryStartRankFinalization("rank0"));
}
