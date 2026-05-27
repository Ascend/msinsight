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

#ifndef DATA_INSIGHT_CORE_NUMBER_UTIL_H
#define DATA_INSIGHT_CORE_NUMBER_UTIL_H

#include <string>
#include <regex>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include "ServerLog.h"
#include "cmath"
#include "algorithm"
#include "NumberSafeUtil.h"

namespace Dic {
const int INVALID_NUMBER = 0xffffffff;
// 16进制
const int HEXADECIMAL = 16;
const std::string THOUSAND_SUFFIX = "000";
const int THOUSAND = 1000;
const int TWO = 2;
const double PERCENTAGE_RATIO_SCALE = 100.0;
const int64_t MAX = pow(10, 12);
constexpr int MAX_RESERVED_DIGITS = 6;
class NumberUtil {
  public:
    static inline int TryParseInt(const std::string &intStr) {
        int ret;
        try {
            ret = std::stoi(intStr);
        } catch (std::invalid_argument &) {
            // no conversion
            return INVALID_NUMBER;
        } catch (std::out_of_range &) {
            // out of range
            return INVALID_NUMBER;
        }
        return ret;
    }

    static inline unsigned long long TryParseUnsignedLongLong(const std::string &longLongStr) {
        unsigned long long ret;
        try {
            ret = std::stoull(longLongStr);
        } catch (std::invalid_argument &) {
            // no conversion
            return INVALID_NUMBER;
        } catch (std::out_of_range &) {
            // out of range
            return INVALID_NUMBER;
        }
        return ret;
    }

    static inline std::string Uint64ToHexString(uint64_t number) {
        std::ostringstream ss;
        ss << "0x" << std::hex << number;
        return ss.str();
    }

    static inline double ConvertBytesToMBytes(const uint64_t bytes) {
        constexpr double factor = 1024.0 * 1024.0;
        double mb = static_cast<double>(bytes) / factor;
        mb = std::round(mb * 1000.0) / 1000.0;
        return mb;
    }

    static inline int HexadecimalStrToDecimalInt(const std::string &hexadecimalStr) {
        try {
            int size = std::stoi(hexadecimalStr, nullptr, HEXADECIMAL);
            return size;
        } catch (std::invalid_argument &) {
            // no conversion
            return INVALID_NUMBER;
        } catch (std::out_of_range &) {
            // out of range
            return INVALID_NUMBER;
        }
    }

    static inline int64_t ConvertNumberUsStrToNanoseconds(const std::string &usString, const char *p) {
        int64_t integerPart = 0;
        int64_t fractionalPart = 0;
        int fractionalLength = 0;
        int roundingDigit = 0;
        // 十分位
        constexpr static int64_t tenthPlace = 10;
        // 千分位
        constexpr static int64_t thousandthPlace = 1000;
        // 小数位数
        constexpr static int64_t decimalPlaces = 3;
        // 四舍五入分界点
        constexpr static int64_t roundingDigitMark = 5;
        // 整数部分余量
        constexpr static int64_t integerMargin = 1;
        constexpr static int64_t integerPartLimit = INT64_MAX / thousandthPlace - integerMargin;
        // 解析整数部分
        while (*p >= '0' && *p <= '9') {
            if (integerPart > integerPartLimit) {
                return 0;
            }
            integerPart = integerPart * tenthPlace + (*p - '0');
            ++p;
        }
        // 如果遇到小数点，解析小数部分
        if (*p == '.') {
            ++p; // 跳过小数点
            while (*p >= '0' && *p <= '9' && fractionalLength < decimalPlaces) {
                fractionalPart = fractionalPart * tenthPlace + (*p - '0');
                ++p;
                ++fractionalLength;
            }

            // 检查第四位，用于四舍五入
            if (*p >= '0' && *p <= '9') {
                roundingDigit = *p - '0';
            }
        }
        // 如果小数部分不足 3 位，用 0 补齐
        while (fractionalLength < decimalPlaces) {
            fractionalPart *= tenthPlace;
            ++fractionalLength;
        }
        // 四舍五入
        if (roundingDigit >= roundingDigitMark) {
            ++fractionalPart;
        }
        // 检查是否有非法字符（不是数字或小数点）
        if (*p != '\0' && (*p < '0' || *p > '9')) {
            return 0; // 如果遇到非数字或小数点字符，返回 0
        }
        if (integerPart > integerPartLimit) {
            return 0;
        }
        // 转换为纳秒
        return integerPart * thousandthPlace + fractionalPart;
    }

