/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#include <gtest/gtest.h>

#include <algorithm>
#include <numeric>
#include "FreeAttributionService.h"

namespace Dic::Module::Timeline {
namespace {
FreeAttributionTarget Target(const std::string &id, const std::string &name, uint64_t start = 0, uint64_t duration = 20,
    const std::string &source = "source-a") {
    FreeAttributionTarget target;
    target.id = id;
    target.name = name;
    target.cardId = "rank-0";
    target.dbPath = source;
    target.pid = "10";
    target.tid = "11";
    target.metaType = name == "Free" ? "OVERLAP_ANALYSIS" : "Python";
    target.timestamp = start;
    target.duration = duration;
    return target;
}

FreeAttributionEvidence Evidence(const std::vector<FreeAttributionTarget> &apis) {
    FreeAttributionEvidence evidence;
    evidence.targetFound = true;
    evidence.flowFound = true;
    evidence.apiTargets = apis;
    return evidence;
}

FreeAttributionRecord Matched(const std::string &id, const std::string &apiName, uint64_t start, uint64_t duration,
    const std::string &source = "source-a") {
    return FreeAttributionService::AttributeFree(Target(id, "Free", start, duration, source), "Computing",
        {Evidence({Target("api-" + id, apiName, 1, 5, source)})});
}

FreeAttributionRecord WithCandidates(const std::string &id, const std::vector<FreeAttributionTarget> &apis,
    bool complete = false, uint64_t start = 100, uint64_t duration = 20) {
    auto evidence = Evidence(apis);
    evidence.complete = complete;
    return FreeAttributionService::AttributeFree(Target(id, "Free", start, duration), "Computing", {evidence});
}

std::vector<std::string> FreeIds(const FreeAttributionDetails &details) {
    std::vector<std::string> ids;
    for (const auto &record : details.records) {
        ids.push_back(record.freeTarget.id);
    }
    return ids;
}

std::vector<std::string> CandidateIds(const FreeAttributionRecord &record) {
    std::vector<std::string> ids;
    for (const auto &api : record.candidateApiTargets) {
        ids.push_back(api.id);
    }
    return ids;
}

FreeAttributionParams Params() {
    FreeAttributionParams params;
    params.rankId = "rank-0";
    return params;
}

TEST(FreeAttributionTest, AttributesDeviceCandidateToItsExactHostApi) {
    const auto api = Target("api-1", "aten::matmul", 10, 7, "host-source");
    const auto free = Target("free-1", "Free", 100, 80, "derived-overlap");
    const auto result = FreeAttributionService::AttributeFree(free, "Computing", {Evidence({api})});
    ASSERT_EQ(result.attributionStatus, "matched");
    ASSERT_TRUE(result.hostApiTarget.has_value());
    EXPECT_EQ(result.hostApiTarget->id, "api-1");
    EXPECT_EQ(result.hostApiTarget->dbPath, "host-source");
    EXPECT_EQ(result.freeTarget.dbPath, "derived-overlap");
    EXPECT_EQ(result.freeDuration, 80U);
    EXPECT_EQ(result.nextBehaviorType, "Computing");
}

TEST(FreeAttributionTest, CoStartingCandidatesWithOneApiDoNotDuplicateFree) {
    const auto api = Target("api-1", "aten::add");
    const auto result = FreeAttributionService::AttributeFree(
        Target("1", "Free", 100, 30), "Computing,Communication", {Evidence({api, api}), Evidence({api})});
    EXPECT_EQ(result.attributionStatus, "matched");
    ASSERT_TRUE(result.hostApiTarget.has_value());
    EXPECT_EQ(result.hostApiTarget->id, api.id);
    EXPECT_EQ(result.freeDuration, 30U);
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize({result}, Params(), summary);
    ASSERT_EQ(summary.groups.size(), 1U);
    EXPECT_EQ(summary.groups[0].freeCount, 1U);
    EXPECT_EQ(summary.groups[0].totalFreeDuration, 30U);
}

TEST(FreeAttributionTest, DistinctInstancesWithSameNameAreAmbiguous) {
    const auto result = FreeAttributionService::AttributeFree(
        Target("1", "Free"), "Computing", {Evidence({Target("api-1", "aten::add"), Target("api-2", "aten::add")})});
    EXPECT_EQ(result.attributionStatus, "ambiguous");
    EXPECT_FALSE(result.hostApiTarget.has_value());
    EXPECT_EQ(result.candidateApiTargets.size(), 2U);
}

TEST(FreeAttributionTest, IdenticalApiIdsFromDifferentSourcesRemainDistinct) {
    const auto result = FreeAttributionService::AttributeFree(Target("1", "Free"), "Communication",
        {Evidence(
            {Target("api-1", "all_reduce", 1, 20, "source-a"), Target("api-1", "all_reduce", 1, 20, "source-b")})});
    EXPECT_EQ(result.attributionStatus, "ambiguous");
    EXPECT_EQ(result.candidateApiTargets.size(), 2U);
}

TEST(FreeAttributionTest, MissingCandidateChainMakesResolvedSubsetIncomplete) {
    FreeAttributionEvidence missing;
    missing.targetFound = true;
    const auto result = FreeAttributionService::AttributeFree(
        Target("1", "Free"), "Computing", {Evidence({Target("api-1", "aten::add")}), missing});
    EXPECT_EQ(result.attributionStatus, "incomplete");
    EXPECT_FALSE(result.hostApiTarget.has_value());
    ASSERT_EQ(result.candidateApiTargets.size(), 1U);
    EXPECT_EQ(result.candidateApiTargets[0].name, "aten::add");
    EXPECT_FALSE(result.reason.empty());
}

TEST(FreeAttributionTest, PartiallyResolvedFlowWithinOneDeviceCandidateIsIncomplete) {
    auto evidence = Evidence({Target("api-1", "aten::add")});
    evidence.complete = false;
    const auto result = FreeAttributionService::AttributeFree(Target("1", "Free"), "Computing", {evidence});
    EXPECT_EQ(result.attributionStatus, "incomplete");
    EXPECT_FALSE(result.hostApiTarget.has_value());
    ASSERT_EQ(result.candidateApiTargets.size(), 1U);
    EXPECT_EQ(result.candidateApiTargets[0].id, "api-1");
}

TEST(FreeAttributionTest, MissingTargetFlowAndPytorchApiKeepFreeReachable) {
    FreeAttributionEvidence noTarget;
    FreeAttributionEvidence noFlow;
    noFlow.targetFound = true;
    const auto noApi = Evidence({});
    for (const auto &evidence : {noTarget, noFlow, noApi}) {
        const auto result =
            FreeAttributionService::AttributeFree(Target("1", "Free", 100, 40), "Computing", {evidence});
        EXPECT_EQ(result.attributionStatus, "unresolved");
        EXPECT_FALSE(result.hostApiTarget.has_value());
        EXPECT_FALSE(result.reason.empty());
        EXPECT_EQ(result.freeTarget.id, "1");
        EXPECT_EQ(result.freeDuration, 40U);
    }
}

TEST(FreeAttributionTest, InvalidSuccessorIsNotPromotedByAnApiCandidate) {
    for (const std::string reason : {"noNextBehavior", "nonContiguousBehavior"}) {
        const auto result = FreeAttributionService::AttributeFree(
            Target("1", "Free"), "Computing", {Evidence({Target("api-1", "aten::add")})}, reason);
        EXPECT_EQ(result.attributionStatus, "unresolved");
        EXPECT_EQ(result.reason, reason);
        EXPECT_FALSE(result.hostApiTarget.has_value());
    }
}

TEST(FreeAttributionTest, GroupsFullApiNamesAndCountsFreeInsteadOfApiDuration) {
    auto first = Matched("1", "aten::add", 100, 80);
    auto second = Matched("2", "aten::add", 200, 30);
    second.hostApiTarget = first.hostApiTarget;
    const auto third = Matched("3", "aten::add.scope", 300, 10);
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize({first, second, third}, Params(), summary);
    EXPECT_EQ(summary.totalGroups, 2U);
    EXPECT_EQ(summary.totalFreeCount, 3U);
    EXPECT_EQ(summary.totalFreeDuration, 120U);
    ASSERT_EQ(summary.groups.size(), 2U);
    EXPECT_EQ(summary.groups[0].apiName, "aten::add");
    EXPECT_EQ(summary.groups[0].totalFreeDuration, 110U);
    EXPECT_EQ(summary.groups[0].freeCount, 2U);
}

TEST(FreeAttributionTest, SearchFiltersBeforeSummaryTotalsSortingAndPagination) {
    const std::vector<FreeAttributionRecord> records = {Matched("1", "aten::other", 100, 10),
        Matched("2", "aten::MatMul", 200, 30), Matched("3", "aten::matmul.scope", 300, 20),
        FreeAttributionService::AttributeFree(Target("4", "Free", 400, 40), "Computing", {})};
    auto params = Params();
    params.searchName = "MATMUL";
    params.current = 2;
    params.pageSize = 1;
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize(records, params, summary);
    EXPECT_EQ(summary.totalGroups, 2U);
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 50U);
    ASSERT_EQ(summary.groups.size(), 1U);
    EXPECT_EQ(summary.groups[0].apiName, "aten::matmul.scope");

