/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy at http://license.coscl.org.cn/MulanPSL2
 */
#include <gtest/gtest.h>
#include <limits>
#include "PmuCounterCalculator.h"

using namespace Dic::Module::Timeline;

namespace {
PmuCounterSample Sample(int64_t timestamp, int64_t raw, int64_t enabled, int64_t running, int valid = 1) {
    return {timestamp, valid, raw, enabled, running};
}

PmuCounterResult Calculate(
    const std::vector<PmuCounterSample> &samples, uint64_t left = 0, uint64_t right = 1000, uint32_t buckets = 1024) {
    PmuCounterInfo counter;
    counter.counterId = 1;
    counter.unit = "count";
    PmuCounterCapabilities capabilities;
    capabilities.available = true;
    capabilities.hasRawValues = true;
    return PmuCounterCalculator::Calculate(samples, counter, capabilities, {left, right, 0, buckets});
}
}

TEST(PmuCounterCalculatorTest, ScalesRawIntervalInsteadOfSubtractingScaledTotals) {
    const auto result = Calculate({Sample(100, 100, 100, 50), Sample(200, 110, 200, 150)});
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_EQ(result.status, "ok");
    EXPECT_DOUBLE_EQ(*result.segments.front().value, 10);
}

TEST(PmuCounterCalculatorTest, PreservesValidZeroAndActualMeasurementBoundaries) {
    const auto result = Calculate({Sample(100, 10, 10, 10), Sample(300, 30, 210, 210), Sample(600, 30, 510, 510)});
    ASSERT_EQ(result.segments.size(), 2u);
    EXPECT_DOUBLE_EQ(*result.segments[0].value, 20);
    EXPECT_DOUBLE_EQ(*result.segments[1].value, 0);
    EXPECT_EQ(result.segments[1].startTime, 300u);
    EXPECT_EQ(result.segments[1].endTime, 600u);
}

TEST(PmuCounterCalculatorTest, InvalidReadBreaksBothAdjacentIntervalsAndThenRecovers) {
    const auto result = Calculate(
        {Sample(100, 10, 10, 10), Sample(200, 20, 20, 20, 0), Sample(300, 30, 30, 30), Sample(400, 40, 40, 40)});
    ASSERT_EQ(result.segments.size(), 3u);
    EXPECT_FALSE(result.segments[0].value);
    EXPECT_FALSE(result.segments[1].value);
    EXPECT_DOUBLE_EQ(*result.segments[2].value, 10);
}

TEST(PmuCounterCalculatorTest, NonRunningAndInvalidEnabledIntervalsAreNotFalseZeros) {
    for (const auto &right : {Sample(200, 10, 20, 10), Sample(200, 10, 10, 10), Sample(200, 20, 20, 30)}) {
        const auto result = Calculate({Sample(100, 10, 10, 10), right});
        ASSERT_EQ(result.segments.size(), 1u);
        EXPECT_EQ(result.status, "invalid-data");
        EXPECT_FALSE(result.segments[0].value);
    }
}

TEST(PmuCounterCalculatorTest, MissingRawValueAndCounterResetRemainBarriers) {
    auto bad = Sample(200, 20, 20, 20);
    bad.rawValue.reset();
    const auto result = Calculate(
        {Sample(100, 10, 10, 10), bad, Sample(300, 30, 30, 30), Sample(400, 5, 5, 5), Sample(500, 15, 15, 15)});
    ASSERT_EQ(result.segments.size(), 4u);
    EXPECT_FALSE(result.segments[0].value);
    EXPECT_FALSE(result.segments[1].value);
    EXPECT_FALSE(result.segments[2].value);
    EXPECT_DOUBLE_EQ(*result.segments[3].value, 10);
}

TEST(PmuCounterCalculatorTest, SubtractsLargeIntegersBeforeConvertingToDouble) {
    constexpr int64_t large = 9007199254740993LL;
    auto result = Calculate({Sample(100, large, 100, 100), Sample(200, large + 1, 200, 200)});
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_DOUBLE_EQ(*result.segments[0].value, 1);
    result = Calculate({Sample(100, 0, 0, 0), Sample(200, 1000000000000000000LL, 1000, 1)});
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_DOUBLE_EQ(*result.segments[0].value, 1e21);
}

