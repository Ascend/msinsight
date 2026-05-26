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
#ifndef PROFILER_SERVER_MEMSNAPSHOTSEGMENTSERVICE_H
#define PROFILER_SERVER_MEMSNAPSHOTSEGMENTSERVICE_H

#include "MemSnapshotDefs.h"

namespace Dic::Module::MemSnapshot {
/**
 * @brief 内存快照Segment服务基类,提供Segment构建和管理的核心功能。
 * Segment是从aclrtMalloc调用返回的内存块，代表内存池中的保留内存。Segments会被缓存并重用于未来的分配。
 * 该类提供以下核心功能：
 * - 根据事件序列构建Segments
 * - 处理segment_alloc/segment_free/segment_map/segment_unmap事件
 * - 将Blocks分配到Segments中
 */
class MemSnapshotSegmentService {
  protected:
    /**
     * @brief 通过二分查找在已排序的segments中查找包含指定地址的segment索引
     * @param segments 已按地址排序的segment列表
     * @param addr 要查找的内存地址
     * @return 包含该地址的segment索引，未找到返回-1
     */
    static int FindSegmentIdxByAddr(const std::vector<Segment> &segments, uint64_t addr);

    /**
     * @brief 根据事件序列构建segments
     * @param events 事件序列（仅包含segment相关事件：alloc/free/map/unmap）
     * @return 构建完成的segments列表
     */
    static std::vector<Segment> BuildSegmentsByEvents(const std::vector<TraceEntry> &events);

    /**
     * @brief 处理segment_alloc事件
     *
     * 当caching allocator请求aclrtMalloc分配更多内存时触发，
     * 将新分配的内存作为一个segment添加到缓存中。
     * @param segments segment列表（会被修改）
     * @param evt segment_alloc事件
     */
    static void HandleSegmentAlloc(std::vector<Segment> &segments, const TraceEntry &evt);

    /**
     * @brief 处理segment_free事件
     *
     * 当caching allocator调用aclrtFree将内存归还给NPU时触发。
     * @param segments segment列表（会被修改）
     * @param evt segment_free事件
     */
    static void HandleSegmentFree(std::vector<Segment> &segments, const TraceEntry &evt);

    /**
     * @brief 处理segment_map事件
     *
     * 处理虚拟内存映射事件，插入新segment并尝试与左侧相邻segment合并。
     * @param segments segment列表（会被修改）
     * @param evt segment_map事件
     */
    static void HandleSegmentMap(std::vector<Segment> &segments, const TraceEntry &evt);

    /**
     * @brief 处理segment_unmap事件
     *
     * 处理虚拟内存解映射事件，可能分割或删除segment。
     * @param segments segment列表（会被修改）
     * @param evt segment_unmap事件
     */
    static void HandleSegmentUnmap(std::vector<Segment> &segments, const TraceEntry &evt);

    /**
     * @brief 合并相邻的segments（双向合并）
     *
     * 尝试将当前segment与左侧和右侧的segment合并。
     * 合并条件：相同stream且地址衔接。
     * @param segments segment列表（会被修改）
     * @param curIdx 当前segment索引
     */
    static void MergeAdjacentSegments(std::vector<Segment> &segments, int curIdx);

    /**
     * @brief 将blocks分配到对应的segments中
     *
     * 遍历所有blocks，根据地址找到所属的segment，更新segment的active和allocated统计。
     * @param segments segment列表（会被修改）
     * @param blocks 要分配的block列表
     */
    static void BuildSegments(std::vector<Segment> &segments, const std::vector<Block> &blocks);
};
} // Dic::Module::MemSnapshot

#endif // PROFILER_SERVER_MEMSNAPSHOTSEGMENTSERVICE_H
