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

#ifndef DATA_INSIGHT_CORE_STRING_UTIL_H
#define DATA_INSIGHT_CORE_STRING_UTIL_H

#include <string>
#include <regex>
#include <cstdint>
#include <memory>
#include <iostream>
#include <sstream>
#include "algorithm"
#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#else
#include <climits>
#endif
#include <map>
#include <iomanip>
#include "NumberUtil.h"

namespace Dic {
class StringUtil {
  public:
    static inline uint32_t StringToUint32(std::string numStr) {
        try {
            long long num = std::stoll(numStr);
            if (num < 0 || num > UINT32_MAX) {
                return UINT32_MAX;
            }
            return static_cast<uint32_t>(num);
        } catch (const std::invalid_argument &e) {
            return UINT32_MAX;
        } catch (const std::out_of_range &) {
            return UINT32_MAX;
        }
    }

    static inline int StringToInt(std::string numStr) {
        try {
            int num = std::stoi(numStr);
            return num;
        } catch (const std::invalid_argument &e) {
            return 0;
        } catch (const std::out_of_range &) {
            return 0;
        }
    }

    static inline const std::string IntToString(int number, size_t length) {
        std::string s = std::to_string(number);
        if (length > s.size()) {
            s.insert(0, length - s.size(), '0');
        }
        return s;
    }

    static inline const std::vector<std::string> Split(const std::string &str, const std::string &regexStr) {
        std::regex regexz(regexStr);
        std::vector<std::string> list(
            std::sregex_token_iterator(str.begin(), str.end(), regexz, -1), std::sregex_token_iterator());
        return list;
    }

    static std::uint8_t ByteNum(const std::uint8_t &byte) {
        const static std::map<std::uint8_t, std::uint8_t> MAP_BYTES = {
            {0xE0, 0xC0}, {0xF0, 0xE0}, {0xF8, 0xF0}, {0xFC, 0xF8}, {0xFE, 0xFC}};
        uint8_t index = 0;
        uint8_t byteNum = 0;
        for (auto kv : MAP_BYTES) {
            index++;
            if ((byte & kv.first) == kv.second) {
                byteNum = index;
                break;
            }
        }
        return byteNum;
    }

    static inline bool IsAllDigits(const std::string &str) { return std::all_of(str.begin(), str.end(), ::isdigit); }

    static inline int ExtractDigitRankIdFromHost(const std::string &str) {
        std::vector<std::string> splitList = StringUtil::Split(str, " ");
        if (splitList.empty()) {
            return -1;
        }
        // 处理字符串为 Host rankId 的场景，按空格分割后，
        std::string rank = splitList.size() > 1 ? splitList[1] : str;
        return IsAllDigits(rank) ? NumberUtil::StringToInt(rank) : -1;
    }

    static inline std::string AnonymousString(const std::string &str) {
        static const size_t MIN_LEN = 3;
        if (str.length() < MIN_LEN) {
            return str;
        }
        std::string res(str);
        size_t pos = res.length() / MIN_LEN;
        res.replace(pos, pos, pos, '*');
        return res;
    }

    static inline std::string Trim(std::string &str) {
        if (str.empty()) {
            return str;
        }
        str.erase(0, str.find_first_not_of(" "));
        str.erase(str.find_last_not_of(" ") + 1);
        return str;
    }

