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

#ifndef PROFILER_SERVER_BASEDOMAIN_H
#define PROFILER_SERVER_BASEDOMAIN_H
#include <vector>
#include <string>
#include <map>
namespace Dic::Module {
struct ColumnAtt {
    std::string name;
    std::string type;
    std::string key;
};

struct LinkInfo {
    std::string tableName;
    std::string col;
};

struct TableData {
    std::vector<ColumnAtt> att;
    std::vector<std::map<std::string, std::string>> dataItems;
    uint64_t count = 0;
};
struct PageFilter {
    std::string col;
    std::string content;
};
struct EqualFilter {
    std::string col;
    std::string content;
};
struct PageQuery {
    uint64_t size = 0;
    uint64_t curPage = 0;
    std::string fileId;
    std::string orderBy;
    std::string order;
    std::string viewName;
    std::string start;
    std::string end;
    std::vector<PageFilter> pageFilters;
    std::vector<EqualFilter> equalFilters;
    uint64_t ComputeOffset() const {
        if (curPage == 0 || size == 0) {
            return 0;
        }
        if (curPage - 1 > UINT64_MAX / size) {
            return 0;
        }
        return (curPage - 1) * size;
    }
};
} // namespace Dic::Module
#endif // PROFILER_SERVER_BASEDOMAIN_H