    /**
     * 将微秒格式的字符串转化为整型纳秒，四舍五入处理,负数字符串或者溢出字符串返回0
     * @param usString
     * @return
     */
    static inline int64_t ConvertUsStrToNanoseconds(const std::string &usString) {
        const char *str = usString.c_str();
        const char *p = str;
        // 检查字符串是否为空或负数
        if (*p == '-' || *p == '\0') {
            return 0; // 如果是负数或空字符串，返回0
        }
        size_t ePos = usString.find_first_of("eE");
        if (ePos == std::string::npos) {
            return ConvertNumberUsStrToNanoseconds(usString, p);
        } else {
            // 科学计数法，含有e或者E
            try {
                return TimestampUsToNs(std::stold(usString));
            } catch (std::exception &e) {
                return 0;
            }
        }
    }

    static inline int64_t TimestampUsToNs(long double us) {
        // 当数字非常大时使用乘法，部分数字会损失精度，故改为字符串操作
        if (us < MAX) {
            return llroundl(us * THOUSAND);
        }
        return TimestampUsToNs(std::to_string(us));
    }

    static inline int64_t TimestampUsToNs(std::string us) {
        auto str = us.append(THOUSAND_SUFFIX);
        auto index = str.find('.');
        if (index != std::string::npos) {
            str.replace(index, 1, "");
            str.insert(index + THOUSAND_SUFFIX.length(), ".");
        }
        try {
            long double tempValue = std::stold(str);
            if (tempValue < INT64_MIN || tempValue > INT64_MAX) {
                return 0;
            }
            return llroundl(tempValue);
        } catch (std::exception &) {
            return 0;
        }
    }

    static inline int64_t TimestampUsToNsStable(std::string us) {
        us.erase(0, us.find_first_not_of(" \t\n\r"));
        us.erase(us.find_last_not_of(" \t\n\r") + 1);
        if (us.empty()) {
            return 0;
        }
        size_t dotPos = us.find('.');
        std::string integerPart;
        std::string decimalPart;

        if (dotPos == std::string::npos) {
            integerPart = us;
            decimalPart = "";
        } else {
            integerPart = us.substr(0, dotPos);
            decimalPart = us.substr(dotPos + 1);
        }

        // 截取小数前3位，不足补0
        if (decimalPart.length() < 3) {
            decimalPart.append(3 - decimalPart.length(), '0');
        } else {
            decimalPart = decimalPart.substr(0, 3); // 只取前3位
        }

        std::string nsStr = integerPart + decimalPart;

        // 使用 stringstream 安全转 int64_t 避免 stoll/stold 精度问题
        try {
            std::istringstream iss(nsStr);
            int64_t result;
            iss >> result;
            if (iss.fail() || !iss.eof()) {
                return 0;
            }
            return result;
        } catch (...) {
            return 0;
        }
    }

    /***
     * 用于预处理数字字符串，找到头尾第一个数字字符并剪裁
     * @attention：该方法无法处理数字字符中夹杂非法数字字符场景！如"123a.456"无法被处理
     * @param numericStr 待预处理字符串
     * @return 预处理后字符串
     */
    static inline std::string TrimNumericString(const std::string &numericStr) {
        if (numericStr.empty()) {
            return "0";
        }
        size_t totalSize = numericStr.size();
        size_t start = 0;
        while (start < totalSize && (numericStr[start] - '0' < 0 || numericStr[start] - '0' > 9)) {
            if (numericStr[start] == '-' || numericStr[start] == '+') {
                break;
            }
            start++;
        }
        if (start >= totalSize) {
            return "0";
        }
        size_t end = totalSize;
        while (end > start && (numericStr[end - 1] - '0' < 0 || numericStr[end - 1] - '0' > 9)) {
            end--;
        }
        return numericStr.substr(start, end - start);
    }

    static inline double StringToDouble(const std::string &usStr, bool trim = false) {
        if (usStr.empty()) {
            return 0;
        }
        try {
            return std::stod(trim ? TrimNumericString(usStr) : usStr);
        } catch (std::exception &) {
            return 0;
        }
    }

    static inline long double StringToLongDouble(const std::string &usStr, bool trim = false) {
        if (usStr.empty()) {
            return 0;
        }
        try {
            return std::stold(trim ? TrimNumericString(usStr) : usStr);
        } catch (std::invalid_argument &e) {
            Server::ServerLog::Error("Value out of range: ", e.what());
            return 0;
        } catch (std::out_of_range &e) {
            Server::ServerLog::Error("Out of range: ", e.what());
            return 0;
        }
    }

    static inline int StringToInt(const std::string &usStr) {
        if (usStr.empty()) {
            return 0;
        }
        try {
            return std::stoi(usStr);
        } catch (std::exception &) {
            return 0;
        }
    }

