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
#include "NumberUtil.h"

using namespace Dic;

TEST(NumberUtil, TryParseIntWithNormalNumberReturnValid) {
    EXPECT_EQ(NumberUtil::TryParseInt("0"), 0);
    EXPECT_EQ(NumberUtil::TryParseInt("001"), 1);
    EXPECT_EQ(NumberUtil::TryParseInt("2147483647"), INT32_MAX);
    EXPECT_EQ(NumberUtil::TryParseInt("-1"), -1);
    EXPECT_EQ(NumberUtil::TryParseInt("-2147483648"), INT32_MIN);

    // Please notice
    EXPECT_EQ(NumberUtil::TryParseInt("0+1"), 0);
    EXPECT_EQ(NumberUtil::TryParseInt("55-2"), 55);
    EXPECT_EQ(NumberUtil::TryParseInt("+55"), 55);
    EXPECT_EQ(NumberUtil::TryParseInt("--1"), -1);
    EXPECT_EQ(NumberUtil::TryParseInt("---1"), -1);
}

TEST(NumberUtil, TryParseIntWithAbnormalNumberReturnInvalid) {
    EXPECT_EQ(NumberUtil::TryParseInt("-"), INVALID_NUMBER);
    EXPECT_EQ(NumberUtil::TryParseInt("A"), INVALID_NUMBER);
    EXPECT_EQ(NumberUtil::TryParseInt("2147483648"), INVALID_NUMBER);
}

TEST(NumberUtil, TryParseUnsignedLongLongWithNormalNumberReturnValid) {
    EXPECT_EQ(NumberUtil::TryParseUnsignedLongLong("0"), 0);
    EXPECT_EQ(NumberUtil::TryParseUnsignedLongLong("001"), 1);
    EXPECT_EQ(NumberUtil::TryParseUnsignedLongLong("18446744073709551615"), UINT64_MAX);
    EXPECT_EQ(NumberUtil::TryParseUnsignedLongLong("-1"), UINT64_MAX);
    EXPECT_EQ(NumberUtil::TryParseUnsignedLongLong("--1"), UINT64_MAX);

    EXPECT_EQ(NumberUtil::TryParseUnsignedLongLong("0+1"), 0);
    EXPECT_EQ(NumberUtil::TryParseUnsignedLongLong("55-2"), 55);
    EXPECT_EQ(NumberUtil::TryParseUnsignedLongLong("+55"), 55);
    const uint64_t maxInt = INT32_MAX;
    const uint64_t expectInt = maxInt + 1;
    std::string input = std::to_string(expectInt);
    EXPECT_EQ(NumberUtil::TryParseUnsignedLongLong(input), expectInt);
}

TEST(NumberUtil, TryParseUnsignedLongLongWithAbnormalNumberReturnInvalid) {
    EXPECT_EQ(NumberUtil::TryParseInt("-"), INVALID_NUMBER);
    EXPECT_EQ(NumberUtil::TryParseInt("A"), INVALID_NUMBER);
    EXPECT_EQ(NumberUtil::TryParseInt("18446744073709551616"), INVALID_NUMBER);
}

TEST(NumberUtil, Uint64ToHexStringWithNormalNumberReturnValid) {
    EXPECT_EQ(NumberUtil::Uint64ToHexString(0), "0x0");
    EXPECT_EQ(NumberUtil::Uint64ToHexString(15), "0xf");
    EXPECT_EQ(NumberUtil::Uint64ToHexString(16), "0x10");
    EXPECT_EQ(NumberUtil::Uint64ToHexString(999999999), "0x3b9ac9ff");
    EXPECT_EQ(NumberUtil::Uint64ToHexString(99999999999999), "0x5af3107a3fff");
    EXPECT_EQ(NumberUtil::Uint64ToHexString(-1), "0xffffffffffffffff");
}

TEST(NumberUtil, HexadecimalStrToDecimalIntWithNormalNumberReturnValid) {
    EXPECT_EQ(NumberUtil::HexadecimalStrToDecimalInt("0x0"), 0);
    EXPECT_EQ(NumberUtil::HexadecimalStrToDecimalInt("0xf"), 15);
    EXPECT_EQ(NumberUtil::HexadecimalStrToDecimalInt("0x10"), 16);
    EXPECT_EQ(NumberUtil::HexadecimalStrToDecimalInt("0x3b9ac9ff"), 999999999);
    EXPECT_EQ(NumberUtil::HexadecimalStrToDecimalInt("0xffffffffffffffff"), UINT64_MAX);
}

