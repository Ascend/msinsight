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
#include "MemoryProtocolRequest.h"
#include "MemoryProtocolRespose.h"
#include "QueryMemoryStaticOperatorGraphHandler.h"

namespace Dic {
namespace Module {
namespace Memory {
using namespace Dic::Server;
bool QueryMemoryStaticOperatorGraphHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    MemoryStaticOperatorGraphRequest &request = dynamic_cast<MemoryStaticOperatorGraphRequest &>(*requestPtr.get());
    std::unique_ptr<MemoryStaticOperatorGraphResponse> responsePtr =
        std::make_unique<MemoryStaticOperatorGraphResponse>();
    MemoryStaticOperatorGraphResponse &response = *responsePtr.get();
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

    if (!request.params.isCompare) {
        if (!database->QueryStaticOperatorGraph(request.params, response.data)) {
            SetMemoryError(ErrorCode::QUERY_MEMORY_STATIC_OPERATOR_GRAPH_FAILED);
            SendResponse(std::move(responsePtr), false);
            return false;
        }
    } else {
        StaticOperatorGraphItem compareData;
        StaticOperatorGraphItem baselineData;
        if (!GetRespectiveData(database, compareData, baselineData, request, errorMsg)) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
        ExecuteComparisonAlgorithm(compareData, baselineData, response);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QueryMemoryStaticOperatorGraphHandler::GetRespectiveData(std::shared_ptr<VirtualMemoryDataBase> database,
    Dic::Protocol::StaticOperatorGraphItem &compareData, Dic::Protocol::StaticOperatorGraphItem &baselineData,
    Dic::Protocol::MemoryStaticOperatorGraphRequest &request, std::string &errorMsg) {
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
    if (!database->QueryStaticOperatorGraph(request.params, compareData)) {
        errorMsg = "Failed to query memory static operator graph compare data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_STATIC_OPERATOR_GRAPH_COMPARE_FAILED);
        return false;
    }
    if (!databaseBaseline->QueryStaticOperatorGraph(request.params, baselineData)) {
        errorMsg = "Failed to query memory static operator graph baseline data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_STATIC_OPERATOR_GRAPH_BASELINE_FAILED);
        return false;
    }
    return true;
}

void QueryMemoryStaticOperatorGraphHandler::ExecuteComparisonAlgorithm(
    const Protocol::StaticOperatorGraphItem &compareData, const Protocol::StaticOperatorGraphItem &baselineData,
    Protocol::MemoryStaticOperatorGraphResponse &response) {
    GetCompareGraphLegends(compareData, baselineData, response.data);
    GetCompareGraphLines(compareData, baselineData, response.data);
}

void QueryMemoryStaticOperatorGraphHandler::GetCompareGraphLegends(const Protocol::StaticOperatorGraphItem &compareData,
    const Protocol::StaticOperatorGraphItem &baselineData, Protocol::StaticOperatorGraphItem &resultData) {
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

void QueryMemoryStaticOperatorGraphHandler::GetCompareGraphLines(const Protocol::StaticOperatorGraphItem &compareData,
    const Protocol::StaticOperatorGraphItem &baselineData, Protocol::StaticOperatorGraphItem &resultData) {
    // compareData.lines和baselineData.lines都已经按照Node Index排好序，接下来将两个有序表进行归并。
    size_t indexCompare = 0;
    size_t indexBaseline = 0;
    while ((indexCompare < compareData.lines.size()) || (indexBaseline < baselineData.lines.size())) {
        // 如果baseline已经遍历完或者compare的Node Index小于baseline的Node Index，返回compare数据并补NULL。
        if (indexBaseline >= baselineData.lines.size() ||
            (indexCompare < compareData.lines.size() &&
                NumberUtil::TryParseUnsignedLongLong(compareData.lines[indexCompare][0]) <
                    NumberUtil::TryParseUnsignedLongLong(baselineData.lines[indexBaseline][0]))) {
            std::vector<std::string> points = compareData.lines[indexCompare];
            if (baselineData.legends.size() > 0) {
                points.insert(points.end(), baselineData.legends.size() - 1, "NULL");
            }
            resultData.lines.emplace_back(points);
            ++indexCompare;
            continue;
        }
        // 如果compare已经遍历完或者compare的Node Index大于baseline的Node Index，返回baseline数据并补NULL。
        if (indexCompare >= compareData.lines.size() ||
            (indexBaseline < baselineData.lines.size() &&
                NumberUtil::TryParseUnsignedLongLong(compareData.lines[indexCompare][0]) >
                    NumberUtil::TryParseUnsignedLongLong(baselineData.lines[indexBaseline][0]))) {
            std::vector<std::string> points = {};
            points.push_back(baselineData.lines[indexBaseline][0]);
            if (compareData.legends.size() > 0) {
                points.insert(points.end(), compareData.legends.size() - 1, "NULL");
            }
            if (baselineData.lines[indexBaseline].size() > 0) {
                points.insert(points.end(), baselineData.lines[indexBaseline].begin() + 1,
                    baselineData.lines[indexBaseline].end());
            }
            resultData.lines.emplace_back(points);
            ++indexBaseline;
            continue;
        }
        // 如果compare的Node Index等于baseline的Node Index，合并compare和baseline的数据。
        std::vector<std::string> points = compareData.lines[indexCompare];
        if (baselineData.lines[indexBaseline].size() > 0) {
            points.insert(
                points.end(), baselineData.lines[indexBaseline].begin() + 1, baselineData.lines[indexBaseline].end());
        }
        resultData.lines.emplace_back(points);
        ++indexCompare;
        ++indexBaseline;
    }
}
} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic
