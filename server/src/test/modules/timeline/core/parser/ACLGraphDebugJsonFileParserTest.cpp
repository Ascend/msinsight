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

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include "ACLGraphDebugJsonFileParser.h"
#include "TextTraceDatabase.h"
#include "ThreadPool.h"

namespace Dic::Module::Timeline {
inline bool operator==(const SliceDto &lhs, const SliceDto &rhs) {
    // 仅比较 PostParse 逻辑直接影响的关键字段
    return lhs.trackId == rhs.trackId && lhs.timestamp == rhs.timestamp && // 排序/传播修改的核心
        lhs.duration == rhs.duration && // WAIT 调整的核心
        lhs.name == rhs.name && // 事件标识
        lhs.id == rhs.id; // 唯一标识
    // 故意省略 tid/pid/cat：这些由数据库提供，测试不验证其值
    // 避免因生产代码新增字段导致测试脆弱
}

namespace Trace {
inline bool operator==(const Flow &lhs, const Flow &rhs) {
    // 仅比较明确设置的字段（测试验证点）
    return lhs.trackId == rhs.trackId && lhs.ts == rhs.ts && lhs.flowId == rhs.flowId && lhs.name == rhs.name &&
        lhs.cat == rhs.cat;
    // 故意省略 tid/pid：由原始事件继承，测试不验证其值
    // std::optional 支持直接 == 比较（C++17+）
}
}
}

using namespace Dic::Module::Timeline;

class MockTextTraceDatabase : public TextTraceDatabase {
  public:
    explicit MockTextTraceDatabase(std::recursive_mutex &m) : TextTraceDatabase(m) {}
    MOCK_METHOD(bool, QuerySliceDtoList, (std::vector<SliceDto> &), (override));
    MOCK_METHOD(bool, ReplaceAllSlices, (const std::vector<SliceDto> &), (override));
    MOCK_METHOD(bool, InsertFlowList, (const std::vector<Trace::Flow> &), (override));
};

// 仅在测试文件中定义
class ACLGraphDebugJsonFileParserForTest : public ACLGraphDebugJsonFileParser {
  public:
    // 继承构造函数（保留 explicit 语义）
    using ACLGraphDebugJsonFileParser::ACLGraphDebugJsonFileParser;

    // 将 protected 方法提升为 public（安全转发）
    bool PostParse(std::shared_ptr<TextTraceDatabase> db, const std::string &rankId = "") override {
        return ACLGraphDebugJsonFileParser::PostParse(std::move(db), rankId);
    }
};

const std::string CAT = "acl";

class ACLGraphPostParseTest : public ::testing::Test {
  protected:
    std::unique_ptr<ACLGraphDebugJsonFileParserForTest> parser;
    std::shared_ptr<MockTextTraceDatabase> mock_db;
    std::shared_ptr<ThreadPool> test_pool;
    std::recursive_mutex test_mutex;

    void SetUp() override {
        test_pool = std::make_shared<ThreadPool>(1);
        parser = std::make_unique<ACLGraphDebugJsonFileParserForTest>(test_pool);
        mock_db = std::make_shared<MockTextTraceDatabase>(test_mutex);
    }

    // RAII 自动管理生命周期（无需自定义 TearDown）
    // 执行顺序：
    // 1. parser (unique_ptr) 析构 → 释放对 test_pool 的引用
    // 2. test_pool (shared_ptr) 析构 → 引用计数归零 → ThreadPool::~ThreadPool() 被调用
    // 3. ThreadPool 析构安全 join 线程（无任务提交，立即退出）

    static SliceDto CreateSlice(uint64_t trackId, uint64_t ts, uint64_t dur, const std::string &name, uint64_t id = 0) {
        SliceDto s;
        s.trackId = trackId;
        s.timestamp = ts;
        s.duration = dur;
        s.name = name;
        s.id = id;
        s.tid = "tid";
        s.pid = "pid";
        s.cat = CAT;
        return s;
    }

    static Trace::Flow CreateFlow(uint64_t trackId, int64_t ts, std::string flowId, std::string name, std::string type,
        std::optional<std::string> cat = std::nullopt) {
        Trace::Flow flow;
        flow.trackId = trackId;
        flow.ts = ts;
        flow.flowId = std::move(flowId);
        flow.name = std::move(name);
        flow.type = type;
        flow.tid = "tid";
        flow.pid = "pid";
        flow.cat = cat;
        return flow;
    }
};

