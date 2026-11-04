/* Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2: http://license.coscl.org.cn/MulanPSL2
 */
#include "QueryMemSnapshotFrameTreeHandler.h"
#include "MemSnapshotFrameTreeService.h"
#include <stdexcept>

namespace Dic::Module::MemSnapshot {
bool QueryMemSnapshotFrameTreeHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    const auto &request = dynamic_cast<Protocol::MemSnapshotFrameTreeRequest &>(*requestPtr);
    auto response = std::make_unique<Protocol::MemSnapshotFrameTreeResponse>();
    SetBaseResponse(request, *response);
    std::string error;
    if (!request.params.CommonCheck(error)) {
        SendResponse(std::move(response), false, error);
        return false;
    }
    try {
        response->tree = MemSnapshotFrameTreeService::Query(GetMemSnapshotDataKey(request), request.params.deviceId,
            request.params.eventId, request.params.includeStatic, request.id);
        response->offset = static_cast<uint64_t>((request.params.currentPage - 1) * request.params.pageSize);
        response->pageSize = static_cast<uint64_t>(request.params.pageSize);
        if (!response->CheckPageBudget()) {
            throw std::runtime_error("Frame tree page exceeds response budget; reduce pageSize and restart loading");
        }
    } catch (const std::exception &exception) {
        // 失败响应不能继续序列化原本超限或已失效的分页内容。
        response->tree.reset();
        response->offset = 0;
        response->pageSize = 0;
        SendResponse(std::move(response), false, exception.what());
        return false;
    }
    SendResponse(std::move(response), true);
    return true;
}
}
