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
#include "DataBaseManager.h"
#include "BaselineManager.h"
#include "TraceTime.h"
#include "MemoryParse.h"
#include "QueryMemoryViewHandler.h"

namespace Dic {
namespace Module {
namespace Memory {
using namespace Dic::Server;
bool QueryMemoryViewHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    MemoryViewRequest &request = dynamic_cast<MemoryViewRequest &>(*requestPtr.get());
    std::unique_ptr<MemoryViewResponse> responsePtr = std::make_unique<MemoryViewResponse>();
    MemoryViewResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        SetMemoryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    double start = NumberUtil::StringToDouble(request.params.start);
    double end = NumberUtil::StringToDouble(request.params.end);
    response.rankOffsetNs = Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp(request.params.rankId);
    if (MemoryParse::Instance().Exist(request.params.GetRequestKey())) {
        CurveView curve = MemoryParse::Instance().ComputeCurve(start, end, request.params.GetRequestKey());
        responsePtr->data.title = curve.title;
        responsePtr->data.legends = curve.legends;
        responsePtr->data.lines = curve.dataLines;
        SendResponse(std::move(responsePtr), true);
        return true;
    }
    bool success = QueryCurveData(response, errorMsg, request, responsePtr);
    if (!success) {
        return false;
    }
    CurveView curve;
    curve.tempData = std::move(responsePtr->data.tempData);
    curve.legends = responsePtr->data.legends;
    curve.title = responsePtr->data.title;
    MemoryParse::Instance().PutCurve(request.params.GetRequestKey(), curve);
    curve = MemoryParse::Instance().ComputeCurve(0, 0, request.params.GetRequestKey());
    responsePtr->data.lines = curve.dataLines;
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QueryMemoryViewHandler::QueryCurveData(MemoryViewResponse &response, std::string &errorMsg,
    MemoryViewRequest &request, std::unique_ptr<MemoryViewResponse> &responsePtr) {
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
        uint64_t offsetTime = Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp(request.params.rankId);
        if (!database || !database->QueryMemoryView(request.params, response.data, offsetTime)) {
            SetMemoryError(ErrorCode::QUERY_MEMORY_VIEW_DATA_FAILED);
            SendResponse(std::move(responsePtr), false);
            return false;
        }
    } else {
        MemoryViewData compareData;
        MemoryViewData baselineData;
        if (!GetRespectiveData(database, compareData, baselineData, request, errorMsg)) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
        ExecuteComparisonAlgorithm(compareData, baselineData, response);
    }
    return true;
}

bool QueryMemoryViewHandler::GetRespectiveData(std::shared_ptr<VirtualMemoryDataBase> database,
    Dic::Protocol::MemoryViewData &compareData, Dic::Protocol::MemoryViewData &baselineData,
    Dic::Protocol::MemoryViewRequest &request, std::string &errorMsg) {
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
    if (!database->QueryMemoryView(request.params, compareData, offsetTimeCompare)) {
        errorMsg = "Failed to query memory view compare data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_VIEW_DATA_COMPARE_FAILED);
        return false;
    }
    request.params.deviceId = FullDb::DataBaseManager::Instance().GetDeviceIdFromRankId(baselineId);
    uint64_t offsetTimeBaseline = Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp(baselineId);
    if (!databaseBaseline->QueryMemoryView(request.params, baselineData, offsetTimeBaseline)) {
        errorMsg = "Failed to query memory view baseline data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_VIEW_DATA_BASELINE_FAILED);
        return false;
    }
    return true;
}

void QueryMemoryViewHandler::ExecuteComparisonAlgorithm(const Protocol::MemoryViewData &compareData,
    const Protocol::MemoryViewData &baselineData, Protocol::MemoryViewResponse &response) {
    GetCompareGraphLegends(compareData, baselineData, response.data);
    GetCompareGraphLines(compareData, baselineData, response.data);
}

void QueryMemoryViewHandler::GetCompareGraphLegends(const Protocol::MemoryViewData &compareData,
    const Protocol::MemoryViewData &baselineData, Protocol::MemoryViewData &resultData) {
    resultData.title = "";
    resultData.legends = compareData.legends;
    for (size_t i = 1; i < compareData.legends.size(); ++i) {
        resultData.legends[i] += " of Compare";
    }
    if (baselineData.legends.size() > 0) {
        resultData.legends.insert(
            resultData.legends.end(), baselineData.legends.begin() + 1, baselineData.legends.end());
    }
    for (size_t i = compareData.legends.size(); i < resultData.legends.size(); ++i) {
        resultData.legends[i] += " of Baseline";
    }
}

void QueryMemoryViewHandler::GetCompareGraphLines(const Protocol::MemoryViewData &compareData,
    const Protocol::MemoryViewData &baselineData, Protocol::MemoryViewData &resultData) {
    size_t indexCompare = 0;
    size_t indexBaseline = 0;
    uint64_t compareSize = compareData.legends.empty() ? 0 : compareData.tempData.size() / compareData.legends.size();
    uint64_t baselineSize =
        baselineData.legends.empty() ? 0 : baselineData.tempData.size() / baselineData.legends.size();
    while ((indexCompare < compareSize) || (indexBaseline < baselineSize)) {
        // 如果baseline已经遍历完或者compare的时间戳小于baseline的时间戳，返回compare数据并补NULL。
        if (indexBaseline >= baselineSize ||
            (indexCompare < compareSize &&
                compareData.tempData[indexCompare * compareData.legends.size()] <
                    baselineData.tempData[indexBaseline * baselineData.legends.size()])) {
            for (size_t i = 0; i < compareData.legends.size(); ++i) {
                resultData.tempData.emplace_back(compareData.tempData[indexCompare * compareData.legends.size() + i]);
            }
            if (!baselineData.legends.empty()) {
                resultData.tempData.insert(resultData.tempData.end(), baselineData.legends.size() - 1,
                    std::numeric_limits<double>::quiet_NaN());
            }
            ++indexCompare;
            continue;
        }
        // 如果compare已经遍历完或者compare的时间戳大于baseline的时间戳，返回baseline数据并补NULL。
        if (indexCompare >= compareSize ||
            (indexBaseline < baselineSize &&
                compareData.tempData[indexCompare * compareData.legends.size()] >
                    baselineData.tempData[indexBaseline * baselineData.legends.size()])) {
            resultData.tempData.emplace_back(baselineData.tempData[indexBaseline * baselineData.legends.size()]);
            if (!compareData.legends.empty()) {
                resultData.tempData.insert(resultData.tempData.end(), compareData.legends.size() - 1,
                    std::numeric_limits<double>::quiet_NaN());
            }
            if ((indexBaseline + 1) * baselineData.legends.size() <= baselineData.tempData.size()) {
                AddBaseLineData(baselineData, resultData, indexBaseline);
            }
            ++indexBaseline;
            continue;
        }
        // 如果compare的时间戳等于baseline的时间戳，合并compare和baseline的数据。
        for (size_t i = 0; i < compareData.legends.size(); ++i) {
            resultData.tempData.emplace_back(compareData.tempData[indexCompare * compareData.legends.size() + i]);
        }
        if (!baselineData.legends.empty()) {
            AddBaseLineData(baselineData, resultData, indexBaseline);
        }
        ++indexCompare;
        ++indexBaseline;
    }
}

void QueryMemoryViewHandler::AddBaseLineData(
    const MemoryViewData &baselineData, MemoryViewData &resultData, size_t indexBaseline) const {
    for (size_t i = 1; i < baselineData.legends.size(); ++i) {
        resultData.tempData.emplace_back(baselineData.tempData[indexBaseline * baselineData.legends.size() + i]);
    }
}

} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic
