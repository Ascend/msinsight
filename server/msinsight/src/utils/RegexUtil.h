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

#ifndef DATA_INSIGHT_CORE_REGEX_UTIL_H
#define DATA_INSIGHT_CORE_REGEX_UTIL_H

#include <regex>
#include <optional>
#include <string>

namespace Dic {
class RegexUtil {
  public:
    static inline std::optional<std::smatch> RegexSearch(const std::string &data, const std::string &pattern) {
        try {
            std::regex regex(pattern);
            std::smatch result;
            if (!std::regex_search(data, result, regex)) {
                return std::nullopt;
            }
            return result;
        } catch (std::exception &e) {
            return std::nullopt;
        }
    }

    static inline std::optional<std::smatch> RegexMatch(const std::string &data, const std::string &pattern) {
        try {
            std::regex regex(pattern);
            std::smatch result;
            if (!std::regex_match(data, result, regex)) {
                return std::nullopt;
            }
            return result;
        } catch (const std::regex_error &e) {
            return std::nullopt;
        }
    }
};
} // end of namespace Dic
#endif // DATA_INSIGHT_CORE_REGEX_UTIL_H
