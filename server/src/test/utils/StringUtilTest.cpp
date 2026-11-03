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
#include <string_view>
#include "../TestSuit.h"
#include "StringUtil.h"

using namespace Dic;

TEST(StringUtil, IntToString) {
    EXPECT_EQ(StringUtil::IntToString(1, 1), "1");
    EXPECT_EQ(StringUtil::IntToString(1, 2), "01");
    EXPECT_EQ(StringUtil::IntToString(1, 3), "001");
    EXPECT_EQ(StringUtil::IntToString(333, 3), "333");
    EXPECT_EQ(StringUtil::IntToString(100, 1), "100");
    EXPECT_EQ(StringUtil::IntToString(100, 2), "100");
}

TEST(StringUtil, Split) {
    EXPECT_EQ(StringUtil::Split("a,a", ",")[0], "a");
    EXPECT_EQ(StringUtil::Split("a,a", ",")[1], "a");
    EXPECT_EQ(StringUtil::Split("wwa,aww", "wa,aw")[0], "w");
    EXPECT_EQ(StringUtil::Split("wwa,aww", "wa,aw")[1], "w");
    EXPECT_EQ(StringUtil::Split("wwa,aww", "a.a")[0], "ww");
    EXPECT_EQ(StringUtil::Split("wwa,aww", "aww")[0], "wwa,");
}

TEST(StringUtil, ByteNum) { std::cout << StringUtil::ByteNum(11) << std::endl; }

TEST(StringUtil, ByteNumWithUtf8LeadingBytes) {
    EXPECT_EQ(StringUtil::ByteNum(0x7F), 0);
    EXPECT_EQ(StringUtil::ByteNum(0xC2), 1);
    EXPECT_EQ(StringUtil::ByteNum(0xE4), 2);
    EXPECT_EQ(StringUtil::ByteNum(0xF0), 3);
}

TEST(StringUtil, IsAllDigitsWithNormalInput) {
    EXPECT_EQ(StringUtil::IsAllDigits("123"), true);
    EXPECT_EQ(StringUtil::IsAllDigits("1"), true);
    EXPECT_EQ(StringUtil::IsAllDigits("1+2"), false);
    EXPECT_EQ(StringUtil::IsAllDigits("-2"), false);
}

TEST(StringUtil, AnonymousString) {
    EXPECT_EQ(StringUtil::AnonymousString("a"), "a");
    EXPECT_EQ(StringUtil::AnonymousString("ab"), "ab");
    EXPECT_EQ(StringUtil::AnonymousString("abc"), "a*c");
    EXPECT_EQ(StringUtil::AnonymousString("abcd"), "a*cd");
    EXPECT_EQ(StringUtil::AnonymousString("abcd1111aaaaaa"), "abcd****aaaaaa");
    EXPECT_EQ(StringUtil::AnonymousString("1111awfdcssf"), "1111****cssf");
    EXPECT_EQ(StringUtil::AnonymousString("************"), "************");
}

TEST(StringUtil, Trim) {
    std::string str1 = " Hello,World! ";
    EXPECT_EQ(StringUtil::Trim(str1), "Hello,World!");
    std::string str2 = "Hello,World! ";
    EXPECT_EQ(StringUtil::Trim(str2), "Hello,World!");
    std::string str3 = "";
    EXPECT_EQ(StringUtil::Trim(str3), "");
    std::string str4 = "  ";
    EXPECT_EQ(StringUtil::Trim(str4), "");
}

TEST(StringUtil, join) {
    std::vector<std::string> list1 = {"a", "b", "c"};
    EXPECT_EQ(StringUtil::join(list1, "1"), "a1b1c");
    std::vector<std::string> list2 = {"a", "b", "c"};
    EXPECT_EQ(StringUtil::join(list2, ","), "a,b,c");
}

TEST(StringUtil, StartWith) {
    EXPECT_EQ(StringUtil::StartWith("aaaa", "aa"), true);
    EXPECT_EQ(StringUtil::StartWith("xdw", "wa"), false);
    EXPECT_EQ(StringUtil::StartWith("xa", " "), false);
    EXPECT_EQ(StringUtil::StartWith(" xd", " "), true);
}