    params.searchName = "missing-api";
    FreeAttributionService::Summarize(records, params, summary);
    EXPECT_TRUE(summary.groups.empty());
    EXPECT_EQ(summary.totalGroups, 0U);
    EXPECT_EQ(summary.totalFreeCount, 0U);
    EXPECT_EQ(summary.totalFreeDuration, 0U);

    params.searchName.clear();
    FreeAttributionService::Summarize(records, params, summary);
    EXPECT_EQ(summary.totalGroups, 4U);
    EXPECT_EQ(summary.totalFreeCount, 4U);
    EXPECT_EQ(summary.totalFreeDuration, 100U);
}

TEST(FreeAttributionTest, SearchMatchesAnyCandidateWithoutChangingEvidenceOrAttribution) {
    const auto incomplete =
        WithCandidates("1", {Target("a", "aten::other"), Target("b", "aten::needle")}, false, 100, 50);
    const auto ambiguous =
        WithCandidates("2", {Target("c", "aten::another"), Target("d", "aten::NeedleScope")}, true, 200, 30);
    const auto excluded = WithCandidates("3", {Target("e", "aten::other")}, false, 300, 20);
    const std::vector<FreeAttributionRecord> records = {incomplete, ambiguous, excluded};
    auto params = Params();
    params.searchName = "NEEDLE";
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize(records, params, summary);
    EXPECT_EQ(summary.totalGroups, 2U);
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 80U);