    /**
     * 判断字符串是否为UTF-8编码，简化快速版
     * 注意：为了效率，只检查UTF8核心规范，虽不完备，在非安全关键场景此规则已足够
     * @param str 字符串
     * @return true / false
     */
    static bool IsUtf8StrFast(const std::string &str) {
        size_t i = 0;
        while (i < str.length()) {
            unsigned char c = static_cast<unsigned char>(str[i]);

            // ASCII: 0xxxxxxx (0-127)
            if (c <= 0x7F) {
                i += 1;
            }
            // 2字节序列: 110xxxxx 10xxxxxx
            else if ((c & 0xE0) == 0xC0) {
                if (i + 1 >= str.length() || (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80) {
                    return false;
                }
                i += 2;
            }
            // 3字节序列: 1110xxxx 10xxxxxx 10xxxxxx
            else if ((c & 0xF0) == 0xE0) {
                if (i + 2 >= str.length() || (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80 ||
                    (static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80) {
                    return false;
                }
                i += 3;
            }
            // 4字节序列: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
            else if ((c & 0xF8) == 0xF0) {
                if (i + 3 >= str.length() || (static_cast<unsigned char>(str[i + 1]) & 0xC0) != 0x80 ||
                    (static_cast<unsigned char>(str[i + 2]) & 0xC0) != 0x80 ||
                    (static_cast<unsigned char>(str[i + 3]) & 0xC0) != 0x80) {
                    return false;
                }
                i += 4;
            } else {
                // 出现不符合 UTF-8 规范的字节（如孤立的 10xxxxxx）
                return false;
            }
        }
        return true;
    }

    static std::string ToUtf8Str(const std::string &input) {
#ifdef _WIN32
        static const unsigned int GBK_CODE_PAGE = 936;

        if (IsUtf8StrFast(input)) {
            return input;
        }

        UINT codePage = GetACP();
        if (codePage == GBK_CODE_PAGE) {
            return StringUtil::GbkToUtf8(input.c_str());
        }
#endif
        return input;
    }

#ifdef _WIN32
    static size_t Utf8CharLen(const std::string &input, size_t index) {
        unsigned char c = static_cast<unsigned char>(input[index]);
        if (c <= 0x7F) {
            return 1;
        }
        size_t len = 0;
        if ((c & 0xE0) == 0xC0) {
            len = 2;
        } else if ((c & 0xF0) == 0xE0) {
            len = 3;
        } else if ((c & 0xF8) == 0xF0) {
            len = 4;
        } else {
            return 0;
        }
        if (index + len > input.size()) {
            return 0;
        }
        for (size_t i = 1; i < len; ++i) {
            if ((static_cast<unsigned char>(input[index + i]) & 0xC0) != 0x80) {
                return 0;
            }
        }
        return len;
    }

    static std::string MixedGbkAndUtf8ToUtf8(const std::string &input) {
        std::string result;
        size_t i = 0;
        while (i < input.size()) {
            size_t utf8Len = Utf8CharLen(input, i);
            if (utf8Len > 0) {
                result.append(input, i, utf8Len);
                i += utf8Len;
                continue;
            }
            size_t gbkStart = i;
            do {
                ++i;
                if (i < input.size()) {
                    utf8Len = Utf8CharLen(input, i);
                }
            } while (i < input.size() && utf8Len == 0);
            result += GbkToUtf8(input.substr(gbkStart, i - gbkStart).c_str());
        }
        return result;
    }
#endif

    static std::string FixGbkMojibakeStr(const std::string &input) {
#ifdef _WIN32
        if (!IsUtf8StrFast(input)) {
            return input;
        }
        std::string fixedUtf8Str = Utf8MojibakeToGbkUtf8(input.c_str());
        return Utf8ToGbk(fixedUtf8Str.c_str());
#else
        return input;
#endif
    }

    static std::string ToLocalStr(const std::string &input) {
#ifdef _WIN32
        static const unsigned int GBK_CODE_PAGE = 936;
        UINT codePage = GetACP();
        if (codePage == GBK_CODE_PAGE) {
            return StringUtil::Utf8ToGbk(input.c_str());
        }
#endif
        return input;
    }

#ifdef _WIN32
    static std::string GbkToUtf8(const char *srcStr) {
        if (srcStr == nullptr) {
            return "";
        }
        static const unsigned int GBK_CODE_PAGE = 936;
        int len = MultiByteToWideChar(GBK_CODE_PAGE, 0, srcStr, -1, nullptr, 0);
        std::unique_ptr<wchar_t[]> wstr = std::make_unique<wchar_t[]>(len + 1);
        MultiByteToWideChar(GBK_CODE_PAGE, 0, srcStr, -1, wstr.get(), len);
        len = WideCharToMultiByte(CP_UTF8, 0, wstr.get(), -1, nullptr, 0, nullptr, nullptr);
        std::unique_ptr<char[]> str = std::make_unique<char[]>(len + 1);
        WideCharToMultiByte(CP_UTF8, 0, wstr.get(), -1, str.get(), len, nullptr, nullptr);
        return std::string(str.get());
    }

    static std::string Utf8MojibakeToGbkUtf8(const char *srcStr) {
        if (srcStr == nullptr) {
            return "";
        }
        int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, srcStr, -1, nullptr, 0);
        if (len <= 0) {
            return srcStr;
        }
        std::unique_ptr<wchar_t[]> wstr = std::make_unique<wchar_t[]>(len + 1);
        if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, srcStr, -1, wstr.get(), len) <= 0) {
            return srcStr;
        }
        static const unsigned int GBK_CODE_PAGE = 936;
        BOOL usedDefaultChar = FALSE;
        len = WideCharToMultiByte(GBK_CODE_PAGE, 0, wstr.get(), -1, nullptr, 0, nullptr, &usedDefaultChar);
        if (len <= 0 || usedDefaultChar) {
            return srcStr;
        }
        std::unique_ptr<char[]> rawUtf8Bytes = std::make_unique<char[]>(len + 1);
        usedDefaultChar = FALSE;
        if (WideCharToMultiByte(GBK_CODE_PAGE, 0, wstr.get(), -1, rawUtf8Bytes.get(), len, nullptr, &usedDefaultChar) <=
                0 ||
            usedDefaultChar) {
            return srcStr;
        }
        std::string fixed(rawUtf8Bytes.get());
        return IsUtf8StrFast(fixed) ? fixed : srcStr;
    }

