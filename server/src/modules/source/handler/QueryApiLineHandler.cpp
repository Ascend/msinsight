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
#include "QueryApiLineHandler.h"

namespace Dic {
namespace Module {
namespace Source {
using namespace Dic::Server;

bool QueryApiLineHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<SourceApiLineRequest &>(*requestPtr);
    std::unique_ptr<SourceApiLineResponse> responsePtr = std::make_unique<SourceApiLineResponse>();
    SourceApiLineResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    if (auto [isValid, errMsg] = request.params.Valid(); isValid == false) {
        ServerLog::Error("Parameter of command ", request.command, "is invalid, error:", errMsg);
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    const std::vector<SourceFileLine> &lines =
        SourceFileParser::Instance().GetApiLinesByCoreAndSource(request.params.coreName, request.params.sourceName);

    std::vector<SourceFileLineRes> lineResArray;
    for (auto line : lines) {
        SourceFileLineRes lineRes;
        lineRes.line = line.line;
        if (!line.cycles.empty()) {
            lineRes.cycle = line.cycles[0];
        }
        if (!line.instructionsExecuted.empty()) {
            lineRes.instructionExecuted = line.instructionsExecuted[0];
        }
        std::vector<std::pair<std::string, std::string>> copiedVector(line.addressRange);
        lineRes.addressRange = copiedVector;

        lineResArray.emplace_back(lineRes);
    }
    response.body.lines = lineResArray;
    SendResponse(std::move(responsePtr), true);
    return true;
}

} // Source
} // Module
} // Dic
