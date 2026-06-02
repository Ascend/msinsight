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

#include <gtest/gtest.h>
#include <vector>
#include "QueryMemcpyOverallHandler.h"

namespace Dic::Module::Timeline::Test {

// 辅助验证函数（复用）
void VerifyMemcpyRes(const MemcpyOverallRes &res, uint64_t expTotalSize, double expTotalTime, uint64_t expCount,
    double expAvgSize, uint64_t expMinSize, uint64_t expMaxSize) {
    EXPECT_EQ(res.totalSize, expTotalSize);
    EXPECT_DOUBLE_EQ(res.totalTime, expTotalTime);
    EXPECT_EQ(res.number, expCount);
    EXPECT_DOUBLE_EQ(res.avgSize, expAvgSize);
    EXPECT_EQ(res.minSize, expMinSize);
    EXPECT_EQ(res.maxSize, expMaxSize);
    // 时间字段验证逻辑类似（略）
}

// ===== 核心测试：直接调用非成员函数 =====
TEST(BuildMemcpyOverallResultTest, EmptyInputYieldsEmptyResult) {
    std::vector<MemcpyRecord> records;
    MemcpyOverallResponse response;
    BuildMemcpyOverallResult(records, response, 1, 10);
    EXPECT_TRUE(response.details.empty());
}

TEST(BuildMemcpyOverallResultTest, SingleThreadSingleMemcpyType) {
    std::vector<MemcpyRecord> records = {{"1", "A", "H2D", 100, 1.0}, {"1", "A", "H2D", 200, 2.0}};
    MemcpyOverallResponse response;
    BuildMemcpyOverallResult(records, response, 1, 10);

    ASSERT_EQ(response.details.size(), 1U);
    const auto &thread = response.details[0];
    EXPECT_EQ(thread.key, "1");
    VerifyMemcpyRes(thread, 300, 3.0, 2, 150.0, 100, 200);

    ASSERT_EQ(thread.children.size(), 1U);
    const auto &type = thread.children[0];
    EXPECT_EQ(type.key, "H2D");
    // ✅ 验证子项使用自身统计（关键Bug修复点）
    VerifyMemcpyRes(type, 300, 3.0, 2, 150.0, 100, 200);
}

TEST(BuildMemcpyOverallResultTest, MultiThreadMultiTypeWithCorrectGrouping) {
    std::vector<MemcpyRecord> records = {
        {"2", "B", "D2H", 50, 0.5}, // Thread2
        {"1", "A", "H2D", 100, 1.0}, // Thread1
        {"1", "A", "D2H", 300, 3.0}, // Thread1
        {"1", "A", "H2D", 150, 1.5} // Thread1
    };
    MemcpyOverallResponse response;
    BuildMemcpyOverallResult(records, response, 1, 10);

    // 验证自动按threadId升序（map特性）
    ASSERT_EQ(response.details.size(), 2U);
    EXPECT_EQ(response.details[0].key, "1"); // tid=1
    EXPECT_EQ(response.details[1].key, "2"); // tid=2

    // 验证Thread1的子类型按字典序（D2H < H2D）
    const auto &t1 = response.details[0];
    ASSERT_EQ(t1.children.size(), 2U);
    EXPECT_EQ(t1.children[0].key, "D2H"); // ✅ 字典序
    VerifyMemcpyRes(t1.children[0], 300, 3.0, 1, 300.0, 300, 300); // D2H独立统计

    EXPECT_EQ(t1.children[1].key, "H2D");
    VerifyMemcpyRes(t1.children[1], 250, 2.5, 2, 125.0, 100, 150); // H2D独立统计
}

TEST(BuildMemcpyOverallResultTest, ZeroValuesHandledSafely) {
    std::vector<MemcpyRecord> records = {{"1", "A", "ZERO", 0, 0.0}, {"1", "A", "ZERO", 0, 0.0}};
    MemcpyOverallResponse response;
    BuildMemcpyOverallResult(records, response, 1, 10);

    ASSERT_EQ(response.details.size(), 1U);
    const auto &res = response.details[0];
    EXPECT_EQ(res.minSize, 0U); // ✅ 非空时返回实际最小值（0）
    EXPECT_EQ(res.maxSize, 0U);
    EXPECT_DOUBLE_EQ(res.avgSize, 0.0);
}

// ===== 边界测试：StatsAccumulator 安全性（补充）=====
TEST(BuildMemcpyOverallResultTest, ExtremeValuesNoCrash) {
    std::vector<MemcpyRecord> records = {
        {"1", "A", "BIG", std::numeric_limits<uint64_t>::max(), 1e300}, {"1", "A", "BIG", 1, 1e-300}};
    MemcpyOverallResponse response;
    EXPECT_NO_THROW(BuildMemcpyOverallResult(records, response, 1, 10));
    ASSERT_EQ(response.details.size(), 1U);
    EXPECT_GT(response.details[0].maxSize, 0U);
}

} // namespace Dic::Module::Timeline::Test
