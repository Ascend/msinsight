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

#include <gtest/gtest.h>
#include "MemSnapshotSegmentService.h"
#include "MemSnapshotDefs.h"
#include "SegmentSummaryCalculator.h"
#include "MemSnapshotStateCache.h"

using namespace Dic::Module::MemSnapshot;

class TestableMemSnapshotSegmentService : public MemSnapshotSegmentService {
  public:
    static int FindSegmentIdxByAddr(const std::vector<Segment> &segments, uint64_t addr) {
        return MemSnapshotSegmentService::FindSegmentIdxByAddr(segments, addr);
    }

    static std::vector<Segment> BuildSegmentsByEvents(const std::vector<TraceEntry> &events) {
        return MemSnapshotSegmentService::BuildSegmentsByEvents(events);
    }

    static void HandleSegmentAlloc(std::vector<Segment> &segments, const TraceEntry &evt) {
        MemSnapshotSegmentService::HandleSegmentAlloc(segments, evt);
    }

    static void HandleSegmentFree(std::vector<Segment> &segments, const TraceEntry &evt) {
        MemSnapshotSegmentService::HandleSegmentFree(segments, evt);
    }

    static void HandleSegmentMap(std::vector<Segment> &segments, const TraceEntry &evt) {
        MemSnapshotSegmentService::HandleSegmentMap(segments, evt);
    }

    static void HandleSegmentUnmap(std::vector<Segment> &segments, const TraceEntry &evt) {
        MemSnapshotSegmentService::HandleSegmentUnmap(segments, evt);
    }

    static void MergeAdjacentSegments(std::vector<Segment> &segments, int curIdx) {
        MemSnapshotSegmentService::MergeAdjacentSegments(segments, curIdx);
    }

    static void BuildSegments(std::vector<Segment> &segments, const std::vector<Block> &blocks) {
        MemSnapshotSegmentService::BuildSegments(segments, blocks);
    }
};

class MemSnapshotSegmentServiceTest : public ::testing::Test {
  protected:
    void SetUp() override {}
    void TearDown() override {}
};

// 测试FindSegmentIdxByAddr - 单个segment
TEST_F(MemSnapshotSegmentServiceTest, FindSegmentIdxByAddrSingleSegment) {
    std::vector<Segment> segments;
    Segment seg(1000, 500);
    segments.push_back(seg);

    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 1000), 0);
    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 1200), 0);
    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 1499), 0);
    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 999), -1);
    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 1500), -1);
}

// 测试FindSegmentIdxByAddr - 多个segments
TEST_F(MemSnapshotSegmentServiceTest, FindSegmentIdxByAddrMultipleSegments) {
    std::vector<Segment> segments;
    Segment seg1(1000, 500), seg2(2000, 500), seg3(3000, 500);
    segments.push_back(seg1);
    segments.push_back(seg2);
    segments.push_back(seg3);

    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 1000), 0);
    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 2000), 1);
    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 3000), 2);
    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 2500), -1);
    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 999), -1);
    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 1500), -1);
    EXPECT_EQ(TestableMemSnapshotSegmentService::FindSegmentIdxByAddr(segments, 3500), -1);
}

// 测试HandleSegmentAlloc - 插入新segment
TEST_F(MemSnapshotSegmentServiceTest, HandleSegmentAllocInsert) {
    std::vector<Segment> segments;
    TraceEntry evt;
    evt.id = 1;
    evt.action = TRACE_ENTRY_ACTION_SEG_ALLOC;
    evt.address = 1000;
    evt.size = 500;
    evt.stream = 0;
    TestableMemSnapshotSegmentService::HandleSegmentAlloc(segments, evt);
    EXPECT_EQ(segments.size(), 1);
    EXPECT_EQ(segments[0].address, 1000);
    EXPECT_EQ(segments[0].totalSize, 500);
    EXPECT_EQ(segments[0].stream, 0);
    EXPECT_EQ(segments[0].allocOrMapEventId, 1);
}

