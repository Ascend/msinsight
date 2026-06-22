#include <gtest/gtest.h>
#include "RankOffsetCalculator.h"

using namespace Dic::Module::Timeline;

TEST(RankOffsetCalculatorTest, ClassifiesDeviceSideMetaTypes) {
    EXPECT_EQ(RankOffsetCalculator::GetSide("Ascend Hardware"), RankOffsetSide::DEVICE);
    EXPECT_EQ(RankOffsetCalculator::GetSide("HCCL"), RankOffsetSide::DEVICE);
    EXPECT_EQ(RankOffsetCalculator::GetSide("OVERLAP_ANALYSIS"), RankOffsetSide::DEVICE);
}

TEST(RankOffsetCalculatorTest, ClassifiesHostSideMetaTypes) {
    EXPECT_EQ(RankOffsetCalculator::GetSide("CANN_API"), RankOffsetSide::HOST);
    EXPECT_EQ(RankOffsetCalculator::GetSide("PYTORCH_API"), RankOffsetSide::HOST);
    EXPECT_EQ(RankOffsetCalculator::GetSide("PYTORCH_API_PYTHON_STACK"), RankOffsetSide::HOST);
    EXPECT_EQ(RankOffsetCalculator::GetSide("OSRT_API"), RankOffsetSide::HOST);
    EXPECT_EQ(RankOffsetCalculator::GetSide("MSTX_EVENTS"), RankOffsetSide::HOST);
    EXPECT_EQ(RankOffsetCalculator::GetSide("TEXT"), RankOffsetSide::HOST);
}

TEST(RankOffsetCalculatorTest, RejectsUnsupportedMetaType) {
    EXPECT_EQ(RankOffsetCalculator::GetSide("UNKNOWN"), RankOffsetSide::UNSUPPORTED);
    EXPECT_EQ(RankOffsetCalculator::GetSide(""), RankOffsetSide::UNSUPPORTED);
}

TEST(RankOffsetCalculatorTest, ParsesAlignType) {
    EXPECT_EQ(RankOffsetCalculator::ParseAlignType("LEFT"), RankOffsetAlignType::LEFT);
    EXPECT_EQ(RankOffsetCalculator::ParseAlignType("RIGHT"), RankOffsetAlignType::RIGHT);
    EXPECT_EQ(RankOffsetCalculator::ParseAlignType("TOP"), RankOffsetAlignType::UNSUPPORTED);
    EXPECT_EQ(RankOffsetCalculator::ParseAlignType(""), RankOffsetAlignType::UNSUPPORTED);
}

TEST(RankOffsetCalculatorTest, IsSameSideReturnsTrueForMatchingSide) {
    EXPECT_TRUE(RankOffsetCalculator::IsSameSide("Ascend Hardware", RankOffsetSide::DEVICE));
    EXPECT_TRUE(RankOffsetCalculator::IsSameSide("HCCL", RankOffsetSide::DEVICE));
    EXPECT_TRUE(RankOffsetCalculator::IsSameSide("CANN_API", RankOffsetSide::HOST));
    EXPECT_TRUE(RankOffsetCalculator::IsSameSide("PYTORCH_API", RankOffsetSide::HOST));
    EXPECT_TRUE(RankOffsetCalculator::IsSameSide("PYTORCH_API_PYTHON_STACK", RankOffsetSide::HOST));
}

TEST(RankOffsetCalculatorTest, IsSameSideReturnsFalseForMismatchingSide) {
    EXPECT_FALSE(RankOffsetCalculator::IsSameSide("Ascend Hardware", RankOffsetSide::HOST));
    EXPECT_FALSE(RankOffsetCalculator::IsSameSide("CANN_API", RankOffsetSide::DEVICE));
    EXPECT_FALSE(RankOffsetCalculator::IsSameSide("UNKNOWN", RankOffsetSide::DEVICE));
    EXPECT_FALSE(RankOffsetCalculator::IsSameSide("UNKNOWN", RankOffsetSide::HOST));
    EXPECT_FALSE(RankOffsetCalculator::IsSameSide("Ascend Hardware", RankOffsetSide::UNSUPPORTED));
}