    static inline long StringToLong(const std::string &usStr) {
        if (usStr.empty()) {
            return 0;
        }
        try {
            return std::stol(usStr);
        } catch (std::exception &) {
            return 0;
        }
    }

    static inline long long StringToLongLong(const std::string &usStr) {
        if (usStr.empty()) {
            return 0;
        }
        try {
            return std::stoll(usStr);
        } catch (std::exception &) {
            return 0;
        }
    }

    static inline unsigned long long StringToUnsignedLongLong(const std::string &usStr) {
        if (usStr.empty()) {
            return 0;
        }
        try {
            return std::stoull(usStr);
        } catch (std::exception &) {
            return 0;
        }
    }

    static std::string StringUnsignedLongLongMinus(const std::string &str1, const std::string &str2) {
        uint64_t num1 = StringToUnsignedLongLong(str1);
        uint64_t num2 = StringToUnsignedLongLong(str2);
        if (num1 >= num2 && num1 - num2 > static_cast<uint64_t>(INT64_MAX)) {
            return std::to_string(INT64_MAX);
        }
        if (num1 < num2 && num2 - num1 > static_cast<uint64_t>(INT64_MAX)) {
            return std::to_string(INT64_MIN);
        }
        int64_t res;
        if (num1 >= num2) {
            res = static_cast<int64_t>(num1 - num2);
        } else {
            res = -static_cast<int64_t>(num2 - num1);
        }
        return std::to_string(res);
    }

    static inline uint32_t StringToUint32(const std::string &usStr) {
        if (usStr.empty()) {
            return 0;
        }
        try {
            int temp = std::stoi(usStr);
            if (temp < 0) {
                return 0;
            }
            return static_cast<uint32_t>(temp);
        } catch (std::exception &) {
            return 0;
        }
    }

    static inline std::string StringDoubleMinus(const std::string &str1, const std::string &str2, int precision = 3) {
        long double num1 = StringToLongDouble(str1);
        long double num2 = StringToLongDouble(str2);
        long double difference = num1 - num2;
        std::stringstream sstream;
        sstream << std::fixed << std::setprecision(precision) << difference;
        return sstream.str();
    }

    /**
     * double字符串减法，保留小鼠点后有效数字
     * @param str1 double字符串1
     * @param str2 double字符串2
     * @param precision 有效数字保留位数
     * @return 减法结果
     */
    static inline std::string StringDoubleMinusKeepSf(
        const std::string &str1, const std::string &str2, int precision = 3) {
        long double num1 = StringToLongDouble(str1);
        long double num2 = StringToLongDouble(str2);
        long double difference = num1 - num2;
        std::stringstream sstream;
        // 获取差值的整数部分，如果是正数则向下取整，如果是负数则向上取整
        long double diffRound = difference > 0 ? std::floor(difference) : std::ceil(difference);
        // 获取差值小数部分
        long double decimal = difference - diffRound;
        int finalPrecision = precision;
        // 如果小数部分存在，则重新计算保留小数位
        if (decimal != 0.0) {
            // 获取差值的小数部分，取绝对值，再取对数
            int zeroNumber = static_cast<int>(std::log10(std::abs(decimal)));
            // 重新计算需要保留的位数：如果对数小于等于0，说明小数点后面存在0，则保留位数加上0的个数
            finalPrecision = zeroNumber >= 0 ? finalPrecision : finalPrecision - zeroNumber;
        }
        sstream << std::fixed << std::setprecision(finalPrecision) << difference;
        return sstream.str();
    }

    /**
     * 去除浮点数字符串后缀0
     * @param numStr 浮点数字符串
     * @return 去除后缀0的结果
     */
    static inline std::string RemoveTrailingZerosAndDecimal(const std::string &numStr) {
        // 找到最后一个非零字符的位置
        size_t pos = numStr.find_last_not_of('0');
        // 如末尾没有零字符，直接返回
        if (pos == numStr.length() - 1) {
            return numStr;
        }
        // 如果所有字符都是零，则返回0
        if (pos == std::string::npos) {
            return "0";
        }
        // 检查小数点后的零是否都是零
        if (numStr[pos] == '.') {
            return numStr.substr(0, pos);
        }
        // 返回去除末尾0的结果
        return numStr.substr(0, pos + 1);
    }

    /**
     * 字符串浮点数减法（去除后缀0）
     * @param str1 被减数
     * @param str2 减数
     * @param maxPrecision 最大保留位数
     * @return 减法结果
     */
    static inline std::string StringDoubleMinusWithoutTrailingZero(
        const std::string &str1, const std::string &str2, int maxPrecision = 3) {
        if (str1.empty() || str2.empty()) {
            return "";
        }
        std::string minusRes = StringDoubleMinusKeepSf(str1, str2, maxPrecision);
        return RemoveTrailingZerosAndDecimal(minusRes);
    }

