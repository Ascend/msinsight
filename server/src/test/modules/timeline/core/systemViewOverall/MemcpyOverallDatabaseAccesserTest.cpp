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
#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <vector>
#include "MemcpyDetailTestDatabase.h"
#include "MemcpyOverallDatabaseAccesser.h"
#include "TraceTime.h"

namespace Dic::Module::Timeline {
class MemcpyOverallDatabaseAccesserIntegrationTest : public ::testing::Test {
  protected:
    void SetUp() override {
        TraceTime::Instance().Reset();
        TraceTime::Instance().UpdateTime(1000, 3000);
        database_ = std::make_shared<MemcpyDetailTestDatabase>(mutex_);
        ASSERT_TRUE(database_->OpenMemory());
        DataBaseManager::Instance().SetDataType(DataType::DB, fileId_);
        const char *schema = R"SQL(
            CREATE TABLE TASK(globalTaskId INTEGER, streamId INTEGER, taskType INTEGER, startNs INTEGER, endNs INTEGER);
            CREATE TABLE MEMCPY_INFO(globalTaskId INTEGER, memcpyOperation INTEGER, size INTEGER);
            CREATE TABLE ENUM_MEMCPY_OPERATION(id INTEGER, name TEXT);
            CREATE TABLE STRING_IDS(id INTEGER, value TEXT);
            INSERT INTO ENUM_MEMCPY_OPERATION VALUES(1,'host to device'),(2,'device to host');
            INSERT INTO STRING_IDS VALUES(1,'MEMCPY_ASYNC');
        )SQL";
        ASSERT_EQ(sqlite3_exec(database_->Handle(), schema, nullptr, nullptr, nullptr), SQLITE_OK);
        // Twelve disjoint records guarantee two pages and both directions on distinct streams.
        for (uint64_t id = 1; id <= 12; ++id) {
            const auto type = id % 2 == 1 ? 1 : 2;
            const auto stream = id % 2 == 1 ? 7 : 8;
            const auto start = 1000 + id * 100;
            const std::string sql = "INSERT INTO TASK VALUES(" + std::to_string(id) + "," + std::to_string(stream) +
                ",1," + std::to_string(start) + "," + std::to_string(start + 10) +
                "); INSERT INTO MEMCPY_INFO VALUES(" + std::to_string(id) + "," + std::to_string(type) + "," +
                std::to_string(id * 64) + ");";
            ASSERT_EQ(sqlite3_exec(database_->Handle(), sql.c_str(), nullptr, nullptr, nullptr), SQLITE_OK);
        }
        accessor_ = std::make_unique<MemcpyOverallDatabaseAccesser>(database_, fileId_);
    }

    void TearDown() override {
        accessor_.reset();
        database_.reset();
        TraceTime::Instance().Reset();
    }

    std::recursive_mutex mutex_;
    std::shared_ptr<MemcpyDetailTestDatabase> database_;
    std::unique_ptr<MemcpyOverallDatabaseAccesser> accessor_;
    const std::string fileId_ = "memcpy-overall-assertions-db";
};

// ======================
// 测试用例1: 验证基础查询功能（修复参数和字段名）
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyRecords_ReturnsValidRecords_WithFullTimeRange) {
    std::vector<MemcpyRecord> records;
    bool success = accessor_->GetMemcpyRecords(0, 0, records);

    EXPECT_TRUE(success) << "查询应成功完成";
    ASSERT_EQ(records.size(), 12u) << "测试数据库应包含Memcpy记录";

    // 验证关键字段（修复字段名：memcpyType 替代 operation）
    if (!records.empty()) {
        for (const auto &rec : records) {
            EXPECT_GT(rec.size, 0u) << "记录大小应大于0";
            EXPECT_FALSE(rec.memcpyType.empty()) << "Memcpy类型不应为空";
            EXPECT_GE(rec.endTime, rec.startTime) << "结束时间应 >= 开始时间";
            EXPECT_DOUBLE_EQ(rec.duration, static_cast<double>(rec.endTime - rec.startTime)) << "持续时间计算应准确";
        }
    }
}

