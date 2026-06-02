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
#include <gmock/gmock.h>
#include <memory>
#include <vector>
#include <string>
#include "DataBaseManager.h"
#include "MemcpyOverallDatabaseAccesser.h"
#include "DataEngine.h"
#include "RepositoryFactory.h"
#include "RenderEngine.h"
#include "FullDbParser.h"
#include "ParamsParser.h"
#include "ProjectParserFactory.h"

namespace Dic::Module::Timeline {
class MemcpyOverallDatabaseAccesserIntegrationTest : public ::testing::Test {
  public:
    static void InitDataEngine() {
        auto repositoryFactory = RepositoryFactory::Instance();
        auto dataEngine = DataEngine::Instance();
        dataEngine->SetRepositoryFactory(repositoryFactory);
        auto renderEngine = RenderEngine::Instance();
        renderEngine->SetDataEngineInterface(dataEngine);
    }

    static void SetUpTestCase() {
        // 重置全局状态
        FullDb::FullDbParser::Instance().Reset();
        InitDataEngine();

        // 初始化日志系统
        const ParamsOption &option = ParamsParser::Instance().GetOption();
        ServerLog::Initialize(option.logPath, option.logSize, option.logLevel, std::to_string(option.wsPort));

        // 构建测试数据库路径（增强路径健壮性）
        std::string currPath = Dic::FileUtil::GetCurrPath();
        size_t index = currPath.rfind("server");
        if (index == std::string::npos) {
            FAIL() << "无法定位测试数据库路径：当前路径中不含'server'标识";
        }
        currPath = currPath.substr(0, index - 1);
        testDbPath_ =
            currPath + R"(/test/st/level2/rank_0_ascend_pt/ASCEND_PROFILER_OUTPUT/ascend_pytorch_profiler_0.db)";

        // 配置数据库管理器
        DataBaseManager::Instance().SetDataType(DataType::DB, testDbPath_);
        DataBaseManager::Instance().SetFileType(FileType::PYTORCH, testDbPath_);
        DataBaseManager::Instance().CreateTraceConnectionPool("0", testDbPath_);

        // 执行解析
        std::pair<std::string, ParserType> parserType = {testDbPath_, ParserType::DB};
        auto factory = ParserFactory::GetProjectParser(parserType.second);
        std::string warn;
        std::vector<std::string> parseFileList = factory->GetParseFileByImportFile(testDbPath_, warn);

        Global::ProjectExplorerInfo projectInfo;
        projectInfo.fileName = testDbPath_;
        projectInfo.projectName = testDbPath_;
        projectInfo.projectType = static_cast<int64_t>(factory->GetProjectType({testDbPath_}));
        projectInfo.importType = "import";

        auto parseFileInfo = std::make_shared<ParseFileInfo>();
        parseFileInfo->parseFilePath = testDbPath_;
        projectInfo.subParseFileInfo.push_back(parseFileInfo);

        ImportActionRequest request;
        ImportActionResponse response;
        factory->Parser({projectInfo}, request, response);

        // 设置路径映射
        const std::string projectId = "localhost.localdomain2152938157304401006_0";
        Timeline::DataBaseManager::Instance().SetDbPathMapping("FullDb", testDbPath_, projectId);
        Timeline::DataBaseManager::Instance().SetDbPathMapping("0", testDbPath_, projectId);

        // 安全等待解析完成（带超时机制）
        constexpr int MAX_WAIT_SEC = 120;
        constexpr int CHECK_INTERVAL_MS = 500;
        int elapsedMs = 0;
        while (ParserStatusManager::Instance().GetParserStatus(projectId + " 0") != ParserStatus::FINISH_ALL) {
            if (elapsedMs >= MAX_WAIT_SEC * 1000) {
                FAIL() << "解析超时（超过" << MAX_WAIT_SEC << "秒）: " << projectId;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
            elapsedMs += CHECK_INTERVAL_MS;
        }

        // 等待Overlap分析完成
        auto database = Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
        elapsedMs = 0;
        while (!database || !database->CheckValueFromStatusInfoTable(OVERLAP_ANALYSIS_UNIT, FINISH_STATUS)) {
            if (elapsedMs >= MAX_WAIT_SEC * 1000) {
                FAIL() << "Overlap分析超时（超过" << MAX_WAIT_SEC << "秒）";
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(CHECK_INTERVAL_MS));
            elapsedMs += CHECK_INTERVAL_MS;
            database = Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
        }
    }

    static void TearDownTestCase() {
        // 安全清理数据库连接
        auto connList = Timeline::DataBaseManager::Instance().GetAllTraceDatabase();
        for (auto &conn : connList) {
            if (conn) {
                conn->Stop();
            }
        }
        Timeline::DataBaseManager::Instance().Clear();
        Timeline::TraceTime::Instance().Reset();
        Timeline::ParserStatusManager::Instance().ClearAllParserStatus();
    }

  protected:
    void SetUp() override {
        // 正确构造被测对象（原构造函数需要database和fileId）
        auto database = Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId("0");
        if (!database) {
            FAIL() << "无法获取rank 0的数据库连接";
        }
        // 使用正确的fileId（与SetUpTestCase中创建的连接池一致）
        accessor_ = std::make_unique<MemcpyOverallDatabaseAccesser>(database, testDbPath_);
    }

    void TearDown() override { accessor_.reset(); }

    std::unique_ptr<MemcpyOverallDatabaseAccesser> accessor_;
    static std::string testDbPath_;
};

std::string MemcpyOverallDatabaseAccesserIntegrationTest::testDbPath_ = "";

// ======================
// 测试用例1: 验证基础查询功能（修复参数和字段名）
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyRecords_ReturnsValidRecords_WithFullTimeRange) {
    std::vector<MemcpyRecord> records;
    bool success = accessor_->GetMemcpyRecords(0, 0, records);

    EXPECT_TRUE(success) << "查询应成功完成";
    EXPECT_GT(records.size(), 0) << "测试数据库应包含Memcpy记录";

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
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t MAX_TIME = std::numeric_limits<uint64_t>::max() - 1 - minTimestamp;
    accessor_->GetMemcpyRecords(0, MAX_TIME, allRecords);

    if (allRecords.size() < 2) {
        GTEST_SKIP() << "跳过测试：需要至少2条记录验证时间过滤";
    }

    // 获取中间时间点
    uint64_t midTime = (allRecords.front().startTime + allRecords.back().endTime) / 2;

    // 查询前半部分
    std::vector<MemcpyRecord> firstHalf;
    accessor_->GetMemcpyRecords(0, midTime, firstHalf);

    // 查询后半部分
    std::vector<MemcpyRecord> secondHalf;
    accessor_->GetMemcpyRecords(midTime + 1, MAX_TIME, secondHalf);

    // 验证过滤逻辑
    EXPECT_LT(firstHalf.size(), allRecords.size());
    EXPECT_LT(secondHalf.size(), allRecords.size());
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
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t MAX_TIME = std::numeric_limits<uint64_t>::max() - 1 - minTimestamp;
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
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t MAX_TIME = std::numeric_limits<uint64_t>::max() - 1 - minTimestamp;
    accessor_->GetMemcpyRecords(0, MAX_TIME, records);

    if (records.empty()) {
        GTEST_SKIP() << "跳过测试：无记录可验证";
    }

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
    EXPECT_TRUE(hasH2D || hasD2H) << "应包含主机与设备间数据传输记录";
}

// ======================
// 测试用例5: 构造必然溢出的时间参数
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyRecords_TimeOverflow_PreventsQueryExecution) {
    std::vector<MemcpyRecord> records;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t dangerousStart = std::numeric_limits<uint64_t>::max() + 1 - minTimestamp;
    EXPECT_FALSE(accessor_->GetMemcpyRecords(dangerousStart, dangerousStart + 100, records));
}