// 测试HandleSegmentAlloc - 按地址排序插入
TEST_F(MemSnapshotSegmentServiceTest, HandleSegmentAllocInsertSorted) {
    std::vector<Segment> segments;
    TraceEntry evt1, evt2, evt3;
    evt1.id = 1;
    evt1.action = TRACE_ENTRY_ACTION_SEG_ALLOC;
    evt1.address = 2000;
    evt1.size = 500;
    evt1.stream = 0;
    evt2.id = 2;
    evt2.action = TRACE_ENTRY_ACTION_SEG_ALLOC;
    evt2.address = 1000;
    evt2.size = 300;
    evt2.stream = 1;
    evt3.id = 3;
    evt3.action = TRACE_ENTRY_ACTION_SEG_ALLOC;
    evt3.address = 3000;
    evt3.size = 700;
    evt3.stream = 2;
    const std::vector<TraceEntry> evts = {evt1, evt2, evt3};
    for (auto &evt : evts) {
        TestableMemSnapshotSegmentService::HandleSegmentAlloc(segments, evt);
    }
    EXPECT_EQ(segments.size(), 3);
    EXPECT_EQ(segments[0].address, 1000);
    EXPECT_EQ(segments[1].address, 2000);
    EXPECT_EQ(segments[2].address, 3000);
}

// 测试HandleSegmentFree - 删除segment
TEST_F(MemSnapshotSegmentServiceTest, HandleSegmentFreeRemove) {
    std::vector<Segment> segments = {{1000, 500, 0}};
    TraceEntry evt;
    evt.address = 1000;
    TestableMemSnapshotSegmentService::HandleSegmentFree(segments, evt);
    EXPECT_EQ(segments.size(), 0);
}

// 测试HandleSegmentMap - 插入并合并
TEST_F(MemSnapshotSegmentServiceTest, HandleSegmentMapInsertAndMerge) {
    std::vector<Segment> segments = {{1000, 500, 0}};
    TraceEntry evt;
    evt.id = 1;
    evt.address = 1500;
    evt.size = 300;
    evt.stream = 0;
    TestableMemSnapshotSegmentService::HandleSegmentMap(segments, evt);
    EXPECT_EQ(segments.size(), 1);
    EXPECT_EQ(segments[0].address, 1000);
    EXPECT_EQ(segments[0].totalSize, 800);
}

// 测试HandleSegmentUnmap - 完全删除segment
TEST_F(MemSnapshotSegmentServiceTest, HandleSegmentUnmapFullDelete) {
    std::vector<Segment> segments = {{1000, 500, 0}};
    TraceEntry evt;
    evt.address = 1000;
    evt.size = 500;
    evt.stream = 0;
    TestableMemSnapshotSegmentService::HandleSegmentUnmap(segments, evt);
    EXPECT_EQ(segments.size(), 0);
}

// 测试HandleSegmentUnmap - 部分删除（从头部）
TEST_F(MemSnapshotSegmentServiceTest, HandleSegmentUnmapPartialFromHead) {
    std::vector<Segment> segments = {{1000, 500, 0}};
    TraceEntry evt;
    evt.address = 1000;
    evt.size = 200;
    evt.stream = 0;
    TestableMemSnapshotSegmentService::HandleSegmentUnmap(segments, evt);
    EXPECT_EQ(segments.size(), 1);
    EXPECT_EQ(segments[0].address, 1200);
    EXPECT_EQ(segments[0].totalSize, 300);
}

// 测试HandleSegmentUnmap - 部分删除（从中间分割）
TEST_F(MemSnapshotSegmentServiceTest, HandleSegmentUnmapPartialFromMiddle) {
    std::vector<Segment> segments = {{1000, 500, 0}};
    TraceEntry evt;
    evt.address = 1200;
    evt.size = 100;
    evt.stream = 0;
    TestableMemSnapshotSegmentService::HandleSegmentUnmap(segments, evt);
    EXPECT_EQ(segments.size(), 2);
    EXPECT_EQ(segments[0].address, 1000);
    EXPECT_EQ(segments[0].totalSize, 200);
    EXPECT_EQ(segments[1].address, 1300);
    EXPECT_EQ(segments[1].totalSize, 200);
}

// 测试BuildSegmentsByEvents - 空事件列表
TEST_F(MemSnapshotSegmentServiceTest, BuildSegmentsByEventsEmpty) {
    std::vector<TraceEntry> events;
    const auto segments = TestableMemSnapshotSegmentService::BuildSegmentsByEvents(events);
    EXPECT_EQ(segments.size(), 0);
}