TEST(RankOffsetCalculatorTest, CalculatesLeftAndRightOffsets) {
    Dic::Protocol::SimpleSlice baseSlice;
    baseSlice.timestamp = 100;
    baseSlice.duration = 30;
    Dic::Protocol::SimpleSlice targetSlice;
    targetSlice.timestamp = 160;
    targetSlice.duration = 40;

    EXPECT_EQ(RankOffsetCalculator::CalculateOffset(baseSlice, targetSlice, RankOffsetAlignType::LEFT), 60);
    EXPECT_EQ(RankOffsetCalculator::CalculateOffset(baseSlice, targetSlice, RankOffsetAlignType::RIGHT), 70);
}

TEST(RankOffsetCalculatorTest, CalculatesNegativeOffsets) {
    Dic::Protocol::SimpleSlice baseSlice;
    baseSlice.timestamp = 200;
    baseSlice.duration = 50;
    Dic::Protocol::SimpleSlice targetSlice;
    targetSlice.timestamp = 150;
    targetSlice.duration = 30;

    EXPECT_EQ(RankOffsetCalculator::CalculateOffset(baseSlice, targetSlice, RankOffsetAlignType::LEFT), -50);
    EXPECT_EQ(RankOffsetCalculator::CalculateOffset(baseSlice, targetSlice, RankOffsetAlignType::RIGHT), -70);
}

TEST(RankOffsetCalculatorTest, CalculatesZeroOffset) {
    Dic::Protocol::SimpleSlice baseSlice;
    baseSlice.timestamp = 100;
    baseSlice.duration = 30;
    Dic::Protocol::SimpleSlice targetSlice;
    targetSlice.timestamp = 100;
    targetSlice.duration = 50;

    EXPECT_EQ(RankOffsetCalculator::CalculateOffset(baseSlice, targetSlice, RankOffsetAlignType::LEFT), 0);
    // Right: (100+50) - (100+30) = 20
    EXPECT_EQ(RankOffsetCalculator::CalculateOffset(baseSlice, targetSlice, RankOffsetAlignType::RIGHT), 20);
}

TEST(RankOffsetCalculatorTest, AnchorTimeReturnsStartTimeForLeft) {
    Dic::Protocol::SimpleSlice slice;
    slice.timestamp = 500;
    slice.duration = 100;
    EXPECT_EQ(RankOffsetCalculator::AnchorTime(slice, RankOffsetAlignType::LEFT), 500);
}

TEST(RankOffsetCalculatorTest, AnchorTimeReturnsEndTimeForRight) {
    Dic::Protocol::SimpleSlice slice;
    slice.timestamp = 500;
    slice.duration = 100;
    EXPECT_EQ(RankOffsetCalculator::AnchorTime(slice, RankOffsetAlignType::RIGHT), 600);
}

TEST(RankOffsetCalculatorTest, AnchorTimeWithZeroDuration) {
    Dic::Protocol::SimpleSlice slice;
    slice.timestamp = 500;
    slice.duration = 0;
    EXPECT_EQ(RankOffsetCalculator::AnchorTime(slice, RankOffsetAlignType::LEFT), 500);
    EXPECT_EQ(RankOffsetCalculator::AnchorTime(slice, RankOffsetAlignType::RIGHT), 500);
}

TEST(RankOffsetCalculatorTest, OffsetWithDifferentDurationsLeftAlign) {
    Dic::Protocol::SimpleSlice baseSlice;
    baseSlice.timestamp = 100;
    baseSlice.duration = 30;
    Dic::Protocol::SimpleSlice targetSlice;
    targetSlice.timestamp = 100;
    targetSlice.duration = 80;

    EXPECT_EQ(RankOffsetCalculator::CalculateOffset(baseSlice, targetSlice, RankOffsetAlignType::LEFT), 0);
}

TEST(RankOffsetCalculatorTest, OffsetWithDifferentDurationsRightAlign) {
    Dic::Protocol::SimpleSlice baseSlice;
    baseSlice.timestamp = 100;
    baseSlice.duration = 30;
    Dic::Protocol::SimpleSlice targetSlice;
    targetSlice.timestamp = 90;
    targetSlice.duration = 40;

    // base.end = 130, target.end = 130
    EXPECT_EQ(RankOffsetCalculator::CalculateOffset(baseSlice, targetSlice, RankOffsetAlignType::RIGHT), 0);
}