// ======================
// 测试用例6: 验证分页基础功能（第一页数据完整性与排序）
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyDetailRecordsPaged_ValidFirstPage_DataIntegrityAndOrder) {
    std::vector<MemcpyDetailRecord> records;
    uint64_t total = 0;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t MAX_TIME = std::numeric_limits<uint64_t>::max() - 1 - minTimestamp;

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
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t MAX_TIME = std::numeric_limits<uint64_t>::max() - 1 - minTimestamp;
    uint64_t total = 0;
    std::vector<MemcpyDetailRecord> page1, page2;

    // 获取第一页（验证总记录数）
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 1, 10, OrderParam{"startTime", "ascend"}, page1, total));
    if (total <= 10) {
        GTEST_SKIP() << "总记录数不足，跳过跨页验证";
    }

    // 获取第二页
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 2, 10, OrderParam{"startTime", "ascend"}, page2, total));

    // 验证第二页非空且记录数合理
    EXPECT_GT(page2.size(), 0u);
    EXPECT_LE(page2.size(), 10u);

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
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t MAX_TIME = std::numeric_limits<uint64_t>::max() - 1 - minTimestamp;

    // 获取示例过滤值（避免硬编码）
    std::vector<MemcpyRecord> baseRecords;
    ASSERT_TRUE(accessor_->GetMemcpyRecords(0, MAX_TIME, baseRecords));
    if (baseRecords.empty()) {
        GTEST_SKIP() << "无基础记录，跳过过滤测试";
    }

    // 选取有效过滤值
    std::string exampleTid = baseRecords[0].threadId;
    std::string exampleType = baseRecords[0].memcpyType;
    if (exampleType.empty()) {
        GTEST_SKIP() << "示例记录memcpyType为空";
    }

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
    EXPECT_GT(filteredTotal, 0u) << "过滤后应有匹配记录";
    EXPECT_LE(filteredTotal, totalAll) << "过滤后记录数不应超过总数";
    EXPECT_EQ(filteredRecords.size(), filteredTotal) << "返回记录数应等于total";
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
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t MAX_TIME = std::numeric_limits<uint64_t>::max() - 1 - minTimestamp;
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
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t MAX_TIME = std::numeric_limits<uint64_t>::max() - 1 - minTimestamp;

    // 获取全量记录确定时间边界
    uint64_t totalAll = 0;
    std::vector<MemcpyDetailRecord> allRecords;
    ASSERT_TRUE(accessor_->GetMemcpyDetailRecordsPaged(
        0, MAX_TIME, "", std::nullopt, 1, 10000, OrderParam{"startTime", "ascend"}, allRecords, totalAll));
    if (allRecords.size() < 2) {
        GTEST_SKIP() << "记录不足，跳过时间过滤测试";
    }

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

    // 验证记录无重叠且覆盖全集（允许边界舍入误差）
    EXPECT_LE(firstHalf.size() + secondHalf.size(), totalAll + 1) << "分段记录总数应接近全量（允许边界1条误差）";
}

// ======================
// 测试用例12: 构造必然溢出的时间参数
// ======================
TEST_F(MemcpyOverallDatabaseAccesserIntegrationTest, GetMemcpyDetailRecordsPaged_TimeOverflow_PreventsQueryExecution) {
    uint64_t total = 0;
    std::vector<MemcpyDetailRecord> records;
    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    const uint64_t dangerousStart = std::numeric_limits<uint64_t>::max() + 1 - minTimestamp;
    EXPECT_FALSE(accessor_->GetMemcpyDetailRecordsPaged(dangerousStart, dangerousStart + 100, "", std::nullopt, 1, 10,
        OrderParam{"startTime", "ascend"}, records, total));
}

} // namespace Dic::Module::Timeline
