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
#include "JsonFileParserManager.h"
#include "KernelParse.h"
#include "MemoryParse.h"
#include "FullDbParser.h"
#include "DataBaseManager.h"
#include "SourceFileParser.h"
#include "MemSnapshotParser.h"
#include "MemScopeParser.h"
#include "ServitizationOpenApi.h"
#include "BaselineManagerService.h"
#include "ParserStatusManager.h"
#include "ResetWindowHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool ResetWindowHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    ResetWindowRequest &request = dynamic_cast<ResetWindowRequest &>(*requestPtr.get());
    std::unique_ptr<ResetWindowResponse> responsePtr = std::make_unique<ResetWindowResponse>();
    ResetWindowResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    ParserStatusManager::Instance().SetAllTerminateStatus();
    ParserStatusManager::Instance().NotifyStartParse();
    std::shared_ptr<IE::ServitizationOpenApi> openApi = std::make_shared<IE::ServitizationOpenApi>();
    openApi->Reset();
    JsonFileParserManager::ResetAll();
    Summary::KernelParse::Instance().Reset();
    Memory::MemoryParse::Instance().Reset();
    Source::SourceFileParser::Instance().Reset();
    FullDb::FullDbParser::Instance().Reset();
    MemSnapshotParser::Instance().Reset();
    MemScopeParser::Instance().Reset();
    BaselineManagerService::ResetBaseline(true);
    SendResponse(std::move(responsePtr), true);
    return true;
}

} // Timeline
} // Module
} // Dic