TEST(NumberUtil, TimestampUsToNsWithLongDoubleWithNormalInputReturnValid) {
    double a = stod("1695297849996490.053");
    EXPECT_EQ(1695297849996489984, llround(a * 1000));
#if defined(__linux__)
    EXPECT_EQ(1695297849996490053, NumberUtil::TimestampUsToNs(stold("1695297849996490.053011100000")));
    EXPECT_EQ(1695297849996490054, NumberUtil::TimestampUsToNs(stold("1695297849996490.0535")));
    EXPECT_EQ(1695297849996490000, NumberUtil::TimestampUsToNs(1695297849996490));
#elif defined(__APPLE__) && defined(__MACH__)
    EXPECT_EQ(1695297849996489984, NumberUtil::TimestampUsToNs(stold("1695297849996490.053011100000")));
    EXPECT_EQ(1695297849996489984, NumberUtil::TimestampUsToNs(stold("1695297849996490.0535")));
#endif
    EXPECT_EQ(0, NumberUtil::TimestampUsToNs(16952978499964900));
}

TEST(NumberUtil, TestConvetUsStrToNanoseconds) {
    double a = stod("1695297849996490.053");
    EXPECT_EQ(1695297849996489984, llround(a * 1000));

    EXPECT_EQ(1695297849996490053, NumberUtil::ConvertUsStrToNanoseconds("1695297849996490.053011100000"));
    EXPECT_EQ(1695297849996490054, NumberUtil::ConvertUsStrToNanoseconds("1695297849996490.0535"));
    EXPECT_EQ(1695297849996490000, NumberUtil::ConvertUsStrToNanoseconds("1695297849996490"));
    EXPECT_EQ(0, NumberUtil::ConvertUsStrToNanoseconds("9223372036854775807"));
    EXPECT_EQ(0, NumberUtil::ConvertUsStrToNanoseconds("-99999999"));
    EXPECT_EQ(0, NumberUtil::ConvertUsStrToNanoseconds("123ada.7899"));
    EXPECT_EQ(0, NumberUtil::ConvertUsStrToNanoseconds("123.7afd899"));
    EXPECT_EQ(0, NumberUtil::ConvertUsStrToNanoseconds("123.7a899"));
    EXPECT_EQ(0, NumberUtil::ConvertUsStrToNanoseconds("12b3.7899"));
    EXPECT_EQ(1695297849996491000, NumberUtil::ConvertUsStrToNanoseconds("1695297849996490.9995"));
#if defined(__linux__)
    EXPECT_EQ(1732274952602387700, NumberUtil::ConvertUsStrToNanoseconds("1.7322749526023877e+15"));
#elif defined(__APPLE__) && defined(__MACH__)
    EXPECT_EQ(1732274952602387712, NumberUtil::ConvertUsStrToNanoseconds("1.7322749526023877e+15"));
#endif
    EXPECT_EQ(0, NumberUtil::ConvertUsStrToNanoseconds("-1.7322749526023877e+15"));
    EXPECT_EQ(0, NumberUtil::ConvertUsStrToNanoseconds("null"));
    EXPECT_EQ(0, NumberUtil::ConvertUsStrToNanoseconds("16952978499964900"));
}

TEST(NumberUtil, TimestampUsToNsWithStringWithNormalInputReturnValid) {
    double a = stod("1695297849996490.053");
    EXPECT_EQ(1695297849996489984, llround(a * 1000));
#if defined(__linux__) || defined(_WIN32)
    EXPECT_EQ(1695297849996490000, NumberUtil::TimestampUsToNs("1695297849996490"));
#elif defined(__APPLE__) && defined(__MACH__)
    EXPECT_EQ(1695297849996489984, NumberUtil::TimestampUsToNs("1695297849996490"));
#endif
    EXPECT_EQ(0, NumberUtil::TimestampUsToNs("16952978499964900"));
}

