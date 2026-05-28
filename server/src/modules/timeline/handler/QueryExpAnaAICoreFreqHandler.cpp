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
#include "QueryExpAnaAICoreFreqHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
bool QueryExpAnaAICoreFreqHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    const uint64_t percentLimit = 5;
    ExpAnaAICoreFreqRequest &request = dynamic_cast<ExpAnaAICoreFreqRequest &>(*requestPtr.get());

    std::unique_ptr<ExpAnaAICoreFreqResponse> responsePtr = std::make_unique<ExpAnaAICoreFreqResponse>();
    ExpAnaAICoreFreqResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Query system view AI core freq failed to get connection.");
        SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    std::string deviceId = DataBaseManager::Instance().GetDeviceIdFromRankId(request.params.rankId);
    if (deviceId.empty()) {
        ServerLog::Error("Query system view AI core freq failed to get deviceId.");
        SetTimelineError(ErrorCode::GET_DEVICE_ID_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    request.params.deviceId = deviceId;
    uint64_t maxFreq = 0;
    uint64_t minFreq = UINT64_MAX;
    std::vector<std::pair<uint64_t, uint64_t>> freqs;
    if (!database->QueryExpAnaAICoreFreqData(request.params, response.body, freqs, maxFreq, minFreq) || freqs.empty()) {
        SetTimelineError(ErrorCode::QUERY_AI_CORE_FREQ_FAILED);
        SetResponseResult(response, false);
        ServerLog::Error("Failed to get system view AI core freq table response data.");
        return false;
    }
    auto dbType = Timeline::DataBaseManager::Instance().GetDataType(request.fileId);
    response.body.rankId = dbType == Timeline::DataType::TEXT ? request.params.rankId : database->GetDbPath();
    if (maxFreq != 0) {
        // 计算百分比需要乘以100
        response.body.percent = static_cast<uint64_t>(((float)(maxFreq - minFreq) / (float)maxFreq) * 100);
    }
    if (response.body.percent > percentLimit) { // 降频百分比超过5，需要提示用户关注
        response.body.hasProblem = true;
    }

    // add response to response queue in session
    SendResponse(std::move(responsePtr), true);
    return true;
}

} // Timeline
} // Module
} // Dic