    static std::string Utf8ToGbk(const char *srcStr) {
        if (srcStr == nullptr) {
            return "";
        }
        int len = MultiByteToWideChar(CP_UTF8, 0, srcStr, -1, nullptr, 0);
        std::unique_ptr<wchar_t[]> wstr = std::make_unique<wchar_t[]>(len + 1);
        MultiByteToWideChar(CP_UTF8, 0, srcStr, -1, wstr.get(), len);
        len = WideCharToMultiByte(CP_ACP, 0, wstr.get(), -1, nullptr, 0, nullptr, nullptr);
        std::unique_ptr<char[]> str = std::make_unique<char[]>(len + 1);
        WideCharToMultiByte(CP_ACP, 0, wstr.get(), -1, str.get(), len, nullptr, nullptr);
        return std::string(str.get());
    }
#endif

    static inline const std::wstring String2WString(const std::string &gbk) {
#ifdef _WIN32
        if (gbk.empty()) {
            return {};
        }
        int size = MultiByteToWideChar(GetACP(), 0, gbk.c_str(), -1, nullptr, 0);
        std::wstring result(size - 1, 0);

        MultiByteToWideChar(GetACP(), 0, gbk.c_str(), -1, result.data(), size);
        return result;
#else
        std::wstring result;
        return result;
#endif
    }