// 测试BuildSegments - 将blocks分配到segments
TEST_F(MemSnapshotSegmentServiceTest, BuildSegmentsWithBlocks) {
    std::vector<Segment> segments = {{1000, 1000, 0}};
    Block block1, block2;
    block1.address = 1000;
    block1.size = 200;
    block1.state = BLOCK_STATE_ACTIVE_ALLOC;
    block2.address = 1300;
    block2.size = 300;
    block2.state = BLOCK_STATE_ACTIVE_PENDING_FREE;
    std::vector<Block> blocks = {block1, block2};
    TestableMemSnapshotSegmentService::BuildSegments(segments, blocks);
    EXPECT_EQ(segments.size(), 1);
    EXPECT_EQ(segments[0].blocks.size(), 2);
    EXPECT_EQ(segments[0].active, 500);
    EXPECT_EQ(segments[0].allocated, 200);
}

// 测试BuildSegments - block不在任何segment中
TEST_F(MemSnapshotSegmentServiceTest, BuildSegmentsBlockNotInSegment) {
    std::vector<Segment> segments = {{1000, 500, 0}};
    Block block1;
    block1.address = 2000;
    block1.size = 100;
    block1.state = BLOCK_STATE_ACTIVE_ALLOC;
    const std::vector<Block> blocks = {block1};
    TestableMemSnapshotSegmentService::BuildSegments(segments, blocks);
    EXPECT_EQ(segments.size(), 1);
    EXPECT_EQ(segments[0].blocks.size(), 0);
    EXPECT_EQ(segments[0].active, 0);
    EXPECT_EQ(segments[0].allocated, 0);
}

// 测试BuildSegments - 按totalSize排序
TEST_F(MemSnapshotSegmentServiceTest, BuildSegmentsSortedBySize) {
    std::vector<Segment> segments = {{1000, 500, 0}, {2000, 1000, 0}, {3000, 300, 0}};
    Block block1, block2;
    block1.address = 2000;
    block1.size = 100;
    block1.state = BLOCK_STATE_ACTIVE_ALLOC;
    block2.address = 3000;
    block2.size = 200;
    block2.state = BLOCK_STATE_ACTIVE_PENDING_FREE;
    const std::vector<Block> blocks = {block1, block2};
    TestableMemSnapshotSegmentService::BuildSegments(segments, blocks);
    EXPECT_EQ(segments.size(), 3);
    EXPECT_EQ(segments[0].totalSize, 300);
    EXPECT_EQ(segments[1].totalSize, 500);
    EXPECT_EQ(segments[2].totalSize, 1000);
}

// 测试SegmentSummary - 空segment
TEST_F(MemSnapshotSegmentServiceTest, ComputeSegmentSummaryEmptySegment) {
    Segment segment(1000, 100, 0);

    const auto summary = ComputeSegmentSummary(segment);

    EXPECT_EQ(summary.segmentSize, 100);
    EXPECT_EQ(summary.allocatedSize, 0);
    EXPECT_EQ(summary.gapSize, 100);
    EXPECT_EQ(summary.blockCount, 0);
    EXPECT_EQ(summary.gapCount, 1);
    EXPECT_EQ(summary.maxGapSize, 100);
}

// 测试SegmentSummary - 小block切割大空闲
TEST_F(MemSnapshotSegmentServiceTest, ComputeSegmentSummaryBlockSplitsFreeSpace) {
    Segment segment(1000, 100, 0);
    segment.allocated = 4;
    Block block;
    block.address = 1040;
    block.size = 4;
    segment.blocks.push_back(block);

    const auto summary = ComputeSegmentSummary(segment);

    EXPECT_EQ(summary.segmentSize, 100);
    EXPECT_EQ(summary.allocatedSize, 4);
    EXPECT_EQ(summary.gapSize, 96);
    EXPECT_EQ(summary.blockCount, 1);
    EXPECT_EQ(summary.gapCount, 2);
    EXPECT_EQ(summary.maxGapSize, 56);
}

