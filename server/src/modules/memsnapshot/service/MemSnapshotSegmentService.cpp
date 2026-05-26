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
#include "MemSnapshotSegmentService.h"

namespace Dic::Module::MemSnapshot {

/**
 * @brief 通过二分查找在已排序的segments中查找包含指定地址的segment索引
 */
int MemSnapshotSegmentService::FindSegmentIdxByAddr(const std::vector<Segment> &segments, const uint64_t addr) {
    int left = 0;
    int right = static_cast<int>(segments.size()) - 1;
    while (left <= right) {
        if (const int mid = (left + right) / 2; addr < segments[mid].address) {
            right = mid - 1;
        } else if (addr >= segments[mid].address + segments[mid].totalSize) {
            left = mid + 1;
        } else {
            return mid;
        }
    }
    return -1;
}

/**
 * @brief 处理segment_alloc事件，插入新的segment
 */
void MemSnapshotSegmentService::HandleSegmentAlloc(std::vector<Segment> &segments, const TraceEntry &evt) {
    auto insertIt = std::lower_bound(segments.begin(), segments.end(), evt.address,
        [](const Segment &seg, uint64_t addr) { return seg.address < addr; });
    Segment seg(evt.address, evt.size, evt.stream, evt.id);
    segments.insert(insertIt, seg);
}

/**
 * @brief 处理segment_free事件，删除匹配的segment
 */
void MemSnapshotSegmentService::HandleSegmentFree(std::vector<Segment> &segments, const TraceEntry &evt) {
    auto it = std::lower_bound(segments.begin(), segments.end(), evt.address,
        [](const Segment &seg, uint64_t addr) { return seg.address < addr; });
    if (it != segments.end() && it->address == evt.address) {
        segments.erase(it);
    }
}

void MemSnapshotSegmentService::MergeAdjacentSegments(std::vector<Segment> &segments, int curIdx) {
    if (curIdx < 0 || static_cast<size_t>(curIdx) >= segments.size()) {
        return;
    }

    auto &curSeg = segments[curIdx];

    // 先尝试向右合并
    while (static_cast<size_t>(curIdx + 1) < segments.size()) {
        auto &rightSeg = segments[curIdx + 1];
        if (rightSeg.stream == curSeg.stream && curSeg.address + curSeg.totalSize == rightSeg.address) {
            curSeg.totalSize += rightSeg.totalSize;
            segments.erase(segments.begin() + curIdx + 1);
        } else {
            break;
        }
    }

    // 再尝试向左合并
    while (curIdx > 0) {
        auto &leftSeg = segments[curIdx - 1];
        if (leftSeg.stream == curSeg.stream && leftSeg.address + leftSeg.totalSize == curSeg.address) {
            leftSeg.totalSize += curSeg.totalSize;
            segments.erase(segments.begin() + curIdx);
            curIdx--;
        } else {
            break;
        }
    }
}

void MemSnapshotSegmentService::HandleSegmentMap(std::vector<Segment> &segments, const TraceEntry &evt) {
    auto insertIt = std::lower_bound(segments.begin(), segments.end(), evt.address,
        [](const Segment &seg, uint64_t addr) { return seg.address < addr; });
    const Segment seg(evt.address, evt.size, evt.stream, evt.id);
    insertIt = segments.insert(insertIt, seg);
    const int curIdx = static_cast<int>(insertIt - segments.begin());
    MergeAdjacentSegments(segments, curIdx);
}

void MemSnapshotSegmentService::HandleSegmentUnmap(std::vector<Segment> &segments, const TraceEntry &evt) {
    int existSegIdx = FindSegmentIdxByAddr(segments, evt.address);
    if (existSegIdx < 0) {
        return;
    }
    auto &existSeg = segments[existSegIdx];
    if (evt.address > existSeg.address) {
        existSegIdx++;
        uint64_t totalSize = existSeg.totalSize;
        existSeg.totalSize = evt.address - existSeg.address;
        const Segment newSeg(evt.address, totalSize - existSeg.totalSize, evt.stream);
        segments.insert(segments.begin() + existSegIdx, newSeg);
    }
    if (existSegIdx >= static_cast<int>(segments.size())) {
        existSegIdx = static_cast<int>(segments.size()) - 1;
    }
    auto &seg = segments[existSegIdx];
    if (seg.totalSize == evt.size) {
        segments.erase(segments.begin() + existSegIdx);
        return;
    }
    seg.address += evt.size;
    seg.totalSize -= evt.size;
}

std::vector<Segment> MemSnapshotSegmentService::BuildSegmentsByEvents(const std::vector<TraceEntry> &events) {
    std::vector<Segment> segments;
    for (const auto &evt : events) {
        if (evt.action == TRACE_ENTRY_ACTION_SEG_ALLOC) {
            HandleSegmentAlloc(segments, evt);
        } else if (evt.action == TRACE_ENTRY_ACTION_SEG_FREE) {
            HandleSegmentFree(segments, evt);
        } else if (evt.action == TRACE_ENTRY_ACTION_SEG_MAP) {
            HandleSegmentMap(segments, evt);
        } else if (evt.action == TRACE_ENTRY_ACTION_SEG_UNMAP) {
            HandleSegmentUnmap(segments, evt);
        }
    }
    return segments;
}

void MemSnapshotSegmentService::BuildSegments(std::vector<Segment> &segments, const std::vector<Block> &blocks) {
    for (const auto &block : blocks) {
        int existSegIdx = FindSegmentIdxByAddr(segments, block.address);
        if (existSegIdx < 0) {
            continue;
        }
        auto &seg = segments[existSegIdx];
        seg.blocks.push_back(block);
        seg.active += block.size;
        if (block.state == BLOCK_STATE_ACTIVE_ALLOC) {
            seg.allocated += block.size;
        }
    }
    for (auto &seg : segments) {
        std::sort(
            seg.blocks.begin(), seg.blocks.end(), [](const Block &a, const Block &b) { return a.address < b.address; });
    }
    std::sort(segments.begin(), segments.end(), [](const Segment &a, const Segment &b) {
        if (a.totalSize > b.totalSize) {
            return false;
        }
        if (a.totalSize < b.totalSize) {
            return true;
        }
        return a.address < b.address;
    });
}
}
