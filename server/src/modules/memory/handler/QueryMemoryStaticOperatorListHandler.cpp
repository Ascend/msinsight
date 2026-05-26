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
#include "WsSessionManager.h"
#include "DataBaseManager.h"
#include "BaselineManager.h"
#include "MemoryProtocolRequest.h"
#include "MemoryProtocolRespose.h"
#include "QueryMemoryStaticOperatorListHandler.h"

namespace Dic {
namespace Module {
namespace Memory {
using namespace Dic::Server;
bool QueryMemoryStaticOperatorListHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<MemoryStaticOperatorListRequest &>(*requestPtr.get());
    std::unique_ptr<MemoryStaticOperatorListCompResponse> responsePtr =
        std::make_unique<MemoryStaticOperatorListCompResponse>();
    MemoryStaticOperatorListCompResponse &response = *responsePtr.get();
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
        SendResponse(std::move(responsePtr), false, "Failed to connect to database.");
        return false;
    }
    response.isCompare = request.params.isCompare;
    if (!request.params.isCompare) {
        std::vector<StaticOperatorItem> opDetails;
        response.totalNum = database->QueryStaticOperatorList(request.params, opDetails);
        if (response.totalNum < 0) {
            SetMemoryError(ErrorCode::QUERY_MEMORY_STATIC_OPERATOR_FAILED);
            SendResponse(std::move(responsePtr), false, "Failed to query memory static operator data.");
            return false;
        }
        for (const auto &item : opDetails) {
            StaticOperatorCompItem element = {item, {}, {}};
            response.operatorDiffDetails.emplace_back(element);
        }
    } else {
        std::vector<StaticOperatorItem> compareData;
        std::vector<StaticOperatorItem> baselineData;
        if (!GetRespectiveData(database, compareData, baselineData, request, errorMsg)) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
        ExecuteComparisonAlgorithm(compareData, baselineData, request, response);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QueryMemoryStaticOperatorListHandler::GetRespectiveData(std::shared_ptr<VirtualMemoryDataBase> database,
    std::vector<StaticOperatorItem> &compareData, std::vector<StaticOperatorItem> &baselineData,
    Dic::Protocol::MemoryStaticOperatorListRequest &request, std::string &errorMsg) {
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
    if (!database->QueryEntireStaticOperatorTable(request.params, compareData)) {
        errorMsg = "Failed to query memory static operator compare data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_STATIC_OPERATOR_COMPARE_FAILED);
        return false;
    }
    if (!databaseBaseline->QueryEntireStaticOperatorTable(request.params, baselineData)) {
        errorMsg = "Failed to query memory static operator baseline data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_STATIC_OPERATOR_BASELINE_FAILED);
        return false;
    }
    return true;
}

void QueryMemoryStaticOperatorListHandler::ExecuteComparisonAlgorithm(
    const std::vector<StaticOperatorItem> &compareData, const std::vector<StaticOperatorItem> &baselineData,
    Dic::Protocol::MemoryStaticOperatorListRequest &request, MemoryStaticOperatorListCompResponse &response) {
    std::vector<StaticOperatorCompItem> fullDiffResult;
    GetOperatorDiff(compareData, baselineData, fullDiffResult);
    SelectDiffResult(request, response, fullDiffResult);
}

void QueryMemoryStaticOperatorListHandler::GetOperatorDiff(const std::vector<StaticOperatorItem> &compareData,
    const std::vector<StaticOperatorItem> &baselineData, std::vector<StaticOperatorCompItem> &diffData) {
    std::set<std::string> opNameSet;
    std::map<std::string, std::vector<StaticOperatorItem>> compareList;
    std::map<std::string, std::vector<StaticOperatorItem>> baselineList;
    for (const auto &item : compareData) {
        opNameSet.insert(item.opName);
        // 即使item.name这个key不存在，也会将item.name添加为新的key
        compareList[item.opName].push_back(item);
    }
    for (const auto &item : baselineData) {
        opNameSet.insert(item.opName);
        // 即使item.name这个key不存在，也会将item.name添加为新的key
        baselineList[item.opName].push_back(item);
    }
    std::vector<StaticOperatorItem> emptyVec;
    for (const auto &name : opNameSet) {
        if (compareList.find(name) == compareList.end()) {
            VectorMerge(emptyVec, baselineList[name], diffData);
            continue;
        }
        if (baselineList.find(name) == baselineList.end()) {
            VectorMerge(compareList[name], emptyVec, diffData);
            continue;
        }
        std::vector<StaticOperatorItem> &compareVec = compareList[name];
        std::vector<StaticOperatorItem> &baselineVec = baselineList[name];
        VectorMerge(compareVec, baselineVec, diffData);
    }
}

void QueryMemoryStaticOperatorListHandler::VectorMerge(std::vector<StaticOperatorItem> &compareVec,
    std::vector<StaticOperatorItem> &baselineVec, std::vector<StaticOperatorCompItem> &diffData) {
    const StaticOperatorItem emptyStaticOp = {"", "", 0, 0, 0};
    for (size_t i = 0; i < std::min(compareVec.size(), baselineVec.size()); ++i) {
        StaticOperatorCompItem element = {compareVec[i], baselineVec[i], {}};
        Subtract(element);
        diffData.emplace_back(element);
    }
    for (size_t i = compareVec.size(); i < baselineVec.size(); ++i) {
        StaticOperatorCompItem element = {emptyStaticOp, baselineVec[i], {}};
        Subtract(element);
        diffData.emplace_back(element);
    }
    for (size_t i = baselineVec.size(); i < compareVec.size(); ++i) {
        StaticOperatorCompItem element = {compareVec[i], emptyStaticOp, {}};
        Subtract(element);
        diffData.emplace_back(element);
    }
}

void QueryMemoryStaticOperatorListHandler::Subtract(Dic::Protocol::StaticOperatorCompItem &element) {
    const int precision = 3;
    element.diff.deviceId = element.compare.deviceId;
    if (!element.compare.opName.empty()) {
        element.diff.opName = element.compare.opName;
    } else {
        element.diff.opName = element.baseline.opName;
    }
    element.diff.nodeIndexStart = element.compare.nodeIndexStart - element.baseline.nodeIndexStart;
    element.diff.nodeIndexEnd = element.compare.nodeIndexEnd - element.baseline.nodeIndexEnd;
    element.diff.size = NumberUtil::DoubleReservedNDigits(element.compare.size - element.baseline.size, precision);
}

void QueryMemoryStaticOperatorListHandler::SelectDiffResult(MemoryStaticOperatorListRequest &request,
    MemoryStaticOperatorListCompResponse &response, std::vector<StaticOperatorCompItem> &fullDiffResult) {
    MemoryStaticOperatorListCompResponse filteredDiffResult;
    for (const auto &item : fullDiffResult) {
        if (IsSelected(request, item)) {
            filteredDiffResult.operatorDiffDetails.push_back(item);
        }
    }
    SortResult(request, filteredDiffResult);
    uint64_t pageSize =
        request.params.pageSize <= 0 ? DEFAULT_PAGE_SIZE : static_cast<uint64_t>(request.params.pageSize);
    uint64_t currentPage = request.params.currentPage < 1 ? 0 : static_cast<uint64_t>(request.params.currentPage - 1);
    uint64_t offset = currentPage * pageSize;
    if (offset != 0 && offset >= filteredDiffResult.operatorDiffDetails.size()) {
        response.operatorDiffDetails.clear();
        response.totalNum = 0;
    } else {
        response.totalNum = NumberSafe::SafeCastSizeTypeToInt64(filteredDiffResult.operatorDiffDetails.size());
        for (size_t i = offset; i < offset + pageSize && i < filteredDiffResult.operatorDiffDetails.size(); ++i) {
            response.operatorDiffDetails.push_back(filteredDiffResult.operatorDiffDetails[i]);
        }
    }
}

bool QueryMemoryStaticOperatorListHandler::IsSelected(
    MemoryStaticOperatorListRequest &request, const StaticOperatorCompItem &op) {
    bool filter = true;
    filter = filter && (op.diff.opName.find(request.params.searchName) != std::string::npos);
    if (request.params.minSize != std::numeric_limits<int64_t>::min()) {
        filter = filter && (op.diff.size >= request.params.minSize);
    }
    if (request.params.maxSize != std::numeric_limits<int64_t>::max()) {
        filter = filter && (op.diff.size <= request.params.maxSize);
    }
    if (request.params.startNodeIndex != -1 && request.params.endNodeIndex != -1) {
        // 要求compare对象的开始和结束时间有一个在startTime endTime内，且baseline对象的开始和结束时间也有一个在startTime endTime内。
        filter = filter &&
            ((op.compare.nodeIndexStart >= request.params.startNodeIndex &&
                 op.compare.nodeIndexStart <= request.params.endNodeIndex) ||
                (op.compare.nodeIndexEnd >= request.params.startNodeIndex &&
                    op.compare.nodeIndexEnd <= request.params.endNodeIndex)) &&
            ((op.baseline.nodeIndexStart >= request.params.startNodeIndex &&
                 op.baseline.nodeIndexStart <= request.params.endNodeIndex) ||
                (op.baseline.nodeIndexEnd >= request.params.startNodeIndex &&
                    op.baseline.nodeIndexEnd <= request.params.endNodeIndex));
    }
    return filter;
}

void QueryMemoryStaticOperatorListHandler::SortResult(
    MemoryStaticOperatorListRequest &request, MemoryStaticOperatorListCompResponse &result) {
    if (request.params.orderBy.empty()) {
        return;
    }
    if (request.params.desc) {
        SortDescend(request, result);
    } else {
        SortAscend(request, result);
    }
}

void QueryMemoryStaticOperatorListHandler::SortAscend(
    MemoryStaticOperatorListRequest &request, MemoryStaticOperatorListCompResponse &result) {
    std::map<std::string, bool (*)(StaticOperatorCompItem &, StaticOperatorCompItem &)> compFunc = {
        {std::string(StaticOpColumn::DEVICE_ID),
            [](StaticOperatorCompItem &op1, StaticOperatorCompItem &op2) {
                return op1.diff.deviceId < op2.diff.deviceId;
            }},
        {std::string(StaticOpColumn::OP_NAME),
            [](StaticOperatorCompItem &op1, StaticOperatorCompItem &op2) { return op1.diff.opName < op2.diff.opName; }},
        {std::string(StaticOpColumn::NODE_INDEX_START),
            [](StaticOperatorCompItem &op1, StaticOperatorCompItem &op2) {
                return op1.diff.nodeIndexStart < op2.diff.nodeIndexStart;
            }},
        {std::string(StaticOpColumn::NODE_INDEX_END),
            [](StaticOperatorCompItem &op1, StaticOperatorCompItem &op2) {
                return op1.diff.nodeIndexEnd < op2.diff.nodeIndexEnd;
            }},
        {std::string(StaticOpColumn::SIZE),
            [](StaticOperatorCompItem &op1, StaticOperatorCompItem &op2) { return op1.diff.size < op2.diff.size; }}};
    if (compFunc.find(request.params.orderBy) != compFunc.end()) {
        std::sort(
            result.operatorDiffDetails.begin(), result.operatorDiffDetails.end(), compFunc[request.params.orderBy]);
    }
}

void QueryMemoryStaticOperatorListHandler::SortDescend(
    MemoryStaticOperatorListRequest &request, MemoryStaticOperatorListCompResponse &result) {
    std::map<std::string, bool (*)(StaticOperatorCompItem &, StaticOperatorCompItem &)> compFunc = {
        {std::string(StaticOpColumn::DEVICE_ID),
            [](StaticOperatorCompItem &op1, StaticOperatorCompItem &op2) {
                return op1.diff.deviceId > op2.diff.deviceId;
            }},
        {std::string(StaticOpColumn::OP_NAME),
            [](StaticOperatorCompItem &op1, StaticOperatorCompItem &op2) { return op1.diff.opName > op2.diff.opName; }},
        {std::string(StaticOpColumn::NODE_INDEX_START),
            [](StaticOperatorCompItem &op1, StaticOperatorCompItem &op2) {
                return op1.diff.nodeIndexStart > op2.diff.nodeIndexStart;
            }},
        {std::string(StaticOpColumn::NODE_INDEX_END),
            [](StaticOperatorCompItem &op1, StaticOperatorCompItem &op2) {
                return op1.diff.nodeIndexEnd > op2.diff.nodeIndexEnd;
            }},
        {std::string(StaticOpColumn::SIZE),
            [](StaticOperatorCompItem &op1, StaticOperatorCompItem &op2) { return op1.diff.size > op2.diff.size; }}};
    if (compFunc.find(request.params.orderBy) != compFunc.end()) {
        std::sort(
            result.operatorDiffDetails.begin(), result.operatorDiffDetails.end(), compFunc[request.params.orderBy]);
    }
}
} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic
