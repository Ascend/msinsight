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
#include "StringUtil.h"

// clang-format off
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

TEST(StringUtil, ByteNum) {
    std::cout << StringUtil::ByteNum(11) << std::endl;
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

TEST(StringUtil, IsUtf8StrFastWithValidInput) {
    // 空字符串
    EXPECT_EQ(StringUtil::IsUtf8StrFast(""), true);
    // ASCII 字符
    EXPECT_EQ(StringUtil::IsUtf8StrFast("Hello"), true);
    EXPECT_EQ(StringUtil::IsUtf8StrFast("123"), true);
    EXPECT_EQ(StringUtil::IsUtf8StrFast("A"), true);
    // 有效的 2 字节 UTF-8 序列（拉丁字符，如 é）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xC3\xA9"), true);
    // 有效的 3 字节 UTF-8 序列（中文字符）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("中"), true);
    EXPECT_EQ(StringUtil::IsUtf8StrFast("中国"), true);
    // 有效的 4 字节 UTF-8 序列（如某些 emoji）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xF0\x9F\x98\x80"), true);
    // 混合 ASCII 和 UTF-8
    EXPECT_EQ(StringUtil::IsUtf8StrFast("Hello 世界"), true);
    EXPECT_EQ(StringUtil::IsUtf8StrFast("test\xE4\xB8\xADtest"), true);
}

TEST(StringUtil, IsUtf8StrFastWithInvalidInput) {
    // 不完整的 2 字节序列（只有起始字节）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xC3"), false);
    // 不完整的 3 字节序列（只有起始字节）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xE4"), false);
    // 不完整的 3 字节序列（只有前两个字节）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xE4\xB8"), false);
    // 不完整的 4 字节序列（只有起始字节）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xF0"), false);
    // 不完整的 4 字节序列（只有前两个字节）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xF0\x9F"), false);
    // 不完整的 4 字节序列（只有前三个字节）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xF0\x9F\x98"), false);
    // 孤立的延续字节（10xxxxxx）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\x80"), false);
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xBF"), false);
    // 无效的起始字节后跟无效的延续字节
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xC3\x00"), false);
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xE4\xB8\x00"), false);
    // 混合有效和无效序列
    EXPECT_EQ(StringUtil::IsUtf8StrFast("Hello\xC3World"), false);
    EXPECT_EQ(StringUtil::IsUtf8StrFast("中\x80国"), false);
    // GBK 编码（非 UTF-8）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xD6\xD0"), false);  // "中" 的 GBK 编码
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xB9\xFA"), false);  // "国" 的 GBK 编码
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xD6\xD0\xB9\xFA"), false);  // "中国" 的 GBK 编码
    EXPECT_EQ(StringUtil::IsUtf8StrFast("Hello\xD6\xD0World"), false);  // GBK 和 ASCII 混合
    // 其他非 UTF-8 编码（Latin-1 高位字节）
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\x80\x81"), false);  // 高位字节序列
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xFF\xFE"), false);  // 无效的字节序列
    EXPECT_EQ(StringUtil::IsUtf8StrFast("\xFE\xFF"), false);  // BOM 标记（但作为普通字节检查）
}


#ifdef _WIN32
static constexpr UINT GBK_CODE_PAGE = 936;

static void SkipIfNotGbkCodePage()
{
    if (GetACP() != GBK_CODE_PAGE) {
        GTEST_SKIP() << "GBK code page required";
    }
}

TEST(StringUtil, GbkToUtf8) {
    EXPECT_EQ(StringUtil::GbkToUtf8(nullptr), "");
    EXPECT_EQ(StringUtil::GbkToUtf8("nullptr"), "nullptr");
    EXPECT_EQ(StringUtil::GbkToUtf8("\xD6\xD0\xB9\xFA"), "\xE4\xB8\xAD\xE5\x9B\xBD");
    EXPECT_EQ(StringUtil::GbkToUtf8("\xD6\xD0"), "\xE4\xB8\xAD");
}

TEST(StringUtil, Utf8ToGbk) {
    SkipIfNotGbkCodePage();
    EXPECT_EQ(StringUtil::Utf8ToGbk(nullptr), "");
    EXPECT_EQ(StringUtil::Utf8ToGbk("nullptr"), "nullptr");
    EXPECT_EQ(StringUtil::Utf8ToGbk("\xE4\xB8\xAD\xE5\x9B\xBD"), "\xD6\xD0\xB9\xFA");
    EXPECT_EQ(StringUtil::Utf8ToGbk("\xE4\xB8\xAD"), "\xD6\xD0");
}

TEST(StringUtil, ToUtf8StrConvertsWholeGbkPathWithoutMixedUtf8Splitting)
{
    SkipIfNotGbkCodePage();
    const std::string gbkPath = std::string("D:\\data\\") + "\xB6\xE0\xBB\xFA\xB6\xE0\xBF\xA8";
    const std::string utf8Path = std::string("D:\\data\\") + "\xE5\xA4\x9A\xE6\x9C\xBA\xE5\xA4\x9A\xE5\x8D\xA1";
    EXPECT_EQ(StringUtil::ToUtf8Str(gbkPath), utf8Path);
}

TEST(StringUtil, Utf8CharLenWithValidInput)
{
    EXPECT_EQ(StringUtil::Utf8CharLen("abc", 0), 1);
    EXPECT_EQ(StringUtil::Utf8CharLen("\xC3\xA9", 0), 2);
    EXPECT_EQ(StringUtil::Utf8CharLen("\xE4\xB8\xAD", 0), 3);
    EXPECT_EQ(StringUtil::Utf8CharLen("\xF0\x9F\x98\x80", 0), 4);
}

TEST(StringUtil, Utf8CharLenWithInvalidInput)
{
    EXPECT_EQ(StringUtil::Utf8CharLen("\x80", 0), 0);
    EXPECT_EQ(StringUtil::Utf8CharLen("\xC3", 0), 0);
    EXPECT_EQ(StringUtil::Utf8CharLen("\xE4\xB8", 0), 0);
    EXPECT_EQ(StringUtil::Utf8CharLen("\xF0\x9F\x98", 0), 0);
    EXPECT_EQ(StringUtil::Utf8CharLen("\xD6\xD0", 0), 0);
}

TEST(StringUtil, MixedGbkAndUtf8ToUtf8WithPureUtf8)
{
    EXPECT_EQ(StringUtil::MixedGbkAndUtf8ToUtf8(""), "");
    EXPECT_EQ(StringUtil::MixedGbkAndUtf8ToUtf8("abc123"), "abc123");
    EXPECT_EQ(StringUtil::MixedGbkAndUtf8ToUtf8("\xE4\xB8\xAD\xE6\x96\x87"), "\xE4\xB8\xAD\xE6\x96\x87");
    EXPECT_EQ(StringUtil::MixedGbkAndUtf8ToUtf8("\xF0\x9F\x98\x80"), "\xF0\x9F\x98\x80");
}

TEST(StringUtil, MixedGbkAndUtf8ToUtf8WithPureGbk)
{
    EXPECT_EQ(StringUtil::MixedGbkAndUtf8ToUtf8("\xD6\xD0"), "\xE4\xB8\xAD");
    EXPECT_EQ(StringUtil::MixedGbkAndUtf8ToUtf8("\xD6\xD0\xCE\xC4"), "\xE4\xB8\xAD\xE6\x96\x87");
}

TEST(StringUtil, MixedGbkAndUtf8ToUtf8WithMixedInput)
{
    const std::string mixed = std::string("abc") + "\xD6\xD0\xCE\xC4" + "\xE4\xB8\xAD\xE6\x96\x87";
    EXPECT_EQ(StringUtil::MixedGbkAndUtf8ToUtf8(mixed), "abc\xE4\xB8\xAD\xE6\x96\x87\xE4\xB8\xAD\xE6\x96\x87");
}

TEST(StringUtil, MixedGbkAndUtf8ToUtf8WithInvalidBytes)
{
    EXPECT_FALSE(StringUtil::MixedGbkAndUtf8ToUtf8("\x80").empty());
    EXPECT_FALSE(StringUtil::MixedGbkAndUtf8ToUtf8("\xC3").empty());
    EXPECT_FALSE(StringUtil::MixedGbkAndUtf8ToUtf8("abc\xE4\xB8").empty());
}

TEST(StringUtil, Utf8MojibakeToGbkUtf8)
{
    const std::string utf8Chinese = "\xE4\xB8\xAD\xE6\x96\x87";
    const std::string mojibake = StringUtil::GbkToUtf8(utf8Chinese.c_str());
    EXPECT_EQ(StringUtil::Utf8MojibakeToGbkUtf8(nullptr), "");
    EXPECT_EQ(StringUtil::Utf8MojibakeToGbkUtf8("abc123"), "abc123");
    EXPECT_EQ(StringUtil::Utf8MojibakeToGbkUtf8(mojibake.c_str()), utf8Chinese);
}

TEST(StringUtil, Utf8MojibakeToGbkUtf8KeepsUnconvertibleUnicode)
{
    const std::string textWithEmoji = "\xE4\xB8\xAD\xE6\x96\x87\xF0\x9F\x98\x80";
    EXPECT_EQ(StringUtil::Utf8MojibakeToGbkUtf8(textWithEmoji.c_str()), textWithEmoji);
}

TEST(StringUtil, FixGbkMojibakeStr)
{
    SkipIfNotGbkCodePage();
    const std::string utf8Chinese = "\xE4\xB8\xAD\xE6\x96\x87";
    const std::string gbkChinese = "\xD6\xD0\xCE\xC4";
    const std::string mojibake = StringUtil::GbkToUtf8(utf8Chinese.c_str());
    const std::string gbkMultiCard = "\xB6\xE0\xBB\xFA\xB6\xE0\xBF\xA8";
    const std::string utf8MultiCard = "\xE5\xA4\x9A\xE6\x9C\xBA\xE5\xA4\x9A\xE5\x8D\xA1";
    EXPECT_EQ(StringUtil::FixGbkMojibakeStr(""), "");
    EXPECT_EQ(StringUtil::FixGbkMojibakeStr("abc123"), "abc123");
    EXPECT_EQ(StringUtil::FixGbkMojibakeStr(utf8Chinese), gbkChinese);
    EXPECT_EQ(StringUtil::FixGbkMojibakeStr(mojibake), gbkChinese);
    EXPECT_EQ(StringUtil::FixGbkMojibakeStr(gbkMultiCard), gbkMultiCard);
    EXPECT_EQ(StringUtil::ToUtf8Str(StringUtil::FixGbkMojibakeStr(mojibake)), utf8Chinese);
    EXPECT_EQ(StringUtil::ToUtf8Str(StringUtil::FixGbkMojibakeStr(gbkMultiCard)), utf8MultiCard);
}

#endif


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

TEST(StringUtil, SplitStringWithParenthesesByCommaTestReturnEmptyWhenEmptyInput)
{
    EXPECT_EQ(StringUtil::SplitStringWithParenthesesByComma("").size(), 0);
}

TEST(StringUtil, SplitStringWithParenthesesByCommaTestReturnNormalWhenInputWithSpace)
{
    auto result = StringUtil::SplitStringWithParenthesesByComma("( a ,b,  c d, )");
    EXPECT_EQ(result.size(), 3); // 3, a, b, c
    EXPECT_EQ(result.at(result.size() - 1), "c d");
}

TEST(StringUtil, CreateQuestionMarkStringWhenInputZero)
{
    auto result = StringUtil::CreateQuestionMarkString(0);
    EXPECT_EQ(result, "");
}

TEST(StringUtil, CreateQuestionMarkStringWhenInputNotZero)
{
    auto result = StringUtil::CreateQuestionMarkString(5);
    EXPECT_EQ(result, "?,?,?,?,?");
}

TEST(StringUtil, StringToUint32)
{
    const uint64_t tempMax = INT64_MAX;
    const uint64_t input = tempMax + 1;
    std::string inputStr = std::to_string(input);
    auto result = StringUtil::StringToUint32(inputStr);
    EXPECT_EQ(result, UINT32_MAX);
}

TEST(StringUtil, StringToInt)
{
    const uint64_t tempMax = INT64_MAX;
    const uint64_t input = tempMax + 1;
    std::string inputStr = std::to_string(input);
    auto result = StringUtil::StringToInt(inputStr);
    EXPECT_EQ(result, 0);
}

TEST(StringUtil, StrJoin)
{
    EXPECT_EQ(StringUtil::StrJoin("test"), "test");
    EXPECT_EQ(StringUtil::StrJoin("test", "hello"), "testhello");
    EXPECT_EQ(StringUtil::StrJoin("test", " hello", " world"), "test hello world");
}

TEST(StringUtil, FormatStringUsingPlaceHolder)
{
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

TEST(StringUtil, CheckSqlVaildWithDoubleUnderline)
{
    const std::string sql = "--thistaskid";
    bool result = StringUtil::CheckSqlValid(sql);
    EXPECT_FALSE(result);
    const std::string sqlMid = "this--taskid";
    EXPECT_FALSE(StringUtil::CheckSqlValid(sqlMid));
    const std::string sqlTail = "thistaskid--";
    EXPECT_FALSE(StringUtil::CheckSqlValid(sqlTail));
}

TEST(StringUtil, CheckSqlVaildNormal)
{
    const std::string sql = "19247823";
    bool result = StringUtil::CheckSqlValid(sql);
    EXPECT_TRUE(result);
}

TEST(StringUtil, SplitCsvLine_EmptyLine)
{
    EXPECT_EQ(SplitCsvLine(""), std::vector<std::string>{""});
}

TEST(StringUtil, SplitCsvLine_SingleField)
{
    EXPECT_EQ(SplitCsvLine("hello"), std::vector<std::string>{"hello"});
}

TEST(StringUtil, SplitCsvLine_SimpleFields)
{
    EXPECT_EQ(SplitCsvLine("a,b,c"), (std::vector<std::string>{"a", "b", "c"}));
}

TEST(StringUtil, SplitCsvLine_TrailingSeparator)
{
    // 行末有分隔符，应补空字段
    EXPECT_EQ(SplitCsvLine("a,b,"), (std::vector<std::string>{"a", "b", ""}));
}

TEST(StringUtil, SplitCsvLine_LeadingSeparator)
{
    EXPECT_EQ(SplitCsvLine(",a,b"), (std::vector<std::string>{"", "a", "b"}));
}

TEST(StringUtil, SplitCsvLine_ConsecutiveSeparators)
{
    EXPECT_EQ(SplitCsvLine("a,,b"), (std::vector<std::string>{"a", "", "b"}));
}

TEST(StringUtil, SplitCsvLine_QuotedFieldWithoutSpecialChars)
{
    EXPECT_EQ(SplitCsvLine("a,,b"), (std::vector<std::string>{"a", "", "b"}));
}

TEST(StringUtil, SplitCsvLine_QuotedFieldWithComma)
{
    EXPECT_EQ(SplitCsvLine("\"hello, world\""), (std::vector<std::string>{"hello, world"}));
}

TEST(StringUtil, SplitCsvLine_QuotedFieldWithQuotes)
{
    EXPECT_EQ(SplitCsvLine("\"She said \"\"Hi\"\"\""), (std::vector<std::string>{"She said \"Hi\""}));
}

TEST(StringUtil, SplitCsvLine_MixedQuotedAndUnquoted)
{
    EXPECT_EQ(SplitCsvLine("apple,\"banana, split\",cherry"),
              (std::vector<std::string>{"apple", "banana, split", "cherry"}));
}

TEST(StringUtil, SplitCsvLine_EmptyQuotedField) {
    EXPECT_EQ(SplitCsvLine("\"\""), (std::vector<std::string>{""}));
}

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
// clang-format on