// ==================== 测试用例 ====================

// 场景1: 数据库查询失败 → 返回 false
TEST_F(ACLGraphPostParseTest, QueryFails_ReturnsFalse) {
    EXPECT_CALL(*mock_db, QuerySliceDtoList(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(std::vector<SliceDto>{}), testing::Return(false)));
    EXPECT_CALL(*mock_db, ReplaceAllSlices(testing::_)).Times(0);
    EXPECT_CALL(*mock_db, InsertFlowList(testing::_)).Times(0);

    EXPECT_FALSE(parser->PostParse(mock_db));
}

// 场景2: 无 slices → 返回 true（空处理）
TEST_F(ACLGraphPostParseTest, EmptySlices_ReturnsTrue) {
    std::vector<SliceDto> empty;
    EXPECT_CALL(*mock_db, QuerySliceDtoList(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(empty), testing::Return(true)));
    EXPECT_CALL(*mock_db, ReplaceAllSlices(testing::_)).Times(0);
    EXPECT_CALL(*mock_db, InsertFlowList(testing::_)).Times(0);

    EXPECT_TRUE(parser->PostParse(mock_db));
}

// 场景3: ReplaceAllSlices 失败 → 返回 false（关键失败点）
TEST_F(ACLGraphPostParseTest, ReplaceSlicesFails_ReturnsFalse) {
    auto slices = std::vector<SliceDto>{CreateSlice(1, 100, 50, "op1", 1), CreateSlice(1, 200, 30, "op2", 2)};
    EXPECT_CALL(*mock_db, QuerySliceDtoList(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(slices), testing::Return(true)));
    EXPECT_CALL(*mock_db, ReplaceAllSlices(testing::_)).WillOnce(testing::Return(false));
    EXPECT_CALL(*mock_db, InsertFlowList(testing::_)).Times(0);

    EXPECT_FALSE(parser->PostParse(mock_db));
}

// 场景4: InsertFlowList 失败 → 返回 true（非致命错误）
TEST_F(ACLGraphPostParseTest, InsertFlowsFails_ReturnsTrue) {
    auto slices = std::vector<SliceDto>{CreateSlice(1, 100, 50, "op1", 1)};
    EXPECT_CALL(*mock_db, QuerySliceDtoList(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(slices), testing::Return(true)));
    // 验证 Replace 被调用（参数简化验证：非空）
    EXPECT_CALL(*mock_db, ReplaceAllSlices(testing::Truly([](const auto &v) {
        return !v.empty();
    }))).WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_db, InsertFlowList(testing::_)).WillOnce(testing::Return(false)); // Flow 插入失败

    EXPECT_TRUE(parser->PostParse(mock_db)); // 非致命，仍返回 true
}

// 场景5: 完整成功流程（无同步对）→ 验证数据流
TEST_F(ACLGraphPostParseTest, Success_NoSyncPairs_VerifyCompacted) {
    // 输入：单 track 两个普通事件（无 WAIT/RECORD）
    auto input_slices = std::vector<SliceDto>{CreateSlice(1, 200, 30, "event2", 2), // 乱序
        CreateSlice(1, 100, 50, "event1", 1)};
    // 预期：按 timestamp 排序后的结果
    auto expected_compacted =
        std::vector<SliceDto>{CreateSlice(1, 100, 50, "event1", 1), CreateSlice(1, 200, 30, "event2", 2)};

    EXPECT_CALL(*mock_db, QuerySliceDtoList(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(input_slices), testing::Return(true)));
    // 验证 ReplaceAllSlices 接收排序后的数据
    EXPECT_CALL(*mock_db, ReplaceAllSlices(testing::Eq(expected_compacted))).WillOnce(testing::Return(true));
    // 无同步对 → Flow 列表为空
    EXPECT_CALL(*mock_db, InsertFlowList(testing::Eq(std::vector<Trace::Flow>{}))).WillOnce(testing::Return(true));

    EXPECT_TRUE(parser->PostParse(mock_db));
}