    for (const auto &original : {incomplete, ambiguous}) {
        params.groupId = "status:" + original.attributionStatus;
        FreeAttributionDetails details;
        FreeAttributionService::SelectDetails(records, params, details);
        EXPECT_EQ(details.totalRecords, 1U);
        ASSERT_EQ(details.records.size(), 1U);
        const auto &record = details.records[0];
        EXPECT_EQ(record.attributionStatus, original.attributionStatus);
        EXPECT_EQ(record.reason, original.reason);
        EXPECT_EQ(record.freeDuration, original.freeDuration);
        EXPECT_FALSE(record.hostApiTarget.has_value());
        EXPECT_EQ(CandidateIds(record), CandidateIds(original));
    }
}

TEST(FreeAttributionTest, SearchFiltersDetailsBeforePaginationAndKeepsRangeSelection) {
    const std::vector<FreeAttributionRecord> records = {
        WithCandidates("1", {Target("a", "aten::needle")}, false, 100, 10),
        WithCandidates("2", {Target("b", "aten::other")}, false, 200, 10),
        WithCandidates("3", {Target("c", "aten::needle.scope")}, false, 300, 10),
        WithCandidates("4", {Target("d", "aten::needle")}, false, 400, 10)};
    auto params = Params();
    params.groupId = "status:incomplete";
    params.searchName = "needle";
    params.orderBy = "freeStartTime";
    params.order = "asc";
    params.current = 2;
    params.pageSize = 1;
    params.rangeMode = "range";
    params.startTime = 0;
    params.endTime = 350;
    FreeAttributionDetails details;
    FreeAttributionService::SelectDetails(records, params, details);
    EXPECT_EQ(details.totalRecords, 2U);
    EXPECT_EQ(FreeIds(details), (std::vector<std::string>{"3"}));

    params.searchName = "missing";
    FreeAttributionService::SelectDetails(records, params, details);
    EXPECT_EQ(details.totalRecords, 0U);
    EXPECT_TRUE(details.records.empty());
}

