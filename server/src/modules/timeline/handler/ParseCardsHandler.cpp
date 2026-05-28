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
#include "ParserStatusManager.h"
#include "FullDbParser.h"
#include "JsonFileParserManager.h"
#include "ParseCardsHandler.h"

namespace Dic::Module::Timeline {
using namespace Dic;
using namespace Dic::Server;
bool Dic::Module::Timeline::ParseCardsHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    ParseCardsRequest &request = dynamic_cast<ParseCardsRequest &>(*requestPtr.get());
    for (size_t i = 0; i < request.params.cards.size() && i < request.params.fileIds.size(); i++) {
        std::string item = request.params.cards[i];
        std::pair<ProjectTypeEnum, std::vector<std::string>> filePathPair =
            ParserStatusManager::Instance().QueryPendingFilePath(item);
        if (std::empty(filePathPair.second)) {
            ServerLog::Warn("Parse cards file path is empty. card: %", item);
            continue;
        }
        if (filePathPair.first == ProjectTypeEnum::ACLGRAPH_DEBUG) {
            JsonFileParserManager::GetACLGraphDebugParser().Parse(
                filePathPair.second, item, "", request.params.fileIds[i]);
            continue;
        }
        if (filePathPair.first == ProjectTypeEnum::TRACE) {
            JsonFileParserManager::GetTraceFileParser().Parse(filePathPair.second, item, "", request.params.fileIds[i]);
            continue;
        }
        FullDb::FullDbParser::Instance().Parse({item}, filePathPair.second[0]);
    }
    std::unique_ptr<ParseCardsResponse> responsePtr = std::make_unique<ParseCardsResponse>();
    ParseCardsResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    SetResponseResult(response, true);
    response.body.isContinueParse = true;
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
