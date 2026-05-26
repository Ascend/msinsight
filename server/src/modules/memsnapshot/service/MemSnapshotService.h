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
#ifndef PROFILER_SERVER_MEMSNAPSHOTSERVICE_H
#define PROFILER_SERVER_MEMSNAPSHOTSERVICE_H
#include "MemSnapshotDatabase.h"
#include "MemSnapshotSegmentService.h"
#include "MemSnapshotDefs.h"

namespace Dic::Module::MemSnapshot {

/**
 * @brief 内存快照服务类
 *
 * 继承自MemSnapshotSegmentService，提供内存快照的高级查询功能。
 * 主要用于获取特定时间点的内存段(Segment)状态。
 */
class MemSnapshotService : MemSnapshotSegmentService {
  public:
    /**
     * @brief 获取指定事件ID时的segments状态
     *
     * 该方法会：
     * 1. 查询指定事件ID之前的所有segment相关事件
     * 2. 根据事件序列构建segments
     * 3. 查询该时刻活跃的blocks
     * 4. 将blocks分配到segments中
     *
     * @param eventId 事件ID，表示要查询的时间点
     * @param deviceId 设备ID
     * @param database 内存快照数据库实例
     * @return 构建完成的segments列表，失败时返回空列表
     */
    static std::vector<Segment> GetSegmentsByEventId(const uint64_t eventId, const std::string &deviceId,
        const std::shared_ptr<FullDb::MemSnapshotDatabase> &database);

  private:
    static inline std::string LOG_TAG = "[MemSnapshotService] ";
};
}

#endif // PROFILER_SERVER_MEMSNAPSHOTSERVICE_H