TEST(FreeAttributionTest, RangeSelectsWholeFreeAndKeepsEvidenceOutsideSelection) {
    auto params = Params();
    params.startTime = 150;
    params.endTime = 160;
    const auto record = Matched("1", "aten::add", 100, 80);
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize({record, Matched("2", "aten::other", 200, 10)}, params, summary);
    EXPECT_EQ(summary.totalFreeCount, 1U);
    EXPECT_EQ(summary.totalFreeDuration, 80U);
    ASSERT_EQ(summary.groups.size(), 1U);
    params.groupId = summary.groups[0].groupId;
    FreeAttributionDetails details;
    FreeAttributionService::SelectDetails({record}, params, details);
    ASSERT_EQ(details.records.size(), 1U);
    ASSERT_TRUE(details.records[0].hostApiTarget.has_value());
    EXPECT_LT(details.records[0].hostApiTarget->timestamp, params.startTime);
    EXPECT_GT(details.records[0].freeTarget.timestamp + details.records[0].freeDuration, params.endTime);
}

TEST(FreeAttributionTest, InclusiveEndpointsAndEqualBoundsFollowExistingRangeConvention) {
    const std::vector<FreeAttributionRecord> records = {
        Matched("1", "a", 100, 50), Matched("2", "b", 200, 30), Matched("3", "c", 300, 40)};
    auto params = Params();
    params.startTime = 150;
    params.endTime = 200;
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize(records, params, summary);
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 80U);
    params.startTime = params.endTime;
    FreeAttributionService::Summarize(records, params, summary);
    EXPECT_EQ(summary.totalFreeCount, 3U);
    EXPECT_EQ(summary.totalFreeDuration, 120U);
}

TEST(FreeAttributionTest, ExplicitAllModeIgnoresBoundsForSummaryAndDetails) {
    const std::vector<FreeAttributionRecord> records = {
        Matched("1", "aten::add", 0, 20), Matched("2", "aten::add", 100, 30)};
    auto params = Params();
    params.rangeMode = "all";
    params.startTime = 500;
    params.endTime = 600;
    params.groupId = "matched:aten::add";
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize(records, params, summary);
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 50U);
    FreeAttributionDetails details;
    FreeAttributionService::SelectDetails(records, params, details);
    EXPECT_EQ(details.totalRecords, 2U);
}

TEST(FreeAttributionTest, ExplicitRangeAtZeroIsAPointInsteadOfAllEvents) {
    const std::vector<FreeAttributionRecord> records = {
        Matched("1", "aten::add", 0, 20), Matched("2", "aten::add", 100, 30)};
    auto params = Params();
    params.rangeMode = "range";
    params.groupId = "matched:aten::add";
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize(records, params, summary);
    EXPECT_EQ(summary.totalFreeCount, 1U);
    EXPECT_EQ(summary.totalFreeDuration, 20U);
    FreeAttributionDetails details;
    FreeAttributionService::SelectDetails(records, params, details);
    ASSERT_EQ(details.totalRecords, 1U);
    ASSERT_EQ(details.records.size(), 1U);
    EXPECT_EQ(details.records[0].freeTarget.id, "1");

    params.startTime = params.endTime = 50;
    FreeAttributionService::Summarize(records, params, summary);
    EXPECT_EQ(summary.totalFreeCount, 0U);
    FreeAttributionService::SelectDetails(records, params, details);
    EXPECT_EQ(details.totalRecords, 0U);
}

