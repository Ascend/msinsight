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
#include "../TestSuit.h"
#include "NumberSafeUtil.h"

using namespace Dic;
using namespace NumberSafe;

class NumberSafeUtilTest : public testing::Test {};

TEST_F(NumberSafeUtilTest, SafeCast) {
    // int -> uint
    const int32_t a = -10;
    bool suc = IsSafeCast<int32_t, uint32_t>(a);
    EXPECT_EQ(suc, false);
    const int test1 = 100;
    suc = IsSafeCast<int, uint32_t>(test1);
    EXPECT_TRUE(suc);
    suc = IsSafeCast<int, uint32_t>(std::numeric_limits<int>::max());
    EXPECT_TRUE(suc);
    suc = IsSafeCast<uint32_t, int>(std::numeric_limits<int>::min());
    EXPECT_FALSE(suc);
    // int -> double
    suc = IsSafeCast<int, double>(a);
    EXPECT_EQ(suc, true);
    suc = IsSafeCast<int, double>(std::numeric_limits<int>::min());
    EXPECT_TRUE(suc);
    // uint -> int
    uint32_t src = 100;
    suc = IsSafeCast<uint32_t, int32_t>(src);
    EXPECT_TRUE(suc);
    src = 0;
    suc = IsSafeCast<uint32_t, int32_t>(src);
    EXPECT_TRUE(suc);
    uint32_t b = std::numeric_limits<uint32_t>::max();
    suc = IsSafeCast<uint32_t, int32_t>(b);
    EXPECT_FALSE(suc);
    uint64_t c = std::numeric_limits<uint64_t>::max();
    suc = IsSafeCast<uint64_t, uint32_t>(c);
    EXPECT_FALSE(suc);
    uint64_t srcUint64_t = 10;
    suc = IsSafeCast<uint64_t, int>(srcUint64_t);
    EXPECT_TRUE(suc);
    // double -> int
    double srcDouble = 10.11;
    suc = IsSafeCast<double, uint32_t>(srcDouble);
    EXPECT_TRUE(suc);
}

TEST_F(NumberSafeUtilTest, FLIP) {
    int a = 10;
    a = Flip(a);
    const int expect = -10;
    EXPECT_EQ(a, expect);
    a = 0;
    EXPECT_EQ(0, Flip(a));
    int64_t b = 100;
    EXPECT_EQ(-1 * b, Flip(b));
}

TEST_F(NumberSafeUtilTest, AddTowIntUpZero) {
    // int + int
    const int left = 10;
    const int right = 11;
    int res = Add(right, left);
    const int expect = 21;
    EXPECT_EQ(res, expect);
}

TEST_F(NumberSafeUtilTest, AddTowIntDifferenSign) {
    const int left = -10;
    const int right = 5;
    const int res = Add(left, right);
    const int expect = -5;
    EXPECT_EQ(res, expect);
}

TEST_F(NumberSafeUtilTest, AddTowIntMax) {
    const int left = std::numeric_limits<int>::max();
    const int right = 10;
    const int expect = 0;
    EXPECT_EQ(Add(left, right), expect);
}

TEST_F(NumberSafeUtilTest, AddTowIntMin) {
    const int left = std::numeric_limits<int>::min();
    const int right = 10;
    EXPECT_EQ(Add(left, right), std::numeric_limits<int>::min() + right);
}

TEST_F(NumberSafeUtilTest, AddIntWithUint) {
    const int32_t left = 10;
    const uint32_t right = 100;
    const uint32_t expect = 110;
    EXPECT_EQ(Add(left, right), expect);
}

TEST_F(NumberSafeUtilTest, AddIntWithUintDiffSign) {
    const int left = -10;
    const int right = 10;
    const int expect1 = 0;
    EXPECT_EQ(Add(left, right), expect1);
    const uint32_t max = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(Add(max, right), expect1);
}