    // 只处理1~6位小数位的截尾
    static inline double DoubleReservedNDigits(double data, int n = 6) {
        if (n <= 0 || n > MAX_RESERVED_DIGITS) { // 最多处理6位小数位
            return data;
        }

        int ratio = 1;
        for (int i = 0; i < n; ++i) {
            ratio *= 10; // 10进制
        }
        return static_cast<double>(std::round(data * ratio) / ratio);
    }

    static inline bool IsDouble(const std::string &str) {
        std::istringstream iss(str);
        double d;
        return (iss >> d) && (iss.eof());
    }

    inline static bool IsStr2DoubleDesc(const std::string &a, const std::string &b) {
        bool isADouble = IsDouble(a);
        bool isBDouble = IsDouble(b);
        // 如果都是数字，则按数值排序
        if (isADouble && isBDouble) {
            return std::stod(a) > std::stod(b);
        }
        // 如果只有一个是数字，数字排在前面
        if (isADouble) {
            return true;
        }
        if (isBDouble) {
            return false;
        }
        // 都不是数字，保持原有顺序
        return false;
    }

    static bool IsStr2DoubleAsce(const std::string &a, const std::string &b) {
        bool isADouble = IsDouble(a);
        bool isBDouble = IsDouble(b);
        // 如果都是数字，则按数值排序
        if (isADouble && isBDouble) {
            return std::stod(a) < std::stod(b);
        }
        // 如果只有一个是数字，数字排在前面
        if (isADouble) {
            return true;
        }
        if (isBDouble) {
            return false;
        }
        // 都不是数字，保持原有顺序
        return false;
    }

    static inline double Sub(double a, double b) {
        return DoubleReservedNDigits(DoubleReservedNDigits(a) - DoubleReservedNDigits(b));
    }

    static inline std::string StrReservedNDigits(const std::string &data, int n) {
        if (n <= 0 || n > MAX_RESERVED_DIGITS) {
            return data;
        }
        auto pos = data.find_last_of('.');
        if (pos != std::string::npos) {
            std::string sub = data.substr(0, pos + n + 1);
            // 保留小数后为零
            bool isZero = std::all_of(sub.begin(), sub.end(), [](char c) { return (c == '0' || c == '.'); });
            return isZero ? data : sub;
        }
        return data;
    }

    static inline bool IsGreater(float a, float b, float epsilon = 1e-9) {
        if (std::fabs(a - b) < epsilon) { // 如何两个浮点数只差小于epsilon, 则认为两数相等
            return false;
        } else {
            return a > b;
        }
    }

    static inline bool IsDoubleEqual(double a, double b, double epsilon = 1e-9) { return std::fabs(a - b) < epsilon; }

    static inline bool IsEqual(float a, float b, float epsilon = 1e-9) {
        return std::fabs(a - b) < epsilon; // 如何两个浮点数只差小于epsilon, 则认为两数相等
    }

    template <typename T> static inline T CeilingClamp(T value, T bound = std::numeric_limits<T>::max()) {
        // 此处为条件预测，能提高效率
        if (__builtin_expect(bound > value, 1)) {
            return value;
        } else {
            return bound;
        }
    }
    // 此方法用于取数值类型的整数部分(主要为浮点数,整数部分不可超过INT64范围)
    static std::string TruncateNumberString(const std::string &numberStr) {
        std::string s = numberStr;
        // 去除前后空格
        s.erase(0, s.find_first_not_of(" \t\n\r"));
        s.erase(s.find_last_not_of(" \t\n\r") + 1);

        // 正则表达式匹配合法的数字：支持正负号、小数点、整数和浮点数
        std::regex pattern(R"([+-]?(\d+\.?\d*|\.\d+))");
        if (std::regex_match(s, pattern)) {
            long double num = StringToLongDouble(numberStr);
            int64_t truncated;
            if (num > INT64_MAX) {
                truncated = INT64_MAX;
            } else if (num < INT64_MIN) {
                truncated = INT64_MIN;
            } else {
                // 截断取整
                truncated = static_cast<int64_t>(num);
            }
            return std::to_string(truncated);
        }

        return "0";
    }

    static inline uint32_t IntToUint32(const int num) {
        if (num < 0) {
            return 0;
        }
        return static_cast<uint32_t>(num);
    }

    static inline uint64_t Int64ToUint64(const int64_t num) {
        if (num < 0) {
            return 0;
        }
        return static_cast<uint64_t>(num);
    }
};
} // end of namespace Dic
#endif // DATA_INSIGHT_CORE_NUMBER_UTIL_H