// 场景6: 含有效同步对 → 验证 Flow 生成（简化验证）
TEST_F(ACLGraphPostParseTest, Success_WithSyncPair_VerifyFlowCount) {
    // 构造配对的 RECORD 和 WAIT（同一 track 简化）
    auto slices = std::vector<SliceDto>{
        CreateSlice(1, 100, 20, "EVENT_RECORD_sync1", 1),
        CreateSlice(1, 150, 10, "EVENT_WAIT_sync1", 2) // WAIT 在 RECORD 后
    };

    EXPECT_CALL(*mock_db, QuerySliceDtoList(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(slices), testing::Return(true)));
    // 验证 Replace 被调用（非空）
    EXPECT_CALL(*mock_db, ReplaceAllSlices(testing::Truly([](const auto &v) {
        return v.size() == 2;
    }))).WillOnce(testing::Return(true));
    // 验证生成 2 个 Flow 事件（1 对）
    EXPECT_CALL(*mock_db, InsertFlowList(testing::Truly([](const std::vector<Trace::Flow> &flows) {
        return flows.size() == 2 && flows[0].type == Protocol::LINE_START && flows[1].type == Protocol::LINE_END;
    }))).WillOnce(testing::Return(true));

    EXPECT_TRUE(parser->PostParse(mock_db));
}

// ============================================================================
// AdjustSyncPairs 用例1: 基础同步调整（单Track，需延长WAIT）
// 验证：delta>0时WAIT.duration延长 + 同Track后续事件时间传播 + Flow生成
// ============================================================================
TEST_F(ACLGraphPostParseTest, AdjustSyncPairs_SingleTrack_WaitExtended) {
    // 输入：WAIT在RECORD前（vector顺序），时间上WAIT结束早于RECORD结束
    auto input_slices = std::vector<SliceDto>{
        CreateSlice(1, 50, 50, "EVENT_WAIT_A", 1), // idx0: 需延长
        CreateSlice(2, 100, 100, "EVENT_RECORD_A", 2), // idx1: 无影响
        CreateSlice(1, 100, 100, "EVENT_RECORD_B", 3) // idx1: 被传播
    };

    // 预期调整结果（AdjustSyncPairs + MergeAndSortSlices后）:
    // WAIT_A: dur=50+100=150 (end=200); RECORD_A.ts=100
    auto expected_compacted = std::vector<SliceDto>{
        CreateSlice(1, 50, 150, "EVENT_WAIT_A", 1), // duration延长
        CreateSlice(1, 200, 100, "EVENT_RECORD_B", 3), // timestamp被传播偏移
        CreateSlice(2, 100, 100, "EVENT_RECORD_A", 2) // 无影响
    };

    // 预期Flow: 使用调整后的RECORD时间戳
    std::vector<Trace::Flow> expected_flows = {
        CreateFlow(2, 100, "syncflow_A_0", "SyncStart_A", Protocol::LINE_START, CAT),
        CreateFlow(1, 50, "syncflow_A_0", "SyncEnd_A", Protocol::LINE_END, CAT)};

    // Mock数据库交互
    EXPECT_CALL(*mock_db, QuerySliceDtoList(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(input_slices), testing::Return(true)));
    EXPECT_CALL(*mock_db, ReplaceAllSlices(testing::Eq(expected_compacted))).WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_db, InsertFlowList(testing::Eq(expected_flows))).WillOnce(testing::Return(true));

    EXPECT_TRUE(parser->PostParse(mock_db));
    // 日志: 应无"Skip WAIT sync"警告
}

