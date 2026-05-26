/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#include "QueryMemSnapshotStateHandler.h"
#include "MemSnapshotService.h"
#include "DataBaseManager.h"

using namespace Dic::Module::MemSnapshot;

namespace Dic::Module::MemSnapshot {

/**
 * @brief 处理内存池状态查询请求
 *
 * 执行流程：
 * 1. 验证请求参数
 * 2. 获取数据库连接
 * 3. 调用MemSnapshotService获取segments状态
 * 4. 转换为响应格式并返回
 */
bool QueryMemSnapshotStateHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    const auto &request = dynamic_cast<MemSnapshotStateRequest &>(*requestPtr);
    auto responsePtr = std::make_unique<MemSnapshotStateResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errMsg;
    if (!request.params.CommonCheck(errMsg)) {
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    const auto database = GetMemSnapshotDatabaseByRequest(request);
    if (database == nullptr || !database->IsOpen()) {
        errMsg = LOG_TAG + "Failed to query state: get database connection failed";
        SendResponse(std::move(responsePtr), false, errMsg);
        return false;
    }
    auto segments = MemSnapshotService::GetSegmentsByEventId(request.params.eventId, request.params.deviceId, database);
    BuildSegmentsStateInfoFromSegments(segments, response.segments);
    SendResponse(std::move(responsePtr), true);
    return true;
}

void QueryMemSnapshotStateHandler::BuildSegmentsStateInfoFromSegments(
    const std::vector<Segment> &segments, std::vector<SegmentItemDTO> &stateInfos) {
    for (const auto &segment : segments) {
        SegmentItemDTO stateInfo;
        stateInfo.address = segment.address;
        stateInfo.stream = segment.stream;
        stateInfo.size = segment.totalSize;
        stateInfo.allocated = segment.allocated;
        stateInfo.allocOrMapEventId = segment.allocOrMapEventId;
        for (const auto &block : segment.blocks) {
            SegmentBlockItemDTO blockInfo;
            blockInfo.id = block.id;
            blockInfo.size = block.size;
            blockInfo.offset = block.address - segment.address;
            stateInfo.blocks.push_back(blockInfo);
        }

        stateInfos.push_back(stateInfo);
    }
}
} // namespace Dic::Module::MemSnapshot