TEST(PmuCounterCalculatorTest, DuplicateTimestampConflictsProduceAGap) {
    const auto first = Sample(100, 10, 10, 10);
    auto result = Calculate({first, first, Sample(200, 20, 20, 20)});
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_DOUBLE_EQ(*result.segments[0].value, 10);
    result = Calculate({first, Sample(100, 11, 10, 10), Sample(200, 20, 20, 20), Sample(300, 30, 30, 30)});
    ASSERT_EQ(result.segments.size(), 2u);
    EXPECT_FALSE(result.segments[0].value);
    EXPECT_DOUBLE_EQ(*result.segments[1].value, 10);
}

TEST(PmuCounterCalculatorTest, FiltersPureContextBeforeAggregatingWholeIntervals) {
    auto result = Calculate({Sample(0, 0, 0, 0), Sample(100, 10, 10, 10), Sample(200, 30, 30, 30),
                                Sample(300, 60, 60, 60), Sample(400, 100, 100, 100)},
        100, 300, 1);
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_EQ(result.segments[0].startTime, 100u);
    EXPECT_EQ(result.segments[0].endTime, 300u);
    EXPECT_DOUBLE_EQ(*result.segments[0].value, 50);
    result = Calculate({Sample(100, 10, 10, 10), Sample(300, 30, 30, 30)}, 150, 250);
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_EQ(result.segments[0].startTime, 100u);
    EXPECT_EQ(result.segments[0].endTime, 300u);
    EXPECT_DOUBLE_EQ(*result.segments[0].value, 20);
}

TEST(PmuCounterCalculatorTest, SinglePointAndTailHaveNoInventedMeasurement) {
    for (const auto &range : {std::pair<uint64_t, uint64_t>{0, 1000}, {200, 300}, {100, 100}}) {
        const auto result = Calculate({Sample(100, 10, 10, 10)}, range.first, range.second);
        EXPECT_EQ(result.status, "empty");
        EXPECT_TRUE(result.segments.empty());
    }
}

TEST(PmuCounterCalculatorTest, AggregatesAfterScalingEachOriginalInterval) {
    const auto result =
        Calculate({Sample(100, 0, 0, 0), Sample(200, 10, 100, 50), Sample(500, 20, 400, 350)}, 100, 500, 1);
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_DOUBLE_EQ(*result.segments[0].value, 30);
}

TEST(PmuCounterCalculatorTest, AggregationCannotBridgeAnInvalidRead) {
    const auto result = Calculate({Sample(100, 10, 10, 10), Sample(200, 20, 20, 20), Sample(300, 30, 30, 30, 0),
                                      Sample(400, 40, 40, 40), Sample(500, 50, 50, 50)},
        0, 600, 1);
    ASSERT_EQ(result.segments.size(), 3u);
    EXPECT_DOUBLE_EQ(*result.segments[0].value, 10);
    EXPECT_FALSE(result.segments[1].value);
    EXPECT_DOUBLE_EQ(*result.segments[2].value, 10);
}

TEST(PmuCounterCalculatorTest, DenseInvalidReadsKeepOutputBoundedWithoutFalseZeros) {
    std::vector<PmuCounterSample> samples;
    for (int64_t i = 0; i < 100; ++i) {
        samples.push_back(Sample(i * 10, i, i, i, i % 3 == 2 ? 0 : 1));
    }
    const auto result = Calculate(samples, 0, 1000, 1);
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_FALSE(result.segments[0].value);
}

TEST(PmuCounterCalculatorTest, MissingOriginalCountersAreUnsupported) {
    PmuCounterInfo counter;
    PmuCounterCapabilities capabilities;
    capabilities.available = true;
    const auto result = PmuCounterCalculator::Calculate({Sample(100, 10, 10, 10)}, counter, capabilities, {});
    EXPECT_EQ(result.status, "unsupported");
    EXPECT_TRUE(result.segments.empty());
}