TEST(StringUtil, EndWith) {
    EXPECT_EQ(StringUtil::EndWith("aaaa", "aa"), true);
    EXPECT_EQ(StringUtil::EndWith("xdw", "w"), true);
    EXPECT_EQ(StringUtil::EndWith("xa", " "), false);
    EXPECT_EQ(StringUtil::EndWith(" xd ", " "), true);
}

TEST(StringUtil, Contains) {
    EXPECT_EQ(StringUtil::Contains(" xdw", "x"), true);
    EXPECT_EQ(StringUtil::Contains(" xdw", " "), true);
    EXPECT_EQ(StringUtil::Contains("xdw", "d"), true);
    EXPECT_EQ(StringUtil::Contains("xdw", "ad"), false);
}

TEST(StringUtil, ReplaceFirstWithNormalInput) {
    EXPECT_EQ(StringUtil::ReplaceFirst("xdw", "x", "y"), "ydw");
    EXPECT_EQ(StringUtil::ReplaceFirst(" xdw", " ", ""), "xdw");
    EXPECT_EQ(StringUtil::ReplaceFirst(" xdw ", " ", ""), "xdw ");
    EXPECT_EQ(StringUtil::ReplaceFirst("aaa ", "b", "c"), "aaa ");
}

TEST(StringUtil, ValidateCommandFilePathParamWithNormalInput) {
#ifdef _WIN32
    EXPECT_EQ(StringUtil::ValidateCommandFilePathParam(""), false);
#else
    EXPECT_EQ(StringUtil::ValidateCommandFilePathParam(""), false);
    EXPECT_EQ(StringUtil::ValidateCommandFilePathParam("/"), true);
    EXPECT_EQ(StringUtil::ValidateCommandFilePathParam("/home/xxx/a|b"), false);
    EXPECT_EQ(StringUtil::ValidateCommandFilePathParam("/home/xxx/${a}"), false);
    EXPECT_EQ(StringUtil::ValidateCommandFilePathParam("/home/xxx/cc;mkdir d"), false);
#endif
}

TEST(StringUtil, ToCamelCaseWithNormalInput) {
    EXPECT_EQ(StringUtil::ToCamelCase("rank_id"), "rankId");
    EXPECT_EQ(StringUtil::ToCamelCase("rank_id, device_Id"), "rankId, deviceId");
    EXPECT_EQ(StringUtil::ToCamelCase("rank__id, device_Id"), "rankId, deviceId");
}

TEST(StringUtil, SplitStringWithParenthesesByCommaTestReturnEmptyWhenEmptyInput) {
    EXPECT_EQ(StringUtil::SplitStringWithParenthesesByComma("").size(), 0);
}

TEST(StringUtil, SplitStringWithParenthesesByCommaTestReturnNormalWhenInputWithSpace) {
    auto result = StringUtil::SplitStringWithParenthesesByComma("( a ,b,  c d, )");
    EXPECT_EQ(result.size(), 3); // 3, a, b, c
    EXPECT_EQ(result.at(result.size() - 1), "c d");
}

TEST(StringUtil, CreateQuestionMarkStringWhenInputZero) {
    auto result = StringUtil::CreateQuestionMarkString(0);
    EXPECT_EQ(result, "");
}

TEST(StringUtil, CreateQuestionMarkStringWhenInputNotZero) {
    auto result = StringUtil::CreateQuestionMarkString(5);
    EXPECT_EQ(result, "?,?,?,?,?");
}

TEST(StringUtil, StringToUint32) {
    const uint64_t tempMax = INT64_MAX;
    const uint64_t input = tempMax + 1;
    std::string inputStr = std::to_string(input);
    auto result = StringUtil::StringToUint32(inputStr);
    EXPECT_EQ(result, UINT32_MAX);
}

TEST(StringUtil, StringToInt) {
    const uint64_t tempMax = INT64_MAX;
    const uint64_t input = tempMax + 1;
    std::string inputStr = std::to_string(input);
    auto result = StringUtil::StringToInt(inputStr);
    EXPECT_EQ(result, 0);
}

TEST(StringUtil, StrJoin) {
    EXPECT_EQ(StringUtil::StrJoin("test"), "test");
    EXPECT_EQ(StringUtil::StrJoin("test", "hello"), "testhello");
    EXPECT_EQ(StringUtil::StrJoin("test", " hello", " world"), "test hello world");
}