TEST_F(NumberSafeUtilTest, AddInt64WithInt32) {
    const int32_t a = 10;
    const int64_t b = 20;
    const int64_t expect1 = 30;
    EXPECT_EQ(Add(a, b), expect1);
    const int64_t int32Max = static_cast<int64_t>(std::numeric_limits<int32_t>::max());
    EXPECT_EQ(Add(a, int32Max), static_cast<int64_t>(std::numeric_limits<int32_t>::max()) + a);
    const int64_t int64Max = std::numeric_limits<int64_t>::max();
    EXPECT_EQ(Add(a, int64Max), 0);
}

TEST_F(NumberSafeUtilTest, Addint64WithUin32) {
    const int64_t a = 100;
    uint32_t b = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(Add(a, b), a + b);
}

TEST_F(NumberSafeUtilTest, AddUint64WithInt32) {
    const uint64_t a = 10;
    const int32_t b = -1;
    EXPECT_EQ(Add(a, b), 0);
}

TEST_F(NumberSafeUtilTest, AddFloatPoint) {
    const double a = 1.71;
    const double b = 2.90;
    EXPECT_EQ(Add(a, b), a + b);
}

TEST_F(NumberSafeUtilTest, AddFloatWithDouble) {
    const double a = 1.71;
    const float b = 2.90;
    EXPECT_EQ(Add(a, b), a + b);
}

TEST_F(NumberSafeUtilTest, AddFloatWithDoubleMinElips) {
    const float b = 2.90;
    const double a = std::numeric_limits<double>::epsilon();
    EXPECT_EQ(Add(a, b), a + b);
}

TEST_F(NumberSafeUtilTest, SubInt) {
    // int to int
    const int32_t a = 100;
    const int32_t b = 10;
    EXPECT_EQ(Sub(a, b), a - b);
    const int32_t rightValue2 = 2;
    EXPECT_EQ(Sub(a, rightValue2), a - rightValue2);
    const int32_t rightValue10 = -10;
    EXPECT_EQ(Sub(a, rightValue10), a - rightValue10);
}

TEST_F(NumberSafeUtilTest, SubUint) {
    const uint32_t a = 100;
    const uint32_t b = 10;
    const uint32_t expect1 = 90;
    EXPECT_EQ(Sub(a, b), expect1);
    const uint32_t c = 10;
    const uint32_t d = 1000;
    const uint32_t expect2 = 0;
    EXPECT_EQ(Sub(c, d), expect2);
}

TEST_F(NumberSafeUtilTest, SubIntWithUint) {
    const uint32_t a = 100;
    const int32_t b = -1000;
    const uint32_t expect1 = 1100;
    EXPECT_EQ(Sub(a, b), expect1);
    const int32_t smallNum = 10;
    const uint32_t expect2 = 90;
    EXPECT_EQ(Sub(a, smallNum), expect2);
}

TEST_F(NumberSafeUtilTest, SubInt64WithUint8) {
    const int64_t a = 484;
    const uint8_t b = 2;
    const int64_t expect = 482;
    EXPECT_EQ(NumberSafe::Sub(a, b), expect);
}

TEST_F(NumberSafeUtilTest, Multiply) {
    const int32_t a = 10;
    const int32_t b = 20;
    const int32_t expect = 200;
    EXPECT_EQ(Muls(a, b), expect);
    const int32_t rightZero = 0;
    EXPECT_EQ(Muls(a, rightZero), 0);
    const int32_t left = 1 << 2;
    const int32_t right = 1 << (sizeof(int32_t) * 8 - 1);
    EXPECT_EQ(Muls(left, right), 0);
}

TEST_F(NumberSafeUtilTest, Div) {
    const uint64_t a = 10;
    const uint64_t b = 0;
    EXPECT_EQ(Division(a, b), 0);
}

TEST_F(NumberSafeUtilTest, multiCall) {
    const int64_t startPos = 12;
    const uint64_t dataSize = 472;
    const uint8_t paddingLength = 2;
    auto res = NumberSafe::Sub(NumberSafe::Add(startPos, static_cast<int64_t>(dataSize)), paddingLength);
    const int64_t expect = 482;
    EXPECT_EQ(res, expect);
}