TEST(PmuCounterCalculatorTest, UsesEpochOriginAndRejectsUnsafeOrUnorderedTimestamps) {
    constexpr uint64_t origin = 1795056377483680000ULL;
    PmuCounterInfo counter;
    counter.unit = "ns";
    PmuCounterCapabilities capabilities;
    capabilities.available = true;
    capabilities.hasRawValues = true;
    const auto result = PmuCounterCalculator::Calculate({Sample(static_cast<int64_t>(origin + 100), 10, 10, 10),
                                                            Sample(static_cast<int64_t>(origin + 200), 20, 20, 20)},
        counter, capabilities, {0, 1000, origin, 100});
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_EQ(result.nativeUnit, "ns");
    EXPECT_EQ(result.segments[0].startTime, 100u);
    EXPECT_EQ(result.segments[0].endTime, 200u);
    for (const auto &samples :
        {std::vector<PmuCounterSample>{Sample(100, 10, 10, 10), Sample(9007199254740992LL, 20, 20, 20)},
            {Sample(200, 10, 10, 10), Sample(100, 20, 20, 20)}}) {
        const auto invalid = Calculate(samples);
        EXPECT_EQ(invalid.status, "invalid-data");
        EXPECT_TRUE(invalid.segments.empty());
    }
}

TEST(PmuCounterCalculatorTest, RejectsRunningDeltaLargerThanEnabledDespiteValidCumulativeTimes) {
    // Each reading has running <= enabled, but this interval cannot have run for 20 out of 10 ns.
    const auto result = Calculate({Sample(100, 10, 100, 20), Sample(200, 20, 110, 40)});
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_EQ(result.status, "invalid-data");
    EXPECT_FALSE(result.segments[0].value);
}

TEST(PmuCounterCalculatorTest, EachCumulativeCounterRollbackCreatesAGapAndANewBaseline) {
    const auto first = Sample(100, 100, 100, 80);
    const std::vector<std::pair<PmuCounterSample, PmuCounterSample>> cases{
        {first, Sample(200, 99, 200, 180)}, // Raw count reset.
        {first, Sample(200, 110, 99, 90)}, // Enabled time reset, with individually valid clock readings.
        {first, Sample(200, 110, 200, 79)}, // Running time reset.
        {Sample(100, std::numeric_limits<int64_t>::max() - 5, 100, 80), Sample(200, 5, 200, 180)}, // Wraparound.
    };
    for (size_t i = 0; i < cases.size(); ++i) {
        SCOPED_TRACE(i);
        const auto &before = cases[i].first;
        const auto &reset = cases[i].second;
        const auto after = Sample(300, *reset.rawValue + 10, *reset.timeEnabled + 100, *reset.timeRunning + 100);
        const auto result = Calculate({before, reset, after});
        ASSERT_EQ(result.segments.size(), 2u);
        EXPECT_FALSE(result.segments[0].value);
        ASSERT_TRUE(result.segments[1].value);
        EXPECT_DOUBLE_EQ(*result.segments[1].value, 10);
        EXPECT_EQ(result.segments[1].startTime, 200u);
        EXPECT_EQ(result.segments[1].endTime, 300u);
    }
}

TEST(PmuCounterCalculatorTest, EightRunsRemainDistinctAndNinthCollapsesTheWholeBucket) {
    std::vector<PmuCounterSample> samples;
    for (int64_t i = 0; i <= 12; ++i) {
        samples.push_back(Sample(i * 10, i, i, i, i % 3 == 2 ? 0 : 1));
    }
    const auto atLimit = Calculate(samples, 0, 200, 1);
    ASSERT_EQ(atLimit.segments.size(), 8u);
    for (size_t i = 0; i < atLimit.segments.size(); ++i) {
        SCOPED_TRACE(i);
        EXPECT_EQ(atLimit.segments[i].startTime, (i / 2) * 30 + (i % 2) * 10);
        EXPECT_EQ(atLimit.segments[i].endTime, (i / 2) * 30 + (i % 2 == 0 ? 10 : 30));
        EXPECT_EQ(atLimit.segments[i].value.has_value(), i % 2 == 0);
        if (atLimit.segments[i].value) {
            EXPECT_DOUBLE_EQ(*atLimit.segments[i].value, 1);
        }
    }
    samples.push_back(Sample(130, 13, 13, 13));
    const auto aboveLimit = Calculate(samples, 0, 200, 1);
    ASSERT_EQ(aboveLimit.segments.size(), 1u);
    EXPECT_FALSE(aboveLimit.segments[0].value);
    EXPECT_EQ(aboveLimit.segments[0].startTime, 0u);
    EXPECT_EQ(aboveLimit.segments[0].endTime, 130u);
    // Once collapsed, later valid readings must extend the gap without recreating a false valid run.
    samples.push_back(Sample(140, 14, 14, 14));
    const auto extended = Calculate(samples, 0, 200, 1);
    ASSERT_EQ(extended.segments.size(), 1u);
    EXPECT_FALSE(extended.segments[0].value);
    EXPECT_EQ(extended.segments[0].endTime, 140u);
}

