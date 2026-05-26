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

#include "TraceTime.h"
#include "DataBaseManager.h"
#include "BaselineManager.h"
#include "MemoryProtocolRequest.h"
#include "MemoryProtocolRespose.h"
#include "QueryMemoryComponentHandler.h"

namespace Dic {
namespace Module {
namespace Memory {
bool QueryMemoryComponentHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    MemoryComponentRequest &request = dynamic_cast<MemoryComponentRequest &>(*requestPtr.get());
    std::unique_ptr<MemoryComponentComparisonResponse> responsePtr =
        std::make_unique<MemoryComponentComparisonResponse>();
    MemoryComponentComparisonResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        SetMemoryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId(request.params.rankId);
    if (!database) {
        SetMemoryError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    std::string deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(request.params.rankId);
    if (deviceId.empty()) {
        SetMemoryError(ErrorCode::GET_DEVICE_ID_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    request.params.deviceId = deviceId;
    if (!request.params.isCompare) {
        std::vector<MemoryComponent> componentDetails;
        if (!database->QueryComponentDetail(request.params, response.columnAttr, componentDetails) ||
            !database->QueryComponentsTotalNum(request.params, response.totalNum)) {
            SetMemoryError(ErrorCode::QUERY_MEMORY_COMPONENT_FAILED);
            SendResponse(std::move(responsePtr), false);
            return false;
        }
        for (const auto &item : componentDetails) {
            MemoryComponentComparison element = {item, {}, {}};
            response.componentDiffDetails.emplace_back(element);
        }
    } else {
        std::vector<MemoryComponent> compareData;
        std::vector<MemoryComponent> baselineData;
        if (!GetRespectiveData(database, compareData, baselineData, request, errorMsg)) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
        ExecuteComparisonAlgorithm(compareData, baselineData, request, response);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QueryMemoryComponentHandler::GetRespectiveData(std::shared_ptr<VirtualMemoryDataBase> database,
    std::vector<MemoryComponent> &compareData, std::vector<MemoryComponent> &baselineData,
    Dic::Protocol::MemoryComponentRequest &request, std::string &errorMsg) {
    std::string baselineId = Global::BaselineManager::Instance().GetBaselineId();
    if (baselineId == "") {
        errorMsg = "Failed to get baseline id.";
        SetMemoryError(ErrorCode::GET_BASELINE_ID_FAILED);
        return false;
    }
    auto databaseBaseline = Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId(baselineId);
    if (!databaseBaseline) {
        errorMsg = "Failed to connect to database of baseline.";
        SetMemoryError(ErrorCode::CONNECT_DATABASE_FAILED);
        return false;
    }
    uint64_t offsetTimeCompare =
        Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp(request.params.rankId);
    if (!database->QueryEntireComponentTable(request.params, compareData, offsetTimeCompare)) {
        errorMsg = "Failed to query memory component compare data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_COMPONENT_COMPARE_FAILED);
        return false;
    }
    request.params.deviceId = FullDb::DataBaseManager::Instance().GetDeviceIdFromRankId(baselineId);
    uint64_t offsetTimeBaseline = Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp(baselineId);
    if (!databaseBaseline->QueryEntireComponentTable(request.params, baselineData, offsetTimeBaseline)) {
        errorMsg = "Failed to query memory component baseline data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_COMPONENT_BASELINE_FAILED);
        return false;
    }
    return true;
}

void QueryMemoryComponentHandler::ExecuteComparisonAlgorithm(const std::vector<MemoryComponent> &compareData,
    const std::vector<MemoryComponent> &baselineData, Dic::Protocol::MemoryComponentRequest &request,
    Dic::Protocol::MemoryComponentComparisonResponse &response) {
    std::vector<MemoryComponentComparison> diffData;
    GetComponentDiff(compareData, baselineData, diffData);
    SelectResult(request, response, diffData);
}

void QueryMemoryComponentHandler::GetComponentDiff(const std::vector<MemoryComponent> &compareData,
    const std::vector<MemoryComponent> &baselineData, std::vector<MemoryComponentComparison> &diffData) {
    // 与算子比对不同，因为数据库查询时同名组件只保留一条，不需要用vector保存同一名称的组件
    std::set<std::string> componentSet;
    std::map<std::string, MemoryComponent> compareMap;
    std::map<std::string, MemoryComponent> baselineMap;
    for (const auto &item : compareData) {
        componentSet.insert(item.component);
        compareMap[item.component] = item;
    }
    for (const auto &item : baselineData) {
        componentSet.insert(item.component);
        baselineMap[item.component] = item;
    }
    MemoryComponent empty = {"", "NA", 0.0, ""};
    MemoryComponentComparison mergeResult;
    for (const auto &component : componentSet) {
        if (compareMap.find(component) == compareMap.end()) {
            Merge(empty, baselineMap[component], mergeResult);
            diffData.emplace_back(mergeResult);
            continue;
        }
        if (baselineMap.find(component) == baselineMap.end()) {
            Merge(compareMap[component], empty, mergeResult);
            diffData.emplace_back(mergeResult);
            continue;
        }
        Merge(compareMap[component], baselineMap[component], mergeResult);
        diffData.emplace_back(mergeResult);
    }
}

void QueryMemoryComponentHandler::Merge(Dic::Protocol::MemoryComponent &componentCompare,
    Dic::Protocol::MemoryComponent &componentBaseline, Dic::Protocol::MemoryComponentComparison &mergeResult) {
    mergeResult.compare = componentCompare;
    mergeResult.baseline = componentBaseline;
    const int precision = 3;
    if (!componentCompare.component.empty()) {
        mergeResult.diff.component = componentCompare.component;
    } else {
        mergeResult.diff.component = componentBaseline.component;
    }
    mergeResult.diff.timestamp = NumberUtil::StringDoubleMinus(componentCompare.timestamp, componentBaseline.timestamp);
    mergeResult.diff.totalReserved =
        NumberUtil::DoubleReservedNDigits(componentCompare.totalReserved - componentBaseline.totalReserved, precision);
}

void QueryMemoryComponentHandler::SelectResult(Dic::Protocol::MemoryComponentRequest &request,
    MemoryComponentComparisonResponse &response, std::vector<MemoryComponentComparison> &fullDiffResult) {
    SortResult(request, fullDiffResult);
    uint64_t pageSize =
        request.params.pageSize <= 0 ? DEFAULT_PAGE_SIZE : static_cast<uint64_t>(request.params.pageSize);
    uint64_t currentPage = request.params.currentPage < 1 ? 0 : static_cast<uint64_t>(request.params.currentPage - 1);
    uint64_t offset = currentPage * pageSize;
    if (offset != 0 && offset >= fullDiffResult.size()) {
        response.componentDiffDetails.clear();
    } else {
        for (size_t i = offset; i < offset + pageSize && i < fullDiffResult.size(); ++i) {
            response.componentDiffDetails.push_back(fullDiffResult[i]);
        }
    }
    response.totalNum = std::min(static_cast<int64_t>(fullDiffResult.size()), std::numeric_limits<int64_t>::max());
    for (const auto &column : tableColumnAttr) {
        response.columnAttr.emplace_back(column);
        if (column.name == "Component") {
            MemoryTableColumnAttr sourceItem = {"Source", "string", "source"};
            response.columnAttr.emplace_back(sourceItem);
        }
    }
}

void QueryMemoryComponentHandler::SortResult(
    Dic::Protocol::MemoryComponentRequest &request, std::vector<MemoryComponentComparison> &result) {
    if (request.params.order.empty() || request.params.orderBy.empty()) {
        return;
    }
    if (request.params.order == "ascend") {
        SortAscend(request, result);
    } else {
        SortDescend(request, result);
    }
}

void QueryMemoryComponentHandler::SortAscend(
    Dic::Protocol::MemoryComponentRequest &request, std::vector<MemoryComponentComparison> &result) {
    std::map<std::string, bool (*)(MemoryComponentComparison &, MemoryComponentComparison &)> compFunc = {
        {"component",
            [](MemoryComponentComparison &comp1, MemoryComponentComparison &comp2) {
                return comp1.diff.component < comp2.diff.component;
            }},
        {"timestamp",
            [](MemoryComponentComparison &comp1, MemoryComponentComparison &comp2) {
                return NumberUtil::StringToDouble(comp1.diff.timestamp) <
                    NumberUtil::StringToDouble(comp2.diff.timestamp);
            }},
        {"totalReserved", [](MemoryComponentComparison &comp1, MemoryComponentComparison &comp2) {
             return comp1.diff.totalReserved < comp2.diff.totalReserved;
         }}};
    if (compFunc.find(request.params.orderBy) != compFunc.end()) {
        std::sort(result.begin(), result.end(), compFunc[request.params.orderBy]);
    }
}

void QueryMemoryComponentHandler::SortDescend(
    Dic::Protocol::MemoryComponentRequest &request, std::vector<MemoryComponentComparison> &result) {
    std::map<std::string, bool (*)(MemoryComponentComparison &, MemoryComponentComparison &)> compFunc = {
        {"component",
            [](MemoryComponentComparison &comp1, MemoryComponentComparison &comp2) {
                return comp1.diff.component > comp2.diff.component;
            }},
        {"timestamp",
            [](MemoryComponentComparison &comp1, MemoryComponentComparison &comp2) {
                return NumberUtil::StringToDouble(comp1.diff.timestamp) >
                    NumberUtil::StringToDouble(comp2.diff.timestamp);
            }},
        {"totalReserved", [](MemoryComponentComparison &comp1, MemoryComponentComparison &comp2) {
             return comp1.diff.totalReserved > comp2.diff.totalReserved;
         }}};
    if (compFunc.find(request.params.orderBy) != compFunc.end()) {
        std::sort(result.begin(), result.end(), compFunc[request.params.orderBy]);
    }
}
} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic
