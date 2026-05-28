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

#include <memory>
#include "ServerLog.h"
#include "DataBaseManager.h"
#include "ACLGraphDebugJsonFileParser.h"

namespace Dic::Module::Timeline {

bool ACLGraphDebugJsonFileParser::PostParse(std::shared_ptr<TextTraceDatabase> db, const std::string &rankId) {
    std::vector<SliceDto> slices;
    // ===== 1. 从数据库获取原始 SliceDto 列表 =====
    if (!db->QuerySliceDtoList(slices)) {
        Server::ServerLog::Warn("[ACLGraph] No slices found for post-processing");
        return false;
    }
    Server::ServerLog::Info("[ACLGraph] slice length: ", slices.size());

    auto groups = GroupSlicesByTrackId(std::move(slices));

    AdjustSyncPairs(groups);
    std::vector<Trace::Flow> flows;
    GenerateSyncFlows(groups, flows); // 纯生成 Flow（不修改 groups）

    // 2. 持久化结果
    if (const auto compacted = MergeAndSortSlices(std::move(groups)); !compacted.empty()) {
        if (db->ReplaceAllSlices(compacted)) { // 替换 slices
            Server::ServerLog::Info("[ACLGraph] Slice replacement succeeded: ", compacted.size(), " records");
            if (db->InsertFlowList(flows)) { // 插入 flows
                Server::ServerLog::Info("[ACLGraph] Sync pairs aligned + ", flows.size(), " flows inserted");
            } else {
                Server::ServerLog::Warn("[ACLGraph] Flow insertion failed (non-fatal)");
            }
        } else {
            Server::ServerLog::Error("[ACLGraph] Slice replacement failed!");
            // 返回失败，触发回滚或标记解析失败
            return false;
        }
        Server::ServerLog::Info("[ACLGraph] PostParse completed: ", compacted.size(), " slices compacted");
    }
    // FIX: 修复单 JSON 单 DeviceId 文件导入时，无法从文件路径知道 rankId deviceId 映射的问题，再次更新 rankIdToDeviceIdMap
    if (DataBaseManager::Instance().GetDeviceIdFromRankId(rankId).empty()) {
        UpdateRankIdDeviceIdMapByProcessData(db, rankId);
    }
    return true;
}

// ========== 按 trackId 分组 ==========
std::unordered_map<uint64_t, std::vector<SliceDto>> ACLGraphDebugJsonFileParser::GroupSlicesByTrackId(
    std::vector<SliceDto> slices) {
    std::unordered_map<uint64_t, std::vector<SliceDto>> groups;

    // 使用移动语义避免拷贝（SliceDto 含多个 string）
    for (auto &slice : slices) {
        groups[slice.trackId].emplace_back(std::move(slice));
    }

    Server::ServerLog::Debug("Grouped into ", groups.size(), " tracks");
    return groups;
}

// ========== 调整同步对 ==========

// ========== 调整同步对.1. 构建依赖图 ==========
static void BuildWaitDependencyGraph(const std::unordered_map<uint64_t, std::vector<SliceDto>> &groups,
    const std::unordered_map<std::string, Key> &recMap, std::vector<WaitInfo> &waitInfos,
    std::unordered_map<Key, std::unordered_set<Key, PairHash>, PairHash> &deps,
    std::unordered_map<Key, int, PairHash> &indeg) {
    // 收集有效 WAIT（含配对 RECORD 位置）
    for (auto &[tid, slices] : groups) {
        for (size_t i = 0; i < slices.size(); ++i) {
            auto &s = slices[i];
            if (s.name.rfind("EVENT_WAIT_", 0) == 0) {
                if (auto it = recMap.find(s.name.substr(11)); it != recMap.end()) {
                    waitInfos.push_back({tid, i, it->second.first, it->second.second});
                }
            }
        }
    }
    if (waitInfos.empty()) {
        return;
    }

    // 构建两种依赖（自动去重）
    for (auto &curr : waitInfos) {
        Key currKey = {curr.waitTrack, curr.waitIdx};

        // 依赖1: 同 track 前序 WAIT（影响 curr.timestamp）
        for (size_t i = 0; i < curr.waitIdx; ++i) {
            auto &e = groups.at(curr.waitTrack)[i];
            if (e.name.rfind("EVENT_WAIT_", 0) == 0 && recMap.count(e.name.substr(11))) {
                Key prev = {curr.waitTrack, i};
                if (deps[prev].insert(currKey).second) {
                    indeg[currKey]++;
                }
            }
        }

        // 依赖2: RECORD 所在 track 前序 WAIT（影响 RECORD.timestamp）
        for (size_t i = 0; i < curr.recIdx; ++i) {
            auto &e = groups.at(curr.recTrack)[i];
            if (e.name.rfind("EVENT_WAIT_", 0) == 0 && recMap.count(e.name.substr(11))) {
                Key prev = {curr.recTrack, i};
                if (deps[prev].insert(currKey).second) {
                    indeg[currKey]++;
                }
            }
        }
        if (!indeg.count(currKey)) {
            indeg[currKey] = 0;
        }
    }
}

// ========== 调整同步对.2. 拓扑排序（Kahn 算法）==========
static std::vector<Key> TopologicalSort(const std::vector<WaitInfo> &waitInfos,
    const std::unordered_map<Key, std::unordered_set<Key, PairHash>, PairHash> &deps,
    std::unordered_map<Key, int, PairHash> indeg) // 传副本
{
    std::queue<Key> q;
    for (auto &wi : waitInfos) {
        if (indeg[{wi.waitTrack, wi.waitIdx}] == 0) {
            q.push({wi.waitTrack, wi.waitIdx});
        }
    }

    std::vector<Key> order;
    while (!q.empty()) {
        auto u = q.front();
        q.pop();
        order.push_back(u);
        if (auto it = deps.find(u); it != deps.end()) {
            for (auto &v : it->second) {
                if (--indeg[v] == 0) {
                    q.push(v);
                }
            }
        }
    }
    // 【核心修改】环检测 + 节点完整性校验
    if (order.size() != waitInfos.size()) { // 无环场景：order.size() == waitInfos.size()
        Server::ServerLog::Error("Topological sort failed: graph contains cycle or input inconsistency. "
                                 "Processed " +
            std::to_string(order.size()) + " of " + std::to_string(waitInfos.size()) + " nodes.");
    }
    return order;
}

// ========== 调整同步对.3. 按序调整 ==========
static void AdjustWaitsInOrder(std::unordered_map<uint64_t, std::vector<SliceDto>> &groups,
    const std::vector<WaitInfo> &waitInfos, const std::vector<Key> &order,
    const std::unordered_map<std::string, Key> &recMap) {
    const uint64_t PADDING = 0u;
    // 快速索引：Key → WaitInfo
    std::unordered_map<Key, const WaitInfo *, PairHash> idx;
    for (auto &wi : waitInfos) {
        idx[{wi.waitTrack, wi.waitIdx}] = &wi;
    }

    for (auto &k : order) {
        if (!idx.count(k)) {
            continue;
        }
        const auto &wi = *idx[k];
        auto &wait = groups[wi.waitTrack][wi.waitIdx];
        const auto &rec = groups[wi.recTrack][wi.recIdx];
        const int64_t delta = static_cast<int64_t>(rec.timestamp + rec.duration + PADDING) -
            static_cast<int64_t>(wait.timestamp + wait.duration);

        if (delta < 0) {
            Server::ServerLog::Warn("Skip WAIT sync (id=", wait.id, "): wait stop time > record stop time + PADDING");
            continue;
        }
        wait.duration += delta;

        // 传播偏移（影响后续事件，含其他 WAIT）
        auto &track = groups[wi.waitTrack];
        for (size_t j = wi.waitIdx + 1; j < track.size(); ++j) {
            track[j].timestamp += delta;
        }
    }
}

void ACLGraphDebugJsonFileParser::AdjustSyncPairs(std::unordered_map<uint64_t, std::vector<SliceDto>> &groups) {
    // 1. 收集 RECORD 位置
    std::unordered_map<std::string, Key> recMap;
    for (auto &[tid, slices] : groups) {
        for (size_t i = 0; i < slices.size(); ++i) {
            if (slices[i].name.rfind("EVENT_RECORD_", 0) == 0) {
                recMap[slices[i].name.substr(13)] = {tid, i};
            }
        }
    }
    if (recMap.empty()) {
        return;
    }

    // 2. 构建依赖 → 拓扑排序 → 按序调整
    std::vector<WaitInfo> waitInfos;
    std::unordered_map<Key, std::unordered_set<Key, PairHash>, PairHash> deps;
    std::unordered_map<Key, int, PairHash> indeg;
    BuildWaitDependencyGraph(groups, recMap, waitInfos, deps, indeg);
    if (!waitInfos.empty()) {
        auto order = TopologicalSort(waitInfos, deps, indeg);
        AdjustWaitsInOrder(groups, waitInfos, order, recMap);
    }
}

// ========== 合并分组并全局排序 ==========
std::vector<SliceDto> ACLGraphDebugJsonFileParser::MergeAndSortSlices(
    std::unordered_map<uint64_t, std::vector<SliceDto>> groups) {
    std::vector<SliceDto> result;
    size_t total_count = 0;

    // 预分配内存避免多次扩容
    for (const auto &[_, group] : groups) {
        total_count += group.size();
    }
    result.reserve(total_count);

    // 移动所有分组合并
    for (auto &[_, group] : groups) {
        result.insert(result.end(), std::make_move_iterator(group.begin()), std::make_move_iterator(group.end()));
    }

    // 全局排序：trackId → timestamp
    std::sort(result.begin(), result.end(), [](const SliceDto &a, const SliceDto &b) {
        return (a.trackId != b.trackId) ? (a.trackId < b.trackId) : (a.timestamp < b.timestamp);
    });

    return result;
}

// ========== 生成 Flow 事件 ==========
void ACLGraphDebugJsonFileParser::GenerateSyncFlows(
    const std::unordered_map<uint64_t, std::vector<SliceDto>> &groups, std::vector<Trace::Flow> &flows) {
    // 1. 收集 RECORD 结束时间 + 位置（id → { start, end, track, idx}）
    struct RecordInfo {
        uint64_t start;
        uint64_t end;
        uint64_t track;
        size_t idx;
    };
    std::unordered_map<std::string, RecordInfo> records;
    for (auto &[tid, slices] : groups) {
        for (size_t i = 0; i < slices.size(); ++i) {
            if (auto &s = slices[i]; s.name.rfind("EVENT_RECORD_", 0) == 0) {
                records[s.name.substr(13)] = {s.timestamp, s.timestamp + s.duration, tid, i};
            }
        }
    }
    if (records.empty()) {
        return;
    }
    const std::string CAT = "record_wait";

    // 2. 为每个 WAIT 生成成对 Flow（起点+终点）
    size_t pair_idx = 0; // 全局配对计数器（保证 flowId 唯一）
    for (auto &[tid, slices] : groups) {
        for (const auto &s : slices) {
            if (s.name.rfind("EVENT_WAIT_", 0) == 0) {
                auto id = s.name.substr(11);
                if (auto it = records.find(id); it != records.end()) {
                    const auto &rec = it->second;
                    std::string flow_id = "syncflow_" + id + "_" + std::to_string(pair_idx++);

                    // 起点 Flow：附加在 RECORD 事件（track=RECORD所在track）
                    Trace::Flow start;
                    start.trackId = rec.track;
                    start.ts = static_cast<int64_t>(rec.start); // ts = RECORD.start
                    start.flowId = flow_id;
                    start.name = "SyncStart_" + id;
                    start.type = Protocol::LINE_START; // 显式设置起点类型
                    // 从 RECORD 事件复制上下文字段
                    const auto &rec_slice = groups.at(rec.track)[rec.idx];
                    start.tid = rec_slice.tid;
                    start.pid = rec_slice.pid;
                    start.cat = rec_slice.cat.empty() ? CAT : std::optional(rec_slice.cat);
                    flows.push_back(std::move(start));

                    // 终点 Flow：附加在 WAIT 事件（track=WAIT所在track）
                    Trace::Flow end;
                    end.trackId = tid;
                    end.ts = static_cast<int64_t>(s.timestamp); // ts = WAIT.start
                    end.flowId = flow_id; // 与起点共享 flowId
                    end.name = "SyncEnd_" + id;
                    end.type = Protocol::LINE_END; // 显式设置终点类型
                    // 从 WAIT 事件复制上下文字段
                    end.tid = s.tid;
                    end.pid = s.pid;
                    end.cat = s.cat.empty() ? CAT : std::optional(s.cat);
                    flows.push_back(std::move(end));
                }
            }
        }
    }
    Server::ServerLog::Debug("Generated ", flows.size() / 2, " sync flow pairs (", flows.size(), " events)");
}

}
