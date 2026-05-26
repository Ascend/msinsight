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

#ifndef PROFILER_SERVER_TIMELINEBODYSTRUCT_H
#define PROFILER_SERVER_TIMELINEBODYSTRUCT_H
#include <string>
#include <map>
#include <vector>
namespace Dic::Protocol {
struct TableColumn {
    std::string name;
    std::string type;
    std::string key;
};

struct TableDataDatail {
    std::vector<TableColumn> columnAttr;
    std::vector<std::map<std::string, std::string>> columnData;
    uint64_t totalNum = 0;
};

struct TableDataListBody {
    std::vector<std::pair<std::string, std::string>> layers;
};

struct SearchAllSlices {
    std::string name;
    uint64_t timestamp = 0;
    uint64_t duration = 0;
    std::string id;
    std::string pid;
    std::string tid;
    uint64_t depth = 0;
    std::string rankId;
    std::string deviceId;
    std::string fileId;
    std::string metaType;
};

struct SearchAllSlicesBody {
    std::vector<SearchAllSlices> searchAllSlices;
    std::string dbPath;
    uint64_t count{};
    uint64_t pageSize{};
    uint64_t currentPage{};
};

template <typename RecordType> struct PaginatedBody {
    std::vector<RecordType> sameOperatorsDetails; // 记录
    std::string rankId;
    uint64_t count{}; // 记录总数
    uint64_t pageSize{};
    uint64_t currentPage{};
};

struct SameOperatorsDetails {
    uint64_t timestamp{};
    uint64_t duration{};
    // id、depth用于支持选中列表;
    std::string id;
    // name用于支持overall metric more details列表
    std::string name;
    uint64_t depth{};
    std::string tid;
    std::string pid;
};

struct SameMemcpyDetails {
    uint64_t timestamp{};
    uint64_t duration{};
    uint64_t size{};
    std::string id;
    std::string name;
};

using UnitThreadsOperatorsBody = PaginatedBody<SameOperatorsDetails>;
using MemcpyDetailsBody = PaginatedBody<SameMemcpyDetails>;

struct ThreadDetail {
    uint64_t selfTime = 0;
    uint64_t duration = 0;
    uint64_t rawTimestamp = 0;
    uint64_t rawEndstamp = 0;
    std::string args;
    std::string title;
    std::string cat;
    std::string inputShapes;
    std::string inputDataTypes;
    std::string inputFormats;
    std::string outputShapes;
    std::string outputDataTypes;
    std::string outputFormats;
    std::string attrInfo;
};
} // namespace Dic::Protocol
#endif // PROFILER_SERVER_TIMELINEBODYSTRUCT_H