TEST(StringUtil, FormatStringUsingPlaceHolder) {
    const std::string pattern = "Pattern format: {}, {}";
    // 异常用例：args字符串数量大于占位符数量
    std::string res = StringUtil::FormatString(pattern, "hello", "world", "!");
    EXPECT_TRUE(res.empty());
    // 异常用例： args字符串数量小于占位符数量
    res = StringUtil::FormatString(pattern, "hello");
    EXPECT_TRUE(res.empty());
    // 正常用例：args字符串数量等于占位符数量
    res = StringUtil::FormatString(pattern, "hello", "world");
    std::string expectFormatResult = "Pattern format: hello, world";
    EXPECT_EQ(res, expectFormatResult);
    // 特殊用例：带有转义字符
    res = StringUtil::FormatString(pattern, "\"hello\"", "\"world\"");
    expectFormatResult = "Pattern format: \"hello\", \"world\"";
    EXPECT_EQ(res, expectFormatResult);
}

TEST(StringUtil, CheckSqlVaildWithDoubleUnderline) {
    const std::string sql = "--thistaskid";
    bool result = StringUtil::CheckSqlValid(sql);
    EXPECT_FALSE(result);
    const std::string sqlMid = "this--taskid";
    EXPECT_FALSE(StringUtil::CheckSqlValid(sqlMid));
    const std::string sqlTail = "thistaskid--";
    EXPECT_FALSE(StringUtil::CheckSqlValid(sqlTail));
}

TEST(StringUtil, CheckSqlVaildNormal) {
    const std::string sql = "19247823";
    bool result = StringUtil::CheckSqlValid(sql);
    EXPECT_TRUE(result);
}

TEST(StringUtil, CheckSqlValidRejectsUnsupportedCharacters) {
    EXPECT_TRUE(StringUtil::CheckSqlValid(""));
    EXPECT_TRUE(StringUtil::CheckSqlValid("rank@device:0-name.1_"));
    EXPECT_FALSE(StringUtil::CheckSqlValid("rank device"));
    EXPECT_FALSE(StringUtil::CheckSqlValid("rank$device"));
}

TEST(StringUtil, SplitCsvLine_EmptyLine) { EXPECT_EQ(SplitCsvLine(""), std::vector<std::string>{""}); }

TEST(StringUtil, SplitCsvLine_SingleField) { EXPECT_EQ(SplitCsvLine("hello"), std::vector<std::string>{"hello"}); }

