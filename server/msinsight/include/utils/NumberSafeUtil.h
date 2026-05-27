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
#ifndef PROFILER_SERVER_NUMBERSAFEUTIL_H
#define PROFILER_SERVER_NUMBERSAFEUTIL_H

#include <iostream>
#include <type_traits>
#include <typeinfo>
#include <limits>
#include <bitset>
#include <utility>
#include <unordered_map>
#include <cmath>

namespace NumberSafe {
template <typename T> struct BaseType {
    using type = typename std::remove_cv<typename std::remove_reference_t<T>>::type;
};

template <typename T> using BaseType_t = typename BaseType<T>::type;

/**
 * @brief 通过位运算比较的s中是否有超出T类型内存返回的值
 */
template <typename S, typename T> bool bitUpper(S &&s) {
    static_assert(std::is_integral_v<S> && std::is_integral_v<T>, "Only used on two int");
    if constexpr (sizeof(S) <= sizeof(T)) {
        return false;
    }
    using SType = BaseType_t<S>;
    using DType = BaseType_t<T>;
    if constexpr (std::is_unsigned_v<DType>) {
        return s > static_cast<SType>(std::numeric_limits<DType>::max());
    } else {
        return s > static_cast<SType>(std::numeric_limits<DType>::max()) * 2;
    }
}

/**
 * @brief 检查符号位是否为0
 */
template <typename T> bool IsSignBitZero(T &&a) {
    using BaseType = BaseType_t<T>;
    if constexpr (std::is_unsigned_v<BaseType>) {
        return a <= std::numeric_limits<BaseType>::max() / 2;
    } else {
        return a >= 0;
    }
}

/**
 * @brief 检查整型转浮点类型是否成功
 */
template <typename S, typename D> bool IsIntegralFloatConversionSafe(const S &&src) {
    using SS = BaseType_t<S>;
    using DD = BaseType_t<D>;
    if constexpr (std::is_integral_v<SS> && std::is_floating_point_v<DD>) {
        return static_cast<D>(src) >= -std::numeric_limits<D>::max() &&
            static_cast<D>(src) <= std::numeric_limits<D>::max();
    }
    return false;
}

/**
 * @brief 检查浮点类型之间的转换
 */
template <typename S, typename D> bool IsFloatFloatConversionSafe(const S &&src) {
    using SS = BaseType_t<S>;
    using DD = BaseType_t<D>;
    if constexpr (std::is_floating_point_v<SS> && std::is_floating_point_v<DD>) {
        if (std::isnan(src) || std::isinf(src)) {
            return false;
        }
        if (std::numeric_limits<SS>::digits < std::numeric_limits<DD>::digits &&
            std::numeric_limits<SS>::max_exponent < std::numeric_limits<DD>::max_exponent) {
            return true;
        }
        if (sizeof(SS) > sizeof(DD)) {
            return src <= (std::numeric_limits<DD>::max()) || src >= std::numeric_limits<DD>::min();
        }
    }
    return false;
}

/**
 * @brief 检查S类型的参数能否被安全转换为T类型, 安全指不发生溢出，回绕，其它未定义行为，精度损失不在此列
 */
template <typename F, typename T> bool IsSafeCast(const F &s) {
    using S = BaseType_t<F>;
    using D = BaseType_t<T>;
    auto src = static_cast<S>(s);
    (void)src;
    if constexpr (std::is_same_v<S, D>) {
        return true;
    }
    if constexpr (std::is_unsigned_v<S> && std::is_unsigned_v<D>) { // 两个类型都为非负整型
        if (sizeof(S) < sizeof(D)) {
            return true; // 源数据类型大小小于模板大小,必定安全转换
        }
        return !bitUpper<S, D>(std::forward<S>(src)); // 检查是否有超出目标类型大小范围的非0位，检查是否会丢失数据
    }
    if constexpr ((std::is_integral_v<S> && std::is_unsigned_v<D>) // 整型和非负整型
        || (std::is_unsigned_v<S> && std::is_integral_v<D>)) {
        if (sizeof(S) < sizeof(D)) {
            return true; //  同上
        }
        if (!IsSignBitZero<S>(std::forward<S>(src))) {
            return false; // 符号位/最高位为1, 转换失败
        }
        return !bitUpper<S, D>(std::forward<S>(src)); // 检查是否有超出目标类型大小的非0位
    }
    if (IsFloatFloatConversionSafe<S, D>(std::forward<S>(src))) {
        return true;
    }
    if (IsIntegralFloatConversionSafe<S, D>(std::forward<S>(src))) {
        return true;
    }
    return std::is_convertible_v<S, D>; // 默认情况，调用STL库函数判断
}

/**
 * @brief: 翻转符号位
 */
template <typename T> auto Flip(T &&a) {
    using BType = BaseType_t<T>;
    static_assert(std::is_integral_v<BType>, "Only integral type can flip");
    static_assert(!std::is_unsigned_v<BType>, "Unsigned type can't flip");
    BType num = a;
    if (IsSignBitZero(std::forward<BType>(a))) {
        num = ~num + 1;
    } else {
        num = ~(num - 1);
    }
    return num;
}

template <typename T, typename F> struct OpInter {
    using addType = decltype(std::declval<T>() + std::declval<F>());
    using mulType = decltype(std::declval<T>() * std::declval<F>());
    using divType = decltype(std::declval<T>() / std::declval<F>());
};

template <typename T, typename F> using OpInterAddType = typename OpInter<T, F>::addType;

template <typename T, typename F> using OpInterMulType = typename OpInter<T, F>::mulType;

template <typename T, typename F> using OpInterDivType = typename OpInter<T, F>::divType;

/**
 * @brief 加法
 * @attention 0 作为异常时的返回值
 */
template <typename T, typename F> static inline auto Add(T &&a, F &&b) {
    using LType = BaseType_t<T>;
    using RType = BaseType_t<F>;
    using InterType = OpInterAddType<LType, RType>;
    // 如果有一个操作数不能安全转换为InterType，则认为失败
    if (!IsSafeCast<LType, InterType>(a) || !IsSafeCast<RType, InterType>(b)) {
        return static_cast<InterType>(0);
    }
    auto l = static_cast<InterType>(a);
    auto r = static_cast<InterType>(b);
    // 检查是否会发生翻转或溢出
    if (r > 0 && l > std::numeric_limits<InterType>::max() - r) {
        return static_cast<InterType>(0);
    }
    InterType temp = l + r;
    return temp;
}

template <typename T, typename F> static inline auto Sub(const T &a, const F &b) {
    using LType = BaseType_t<T>;
    using RType = BaseType_t<F>;
    using InterType = OpInterAddType<LType, RType>;
    if constexpr (std::is_unsigned_v<LType> && std::is_unsigned_v<RType>) { // 两个非负数相减，比较大小
        if (static_cast<InterType>(a) < static_cast<InterType>(b)) {
            return static_cast<InterType>(0);
        }
        return static_cast<InterType>(a - b);
    }
    if constexpr (std::is_floating_point_v<InterType>) { // 浮点数
        return Add(static_cast<InterType>(a), static_cast<InterType>(-1 * b));
    }
    if constexpr ((std::is_integral_v<RType> && !std::is_unsigned_v<RType>) || std::is_floating_point_v<RType>) {
        // 整型或浮点型，a - b = a + (-b)
        F flipLeft = static_cast<F>(-1) * b;
        if (std::is_unsigned_v<InterType> && !IsSafeCast<RType, InterType>(flipLeft)) {
            InterType t = a - b;
            if (t > a) {
                return static_cast<InterType>(0);
            }
            return t;
        }
        auto Lop = static_cast<InterType>(a);
        auto Rop = static_cast<InterType>(flipLeft);
        return Add(Lop, Rop);
    }
    InterType res = a - b;
    if (static_cast<InterType>(a) < res) {
        return static_cast<InterType>(0);
    }
    return res;
}

template <typename T, typename F> static inline auto Division(T &&a, F &&b) {
    using LType = BaseType_t<T>;
    using RType = BaseType_t<F>;
    using InterType = OpInterMulType<LType, RType>;
    // 防止除0异常
    if (b == 0) {
        return static_cast<InterType>(0);
    }
    if (std::is_floating_point_v<InterType> && std::fabs(b) < std::numeric_limits<InterType>::epsilon()) {
        return static_cast<InterType>(0);
    }
    if (!IsSafeCast<LType, InterType>(a) || !IsSafeCast<RType, InterType>(b)) {
        return static_cast<InterType>(0);
    }
    // overflow check
    auto Lop = static_cast<InterType>(a);
    auto Rop = static_cast<InterType>(b);
    if (std::is_integral_v<InterType> && !std::is_unsigned_v<InterType>) {
        if (Lop == std::numeric_limits<InterType>::min() && Rop == -1) {
            return static_cast<InterType>(0);
        }
    }
    InterType tmp = static_cast<InterType>(a) / static_cast<InterType>(b);
    if (std::is_floating_point_v<InterType> && (std::isinf(tmp) || std::isnan(tmp))) {
        return static_cast<InterType>(0);
    }
    return tmp;
}

template <typename T, typename F> static inline auto Muls(T &&a, F &&b) {
    using LType = BaseType_t<T>;
    using RType = BaseType_t<F>;
    using InterType = OpInterMulType<LType, RType>;
    if (a == 0 || b == 0) {
        return static_cast<InterType>(0);
    }
    if (!IsSafeCast<LType, InterType>(a) || !IsSafeCast<RType, InterType>(b)) {
        return static_cast<InterType>(0);
    }
    auto Lop = static_cast<InterType>(a);
    auto Rop = static_cast<InterType>(b);
    // 溢出检测
    if (std::is_unsigned_v<InterType> && Lop > std::numeric_limits<InterType>::max() / Rop) {
        return static_cast<InterType>(0);
    } else if (std::is_integral_v<InterType>) {
        if (Lop > std::numeric_limits<InterType>::max() / Rop || Lop < std::numeric_limits<InterType>::min() / Rop) {
            return static_cast<InterType>(0);
        }
    }
    InterType tmp = Lop * Rop;
    // 浮点型判断是否异常值
    if (std::is_floating_point_v<InterType> && std::isinf(tmp)) {
        return static_cast<InterType>(0);
    }
    if (b > 1 && tmp < static_cast<InterType>(a)) {
        return static_cast<InterType>(0);
    }
    return tmp;
}

static inline int64_t SafeCastSizeTypeToInt64(size_t sizeTypeNum) {
    return static_cast<uint64_t>(sizeTypeNum) > static_cast<uint64_t>(INT64_MAX) ? INT64_MAX
                                                                                 : static_cast<int64_t>(sizeTypeNum);
}
}
#endif // PROFILER_SERVER_NUMBERSAFEUTIL_H