// ======================
// 测试用例2: 验证时间范围过滤功能
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyRecords_RespectsTimeRangeFilter) {
    std::vector<MemcpyRecord> allRecords;
    const uint64_t MAX_TIME = 10000;
    ASSERT_TRUE(accessor_->GetMemcpyRecords(0, MAX_TIME, allRecords));

    ASSERT_EQ(allRecords.size(), 12u);

    std::sort(allRecords.begin(), allRecords.end(),
        [](const auto &left, const auto &right) { return left.startTime < right.startTime; });
    // 固定样本在中点两侧各有六条记录
    uint64_t midTime = (allRecords.front().startTime + allRecords.back().endTime) / 2;

    // 查询前半部分
    std::vector<MemcpyRecord> firstHalf;
    ASSERT_TRUE(accessor_->GetMemcpyRecords(0, midTime, firstHalf));

    // 查询后半部分
    std::vector<MemcpyRecord> secondHalf;
    ASSERT_TRUE(accessor_->GetMemcpyRecords(midTime + 1, MAX_TIME, secondHalf));

    // 验证过滤逻辑
    EXPECT_EQ(firstHalf.size(), 6u);
    EXPECT_EQ(secondHalf.size(), 6u);
    EXPECT_EQ(firstHalf.size() + secondHalf.size(), allRecords.size());

    // 验证时间范围正确性
    for (const auto &rec : firstHalf) {
        EXPECT_LE(rec.endTime, midTime) << "前半部分记录应在时间范围内";
    }
    for (const auto &rec : secondHalf) {
        EXPECT_GT(rec.startTime, midTime) << "后半部分记录应在时间范围内";
    }
}

// ======================
// 测试用例3: 验证空时间范围返回空结果
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyRecords_ReturnsEmpty_WhenNoRecordsInTimeRange) {
    std::vector<MemcpyRecord> records;
    const uint64_t MAX_TIME = 10000;
    // 使用极大时间范围外的区间（应无记录）
    bool success = accessor_->GetMemcpyRecords(MAX_TIME - 100, MAX_TIME, records);

    // 注意：查询成功但无数据是正常行为（返回true）
    EXPECT_TRUE(success) << "空结果集查询应成功";
    EXPECT_TRUE(records.empty()) << "指定时间范围外应无记录";
}

// ======================
// 测试用例4: 验证关键Memcpy操作类型存在
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyRecords_ContainsExpectedOperationTypes) {
    std::vector<MemcpyRecord> records;
    const uint64_t MAX_TIME = 10000;
    ASSERT_TRUE(accessor_->GetMemcpyRecords(0, MAX_TIME, records));

    ASSERT_EQ(records.size(), 12u);

    // 统计操作类型
    bool hasH2D = false;
    bool hasD2H = false;

    for (const auto &rec : records) {
        if (rec.memcpyType == "host to device") {
            hasH2D = true;
        }
        if (rec.memcpyType == "device to host") {
            hasD2H = true;
        }
    }

    // 根据PyTorch Profiler典型场景验证
    EXPECT_TRUE(hasH2D);
    EXPECT_TRUE(hasD2H) << "应包含主机与设备间数据传输记录";
}

// ======================
// 测试用例5: 构造必然溢出的时间参数
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyRecords_TimeOverflow_PreventsQueryExecution) {
    std::vector<MemcpyRecord> records;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t dangerousStart = std::numeric_limits<uint64_t>::max() + 1 - minTimestamp;
    const auto prepareCount = database_->prepareCount;
    EXPECT_FALSE(accessor_->GetMemcpyRecords(dangerousStart, dangerousStart + 100, records));
    EXPECT_EQ(database_->prepareCount, prepareCount);
    EXPECT_TRUE(records.empty());
}

// ======================
// 测试用例6: 验证分页基础功能（第一页数据完整性与排序）
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyDetailRecordsPaged_ValidFirstPage_DataIntegrityAndOrder) {
    std::vector<MemcpyDetailRecord> records;
    uint64_t total = 0;
    const uint64_t MAX_TIME = 10000;

    const OrderParam orderParam;
    // 查询第一页（10条）
    bool success = accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 1, 10, OrderParam{"startTime", "ascend"}, records, total);

    EXPECT_TRUE(success) << "分页查询应成功";
    EXPECT_GT(total, 0u) << "总记录数应大于0";
    EXPECT_LE(records.size(), 10u) << "第一页记录数不应超过pageSize";
    EXPECT_EQ(records.size(), std::min(total, static_cast<uint64_t>(10))) << "返回记录数应与总记录数和pageSize匹配";

    // 验证字段有效性与升序排序
    for (size_t i = 0; i < records.size(); ++i) {
        EXPECT_GT(records[i].timestamp, 0u) << "时间戳应有效";
        EXPECT_GE(records[i].duration, 0u) << "持续时间应非负";
        EXPECT_GT(records[i].size, 0u) << "数据大小应大于0";
        EXPECT_FALSE(records[i].name.empty()) << "操作类型名称不应为空";
        EXPECT_FALSE(records[i].id.empty()) << "记录ID不应为空";

        if (i > 0) {
            EXPECT_LE(records[i - 1].timestamp, records[i].timestamp) << "记录应按时间戳升序排列";
        }
    }
}