TEST(StringUtil, SplitCsvLine_SimpleFields) {
    EXPECT_EQ(SplitCsvLine("a,b,c"), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(StringUtil, SplitCsvLine_TrailingSeparator) {
    // 行末有分隔符，应补空字段
    EXPECT_EQ(SplitCsvLine("a,b,"), (std::vector<std::string>{"a", "b", ""}));
}

TEST(StringUtil, SplitCsvLine_LeadingSeparator) {
    EXPECT_EQ(SplitCsvLine(",a,b"), (std::vector<std::string>{"", "a", "b"}));
}

TEST(StringUtil, SplitCsvLine_ConsecutiveSeparators) {
    EXPECT_EQ(SplitCsvLine("a,,b"), (std::vector<std::string>{"a", "", "b"}));
}

TEST(StringUtil, SplitCsvLine_QuotedFieldWithoutSpecialChars) {
    EXPECT_EQ(SplitCsvLine("a,,b"), (std::vector<std::string>{"a", "", "b"}));
}

TEST(StringUtil, SplitCsvLine_QuotedFieldWithComma) {
    EXPECT_EQ(SplitCsvLine("\"hello, world\""), (std::vector<std::string>{"hello, world"}));
}

TEST(StringUtil, SplitCsvLine_QuotedFieldWithQuotes) {
    EXPECT_EQ(SplitCsvLine("\"She said \"\"Hi\"\"\""), (std::vector<std::string>{"She said \"Hi\""}));
}

TEST(StringUtil, SplitCsvLine_MixedQuotedAndUnquoted) {
    EXPECT_EQ(
        SplitCsvLine("apple,\"banana, split\",cherry"), (std::vector<std::string>{"apple", "banana, split", "cherry"}));
}

TEST(StringUtil, SplitCsvLine_EmptyQuotedField) { EXPECT_EQ(SplitCsvLine("\"\""), (std::vector<std::string>{""})); }

TEST(StringUtil, SplitCsvLine_UnmatchedQuote) {
    // RFC 4180 要求引号成对，但SplitCsvLine实现会把未闭合引号视为普通字符结尾
    EXPECT_EQ(SplitCsvLine("\"unmatched"), (std::vector<std::string>{"unmatched"}));
    EXPECT_EQ(SplitCsvLine("a,\"b,c"), (std::vector<std::string>{"a", "b,c"}));
}

TEST(StringUtil, SplitCsvLine_WhitespacePreservation) {
    // CSV 不自动 trim 空格
    EXPECT_EQ(SplitCsvLine(" a , b "), (std::vector<std::string>{" a ", " b "}));
    EXPECT_EQ(SplitCsvLine("\" a \",\" b \""), (std::vector<std::string>{" a ", " b "}));
}

TEST(StringUtil, SplitCsvLine_NewlineAndCarriageReturnIgnoredAtEnd) {
    // 注意：\r\n 在中间不会被忽略（但单行不应含出现换行）
    EXPECT_EQ(SplitCsvLine("a,b\n"), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(SplitCsvLine("a,b\r"), (std::vector<std::string>{"a", "b"}));
    EXPECT_EQ(SplitCsvLine("a,b\r\n"), (std::vector<std::string>{"a", "b"}));
}

TEST(StringUtil, SplitCsvLine_EscapedQuotesOnlyInQuotedFields) {
    // 未加引号的 "" 应视为字面两个双引号
    EXPECT_EQ(SplitCsvLine("a\"\"b,c"), (std::vector<std::string>{"a\"\"b", "c"}));
}

TEST(StringUtil, ExtractDigitRankIdFromHost) {
    EXPECT_EQ(StringUtil::ExtractDigitRankIdFromHost("Host 3"), 3);
    EXPECT_EQ(StringUtil::ExtractDigitRankIdFromHost("7"), 7);
    EXPECT_EQ(StringUtil::ExtractDigitRankIdFromHost("Host rank0"), -1);
    EXPECT_EQ(StringUtil::ExtractDigitRankIdFromHost("rank0"), -1);
}

TEST(StringUtil, Join4SqlGroupSkipsEmptyItems) {
    EXPECT_EQ(StringUtil::Join4SqlGroup({"rank0", "", "rank1"}), "'rank0','rank1'");
    EXPECT_EQ(StringUtil::Join4SqlGroup({}), "");
    EXPECT_EQ(StringUtil::Join4SqlGroup({"", ""}), "");
}

TEST(StringUtil, ContainsIgnoreCaseAndToLower) {
    EXPECT_EQ(StringUtil::ToLower("Rank_\xF0\x9F\x98\x80"), "rank_\xF0\x9F\x98\x80");
    EXPECT_TRUE(StringUtil::ContainsIgnoreCase("Rank_\xF0\x9F\x98\x80", "RANK_\xF0\x9F\x98\x80"));
    EXPECT_TRUE(StringUtil::ContainsIgnoreCase("Rank_Device-01", "RANK"));
    EXPECT_FALSE(StringUtil::ContainsIgnoreCase("Rank_Device-01", "host"));
    EXPECT_TRUE(StringUtil::ContainAnyOfSubStr("rank_device", {"host", "device"}));
    EXPECT_FALSE(StringUtil::ContainAnyOfSubStr("rank_device", {"host", "cluster"}));
}

TEST(StringUtil, ValidateStringParamRejectsCommandInjectionCharacters) {
    EXPECT_TRUE(StringUtil::ValidateStringParam(""));
    EXPECT_TRUE(StringUtil::ValidateStringParam("rank_device-01"));
    EXPECT_FALSE(StringUtil::ValidateStringParam("rank|device"));
    EXPECT_FALSE(StringUtil::ValidateStringParam("rank\ndevice"));
    EXPECT_FALSE(StringUtil::ValidateStringParam(std::string("rank\0device", 11)));
}

TEST(StringUtil, StringSplitKeepsQuotedCommaField) {
    EXPECT_EQ(StringUtil::StringSplit("a,\"b,c\",d"), (std::vector<std::string>{"a", "\"b,c\"", "d"}));
    EXPECT_EQ(StringUtil::StringSplit("a,b\r\n"), (std::vector<std::string>{"a", "b"}));
    EXPECT_TRUE(StringUtil::StringSplit("").empty());
}

TEST(StringUtil, FormattingAndPrintableHelpers) {
    EXPECT_EQ(StringUtil::DoubleToStringWithTwoDecimalPlaces(1.236), "1.24");
    EXPECT_EQ(StringUtil::DoubleToStringWithTwoDecimalPlaces(-1.2), "-1.20");
    EXPECT_EQ(StringUtil::GetPrintAbleString("a\nb\tc"), "a_b_c");

    const std::string_view cols[] = {"rank", "device", "value"};
    EXPECT_EQ(StringUtil::GenerateColumnString(cols), "rank ,device ,value");
    EXPECT_EQ(StringUtil::JoinNumberStrWithParenthesesByOrder({"10", "2", "1"}), "(1, 2, 10)");
}

TEST(StringUtil, NameAndNumberHelpers) {
    EXPECT_EQ(StringUtil::FindLCP("rank_id", "rank_name"), "rank_");
    EXPECT_EQ(StringUtil::FindLCP("rank", "host"), "");

    std::string wrappedColumn = "`Table Column`";
    StringUtil::StripDbColumnName(wrappedColumn);
    EXPECT_EQ(wrappedColumn, "Table Column");

    std::string plainColumn = "TableColumn";
    StringUtil::StripDbColumnName(plainColumn);
    EXPECT_EQ(plainColumn, "TableColumn");

    EXPECT_EQ(StringUtil::GetOriginHostName("host_0"), "host");
    EXPECT_EQ(StringUtil::GetOriginHostName("host_rank_1"), "host_rank");
    EXPECT_EQ(StringUtil::GetOriginHostName("host"), "host");
    EXPECT_EQ(StringUtil::StrNumMax("10", "2"), "10");
    EXPECT_EQ(StringUtil::StrNumMax("2", "10"), "10");
    EXPECT_EQ(StringUtil::StrNumMax("abc", "10"), "abc");
}

#ifdef _WIN32
TEST(StringUtil, StringWideRoundTripOnWindows) {
    const std::string local = "rank_device_01";
    std::wstring wide = StringUtil::String2WString(local);
    EXPECT_EQ(StringUtil::WString2String(wide), local);
    EXPECT_TRUE(StringUtil::String2WString("").empty());
    EXPECT_TRUE(StringUtil::WString2String(L"").empty());
}

TEST(StringUtil, Utf8ToUtf16RejectsNonScalarAndOverlongSequences) {
    const std::vector<std::string> invalid = {"\xC1\xB7", "\xC0\x80", "\xE0\x80\x80", "\xF0\x80\x80\x80",
        "\xED\xA0\x80", "\xED\xBF\xBF", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\x80", "\xFF", "\xE7\xBB",
        "\xF0\x9F\x98", "\xE7\x20\x83"};
    for (const auto &bytes : invalid) {
        EXPECT_TRUE(StringUtil::String2WString(bytes).empty());
    }
}

TEST(StringUtil, Utf8Utf16RoundTripIsIndependentOfLocalCodePage) {
    const std::vector<std::string> values = {
        "", "ASCII", u8"练", u8"中文 é Русский 日本語", u8"😀𠮷", "\xF4\x8F\xBF\xBF"};
    for (const auto &value : values) {
        const auto wide = StringUtil::String2WString(value);
        EXPECT_EQ(StringUtil::WString2String(wide), value);
    }
    EXPECT_EQ(StringUtil::String2WString(u8"练"), L"\u7EC3");
    EXPECT_EQ(StringUtil::String2WString(u8"😀"), std::wstring({wchar_t(0xD83D), wchar_t(0xDE00)}));
    const std::string withNul("a\0b", 3);
    EXPECT_EQ(StringUtil::WString2String(StringUtil::String2WString(withNul)), withNul);
}

TEST(StringUtil, Utf16RejectsUnpairedSurrogates) {
    EXPECT_TRUE(StringUtil::WString2String(std::wstring(1, wchar_t(0xD800))).empty());
    EXPECT_TRUE(StringUtil::WString2String(std::wstring(1, wchar_t(0xDC00))).empty());
    EXPECT_TRUE(StringUtil::WString2String(std::wstring({wchar_t(0xD800), L'a'})).empty());
}
#endif