TEST(FreeAttributionTest, ExplicitPointRangeIncludesBothTouchingEndpoints) {
    const std::vector<FreeAttributionRecord> records = {
        Matched("1", "aten::add", 0, 20), Matched("2", "aten::add", 20, 10), Matched("3", "aten::add", 100, 30)};
    auto params = Params();
    params.rangeMode = "range";
    params.startTime = params.endTime = 20;
    params.groupId = "matched:aten::add";
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize(records, params, summary);
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 30U);
    FreeAttributionDetails details;
    FreeAttributionService::SelectDetails(records, params, details);
    EXPECT_EQ(details.totalRecords, 2U);
}

TEST(FreeAttributionTest, ExplicitEmptyModeClearsPreviouslyPopulatedSummaryAndDetails) {
    const std::vector<FreeAttributionRecord> records = {
        Matched("1", "aten::add", 0, 20), Matched("2", "aten::add", 100, 30)};
    auto params = Params();
    params.groupId = "matched:aten::add";
    FreeAttributionSummary summary;
    FreeAttributionDetails details;
    FreeAttributionService::Summarize(records, params, summary);
    FreeAttributionService::SelectDetails(records, params, details);
    ASSERT_EQ(summary.totalFreeCount, 2U);
    ASSERT_EQ(details.totalRecords, 2U);
    params.rangeMode = "empty";
    FreeAttributionService::Summarize(records, params, summary);
    FreeAttributionService::SelectDetails(records, params, details);
    EXPECT_TRUE(summary.groups.empty());
    EXPECT_EQ(summary.totalGroups, 0U);
    EXPECT_EQ(summary.totalFreeCount, 0U);
    EXPECT_EQ(summary.totalFreeDuration, 0U);
    EXPECT_TRUE(details.records.empty());
    EXPECT_EQ(details.totalRecords, 0U);
}

TEST(FreeAttributionTest, SourceIdentityDeduplicatesSameFreeButPreservesCrossSourceIds) {
    const auto first = Matched("1", "aten::add", 100, 30, "source-a");
    const auto second = Matched("1", "aten::add", 200, 20, "source-b");
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize({first, first, second}, Params(), summary);
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 50U);
    ASSERT_EQ(summary.groups.size(), 1U);
    auto params = Params();
    params.groupId = summary.groups[0].groupId;
    FreeAttributionDetails details;
    FreeAttributionService::SelectDetails({first, first, second}, params, details);
    EXPECT_EQ(details.totalRecords, 2U);
}

TEST(FreeAttributionTest, SummarySortsAllGroupsBeforePaginationAndKeepsGlobalTotals) {
    const std::vector<FreeAttributionRecord> records = {
        Matched("1", "small", 100, 10), Matched("2", "largest", 200, 90), Matched("3", "middle", 300, 40)};
    auto params = Params();
    params.pageSize = 1;
    params.current = 2;
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize(records, params, summary);
    ASSERT_EQ(summary.groups.size(), 1U);
    EXPECT_EQ(summary.groups[0].apiName, "middle");
    EXPECT_EQ(summary.totalGroups, 3U);
    EXPECT_EQ(summary.totalFreeCount, 3U);
    EXPECT_EQ(summary.totalFreeDuration, 140U);
}