// 测试SegmentSummary - 无序blocks输入
TEST_F(MemSnapshotSegmentServiceTest, ComputeSegmentSummarySortsBlocksByAddress) {
    Segment segment(1000, 100, 0);
    segment.allocated = 30;
    Block second;
    second.address = 1060;
    second.size = 10;
    Block first;
    first.address = 1020;
    first.size = 20;
    segment.blocks = {second, first};

    const auto summary = ComputeSegmentSummary(segment);

    EXPECT_EQ(summary.allocatedSize, 30);
    EXPECT_EQ(summary.gapSize, 70);
    EXPECT_EQ(summary.gapCount, 3);
    EXPECT_EQ(summary.maxGapSize, 30);
}

// 测试SegmentSummary - 完全占满
TEST_F(MemSnapshotSegmentServiceTest, ComputeSegmentSummaryFullyAllocated) {
    Segment segment(1000, 100, 0);
    segment.allocated = 100;
    Block block;
    block.address = 1000;
    block.size = 100;
    segment.blocks.push_back(block);

    const auto summary = ComputeSegmentSummary(segment);

    EXPECT_EQ(summary.allocatedSize, 100);
    EXPECT_EQ(summary.gapSize, 0);
    EXPECT_EQ(summary.gapCount, 0);
    EXPECT_EQ(summary.maxGapSize, 0);
}

// 测试SegmentSummary - allocatedSize沿用Segment::allocated语义
TEST_F(MemSnapshotSegmentServiceTest, ComputeSegmentSummaryUsesSegmentAllocatedSize) {
    Segment segment(1000, 100, 0);
    segment.allocated = 20;
    Block activeAllocated;
    activeAllocated.address = 1000;
    activeAllocated.size = 20;
    activeAllocated.state = BLOCK_STATE_ACTIVE_ALLOC;
    Block pendingFree;
    pendingFree.address = 1040;
    pendingFree.size = 30;
    pendingFree.state = BLOCK_STATE_ACTIVE_PENDING_FREE;
    segment.blocks = {activeAllocated, pendingFree};

    const auto summary = ComputeSegmentSummary(segment);

    EXPECT_EQ(summary.allocatedSize, segment.allocated);
    EXPECT_EQ(summary.gapSize, 50);
    EXPECT_EQ(summary.blockCount, 2);
}

// 测试MemSnapshotStateCache - 按数据源、设备、事件隔离
TEST_F(MemSnapshotSegmentServiceTest, StateCacheIsScopedByDataDeviceAndEvent) {
    MemSnapshotStateCache::ClearAll();
    std::vector<Segment> segments = {{1000, 100, 0}};

    MemSnapshotStateCache::Put("dataA", "0", 1, segments);

    EXPECT_TRUE(MemSnapshotStateCache::Get("dataA", "0", 1).has_value());
    EXPECT_FALSE(MemSnapshotStateCache::Get("dataA", "0", 2).has_value());
    EXPECT_FALSE(MemSnapshotStateCache::Get("dataA", "1", 1).has_value());
    EXPECT_FALSE(MemSnapshotStateCache::Get("dataB", "0", 1).has_value());
}

// 测试MemSnapshotStateCache - 清理指定数据源
TEST_F(MemSnapshotSegmentServiceTest, StateCacheClearsDataSourceOnly) {
    MemSnapshotStateCache::ClearAll();
    std::vector<Segment> segments = {{1000, 100, 0}};
    MemSnapshotStateCache::Put("dataA", "0", 1, segments);
    MemSnapshotStateCache::Put("dataB", "0", 1, segments);

    MemSnapshotStateCache::ClearData("dataA");

    EXPECT_FALSE(MemSnapshotStateCache::Get("dataA", "0", 1).has_value());
    EXPECT_TRUE(MemSnapshotStateCache::Get("dataB", "0", 1).has_value());
    MemSnapshotStateCache::ClearAll();
}
// 测试MemSnapshotStateCache - 缓存条目数量有上限
TEST_F(MemSnapshotSegmentServiceTest, StateCacheEvictsOldEntries) {
    MemSnapshotStateCache::ClearAll();
    std::vector<Segment> segments = {{1000, 100, 0}};

    for (uint64_t eventId = 0; eventId < 33; ++eventId) {
        MemSnapshotStateCache::Put("dataA", "0", eventId, segments);
    }

    EXPECT_FALSE(MemSnapshotStateCache::Get("dataA", "0", 0).has_value());
    EXPECT_TRUE(MemSnapshotStateCache::Get("dataA", "0", 32).has_value());
    MemSnapshotStateCache::ClearAll();
}