TEST(NumberUtil, StringToDoubleWithNormalStringReturnValidNumber) {
    EXPECT_EQ(NumberUtil::StringToDouble("0"), 0.0);
    EXPECT_EQ(NumberUtil::StringToDouble("490.053"), 490.053);
    EXPECT_EQ(NumberUtil::StringToDouble("0xffff"), 65535);
    EXPECT_TRUE(abs(NumberUtil::StringToDouble("1695297849996490.053") - 1695297849996490.053) < 0.00000001);
}

TEST(NumberUtil, StringToDoubleWithAbnormalStringReturnInvalidNumber) {
    EXPECT_EQ(NumberUtil::StringToDouble(""), 0);
    EXPECT_EQ(NumberUtil::StringToDouble("a+b"), 0);
}

TEST(NumberUtil, StringToLongDoubleWithNormalStringReturnValidNumber) {
    EXPECT_EQ(NumberUtil::StringToDouble("0"), 0.0);
    EXPECT_EQ(NumberUtil::StringToDouble("490.053"), 490.053);
    EXPECT_EQ(NumberUtil::StringToDouble("0xffff"), 65535);
    EXPECT_TRUE(abs(NumberUtil::StringToDouble("1695297849996490.053") - 1695297849996490.053) < 0.00000001);
}

TEST(NumberUtil, StringToLongDoubleWithAbnormalStringReturnInvalidNumber) {
    EXPECT_EQ(NumberUtil::StringToLongDouble(""), 0);
    EXPECT_EQ(NumberUtil::StringToLongDouble("a+b"), 0);
}

TEST(NumberUtil, StringToIntWithNormalStringReturnValidNumber) {
    EXPECT_EQ(NumberUtil::StringToInt("0"), 0);
    EXPECT_EQ(NumberUtil::StringToInt("2537"), 2537);
    EXPECT_EQ(NumberUtil::StringToInt("-1064"), -1064);
}

TEST(NumberUtil, StringToIntWithAbnormalStringReturnZero) {
    EXPECT_EQ(NumberUtil::StringToInt(""), 0);
    EXPECT_EQ(NumberUtil::StringToInt("abc"), 0);
}

TEST(NumberUtil, StringToLongWithNormalStringReturnValidNumber) {
    EXPECT_EQ(NumberUtil::TryParseInt("0"), 0);
    EXPECT_EQ(NumberUtil::TryParseInt("001"), 1);
    EXPECT_EQ(NumberUtil::TryParseInt("2147483647"), INT32_MAX);
    EXPECT_EQ(NumberUtil::TryParseInt("-1"), -1);
    EXPECT_EQ(NumberUtil::TryParseInt("-2147483648"), INT32_MIN);

    // Please notice
    EXPECT_EQ(NumberUtil::TryParseInt("0+1"), 0);
    EXPECT_EQ(NumberUtil::TryParseInt("55-2"), 55);
    EXPECT_EQ(NumberUtil::TryParseInt("+55"), 55);
    EXPECT_EQ(NumberUtil::TryParseInt("--1"), -1);
    EXPECT_EQ(NumberUtil::TryParseInt("---1"), -1);
}

TEST(NumberUtil, StringToLongWithAbnormalStringReturnInalidNumber) {
    EXPECT_EQ(NumberUtil::TryParseInt("-"), INVALID_NUMBER);
    EXPECT_EQ(NumberUtil::TryParseInt("A"), INVALID_NUMBER);
    EXPECT_EQ(NumberUtil::TryParseInt("2147483648"), INVALID_NUMBER);
}

TEST(NumberUtil, StringUnsignedLongLongMinusTest) {
    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus("1", "0"), "1");
    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus("100", "1"), "99");
    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus("106", "104"), "2");
    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus("0", "1"), "-1");
    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus("10", "20"), "-10");
    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus("2", "107"), "-105");

    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus(std::to_string(INT64_MAX), "0"), std::to_string(INT64_MAX));
    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus(std::to_string(UINT64_MAX), "0"), std::to_string(INT64_MAX));
    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus(std::to_string(UINT64_MAX), std::to_string(INT64_MAX)),
        std::to_string(INT64_MAX));

    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus("0", std::to_string(INT64_MAX)), std::to_string(-INT64_MAX));
    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus("0", std::to_string(UINT64_MAX)), std::to_string(INT64_MIN));
    EXPECT_EQ(NumberUtil::StringUnsignedLongLongMinus(std::to_string(INT64_MAX), std::to_string(UINT64_MAX)),
        std::to_string(INT64_MIN));
}