TEST(FreeAttributionTest, DetailsAreGroupScopedSortedAndPagedWithoutLosingTotal) {
    const std::vector<FreeAttributionRecord> records = {Matched("1", "aten::add", 300, 10),
        Matched("2", "aten::add", 100, 30), Matched("3", "aten::add", 200, 20), Matched("4", "other", 150, 80)};
    auto params = Params();
    params.groupId = "matched:aten::add";
    params.orderBy = "freeStartTime";
    params.order = "asc";
    params.pageSize = 1;
    params.current = 2;
    FreeAttributionDetails details;
    FreeAttributionService::SelectDetails(records, params, details);
    EXPECT_EQ(details.totalRecords, 3U);
    ASSERT_EQ(details.records.size(), 1U);
    EXPECT_EQ(details.records[0].freeTarget.timestamp, 200U);
    params.current = 9;
    FreeAttributionService::SelectDetails(records, params, details);
    EXPECT_EQ(details.totalRecords, 3U);
    EXPECT_TRUE(details.records.empty());
}

TEST(FreeAttributionTest, IncompleteDetailsSortSingleCandidateValuesInBothDirections) {
    const std::vector<FreeAttributionRecord> records = {WithCandidates("1", {Target("api-1", "zeta", 30, 2)}),
        WithCandidates("2", {Target("api-2", "alpha", 2, 100)}),
        WithCandidates("3", {Target("api-3", "middle", 100, 30)})};
    struct SortCase {
        std::string field;
        std::vector<std::string> ascendingIds;
    };
    const std::vector<SortCase> cases = {
        {"apiName", {"2", "3", "1"}}, {"apiStartTime", {"2", "1", "3"}}, {"apiDuration", {"1", "3", "2"}}};
    auto params = Params();
    params.groupId = "status:incomplete";
    for (const auto &test : cases) {
        SCOPED_TRACE(test.field);
        params.orderBy = test.field;
        params.order = "asc";
        FreeAttributionDetails details;
        FreeAttributionService::SelectDetails(records, params, details);
        EXPECT_EQ(FreeIds(details), test.ascendingIds);
        params.order = "desc";
        FreeAttributionService::SelectDetails(records, params, details);
        auto descendingIds = test.ascendingIds;
        std::reverse(descendingIds.begin(), descendingIds.end());
        EXPECT_EQ(FreeIds(details), descendingIds);
        EXPECT_EQ(details.totalRecords, 3U);
        for (const auto &record : details.records) {
            EXPECT_EQ(record.attributionStatus, "incomplete");
            EXPECT_FALSE(record.hostApiTarget.has_value());
            EXPECT_EQ(record.candidateApiTargets.size(), 1U);
        }
    }
}

TEST(FreeAttributionTest, CandidateCollectionsSortIndependentlyOfInputOrderAndKeepPaginationStable) {
    const std::vector<FreeAttributionTarget> first = {
        Target("zeta", "zeta", 90, 900), Target("alpha", "alpha", 10, 100)};
    const std::vector<FreeAttributionTarget> second = {
        Target("beta", "beta", 100, 200), Target("alpha", "alpha", 10, 100)};
    std::vector<FreeAttributionRecord> records = {WithCandidates("4", first), WithCandidates("2", second),
        WithCandidates("1", first), WithCandidates("3", {Target("alpha", "alpha", 10, 100)})};
    struct SortCase {
        std::string field;
        std::vector<std::string> ascendingIds;
        std::vector<std::string> descendingIds;
    };
    const std::vector<SortCase> cases = {{"apiName", {"3", "2", "1", "4"}, {"1", "4", "2", "3"}},
        {"apiStartTime", {"3", "1", "4", "2"}, {"2", "1", "4", "3"}},
        {"apiDuration", {"3", "2", "1", "4"}, {"1", "4", "2", "3"}}};
    auto params = Params();
    params.groupId = "status:incomplete";
    for (int permutation = 0; permutation < 2; ++permutation) {
        for (const auto &test : cases) {
            for (const auto &order : {std::string("asc"), std::string("desc")}) {
                SCOPED_TRACE(test.field + "/" + order + "/" + std::to_string(permutation));
                params.orderBy = test.field;
                params.order = order;
                params.current = 1;
                params.pageSize = 100;
                const auto &expected = order == "asc" ? test.ascendingIds : test.descendingIds;
                FreeAttributionDetails details;
                FreeAttributionService::SelectDetails(records, params, details);
                EXPECT_EQ(FreeIds(details), expected);
                for (const auto &record : details.records) {
                    const auto source = std::find_if(records.begin(), records.end(),
                        [&](const auto &item) { return item.detailId == record.detailId; });
                    ASSERT_NE(source, records.end());
                    EXPECT_EQ(CandidateIds(record), CandidateIds(*source));
                    EXPECT_EQ(record.attributionStatus, "incomplete");
                    EXPECT_FALSE(record.hostApiTarget.has_value());
                }
                params.pageSize = 1;
                for (size_t index = 0; index < expected.size(); ++index) {
                    params.current = index + 1;
                    FreeAttributionService::SelectDetails(records, params, details);
                    EXPECT_EQ(details.totalRecords, 4U);
                    ASSERT_EQ(details.records.size(), 1U);
                    EXPECT_EQ(details.records[0].freeTarget.id, expected[index]);
                }
            }
        }
        std::reverse(records.begin(), records.end());
        for (auto &record : records) {
            std::reverse(record.candidateApiTargets.begin(), record.candidateApiTargets.end());
        }
    }
}