// ============================================================================
// AdjustSyncPairs 用例2: 跨Track依赖传播（核心路径）
// 验证：依赖图构建 → 拓扑序(WAIT_B→WAIT_A) → 跨Track时间传播链
// ============================================================================
TEST_F(ACLGraphPostParseTest, AdjustSyncPairs_CrossTrack_DependencyPropagation) {
    // 输入：构造依赖链 WAIT_B → WAIT_A (因WAIT_A依赖track1中WAIT_B前的事件)
    auto input_slices = std::vector<SliceDto>{// Track1: WAIT_B 依赖 Track2的RECORD_B; RECORD_A 被 WAIT_B调整传播影响
        CreateSlice(1, 50, 50, "EVENT_WAIT_B", 1), CreateSlice(1, 150, 50, "EVENT_RECORD_A", 2),
        // Track2: RECORD_B 无依赖; WAIT_A 依赖 Track1的RECORD_A (且受WAIT_B传播影响)
        CreateSlice(2, 200, 100, "EVENT_RECORD_B", 3), CreateSlice(2, 100, 50, "EVENT_WAIT_A", 4)};

    // 预期调整结果:
    // 1. WAIT_B: delta=(200+100)-(50+50)=200 → dur=250; 传播200至Track1后续 → RECORD_A.ts=350
    // 2. WAIT_A: delta=(350+50)-(100+50)=250 → dur=300; 无后续事件
    auto expected_compacted = std::vector<SliceDto>{
        CreateSlice(1, 50, 250, "EVENT_WAIT_B", 1), // track1: WAIT_B延长
        CreateSlice(1, 350, 50, "EVENT_RECORD_A", 2), // track1: 被WAIT_B传播偏移
        CreateSlice(2, 100, 300, "EVENT_WAIT_A", 4), // track2: WAIT_A延长（注意：ts未变，dur变）
        CreateSlice(2, 200, 100, "EVENT_RECORD_B", 3) // track2: RECORD_B未被修改（不在传播路径）
    };

    // 预期Flow: 2对，flowId唯一；ts取调整后事件start时间
    std::vector<Trace::Flow> expected_flows = {
        // Flow for A (pair_idx=0)
        CreateFlow(1, 350, "syncflow_A_0", "SyncStart_A", Protocol::LINE_START, CAT), // Adjusted RECORD_A
        CreateFlow(2, 100, "syncflow_A_0", "SyncEnd_A", Protocol::LINE_END, CAT), // WAIT_A in track2
        // Flow for B (pair_idx=1)
        CreateFlow(2, 200, "syncflow_B_1", "SyncStart_B", Protocol::LINE_START, CAT), // RECORD_B in track2
        CreateFlow(1, 50, "syncflow_B_1", "SyncEnd_B", Protocol::LINE_END, CAT) // WAIT_B in track1
    };
    EXPECT_CALL(*mock_db, QuerySliceDtoList(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(input_slices), testing::Return(true)));
    EXPECT_CALL(*mock_db, ReplaceAllSlices(testing::Eq(expected_compacted))).WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_db, InsertFlowList(testing::Eq(expected_flows))).WillOnce(testing::Return(true));

    EXPECT_TRUE(parser->PostParse(mock_db));
    // 日志: 应包含 "Generated 2 sync flow pairs"
}

// ============================================================================
// AdjustSyncPairs 用例3: 跳过无效调整（delta<0）
// 验证：delta<0时跳过调整 + 仍生成Flow + 无时间传播
// ============================================================================
TEST_F(ACLGraphPostParseTest, AdjustSyncPairs_SkipInvalidDelta_NegativeDelta) {
    // 输入：WAIT结束时间 > RECORD结束时间 → delta = (100+50) - (200+100) = -150
    auto input_slices = std::vector<SliceDto>{
        CreateSlice(1, 100, 50, "EVENT_RECORD_X", 1), // end=150
        CreateSlice(1, 200, 100, "EVENT_WAIT_X", 2) // end=300 → delta<0
    };

    // 预期：事件数据不变（仅按timestamp排序）
    auto expected_compacted = std::vector<SliceDto>{
        CreateSlice(1, 100, 50, "EVENT_RECORD_X", 1), CreateSlice(1, 200, 100, "EVENT_WAIT_X", 2)};

    // 预期Flow: 仍生成（同步关系存在），使用原始时间戳
    std::vector<Trace::Flow> expected_flows = {
        CreateFlow(1, 100, "syncflow_X_0", "SyncStart_X", Protocol::LINE_START, CAT),
        CreateFlow(1, 200, "syncflow_X_0", "SyncEnd_X", Protocol::LINE_END, CAT)};
    EXPECT_CALL(*mock_db, QuerySliceDtoList(testing::_))
        .WillOnce(testing::DoAll(testing::SetArgReferee<0>(input_slices), testing::Return(true)));
    EXPECT_CALL(*mock_db, ReplaceAllSlices(testing::Eq(expected_compacted))).WillOnce(testing::Return(true));
    EXPECT_CALL(*mock_db, InsertFlowList(testing::Eq(expected_flows))).WillOnce(testing::Return(true));

    EXPECT_TRUE(parser->PostParse(mock_db));
    // 日志: 应包含"Skip WAIT sync (id=2): wait stop time > record stop time + PADDING"
}