// ======================
// 测试用例7: 验证分页连续性与跨页数据一致性
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyDetailRecordsPaged_PageContinuity_NoDuplicationOrGap) {
    const uint64_t MAX_TIME = 10000;
    uint64_t total = 0;
    std::vector<MemcpyDetailRecord> page1, page2;

    // 获取第一页（验证总记录数）
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 1, 10, OrderParam{"startTime", "ascend"}, page1, total));
    ASSERT_EQ(total, 12u);
    ASSERT_EQ(page1.size(), 10u);

    // 获取第二页
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 2, 10, OrderParam{"startTime", "ascend"}, page2, total));

    // 验证第二页非空且记录数合理
    ASSERT_EQ(page2.size(), 2u);
    EXPECT_EQ(page1.front().id, "1");
    EXPECT_EQ(page1.back().id, "10");
    EXPECT_EQ(page2.front().id, "11");
    EXPECT_EQ(page2.back().id, "12");

    // 验证无重复：第二页首条时间戳 > 第一页末条时间戳
    if (!page1.empty() && !page2.empty()) {
        EXPECT_GT(page2.front().timestamp, page1.back().timestamp) << "分页间应无重复且严格升序";
    }

    // 验证总记录数一致性（两次查询total应相同）
    uint64_t total_check = 0;
    std::vector<MemcpyDetailRecord> dummy;
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 1, 1, OrderParam{"startTime", "ascend"}, dummy, total_check));
    EXPECT_EQ(total, total_check) << "多次查询总记录数应保持一致";
}

// ======================
// 测试用例8: 验证复合过滤条件（tid + memcpyType）
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyDetailRecordsPaged_CompositeFilter_CorrectSubset) {
    const uint64_t MAX_TIME = 10000;

    // 固定样本包含两个stream，每个stream有六条对应方向的记录
    std::vector<MemcpyRecord> baseRecords;
    ASSERT_TRUE(accessor_->GetMemcpyRecords(0, MAX_TIME, baseRecords));
    ASSERT_EQ(baseRecords.size(), 12u);

    // 选取有效过滤值
    const std::string exampleTid = "7";
    const std::string exampleType = "host to device";
    ASSERT_FALSE(exampleType.empty());

    // 无过滤总记录数
    uint64_t totalAll = 0;
    std::vector<MemcpyDetailRecord> allRecords;
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 1, 10000, OrderParam{"startTime", "ascend"}, allRecords, totalAll));

    // 应用复合过滤
    uint64_t filteredTotal = 0;
    std::vector<MemcpyDetailRecord> filteredRecords;
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(0, MAX_TIME, exampleTid, exampleType, 1, 10000,
        OrderParam{"startTime", "ascend"}, filteredRecords, filteredTotal));

    // 验证过滤效果
    EXPECT_EQ(filteredTotal, 6u) << "过滤后应有匹配记录";
    EXPECT_LE(filteredTotal, totalAll) << "过滤后记录数不应超过总数";
    ASSERT_EQ(filteredRecords.size(), 6u);
    for (size_t i = 0; i < filteredRecords.size(); ++i) {
        EXPECT_EQ(filteredRecords[i].id, std::to_string(1 + i * 2));
    }
}