    static inline const std::string WString2String(const std::wstring &wstr) {
#ifdef _WIN32
        if (wstr.empty()) {
            return {};
        }

        int size = WideCharToMultiByte(GetACP(), 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
        std::string result(size - 1, 0);
        WideCharToMultiByte(GetACP(), 0, wstr.c_str(), -1, result.data(), size, nullptr, nullptr);
        return result;
#else
        std::string result;
        return result;
#endif
    }

    template <typename T, typename S> static std::string join(std::vector<T> list, S separator) {
        std::stringstream ss;
        for (size_t i = 0; i < list.size(); i++) {
            if (i != 0) {
                ss << separator;
            }
            ss << list.at(i);
        }
        return ss.str();
    }

    static std::string Join4SqlGroup(std::vector<std::string> list) {
        std::stringstream ss;
        for (size_t i = 0; i < list.size(); i++) {
            if (list.at(i).empty()) {
                continue;
            }
            if (i != 0) {
                ss << ",";
            }
            ss << ("'" + list.at(i) + "'");
        }
        return ss.str();
    }

    static bool StartWith(const std::string &str, const std::string &start) {
        size_t srcLen = str.size();
        size_t startLen = start.size();
        return srcLen >= startLen && str.substr(0, startLen) == start;
    }

    static bool EndWith(const std::string &str, const std::string &suffix) {
        if (str.length() < suffix.length()) {
            return false;
        }
        return str.substr(str.length() - suffix.length()) == suffix;
    }

    static bool Contains(const std::string &str, const std::string &subStr) {
        return str.find(subStr) != std::string::npos;
    }
    static bool ContainAnyOfSubStr(const std::string &str, const std::vector<std::string> &subStrs) {
        return std::any_of(
            subStrs.begin(), subStrs.end(), [&str](const std::string &subStr) { return Contains(str, subStr); });
    }

    static bool ContainsIgnoreCase(const std::string &str, const std::string &subStr) {
        return Contains(ToLower(str), ToLower(subStr));
    }

    static std::string ToLower(const std::string &input) {
        std::string lowerInput = input;
        std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
        return lowerInput;
    }

    /**
     * 替换第一个对应的字符串
     * @param target 目标字符串
     * @param replaceStr 被替换的字符串
     * @param subStr 用于替换的字符串
     * @return true / false
     */
    static std::string ReplaceFirst(
        const std::string &target, const std::string &replaceStr, const std::string &subStr) {
        auto result = std::string(target);
        auto index = target.find(replaceStr);
        if (index == std::string::npos) {
            return result;
        }
        return result.replace(index, replaceStr.length(), subStr);
    }

    /**
     * 校验命令行路径参数
     * @param path 路径
     * @return true / false
     */
    static bool ValidateCommandFilePathParam(const std::string &path) {
        if (path.empty()) {
            return false;
        }
        for (const auto &ch : path) {
            if (std::find(std::begin(injectList), std::end(injectList), ch) != std::end(injectList)) {
                return false;
            }
        }
        return true;
    }

    /**
     * 校验字符串类型参数
     * 和ValidateCommandFilePathParam函数不同，该函数认为空字符串是合法的
     * @param str 字符串
     * @return true / false
     */
    static bool ValidateStringParam(const std::string &str) {
        for (const auto &ch : str) {
            if (std::find(std::begin(injectList), std::end(injectList), ch) != std::end(injectList)) {
                return false;
            }
        }
        return true;
    }

    static std::vector<std::string> StringSplit(const std::string &str) {
        std::vector<std::string> result;
        std::string subStr = "";
        int count = 0;
        for (char ch : str) {
            // 根据字符串内 ” 的数量来判断是否是一个完整的字符串，count % 2 = 0 为偶数个，满足要求
            if (ch == ',' and count % 2 == 0) {
                if (count != 0) {
                    subStr = '\"' + subStr + '\"';
                }
                result.push_back(subStr);
                subStr = "";
                count = 0;
            } else if (ch == '\"') {
                count++;
            } else if (ch == '\r' || ch == '\n') {
                break;
            } else {
                subStr += ch;
            }
        }
        if (subStr.empty()) {
            return result;
        }
        result.push_back(subStr);
        return result;
    }

    static std::string ToCamelCase(const std::string &str) {
        std::string res;
        if (str.empty()) {
            return res;
        }
        // 将字符串使用“_”进行分割
        std::vector<std::string> strList = StringUtil::Split(str, "_");
        if (strList.empty()) {
            return res;
        }
        res = strList[0];
        // 对第二个子串开始，如果子串不为空，则将子串的首字母变成大写
        for (size_t i = 1; i < strList.size(); ++i) {
            if (!strList[i].empty()) {
                strList[i][0] = std::toupper(strList[i][0]);
                res += strList[i];
            }
        }
        return res;
    }

    static bool CheckSqlValid(const std::string &input) {
        // 只允许 a-zA-z0-9 _ @ : -  . 不允许连续的--
        std::string pattern = "^(?!.*--)[.a-zA-Z0-9_@:-]+$";
        static std::regex regex(pattern);
        if (input.empty()) {
            return true;
        }
        // 该方法用于处理SQL参数，当前只验证字母数字下划线中划线，后续可以兼容扩展
        if (!std::regex_match(input, regex)) {
            return false;
        }
        return true;
    }

    inline static std::string DoubleToStringWithTwoDecimalPlaces(double value) {
        std::stringstream stream;
        stream << std::fixed << std::setprecision(2) << value; // 保留有效位数为2，四舍五入
        return stream.str();
    }

    static std::string GetPrintAbleString(const std::string &input) {
        std::string sanitized;
        for (const auto &item : input) {
            if (isprint(item)) {
                sanitized += item;
            } else {
                sanitized += "_";
            }
        }
        return sanitized;
    }

    /**
 * 创建占位符字符串
 *
 * @param n 占位符数量
 * @return 由n个占位符拼接而成的字符串，比如输入2，则输出为: ?,?
 */
    static std::string CreateQuestionMarkString(uint64_t n) {
        std::stringstream res;
        for (uint64_t i = 0; i < n; ++i) {
            res << "?";
            if (i < n - 1) {
                res << ",";
            }
        }
        return res.str();
    }
    template <size_t N> static std::string GenerateColumnString(const std::string_view (&cols)[N]) {
        std::vector<std::string> list(std::begin(cols), std::end(cols));
        return StringUtil::join(list, " ,");
    }
    // 分隔用圆括号括起来的用逗号分割的字符串，如"(a, b, c, d)"，生成由a,b,c,d组成的数组,字符前后的空格会被删除
    static std::vector<std::string> SplitStringWithParenthesesByComma(std::string str) {
        std::vector<std::string> subStr{};
        if (str.empty()) {
            return subStr;
        }

        str.erase(std::remove(str.begin(), str.end(), '('), str.end());
        str.erase(std::remove(str.begin(), str.end(), ')'), str.end());
        std::vector<std::string> tmpList = Split(str, ",");
        for (auto item : tmpList) {
            std::string tmpStr = Trim(item);
            if (!tmpStr.empty()) {
                subStr.push_back(tmpStr);
            }
        }
        return subStr;
    }

    static std::string JoinNumberStrWithParenthesesByOrder(const std::vector<std::string> &input) {
        std::vector<std::string> sortList = input;
        std::sort(sortList.begin(), sortList.end(),
            [](const std::string &a, const std::string &b) { return StringToInt(a) < StringToInt(b); });
        return "(" + join(sortList, ", ") + ")";
    }

    // LCOV_EXCL_BR_START
    inline static std::string StrJoin(const std::string &str) { return str; }

    template <typename... Args> inline static std::string StrJoin(const std::string &first, Args... args) {
        return first + StrJoin(args...);
    }
    // LCOV_EXCL_BR_STOP
    /**
 * 使用时传入可变长的string或string_view作为参数，使用{}作为占位符按顺序进行匹配和替换fmtStr, !!!注意在使用时需确保入参本身不存在注入问题
 *
 * @param fmtStr 模式串
 * @param stringArgs 待替换的字符串列表
 * @param errMsg 异常信息
 * @return 替换结果
 */
    template <typename... StringArgs>
    static std::string FormatString(const std::string &fmtStr, StringArgs &&...stringArgs) {
        static const std::regex placeholder_regex("\\{\\}");
        std::vector<std::string_view> strings;
        (strings.push_back(std::forward<StringArgs>(stringArgs)), ...);
        // 统计占位符数量
        std::sregex_iterator it(fmtStr.begin(), fmtStr.end(), placeholder_regex);
        std::sregex_iterator end;
        int count = std::distance(it, end);
        uint64_t placeholder_count = static_cast<uint64_t>(count > 0 ? count : 0);
        // Number of placeholders does not match the number of arguments
        if (placeholder_count != strings.size()) {
            return "";
        }
        std::string result;
        std::smatch match;
        std::string::const_iterator search_start(fmtStr.begin());
        size_t index = 0;

        while (std::regex_search(search_start, fmtStr.cend(), match, placeholder_regex)) {
            // 添加匹配前的内容
            result.append(match.prefix().first, match.prefix().second);
            // 添加替换内容
            result.append(strings[index++]);
            // 更新搜索起点
            search_start = match.suffix().first;
        }
        // 添加最后剩余部分
        result.append(search_start, fmtStr.cend());
        return result;
    }
    // 该方法用于寻找两个字符串的最长前缀, 时间复杂度为O(n)
    static std::string FindLCP(const std::string &str1, const std::string &str2) {
        size_t minLength = std::min(str1.size(), str2.size());
        size_t i = 0;
        while (i < minLength && str1[i] == str2[i]) {
            i++;
        }
        return str1.substr(0, i);
    }
    /***
 * 处理db列名，部分列名可能被反引号包裹，如 "`Table Column`" 返回给前端展示时需要处理掉``
 */
    static void StripDbColumnName(std::string &columnName) {
        const char wrapperChar = '`';
        if (columnName.length() <= 2) {
            return;
        }
        if (columnName.front() == wrapperChar && columnName.back() == wrapperChar) {
            columnName = columnName.substr(1, columnName.length() - 2);
        }
    }

    static std::string GetOriginHostName(const std::string &hostName) {
        size_t lastUnderlinePos = hostName.find_last_of('_');
        if (lastUnderlinePos != std::string::npos) {
            return hostName.substr(0, lastUnderlinePos);
        }
        return hostName;
    }

    /**
 * @brief 对两个str类型的数字做max操作
 */

    static std::string StrNumMax(const std::string &a, const std::string &b) {
        try {
            auto numA = std::stoll(a);
            auto numB = std::stoll(b);
            return numA >= numB ? a : b;
        } catch (...) {
            return a;
        }
    }

  private:
#ifdef _WIN32
    static inline char injectList[] = {
        '|', ';', '&', '$', '>', '<', '`', '!', '\n', '\"', '\'', '\t', '\r', '\f', '\u0000'};
#else
    static inline char injectList[] = {
        '|', ';', '&', '$', '>', '<', '`', '\\', '!', '\n', '\"', '\'', '\t', '\r', '\f', '\u0000'};
#endif
};

/**
 * 该方法用于切分符合RFC 4180格式的标准CSV单行（以','为分隔符）, 在行末缺失数据时也能padding一个空字符串
 * @param csvLine csv文件单行
 * @return 切分后的字符串数组
 */
[[maybe_unused]] static std::vector<std::string> SplitCsvLine(const std::string &csvLine) {
    const char sep = ',';
    std::vector<std::string> result;
    std::string field;
    bool inQuotes = false;
    size_t i = 0;
    while (i < csvLine.size()) {
        char c = csvLine[i];
        if (!inQuotes) {
            i++;
            // 当前不在引号中，只有字段开头的 " 才开启引号模式
            if (c == '"' && field.empty()) {
                inQuotes = true;
                continue;
            }
            if (c == sep) {
                result.push_back(field);
                field.clear();
                continue;
            }
            // 忽略行尾结束符如'\r''\n'
            if (c != '\r' && c != '\n') {
                field += c;
            }
            continue;
        }
        // 当前在引号中
        if (c == '"') {
            // 检查是否为转义序列
            if (i + 1 < csvLine.size() && csvLine[i + 1] == '"') {
                field += '"';
                i += 2; // 跳过""
                continue;
            }
            // 结束引号
            inQuotes = false;
        } else {
            // 引号内的普通字符均保留
            field += c;
        }
        i++;
    }
    // 处理最后一个字段（无论是否在引号中）
    result.push_back(field);
    return result;
}

}

#endif // DATA_INSIGHT_CORE_STRING_UTIL_H
