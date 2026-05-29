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

#ifndef PROFILER_SERVER_COMMONREQUESTS_H
#define PROFILER_SERVER_COMMONREQUESTS_H

#include "JsonUtil.h"
#include "ProtocolMessage.h"

namespace Dic::Protocol {

struct TableViewColumn {
    std::string_view name;
    std::string_view
        key; // ！！！注意，需要计算/转换的列，建议key与数据库表列名不要一致，否则可能导致where条件时以计算/转换前的值来判断
    bool visible{true}; // 是否可见
    bool sortable{false}; // 是否可排序
    bool searchable{false}; // 是否可搜索
    bool rangeFilterable{false}; // 是否可范围过滤

    TableViewColumn(
        std::string_view name, std::string_view key, bool visible, bool sortable, bool searchable, bool rangeFilterable)
        : name(name), key(key), visible(visible), sortable(sortable), searchable(searchable),
          rangeFilterable(rangeFilterable) {}

    // 简单不可见列
    TableViewColumn(std::string_view name, std::string_view key) : name(name), key(key), visible(false) {}

    json_t ToTableHeaderJson(MemoryPoolAllocator<> &allocator) const;
    static json_t CommonBuildTableHeadersJson(
        MemoryPoolAllocator<> &allocator, const std::vector<TableViewColumn> &columns);
};

class PaginationParam {
  public:
    int64_t currentPage{};
    int64_t pageSize{};

    bool Check(std::string &errorMsg) const {
        if (pageSize == 0 && currentPage == 0) {
            return true;
        }
        if (!Dic::Protocol::CheckPageValid(pageSize, currentPage, errorMsg)) {
            errorMsg = "Invalid pagination params, detail: " + errorMsg;
            return false;
        }
        return true;
    }

    void SetPaginationParamFromJson(const json_t &json) {
        JsonUtil::SetByJsonKeyValue(currentPage, json, "currentPage");
        JsonUtil::SetByJsonKeyValue(pageSize, json, "pageSize");
    }
};

using SearchMap = std::map<std::string, std::string>;
class FiltersParam {
  public:
    SearchMap filters;

    bool SetFiltersFromJson(const json_t &json, const std::vector<TableViewColumn> &columns, std::string &errorMsg);
};

class OrderByParam {
  public:
    std::string orderBy;
    bool desc{};

    bool SetOrderFromJson(const json_t &json, const std::vector<TableViewColumn> &columns, std::string &errorMsg);
};

using RangeMap = std::map<std::string, std::pair<double, double>>;
class RangeFiltersParam {
  public:
    RangeMap rangeFilters;

    bool SetRangeFiltersFromJson(
        const json_t &json, const std::vector<TableViewColumn> &columns, std::string &errorMsg);
};

class CommonTableParams : public PaginationParam, public FiltersParam, public OrderByParam, public RangeFiltersParam {
  public:
    bool SetFromJson(const json_t &json, const std::vector<TableViewColumn> &columns, std::string &errorMsg);
};

inline std::vector<TableViewColumn>::const_iterator FindColumnByKey(
    std::string_view key, const std::vector<TableViewColumn> &columns) {
    return std::find_if(columns.begin(), columns.end(), [key](const TableViewColumn &col) { return key == col.key; });
}

}
#endif // PROFILER_SERVER_COMMONREQUESTS_H