TEST(FreeAttributionTest, AmbiguousDetailsCompareEveryCandidateWithoutChangingAttributionOrTotals) {
    const auto first = WithCandidates("1", {Target("a", "alpha", 10, 100), Target("z", "zeta", 90, 900)}, true);
    const auto second = WithCandidates("2", {Target("a", "alpha", 10, 100), Target("b", "beta", 100, 200)}, true);
    const std::vector<FreeAttributionRecord> records = {first, second};
    auto params = Params();
    params.groupId = "status:ambiguous";
    for (const auto &field : {std::string("apiName"), std::string("apiStartTime"), std::string("apiDuration")}) {
        params.orderBy = field;
        for (const auto &order : {std::string("asc"), std::string("desc")}) {
            SCOPED_TRACE(field + "/" + order);
            params.order = order;
            FreeAttributionDetails details;
            FreeAttributionService::SelectDetails(records, params, details);
            const bool firstComesFirst = (field == "apiStartTime") == (order == "asc");
            EXPECT_EQ(FreeIds(details),
                (firstComesFirst ? std::vector<std::string>{"1", "2"} : std::vector<std::string>{"2", "1"}));
            for (const auto &record : details.records) {
                EXPECT_EQ(record.attributionStatus, "ambiguous");
                EXPECT_FALSE(record.hostApiTarget.has_value());
                EXPECT_EQ(record.candidateApiTargets.size(), 2U);
            }
        }
    }
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize(records, Params(), summary);
    ASSERT_EQ(summary.groups.size(), 1U);
    EXPECT_EQ(summary.groups[0].groupType, "ambiguous");
    EXPECT_EQ(summary.groups[0].freeCount, 2U);
    EXPECT_EQ(summary.totalFreeCount, 2U);
    EXPECT_EQ(summary.totalFreeDuration, 40U);
}

TEST(FreeAttributionTest, MatchedApiSortingUsesTheHostTargetBeforeCandidateFallback) {
    auto first = Matched("1", "aten::add", 100, 20);
    auto second = Matched("2", "aten::add", 200, 30);
    first.hostApiTarget->timestamp = 10;
    first.hostApiTarget->duration = 100;
    second.hostApiTarget->timestamp = 100;
    second.hostApiTarget->duration = 10;
    first.candidateApiTargets = {Target("ignored-1", "zeta", 1000, 1)};
    second.candidateApiTargets = {Target("ignored-2", "alpha", 1, 1000)};
    const std::vector<FreeAttributionRecord> records = {second, first};
    auto params = Params();
    params.groupId = "matched:aten::add";
    for (const auto &field : {std::string("apiName"), std::string("apiStartTime"), std::string("apiDuration")}) {
        params.orderBy = field;
        for (const auto &order : {std::string("asc"), std::string("desc")}) {
            SCOPED_TRACE(field + "/" + order);
            params.order = order;
            FreeAttributionDetails details;
            FreeAttributionService::SelectDetails(records, params, details);
            const bool firstComesFirst = field == "apiName" || ((field == "apiStartTime") == (order == "asc"));
            EXPECT_EQ(FreeIds(details),
                (firstComesFirst ? std::vector<std::string>{"1", "2"} : std::vector<std::string>{"2", "1"}));
        }
    }
}