TEST(NumberUtil, StringDoubleMinusWithNormalStringReturnValid) {
    EXPECT_EQ(NumberUtil::StringDoubleMinus("111111.555555", "0.555555"), "111111.000");
    EXPECT_EQ(NumberUtil::StringDoubleMinus("111111.555555", "0.555555", 2), "111111.00");
    EXPECT_EQ(NumberUtil::StringDoubleMinus("111111.555555", "0.555550", 2), "111111.00");
}

TEST(NumberUtil, StringDoubleMinusKeepSfWithNormalStringReturnValid) {
    EXPECT_EQ(NumberUtil::StringDoubleMinusKeepSf("111111.555555", "0.555555"), "111111.000");
    EXPECT_EQ(NumberUtil::StringDoubleMinusKeepSf("111111.555555", "0.555555", 2), "111111.00");
    // please notice
    EXPECT_EQ(NumberUtil::StringDoubleMinusKeepSf("111111.555555", "0.555550", 2), "111111.0000050");
}

TEST(NumberUtil, RemoveTrailingZerosAndDecimalWithNormalStringReturnValidString) {
    EXPECT_EQ(NumberUtil::RemoveTrailingZerosAndDecimal("111111.55555500"), "111111.555555");
    EXPECT_EQ(NumberUtil::RemoveTrailingZerosAndDecimal("111111.00555555"), "111111.00555555");
}

TEST(NumberUtil, RemoveTrailingZerosAndDecimalWithAbnormalStringReturnInvalidString) {
    // please notice !!!
    EXPECT_EQ(NumberUtil::RemoveTrailingZerosAndDecimal("11111100"), "111111");
    EXPECT_EQ(NumberUtil::RemoveTrailingZerosAndDecimal("11111100.00"), "11111100");
    EXPECT_EQ(NumberUtil::RemoveTrailingZerosAndDecimal("00"), "0");
    EXPECT_EQ(NumberUtil::RemoveTrailingZerosAndDecimal("00.00"), "00");
}

TEST(NumberUtil, StringDoubleMinusWithoutTrailingZeroWithNormalStringReturnValidString) {
    EXPECT_EQ(NumberUtil::StringDoubleMinusWithoutTrailingZero("111111.555555", "0.555555"), "111111");
    EXPECT_EQ(NumberUtil::StringDoubleMinusWithoutTrailingZero("111111.555555", "0.555555", 2), "111111");
    // please notice
    EXPECT_EQ(NumberUtil::StringDoubleMinusWithoutTrailingZero("111111.555555", "0.555550", 2), "111111.000005");
}

TEST(NumberUtil, StringDoubleMinusWithoutTrailingZeroWithAbnormalStringReturnInvalidString) {
    EXPECT_EQ(NumberUtil::StringDoubleMinusWithoutTrailingZero("", ""), "");
    EXPECT_EQ(NumberUtil::StringDoubleMinusWithoutTrailingZero("aa", ""), "");
    EXPECT_EQ(NumberUtil::StringDoubleMinusWithoutTrailingZero("", "aa"), "");
}

TEST(NumberUtil, DoubleReservedNDigitsWithNormalStringReturnInvalidNumber) {
    double a = stod("490.053849996");
    EXPECT_EQ(NumberUtil::DoubleReservedNDigits(a, 5), stod("490.05385"));
}

TEST(NumberUtil, DoubleReservedNDigitsWithAbnormalStringReturnInvalidNumber) {
    double a = stod("490.053849996");
    EXPECT_EQ(NumberUtil::DoubleReservedNDigits(a, 0), a);
    EXPECT_EQ(NumberUtil::DoubleReservedNDigits(a, 7), a);
}

TEST(NumberUtil, SubWithNormalStringReturnInvalidNumber) {
    double a = stod("490.053849996");
    double b = stod("489.1234685");
    EXPECT_EQ(NumberUtil::Sub(a, b), 0.930381);
    EXPECT_EQ(NumberUtil::Sub(b, a), -0.930381);
}

