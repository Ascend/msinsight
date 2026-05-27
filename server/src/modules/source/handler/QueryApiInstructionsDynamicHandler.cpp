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
#include "pch.h"
#include "WsSessionManager.h"
#include "SourceProtocolRequest.h"
#include "SourceProtocolResponse.h"
#include "SourceFileParser.h"
#include "QueryApiInstructionsDynamicHandler.h"

namespace Dic {
namespace Module {
namespace Source {
using namespace Dic::Server;

bool QueryApiInstructionsDynamicHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<SourceApiInstrDynamicRequest &>(*requestPtr);
    std::unique_ptr<SourceApiInstrDynamicResponse> responsePtr = std::make_unique<SourceApiInstrDynamicResponse>();
    SourceApiInstrDynamicResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    if (!request.params.coreName.empty()) { // 如果请求的coreName为空字符串，那么默认查找第一组
        if (auto [isValid, errMsg] = request.params.Valid(); !isValid) {
            ServerLog::Error("Parameter of command ", request.command, "is invalid, error:", errMsg);
            SendResponse(std::move(responsePtr), false, errMsg);
            return false;
        }
    }
    SetResponseBody(response, request);
    SendResponse(std::move(responsePtr), true);
    return true;
}

void QueryApiInstructionsDynamicHandler::SetResponseBody(
    SourceApiInstrDynamicResponse &response, SourceApiInstrDynamicRequest &request) {
    auto instructions = SourceFileParser::Instance().GetInstrDynamic(request.params.coreName);
    auto columNameMap = SourceFileParser::Instance().GetInstructionColumnTypeMap();
    response.body.coreName = request.params.coreName;
    if (columNameMap.empty()) { // 如果列名映射表为空，说明是老版本数据
        const auto &vector = SourceFileParser::Instance().GetInstructions(request.params.coreName);
        for (const auto &item : vector) {
            SourceApiInstrRes temp;
            temp.pipe = item.pipe;
            temp.ascendCInnerCode = item.ascendCInnerCode;
            temp.source = item.source;
            temp.address = item.address;
            temp.cycles = item.cycles.empty() ? 0 : item.cycles[0];
            temp.theoreticalStallCycles = item.theoreticalStallCycles.empty() ? 0 : item.theoreticalStallCycles[0];
            temp.realStallCycles = item.realStallCycles.empty() ? 0 : item.realStallCycles[0];
            temp.instructionsExecuted = item.instructionsExecuted.empty() ? 0 : item.instructionsExecuted[0];
            response.body.instructions.emplace_back(temp);
        }
        return;
    }
    // 组装每一列的表头信息
    response.body.columnNameMap = columNameMap;
    // 组装每一列的数据
    for (const auto &item : instructions) {
        SourceColumnValueMap col;
        TransformColumnData(item.floatColumnMap, col.floatMap);
        TransformColumnData(item.intColumnMap, col.intMap);
        TransformColumnData(item.stringColumnMap, col.stringMap);
        TransformColumnData(item.jsonStringColumnMap, col.stringMap); // json_str 复用string map即可
        col.percentAndDetailsColumnMap = item.percentAndDetailsColumnMap;
        response.body.columnValues.emplace_back(col);
    }
}

template <typename T>
void QueryApiInstructionsDynamicHandler::TransformColumnData(
    const std::unordered_map<std::string, std::vector<T>> &source, std::unordered_map<std::string, T> &target) {
    for (const auto &innerItem : source) {
        if (innerItem.second.empty()) {
            continue;
        }
        target[innerItem.first] = innerItem.second[0];
    }
}

} // Source
} // Module
} // Dic
