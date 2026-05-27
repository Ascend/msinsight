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
#include "SourceFileParser.h"
#include "QueryApiLineDynamicHandler.h"

namespace Dic {
namespace Module {
namespace Source {
using namespace Dic::Server;

bool QueryApiLineDynamicHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<SourceApiLineDynamicRequest &>(*requestPtr);
    std::unique_ptr<SourceApiLineDynamicResponse> responsePtr = std::make_unique<SourceApiLineDynamicResponse>();
    SourceApiLineDynamicResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    if (auto [isValid, errMsg] = request.params.Valid(); isValid == false) {
        ServerLog::Error("Parameter of command ", request.command, "is invalid, error:", errMsg);
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    SetResponseBody(response, request);
    SendResponse(std::move(responsePtr), true);
    return true;
}

void QueryApiLineDynamicHandler::SetResponseBody(
    SourceApiLineDynamicResponse &response, SourceApiLineDynamicRequest &request) {
    auto linesDynamic =
        SourceFileParser::Instance().GetApiLinesDynamic(request.params.coreName, request.params.sourceName);
    response.body.columnNameMap = SourceFileParser::Instance().GetSourceLineColumnTypeMap();
    if (response.body.columnNameMap.empty()) { // 如果列名映射表为空，说明是老版本数据
        const std::vector<SourceFileLine> &lines =
            SourceFileParser::Instance().GetApiLinesByCoreAndSource(request.params.coreName, request.params.sourceName);
        std::vector<SourceFileLineRes> lineResArray;
        for (const auto &line : lines) {
            SourceFileLineRes lineRes;
            lineRes.line = line.line;
            if (!line.cycles.empty()) {
                lineRes.cycle = line.cycles[0];
            }
            if (!line.instructionsExecuted.empty()) {
                lineRes.instructionExecuted = line.instructionsExecuted[0];
            }
            lineRes.addressRange = line.addressRange;

            lineResArray.emplace_back(lineRes);
        }
        response.body.lines = lineResArray;
        return;
    }
    for (const auto &item : linesDynamic) {
        SourceFileLineDynamic line;
        line.addressRange = item.addressRange;
        TransformColumnData(item.floatColumnMap, line.columnValueMap.floatMap);
        TransformColumnData(item.intColumnMap, line.columnValueMap.intMap);
        TransformColumnData(item.stringColumnMap, line.columnValueMap.stringMap);
        line.columnValueMap.percentAndDetailsColumnMap = item.percentAndDetailsColumnMap;
        response.body.sourceFileLines.emplace_back(line);
    }
}

template <typename T>
void QueryApiLineDynamicHandler::TransformColumnData(
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