// ======================
// 测试用例9: 验证空结果场景（无效过滤条件）
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyDetailRecordsPaged_EmptyResult_ValidState) {
    uint64_t total = 0;
    std::vector<MemcpyDetailRecord> records;

    // 使用不可能存在的过滤条件
    bool success = accessor_->GetMemcpyDetailRecordsPaged(
        0, 100, "999999999", "INVALID_TYPE", 1, 10, OrderParam{"startTime", "ascend"}, records, total);

    EXPECT_TRUE(success) << "空结果查询应成功（非错误）";
    EXPECT_TRUE(records.empty()) << "应返回空记录列表";
    EXPECT_EQ(total, 0u) << "总记录数应为0";
}

// ======================
// 测试用例10: 验证边界参数处理（超大页码/页大小）
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyDetailRecordsPaged_BoundaryParams_HandledGracefully) {
    const uint64_t MAX_TIME = 10000;
    uint64_t total = 0;
    std::vector<MemcpyDetailRecord> records;

    // 场景1: 页码超出范围（应返回空列表，total有效）
    EXPECT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 999999, 10, OrderParam{"startTime", "ascend"}, records, total));
    EXPECT_TRUE(records.empty()) << "超大页码应返回空列表";
    EXPECT_GT(total, 0u) << "total应仍为有效总记录数";

    // 场景2: 页大小为0（实现应安全处理，返回空页）
    records.clear();
    EXPECT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 1, 0, OrderParam{"startTime", "ascend"}, records, total));
    EXPECT_TRUE(records.empty()) << "pageSize=0应返回空列表";

    // 场景3: 页大小超建议上限（验证大页查询）
    records.clear();
    EXPECT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 1, 2000, OrderParam{"startTime", "ascend"}, records, total));
    EXPECT_LE(records.size(), total) << "大页查询应返回有效子集";
    if (total > 0) {
        EXPECT_GT(records.size(), 0u) << "应返回至少一条记录";
    }
}

// ======================
// 测试用例11: 验证时间范围过滤与相对时间处理
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyDetailRecordsPaged_TimeRangeFilter_Accurate) {
    const uint64_t MAX_TIME = 10000;

    // 获取全量记录确定时间边界
    uint64_t totalAll = 0;
    std::vector<MemcpyDetailRecord> allRecords;
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 1, 10000, OrderParam{"startTime", "ascend"}, allRecords, totalAll));
    ASSERT_EQ(allRecords.size(), 12u);

    // 选取中间时间点（相对时间）
    uint64_t midRelTime = (allRecords.front().timestamp + allRecords.back().timestamp) / 2;

    // 查询前半段
    uint64_t totalFirst = 0;
    std::vector<MemcpyDetailRecord> firstHalf;
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, midRelTime, "", std::nullopt, 1, 10000, OrderParam{"startTime", "ascend"}, firstHalf, totalFirst));

    // 查询后半段
    uint64_t totalSecond = 0;
    std::vector<MemcpyDetailRecord> secondHalf;
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(midRelTime + 1, MAX_TIME, "", std::nullopt, 1, 10000,
        OrderParam{"startTime", "ascend"}, secondHalf, totalSecond));

    // 验证时间范围准确性
    for (const auto &rec : firstHalf) {
        EXPECT_LE(rec.timestamp, midRelTime) << "前半段记录应在时间范围内";
    }
    for (const auto &rec : secondHalf) {
        EXPECT_GT(rec.timestamp, midRelTime) << "后半段记录应在时间范围内";
    }

    // 固定样本的中点位于记录间隙，两段结果应恰好覆盖全集
    EXPECT_EQ(firstHalf.size(), 6u);
    EXPECT_EQ(secondHalf.size(), 6u);
    EXPECT_EQ(firstHalf.size() + secondHalf.size(), totalAll);
}

// ======================
// 测试用例12: 构造必然溢出的时间参数
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyDetailRecordsPaged_TimeOverflow_PreventsQueryExecution) {
    uint64_t total = 0;
    std::vector<MemcpyDetailRecord> records;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t dangerousStart = std::numeric_limits<uint64_t>::max() + 1 - minTimestamp;
    const auto prepareCount = database_->prepareCount;
    EXPECT_FALSE(accessor_->GetMemcpyDetailRecordsPaged(dangerousStart, dangerousStart + 100, "", std::nullopt, 1, 10,
        OrderParam{"startTime", "ascend"}, records, total));
    EXPECT_EQ(database_->prepareCount, prepareCount);
    EXPECT_TRUE(records.empty());
    EXPECT_EQ(total, 0u);
}

} // namespace Dic::Module::Timeline