TEST(FreeAttributionTest, CandidateApiSortingDoesNotReplaceFreeSortingOrChangeSummaryStatistics) {
    const std::vector<FreeAttributionRecord> records = {
        WithCandidates("1", {Target("api-1", "zeta", 30, 2)}, false, 100, 30),
        WithCandidates("2", {Target("api-2", "alpha", 2, 100)}, false, 300, 10),
        WithCandidates("3", {Target("api-3", "middle", 100, 30)}, false, 200, 20)};
    auto params = Params();
    params.groupId = "status:incomplete";
    for (const auto &field : {std::string("freeStartTime"), std::string("freeDuration")}) {
        params.orderBy = field;
        for (const auto &order : {std::string("asc"), std::string("desc")}) {
            SCOPED_TRACE(field + "/" + order);
            params.order = order;
            FreeAttributionDetails details;
            FreeAttributionService::SelectDetails(records, params, details);
            auto expected = field == "freeStartTime" ? std::vector<std::string>{"1", "3", "2"}
                                                     : std::vector<std::string>{"2", "3", "1"};
            if (order == "desc") {
                std::reverse(expected.begin(), expected.end());
            }
            EXPECT_EQ(FreeIds(details), expected);
        }
    }
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize(records, Params(), summary);
    ASSERT_EQ(summary.groups.size(), 1U);
    EXPECT_EQ(summary.groups[0].groupType, "incomplete");
    EXPECT_EQ(summary.groups[0].freeCount, 3U);
    EXPECT_EQ(summary.groups[0].totalFreeDuration, 60U);
    EXPECT_EQ(summary.totalFreeCount, 3U);
    EXPECT_EQ(summary.totalFreeDuration, 60U);
}

TEST(FreeAttributionTest, AllStatusGroupsConserveFreeDurationAndFrequency) {
    const auto free = Target("3", "Free", 300, 30);
    FreeAttributionEvidence missing;
    const std::vector<FreeAttributionRecord> records = {Matched("1", "aten::add", 100, 10),
        FreeAttributionService::AttributeFree(Target("2", "Free", 200, 20), "", {}, "noNextBehavior"),
        FreeAttributionService::AttributeFree(free, "Computing", {Evidence({Target("a", "x")}), missing}),
        FreeAttributionService::AttributeFree(
            Target("4", "Free", 400, 40), "Computing", {Evidence({Target("a", "x"), Target("b", "y")})})};
    FreeAttributionSummary summary;
    FreeAttributionService::Summarize(records, Params(), summary);
    ASSERT_EQ(summary.groups.size(), 4U);
    const auto duration = std::accumulate(summary.groups.begin(), summary.groups.end(), uint64_t{0},
        [](uint64_t total, const auto &group) { return total + group.totalFreeDuration; });
    const auto count = std::accumulate(summary.groups.begin(), summary.groups.end(), uint64_t{0},
        [](uint64_t total, const auto &group) { return total + group.freeCount; });
    EXPECT_EQ(duration, 100U);
    EXPECT_EQ(count, 4U);
    EXPECT_EQ(summary.totalFreeDuration, duration);
    EXPECT_EQ(summary.totalFreeCount, count);
    for (const auto &group : summary.groups) {
        auto params = Params();
        params.groupId = group.groupId;
        FreeAttributionDetails details;
        FreeAttributionService::SelectDetails(records, params, details);
        ASSERT_EQ(details.records.size(), 1U);
        EXPECT_EQ(details.records[0].freeDuration, group.totalFreeDuration);
    }
}
} // namespace
} // namespace Dic::Module::Timeline
