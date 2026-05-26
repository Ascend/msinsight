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
#include <limits>
#include "DataBaseManager.h"
#include "BaselineManager.h"
#include "MemoryProtocolRequest.h"
#include "MemoryProtocolRespose.h"
#include "TraceTime.h"
#include "TrackInfoManager.h"
#include "OperatorMemoryService.h"
#include "QueryMemoryOperatorHandler.h"

namespace Dic {
namespace Module {
namespace Memory {
using namespace Dic::Server;
using namespace Timeline;
bool QueryMemoryOperatorHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    MemoryOperatorRequest &request = dynamic_cast<MemoryOperatorRequest &>(*requestPtr.get());
    std::unique_ptr<MemoryOperatorComparisonResponse> responsePtr =
        std::make_unique<MemoryOperatorComparisonResponse>();
    MemoryOperatorComparisonResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    response.isCompare = request.params.isCompare;
    uint64_t minTimeStamp = Timeline::TraceTime::Instance().GetStartTime();
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg, minTimeStamp)) {
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
        if (!GetRespectiveDataNotCompare(database, request, response)) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
    } else {
        std::vector<MemoryOperator> compareData;
        std::vector<MemoryOperator> baselineData;
        if (!GetRespectiveData(database, compareData, baselineData, request, errorMsg)) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
        ExecuteComparisonAlgorithm(compareData, baselineData, request, response);
    }
    // add response to response queue in session
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QueryMemoryOperatorHandler::GetRespectiveDataNotCompare(std::shared_ptr<VirtualMemoryDataBase> database,
    MemoryOperatorRequest &request, MemoryOperatorComparisonResponse &response) {
    std::vector<MemoryOperator> opDetails;
    response.totalNum = database->QueryOperatorDetail(request.params, opDetails);
    if (response.totalNum < 0) {
        SetMemoryError(ErrorCode::QUERY_MEMORY_OPERATOR_FAILED);
        return false;
    }
    for (const auto &item : opDetails) {
        MemoryOperatorComparison element = {item, {}, {}};
        response.operatorDiffDetails.emplace_back(element);
    }
    return true;
}

bool QueryMemoryOperatorHandler::GetRespectiveData(std::shared_ptr<VirtualMemoryDataBase> database,
    std::vector<MemoryOperator> &compareData, std::vector<MemoryOperator> &baselineData, MemoryOperatorRequest &request,
    std::string &errorMsg) {
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
    if (!database->QueryEntireOperatorTable(request.params, compareData, offsetTimeCompare)) {
        errorMsg = "Failed to query memory operator compare data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_OPERATOR_COMPARE_FAILED);
        return false;
    }
    request.params.deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(baselineId);
    uint64_t offsetTimeBaseline = Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp(baselineId);
    if (!databaseBaseline->QueryEntireOperatorTable(request.params, baselineData, offsetTimeBaseline)) {
        errorMsg = "Failed to query memory operator baseline data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_OPERATOR_BASELINE_FAILED);
        return false;
    }
    return true;
}

void QueryMemoryOperatorHandler::ExecuteComparisonAlgorithm(const std::vector<MemoryOperator> &compareData,
    const std::vector<MemoryOperator> &baselineData, Dic::Protocol::MemoryOperatorRequest &request,
    MemoryOperatorComparisonResponse &response) {
    std::vector<MemoryOperatorComparison> fullDiffResult;
    GetOperatorDiff(compareData, baselineData, fullDiffResult);
    SelectDiffResult(request, response, fullDiffResult);
}

void QueryMemoryOperatorHandler::GetOperatorDiff(const std::vector<MemoryOperator> &compareData,
    const std::vector<MemoryOperator> &baselineData, std::vector<MemoryOperatorComparison> &diffData) {
    std::set<std::string> opName;
    std::map<std::string, std::vector<MemoryOperator>> compareList;
    std::map<std::string, std::vector<MemoryOperator>> baselineList;
    for (const auto &item : compareData) {
        opName.insert(item.name);
        // 即使item.name这个key不存在，也会将item.name添加为新的key
        compareList[item.name].push_back(item);
    }
    for (const auto &item : baselineData) {
        opName.insert(item.name);
        // 即使item.name这个key不存在，也会将item.name添加为新的key
        baselineList[item.name].push_back(item);
    }
    std::vector<MemoryOperator> emptyVec;
    for (const auto &name : opName) {
        if (compareList.find(name) == compareList.end()) {
            VectorMerge(emptyVec, baselineList[name], diffData);
            continue;
        }
        if (baselineList.find(name) == baselineList.end()) {
            VectorMerge(compareList[name], emptyVec, diffData);
            continue;
        }
        std::vector<MemoryOperator> &compareVec = compareList[name];
        std::vector<MemoryOperator> &baselineVec = baselineList[name];
        VectorMerge(compareVec, baselineVec, diffData);
    }
}

void QueryMemoryOperatorHandler::VectorMerge(std::vector<MemoryOperator> &compareVec,
    std::vector<MemoryOperator> &baselineVec, std::vector<MemoryOperatorComparison> &diffData) {
    const MemoryOperator emptyOperator = {"", 0, "NA", "NA", 0, "NA", 0, 0, 0, 0, 0, 0, 0, "", ""};
    for (size_t i = 0; i < std::min(compareVec.size(), baselineVec.size()); ++i) {
        MemoryOperatorComparison element = {compareVec[i], baselineVec[i], {}};
        Subtract(element);
        diffData.emplace_back(element);
    }
    for (size_t i = compareVec.size(); i < baselineVec.size(); ++i) {
        MemoryOperatorComparison element = {emptyOperator, baselineVec[i], {}};
        Subtract(element);
        diffData.emplace_back(element);
    }
    for (size_t i = baselineVec.size(); i < compareVec.size(); ++i) {
        MemoryOperatorComparison element = {compareVec[i], emptyOperator, {}};
        Subtract(element);
        diffData.emplace_back(element);
    }
}

void QueryMemoryOperatorHandler::Subtract(Dic::Protocol::MemoryOperatorComparison &element) {
    const int precision = 3;
    if (!element.compare.name.empty()) {
        element.diff.name = element.compare.name;
    } else {
        element.diff.name = element.baseline.name;
    }
    element.diff.size = NumberUtil::DoubleReservedNDigits(element.compare.size - element.baseline.size, precision);
    element.diff.allocationTime =
        NumberUtil::StringDoubleMinus(element.compare.allocationTime, element.baseline.allocationTime);
    element.diff.releaseTime = NumberUtil::StringDoubleMinus(element.compare.releaseTime, element.baseline.releaseTime);
    element.diff.duration =
        NumberUtil::DoubleReservedNDigits(element.compare.duration - element.baseline.duration, precision);
    element.diff.activeReleaseTime =
        NumberUtil::StringDoubleMinus(element.compare.activeReleaseTime, element.baseline.activeReleaseTime);
    element.diff.activeDuration =
        NumberUtil::DoubleReservedNDigits(element.compare.activeDuration - element.baseline.activeDuration, precision);
    element.diff.allocationAllocated = NumberUtil::DoubleReservedNDigits(
        element.compare.allocationAllocated - element.baseline.allocationAllocated, precision);
    element.diff.allocationReserved = NumberUtil::DoubleReservedNDigits(
        element.compare.allocationReserved - element.baseline.allocationReserved, precision);
    element.diff.allocationActive = NumberUtil::DoubleReservedNDigits(
        element.compare.allocationActive - element.baseline.allocationActive, precision);
    element.diff.releaseAllocated = NumberUtil::DoubleReservedNDigits(
        element.compare.releaseAllocated - element.baseline.releaseAllocated, precision);
    element.diff.releaseReserved = NumberUtil::DoubleReservedNDigits(
        element.compare.releaseReserved - element.baseline.releaseReserved, precision);
    element.diff.releaseActive =
        NumberUtil::DoubleReservedNDigits(element.compare.releaseActive - element.baseline.releaseActive, precision);
    element.diff.streamId = "";
    element.diff.deviceType = "";
}

void QueryMemoryOperatorHandler::SelectDiffResult(MemoryOperatorRequest &request,
    MemoryOperatorComparisonResponse &response, std::vector<MemoryOperatorComparison> &fullDiffResult) {
    MemoryOperatorComparisonResponse filteredDiffResult;
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

bool QueryMemoryOperatorHandler::IsWithinInterval(const long double num, const double start, const double end) {
    return num >= start && num <= end;
}

bool QueryMemoryOperatorHandler::IsSelected(MemoryOperatorRequest &request, const MemoryOperatorComparison &op) {
    bool filter = true;
    filter = filter && (op.diff.name.find(request.params.searchName) != std::string::npos);
    if (request.params.minSize != std::numeric_limits<int64_t>::min()) {
        filter = filter && (op.diff.size >= request.params.minSize);
    }
    if (request.params.maxSize != std::numeric_limits<int64_t>::max()) {
        filter = filter && (op.diff.size <= request.params.maxSize);
    }
    if (request.params.startTime != -1 && request.params.endTime != -1) {
        long double compareAlloTime = NumberUtil::StringToLongDouble(op.compare.allocationTime);
        long double compareReleTime = NumberUtil::StringToLongDouble(op.compare.releaseTime);
        long double baselineAlloTime = NumberUtil::StringToLongDouble(op.baseline.allocationTime);
        long double baselineReleTime = NumberUtil::StringToLongDouble(op.baseline.releaseTime);
        if (request.params.isOnlyShowAllocatedOrReleasedWithinInterval) {
            // 要求compare对象的在这段时间分配或释放了内存，且baseline对象的开始和结束时间也在这段时间分配或释放了内存。
            filter = filter &&
                (QueryMemoryOperatorHandler::IsWithinInterval(
                     compareAlloTime, request.params.startTime, request.params.endTime) ||
                    QueryMemoryOperatorHandler::IsWithinInterval(
                        compareReleTime, request.params.startTime, request.params.endTime)) &&
                (QueryMemoryOperatorHandler::IsWithinInterval(
                     baselineAlloTime, request.params.startTime, request.params.endTime) ||
                    QueryMemoryOperatorHandler::IsWithinInterval(
                        baselineReleTime, request.params.startTime, request.params.endTime));
        } else {
            // 要求compare对象的开始和结束时间有一个在startTime endTime内或在这段时间一直存在，且baseline对象的开始和结束时间也有一个在startTime endTime内或在这段时间一直存在。
            filter = filter && (compareReleTime == 0 || compareReleTime >= request.params.startTime) &&
                (compareAlloTime <= request.params.endTime) &&
                (baselineReleTime == 0 || baselineReleTime >= request.params.startTime) &&
                (baselineAlloTime <= request.params.endTime);
        }
    }
    return filter;
}

void QueryMemoryOperatorHandler::SortResult(MemoryOperatorRequest &request, MemoryOperatorComparisonResponse &result) {
    if (request.params.orderBy.empty()) {
        return;
    }
    auto comparator = OperatorMemoryService::GetComparatorByColumn(request.params.orderBy, request.params.desc);
    std::sort(result.operatorDiffDetails.begin(), result.operatorDiffDetails.end(),
        [comparator](const MemoryOperatorComparison &op1, const MemoryOperatorComparison &op2) {
            return comparator(op1.diff, op2.diff);
        });
}
} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic
