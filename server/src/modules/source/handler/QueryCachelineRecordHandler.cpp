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
#include "BinFileParseUtil.h"
#include "WsSessionManager.h"
#include "SourceProtocolRequest.h"
#include "SourceProtocolResponse.h"
#include "SourceFileParser.h"
#include "QueryCachelineRecordHandler.h"

namespace Dic {
namespace Module {
namespace Source {
using namespace Dic::Server;

bool QueryCachelineRecordHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    std::unique_ptr<CachelineRecordResponse> responsePtr = std::make_unique<CachelineRecordResponse>();
    CachelineRecordResponse &response = *responsePtr;
    SetBaseResponse(*requestPtr, response);
    response.body.cachelineRecords = QueryCachelineRecord(SourceFileParser::Instance().GetFilePath());
    // add response to response queue in session
    SendResponse(std::move(responsePtr), true);
    return true;
}

std::string QueryCachelineRecordHandler::QueryCachelineRecord(const std::string &filePath) {
    std::ifstream file = OpenReadFileSafely(filePath, std::ios::binary);
    if (!file) {
        ServerLog::Error("Can't open file, please check file exist or not, file name: ", filePath);
        return {};
    }
    std::vector<Module::Source::Position> cacheLinePos =
        Source::SourceFileParser::Instance().GetPositionByType(Module::Source::DataTypeEnum::DISPLAY_CACHE);
    if (cacheLinePos.empty()) {
        return {};
    }
    Module::Source::Position &pair = cacheLinePos.at(0);
    return BinFileParseUtil::GetContentStr(file, pair);
}

} // Source
} // Module
} // Dic
