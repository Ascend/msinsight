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

#ifndef DATA_INSIGHT_CORE_MODULE_CORE_ACLGRAPH_DEBUG_JSON_FILE_PARSER_H
#define DATA_INSIGHT_CORE_MODULE_CORE_ACLGRAPH_DEBUG_JSON_FILE_PARSER_H

#include "TraceFileParser.h"
#include "TextTraceDatabase.h"

namespace Dic::Module::Timeline {
struct PairHash { // Key = pair<uint64_t, size_t> 的哈希
    size_t operator()(const std::pair<uint64_t, size_t> &p) const {
        return std::hash<uint64_t>()(p.first) ^ std::hash<size_t>()(p.second);
    }
};
struct WaitInfo {
    uint64_t waitTrack, waitIdx;
    uint64_t recTrack, recIdx; // 配对 RECORD 位置
};
using Key = std::pair<uint64_t, size_t>; // (track, idx)
class ACLGraphDebugJsonFileParser : public TraceFileParser {
  public:
    // 构造时注入共享线程池
    explicit ACLGraphDebugJsonFileParser(std::shared_ptr<ThreadPool> threadPool)
        : TraceFileParser(std::move(threadPool)) {}
    ~ACLGraphDebugJsonFileParser() override = default;

  protected:
    // 后处理 Hook 函数
    bool PostParse(std::shared_ptr<TextTraceDatabase> db, const std::string &rankId) override;

  private:
    /// @brief 按 trackId 分组，使用移动语义避免拷贝
    static std::unordered_map<uint64_t, std::vector<SliceDto>> GroupSlicesByTrackId(std::vector<SliceDto> slices);

    /// @brief 调整 EVENT_RECORD_<id> 与 EVENT_WAIT_<id> 的结束时间对齐
    /// @note 仅修改 WAIT 事件 duration 及其所在 track 后续事件 timestamp
    static void AdjustSyncPairs(std::unordered_map<uint64_t, std::vector<SliceDto>> &groups);

    /// @brief 合并所有分组并按 (trackId, timestamp) 全局排序
    static std::vector<SliceDto> MergeAndSortSlices(std::unordered_map<uint64_t, std::vector<SliceDto>> groups);

    /// @brief 生成同步 Flow 事件（不修改 groups，纯输出 flows）
    /// @param flows [out] 生成的 Flow 事件列表（每个 WAIT 生成 1 个终点事件）
    static void GenerateSyncFlows(
        const std::unordered_map<uint64_t, std::vector<SliceDto>> &groups, std::vector<Trace::Flow> &flows);
};
} // end of namespace Dic::Module::Timeline

#endif // DATA_INSIGHT_CORE_MODULE_CORE_ACLGRAPH_DEBUG_JSON_FILE_PARSER_H
