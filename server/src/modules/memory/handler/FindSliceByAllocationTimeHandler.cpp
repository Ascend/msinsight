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
#include "TraceTime.h"
#include "ProjectExplorerManager.h"
#include "FindSliceByAllocationTimeHandler.h"

namespace Dic::Module::Memory {
using namespace Dic::Server;

bool FindSliceByAllocationTimeHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    MemoryFindSliceRequest &request = dynamic_cast<MemoryFindSliceRequest &>(*requestPtr.get());
    std::unique_ptr<MemoryFindSliceResponse> responsePtr = std::make_unique<MemoryFindSliceResponse>();
    MemoryFindSliceResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    std::string paramCheckErrMsg;
    if (!request.params.CommonCheck(paramCheckErrMsg)) {
        SetMemoryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        ServerLog::Warn("Invalid paramn. ", paramCheckErrMsg);
        return false;
    }
    if (renderEngine == nullptr) {
        SetMemoryError(ErrorCode::QUERY_SLICE_FAILED);
        SendResponse(std::move(responsePtr), false);
        ServerLog::Warn("Failed to find slice. timeline not exist!");
        return false;
    }
    OperatorDomain target = operatorMemoryService->ComputeAllocationTimeById(request.params.rankId, request.params.id);
    if (std::empty(target.metaType)) {
        SetMemoryError(ErrorCode::QUERY_MEMORY_OPERATOR_FAILED);
        SendResponse(std::move(responsePtr), false);
        ServerLog::Warn("Failed to query memory operator!");
        return false;
    }
    Timeline::CompeteSliceDomain slice = renderEngine->FindSliceByTimePoint(
        request.params.rankId, request.params.name, target.allocationTime, target.metaType);
    if (std::empty(slice.tid) && std::empty(slice.pid)) {
        SetMemoryError(ErrorCode::QUERY_SLICE_IN_TIMELINE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    // multi device text data, need replace cardId with min deviceId
    auto projects = ProjectExplorerManager::Instance().QueryProjectExplorer(request.projectName, {});
    if (!projects.empty() && projects[0].projectType != static_cast<int>(ProjectTypeEnum::DB)) {
        auto deviceInfos = projects[0].GetDeviceInfos();
        std::sort(deviceInfos.begin(), deviceInfos.end(), [](auto &l, auto &r) { return l->deviceId < r->deviceId; });
        if (!deviceInfos.empty()) {
            slice.cardId = deviceInfos[0]->rankId;
        }
    }
    populateResponseData(response, target, slice);
    SendResponse(std::move(responsePtr), true);
    return true;
}

void FindSliceByAllocationTimeHandler::populateResponseData(
    MemoryFindSliceResponse &response, OperatorDomain &target, Timeline::CompeteSliceDomain &slice) {
    response.data.metaType = target.metaType;
    response.data.depth = slice.depth;
    response.data.processId = slice.pid;
    response.data.threadId = slice.tid;
    response.data.rankId = slice.cardId;
    response.data.startTime = slice.timestamp - Timeline::TraceTime::Instance().GetStartTime();
    response.data.id = std::to_string(slice.id);
    response.data.duration = slice.duration;
}
} // namespace Dic::Module::Memory