TEST(NumberUtil, SubWithAbnormalStringReturnInvalidNumber) {
    double a = stod("490.053849996");
    EXPECT_EQ(NumberUtil::Sub(a, a), 0);
}

TEST(NumberUtil, StringRservedNDigitsWithNormalInputReturnValidString) {
    EXPECT_EQ(NumberUtil::StrReservedNDigits("3.1415", 2), "3.14");
    EXPECT_EQ(NumberUtil::StrReservedNDigits("3.1", 2), "3.1");
    EXPECT_EQ(NumberUtil::StrReservedNDigits("31", 2), "31");
    EXPECT_EQ(NumberUtil::StrReservedNDigits("3.000", 2), "3.00");
    EXPECT_EQ(NumberUtil::StrReservedNDigits("0.0002", 2), "0.0002");
    EXPECT_EQ(NumberUtil::StrReservedNDigits("0.0002", 3), "0.0002");
}

TEST(NumberUtil, StringRservedNDigitsWithAbnormalStringReturnInvalidNumber) {
    EXPECT_EQ(NumberUtil::StrReservedNDigits("3.1415", 0), "3.1415");
    EXPECT_EQ(NumberUtil::StrReservedNDigits("3.1415", 7), "3.1415");
    EXPECT_EQ(NumberUtil::StrReservedNDigits("3.1415", 5), "3.1415");
    EXPECT_EQ(NumberUtil::StrReservedNDigits("0.00", 2), "0.00");
}

TEST(NumberUtil, IsGreaterWithNormalInputReturnValidString) {
    EXPECT_EQ(NumberUtil::IsGreater(3.1415, 3.1416), false);
    EXPECT_EQ(NumberUtil::IsGreater(3.1415, 3.1415), false);
    EXPECT_FALSE(NumberUtil::IsGreater(3.1416, 3.1415, 0.001));
    EXPECT_FALSE(NumberUtil::IsGreater(3.141, 3.1415, 0.00001));

    EXPECT_EQ(NumberUtil::IsGreater(-3.1415, -3.1416), true);
    EXPECT_EQ(NumberUtil::IsGreater(-3.1415, -3.1415), false);
}

TEST(NumberUtil, TruncateNumberString) {
    std::unordered_map<std::string, std::string> testMap = {{"123.456", "123"}, {"-123.987", "-123"}, {"abc", "0"},
        {"0.000", "0"}, {"-0.1", "0"}, {"0", "0"},
        {std::to_string(std::numeric_limits<uint64_t>::max()), std::to_string(std::numeric_limits<int64_t>::max())},
        {std::to_string(std::numeric_limits<uint64_t>::min()), std::to_string(std::numeric_limits<int64_t>::min())}};
    for (auto &testItem : testMap) {
        EXPECT_EQ(NumberUtil::TruncateNumberString(testItem.first), testItem.second);
    }
}

TEST(NumberUtil, TrimNumericString) {
    std::unordered_map<std::string, std::string> testMap = {
        {"\"123.456\t\"", "123.456"},
        {"\t\"-123.987\t\"", "-123.987"},
        {"0.000\t\n", "0.000"},
        {"", "0"},
        {"\t", "0"},
        {"\t\t", "0"},
    };

    for (auto &testItem : testMap) {
        EXPECT_EQ(NumberUtil::TrimNumericString(testItem.first), testItem.second);
    }
}

TEST(NumberUtil, TimestampUsToNsStableReturnValid) {
    EXPECT_EQ(1695297849996490053, NumberUtil::TimestampUsToNsStable("1695297849996490.053011100000"));
    EXPECT_EQ(1695297849996490053, NumberUtil::TimestampUsToNsStable("1695297849996490.0535"));
    EXPECT_EQ(1757124456975771780, NumberUtil::TimestampUsToNsStable("1757124456975771.780"));
    EXPECT_EQ(0, NumberUtil::TimestampUsToNsStable("1757124456975771.qw0"));
    EXPECT_EQ(0, NumberUtil::TimestampUsToNsStable("qad123.780"));
    EXPECT_EQ(0, NumberUtil::TimestampUsToNsStable("12122qq975771.780"));
}
