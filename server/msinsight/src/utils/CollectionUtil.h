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

#ifndef PROFILER_SERVER_COLLECTIONUTIL_H
#define PROFILER_SERVER_COLLECTIONUTIL_H
#include <vector>
#include <set>
#include <unordered_set>

namespace Dic {
class CollectionUtil {
  public:
    template <typename T>
    static inline T FindValueByKey(
        const std::map<std::string, T> &info, const std::string &key, const T &defaultValue) {
        auto it = info.find(key);
        if (it == info.end()) {
            return defaultValue;
        }
        return it->second;
    }

    static inline bool IsEleInContainer(const std::string &ele, const std::vector<std::string> &container) {
        return std::find(container.begin(), container.end(), ele) != container.end();
    }
    /**
     * 模板方法，求两个基础类型集合的差集
     *
     * @tparam T 模板类型
     * @param sourceCollection 源集合
     * @param subCollection 减去元素的集合
     * @return
     */
    template <typename T>
    static inline std::vector<T> CalDifferenceVector(
        const std::vector<T> &sourceCollection, const std::vector<T> &subCollection) {
        std::set<T> set2(subCollection.begin(), subCollection.end());
        std::vector<T> result;

        // 遍历sourceCollection中的每个元素
        for (const T &element : sourceCollection) {
            // 如果subCollection的集合中不存在当前元素，则添加到结果中
            if (set2.find(element) == set2.end()) {
                result.push_back(element);
            }
        }

        return result;
    }

    template <typename T>
    static inline std::vector<T> CalIntersection(const std::vector<T> &vec1, const std::vector<T> &vec2) {
        std::unordered_set<T> set1(vec1.begin(), vec1.end());
        std::vector<T> result;

        for (const auto &item : vec2) {
            if (set1.count(item) > 0) {
                result.push_back(item);
                set1.erase(item);
            }
        }
        return result;
    }

    // 检查类型T是否支持 == 操作符
    template <typename T>
    struct has_equal_operator
        : std::integral_constant<bool, !std::is_same<decltype(std::declval<T>() == std::declval<T>()), void>::value> {};

    // 模板函数，仅当T具备hash属性才可用
    template <typename T, typename = typename std::enable_if<has_equal_operator<T>::value>::type>
    static inline bool IsVectorEqualIgnoreOrder(std::vector<T> const &vec1, std::vector<T> const &vec2) {
        // 如果长度不同，直接返回false
        if (vec1.size() != vec2.size()) {
            return false;
        }

        std::unordered_map<T, int> count;
        for (const auto &i : vec1) {
            ++count[i];
        }

        for (const auto &i : vec2) {
            if (count[i] == 0) {
                return false;
            }
            --count[i];
        }
        return true;
    }

    static const inline std::string EMPTY_STRING = "";
};
}
#endif // PROFILER_SERVER_COLLECTIONUTIL_H
