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

#ifndef PROFILER_SERVER_TIMELINEPARAMSTRCUT_H
#define PROFILER_SERVER_TIMELINEPARAMSTRCUT_H
#include <string>
#include <vector>
#include <set>

// clang-format off
namespace Dic::Protocol {
struct Filtercondition {
    std::string col;
    std::string content;
};
struct EqualCondition {
    std::string col;
    std::string content;
};
struct TableDataDetailParams {
    std::string rankId;
    uint64_t tableIndex = 0;
    uint64_t pageSize = 0;
    uint64_t currentPage = 0;
    std::string order;
    std::string orderBy;
    std::string type;
    std::vector<Filtercondition> filterconditions;
    std::vector<EqualCondition> equalConditions;
    bool CommonCheck(std::string& errorMsg) const
    {
        static const uint64_t PAGE_LIMIT = 500;
        if (pageSize == 0 || pageSize > PAGE_LIMIT) {
            errorMsg = "Page size invalid!";
            return false;
        }
        if (currentPage == 0 || currentPage - 1 > UINT64_MAX / pageSize) {
            errorMsg = "Current page invalid!";
            return false;
        }
        return true;
    }
};

struct TableDataNameListParams {
    std::string rankId;
    bool isZh = true;
};

struct Metadata {
    std::string tid;
    std::string pid;
    std::string metaType;
    std::string rankId;
    uint64_t lockStartTime = 0;
    uint64_t lockEndTime = 0;
    bool hidePythonFunction = false;
};

struct SearchAllSliceParams {
    bool isMatchCase = false;
    bool isMatchExact = false;
    std::string rankId;
    std::string searchContent;
    std::string nameFilter;  // 二级筛选关键字
    std::string orderBy;
    std::string order;
    std::string fileId;
    uint64_t current = 0;
    uint64_t pageSize = 0;
    std::vector<Metadata> metadataList;
    bool CheckParams(uint64_t minTime, std::string& warnMsg) const
    {
        for (const auto& item : metadataList) {
            if (item.lockStartTime > item.lockEndTime) {
                warnMsg = "Search all slice lock start time is bigger than lock end time";
                return false;
            }
            if (item.lockEndTime > UINT64_MAX - minTime) {
                warnMsg = "Search all slice lock end time is invalid";
                return false;
            }
        }
        if (!StringUtil::CheckSqlValid(orderBy)) {
            warnMsg = "Search all slice order column name is invalid";
            return false;
        }
        return CheckUnsignPageValid(pageSize, current, warnMsg);
    }
};
}  // namespace Dic::Protocol
// clang-format on

#endif // PROFILER_SERVER_TIMELINEPARAMSTRCUT_H