TEST(PmuCounterCalculatorTest, ZeroTargetBucketsClampsToOneAndPreservesTheScaledSum) {
    const auto result =
        Calculate({Sample(100, 0, 0, 0), Sample(200, 10, 100, 50), Sample(500, 20, 400, 350)}, 100, 500, 0);
    EXPECT_EQ(result.status, "ok");
    ASSERT_EQ(result.segments.size(), 1u);
    EXPECT_EQ(result.segments[0].startTime, 100u);
    EXPECT_EQ(result.segments[0].endTime, 500u);
    ASSERT_TRUE(result.segments[0].value);
    EXPECT_DOUBLE_EQ(*result.segments[0].value, 30);
}

TEST(PmuCounterCalculatorTest, WindowTailEndsAtLastReadAndExclusiveContextDoesNotCreateASegment) {
    const std::vector<PmuCounterSample> samples{
        Sample(100, 10, 10, 10), Sample(200, 20, 20, 20), Sample(300, 40, 40, 40)};
    const auto result = Calculate(samples, 150, 500);
    ASSERT_EQ(result.segments.size(), 2u);
    EXPECT_EQ(result.segments.front().startTime, 100u);
    EXPECT_EQ(result.segments.back().endTime, 300u);
    EXPECT_DOUBLE_EQ(*result.segments[0].value, 10);
    EXPECT_DOUBLE_EQ(*result.segments[1].value, 20);
    for (const auto &range : {std::pair<uint64_t, uint64_t>{0, 100}, {300, 500}}) {
        const auto outside = Calculate(samples, range.first, range.second);
        EXPECT_EQ(outside.status, "empty");
        EXPECT_TRUE(outside.segments.empty());
    }
    const auto boundary = Calculate(samples, 200, 300);
    ASSERT_EQ(boundary.segments.size(), 1u);
    EXPECT_EQ(boundary.segments[0].startTime, 200u);
    EXPECT_EQ(boundary.segments[0].endTime, 300u);
    EXPECT_DOUBLE_EQ(*boundary.segments[0].value, 20);
}

TEST(PmuCounterCalculatorTest, InvalidTimelineDiscardsAlreadyAccumulatedValidSegments) {
    for (const auto &bad : {Sample(150, 30, 30, 30), Sample(9007199254740992LL, 30, 30, 30)}) {
        const auto result = Calculate({Sample(100, 10, 10, 10), Sample(200, 20, 20, 20), bad});
        EXPECT_EQ(result.status, "invalid-data");
        EXPECT_TRUE(result.segments.empty());
    }
}

TEST(PmuCounterCalculatorTest, InvalidQueryBoundsAndEpochOverflowAreRejected) {
    const auto maxEpoch = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    const std::vector<PmuCounterQuery> queries{
        {200, 100, 0, 1},
        {0, 9007199254740992ULL, 0, 1},
        {0, 1, maxEpoch + 1, 1},
        {0, 2, maxEpoch - 1, 1},
    };
    PmuCounterCapabilities capabilities;
    capabilities.available = true;
    capabilities.hasRawValues = true;
    for (const auto &query : queries) {
        const auto result = PmuCounterCalculator::Calculate(
            {Sample(100, 10, 10, 10), Sample(200, 20, 20, 20)}, {}, capabilities, query);
        EXPECT_EQ(result.status, "invalid-data");
        EXPECT_TRUE(result.segments.empty());
    }
}
