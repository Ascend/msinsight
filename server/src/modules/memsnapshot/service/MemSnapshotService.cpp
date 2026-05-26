/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

#include "MemSnapshotService.h"

namespace Dic::Module::MemSnapshot {

/**
 * @brief 获取指定事件ID时的segments状态
 *
 * 执行流程：
 * 1. 检查数据库连接有效性
 * 2. 查询指定事件ID之前的所有segment相关事件
 * 3. 根据事件序列构建segments
 * 4. 查询该时刻活跃的blocks
 * 5. 将blocks分配到segments中并更新统计信息
 */
std::vector<Segment> MemSnapshotService::GetSegmentsByEventId(
    const uint64_t eventId, const std::string &deviceId, const std::shared_ptr<FullDb::MemSnapshotDatabase> &database) {
    if (database == nullptr || !database->IsOpen()) {
        Server::ServerLog::Error(LOG_TAG + "Failed to get segments by event id: Database is null");
        return {};
    }
    std::vector<TraceEntry> segmentEvents;
    if (!database->QuerySegmentEventsUntil(eventId, deviceId, segmentEvents)) {
        Server::ServerLog::Error(LOG_TAG + "Failed to query segment events for event id: " + std::to_string(eventId));
        return {};
    }

    auto segments = BuildSegmentsByEvents(segmentEvents);

    std::vector<Block> blocks;
    if (!database->QueryActiveBlocksByEventId(eventId, deviceId, blocks)) {
        Server::ServerLog::Error(LOG_TAG + "Failed to query blocks for event id: " + std::to_string(eventId));
        return {};
    }

    BuildSegments(segments, blocks);
    return segments;
}
}
