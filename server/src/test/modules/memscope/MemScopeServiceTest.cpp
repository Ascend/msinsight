/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
#include "MemScopeRequestHandler.h"
#include "MemScopeParser.h"
#include "DataBaseManager.h"
#include "TraceTime.h"
#include "MemScopeEntities.h"
#include "MemScopeDatabase.h"
#include "../../TestSuit.h"
#include "MemScopeService.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::FullDb;
using namespace Dic::Module::MemScope;
using namespace Dic;

class MemScopeServiceTest : public ::testing::Test {
  public:
    const static uint64_t SECOND = 1000000000;

    static void SetUpTestSuite() {
        std::string dbPath = TestSuit::GetTestDataFile("full_db", "leaks_dump_20250806.dat");
        auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
        ASSERT_TRUE(memoryDatabase->OpenDb(dbPath, false));
        MemScopeParser::ParseMemoryMemScopeDumpEventsAndPythonTraces("0");
    }

    static void TearDownTestSuite() {
        auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
        memoryDatabase->CloseDb();
        DataBaseManager::Instance().Clear();
    }

    static void ExpectTreeEQ(MemScopeMemoryDetailTreeNode *origin, MemScopeMemoryDetailTreeNode *target) {
        EXPECT_EQ(origin->tag, target->tag);
        EXPECT_EQ(origin->children.size(), target->children.size());
        if (origin->children.size() != target->children.size()) {
            return;
        }
        for (size_t i = 0; i < origin->children.size(); i++) {
            ExpectTreeEQ(origin->children[i].get(), target->children[i].get());
        }
    }
};

TEST_F(MemScopeServiceTest, IsValidMemoryEventTypeAcceptsHostPinnedAndTraceLifecycleEvents) {
    EXPECT_TRUE(MemScopeService::IsValidMemoryEventType(
        MEM_SCOPE_DUMP_EVENT::MALLOC, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED));
    EXPECT_TRUE(MemScopeService::IsValidMemoryEventType(
        MEM_SCOPE_DUMP_EVENT::FREE, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED));
    EXPECT_TRUE(MemScopeService::IsValidMemoryEventType(
        MEM_SCOPE_DUMP_EVENT::SYSTEM, MEM_SCOPE_DUMP_EVENT_TYPE::SYSTEM_START_TRACE));
    EXPECT_TRUE(MemScopeService::IsValidMemoryEventType(
        MEM_SCOPE_DUMP_EVENT::SYSTEM, MEM_SCOPE_DUMP_EVENT_TYPE::SYSTEM_STOP_TRACE));
}

/***
 * 用于测试当时间戳非法时（如出现负值，或超出最大时间戳时），通过非法时间戳构建内存拆解树的情况
 * 预期：无报错，能够构建出根节点HAL节点，但无子节点
 */
TEST_F(MemScopeServiceTest, BuildMemoryAllocDetailTreeWithInvalidTimestamp) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    std::unique_ptr<MemScopeMemoryDetailTreeNode> tree{};
    const std::string deviceId = "1";
    const std::string eventType = "PTA";
    const uint64_t expectDuration = -1;
    auto ctx = BuildDetailTreeContext(deviceId, expectDuration, eventType, true);
    MemScopeService::BuildMemoryAllocDetailTreeByContext(tree, ctx);
    ASSERT_TRUE(tree.get() != nullptr);
    EXPECT_EQ(tree->tag, MEM_SCOPE_ALLOC_OWNER_HAL);
    EXPECT_EQ(tree->name, MEM_SCOPE_ALLOC_OWNER_HAL_NAME);
    EXPECT_EQ(tree->children.size(), 0);
}

/***
 *  用于测试当查询无实际数据时构建内存拆解树的情况
 *  预期：无报错，能够构建出根节点HAL节点，但无子节点
 */
TEST_F(MemScopeServiceTest, BuildMemoryAllocDetailTreeWhenDataEmpty) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    std::unique_ptr<MemScopeMemoryDetailTreeNode> tree{};
    const std::string deviceId = "7";
    const std::string eventType = "PTA";
    const uint64_t expectDuration = 20000000000;
    auto ctx = BuildDetailTreeContext(deviceId, expectDuration, eventType, true);
    MemScopeService::BuildMemoryAllocDetailTreeByContext(tree, ctx);
    ASSERT_TRUE(tree.get() != nullptr);
    EXPECT_EQ(tree->tag, MEM_SCOPE_ALLOC_OWNER_HAL);
    EXPECT_EQ(tree->name, MEM_SCOPE_ALLOC_OWNER_HAL_NAME);
    EXPECT_EQ(tree->children.size(), 0);
}

/***
 *  用于测试常规场景构建内存拆解树的情况
 *  预期：内存拆解树符合预期
 */
TEST_F(MemScopeServiceTest, BuildMemoryAllocDetailTreeNormal) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    std::unique_ptr<MemScopeMemoryDetailTreeNode> tree{};
    const std::string deviceId = "1";
    const std::string eventType = "PTA";
    const uint64_t expectDuration = 20000000000;
    auto ctx = BuildDetailTreeContext(deviceId, expectDuration, eventType, true);
    MemScopeService::BuildMemoryAllocDetailTreeByContext(tree, ctx);
    ASSERT_TRUE(tree.get() != nullptr);
    // 根节点HAL
    auto expectTree = std::make_unique<MemScopeMemoryDetailTreeNode>(MEM_SCOPE_ALLOC_OWNER_HAL);
    // 单级子节点PTA
    auto ptaNode = std::make_unique<MemScopeMemoryDetailTreeNode>(MEM_SCOPE_ALLOC_OWNER_PTA);
    // PTA的两个子节点：PTA@ops和PTA@model
    auto ptaModelNode = std::make_unique<MemScopeMemoryDetailTreeNode>(MEM_SCOPE_ALLOC_OWNER_PTA_MODEL);
    auto ptaOpsNode = std::make_unique<MemScopeMemoryDetailTreeNode>(MEM_SCOPE_ALLOC_OWNER_PTA_OPS);
    // PTA@model的子节点：PTA@model@weight
    auto ptaModelWeightNode = std::make_unique<MemScopeMemoryDetailTreeNode>(MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_WEIGHT);
    // PTA@ops的子节点：PTA@ops@aten
    auto ptaOpsAtenNode = std::make_unique<MemScopeMemoryDetailTreeNode>(MEM_SCOPE_ALLOC_OWNER_PTA_OPS_ATEN);
    // PTA@ops的子节点：PTA@ops@aten@leaks_mem
    auto ptaOpsAtenLeaksNode = std::make_unique<MemScopeMemoryDetailTreeNode>("PTA@ops@aten@leaks_mem");

    // 从子节点向父节点构造
    ptaModelNode->children.emplace_back(std::move(ptaModelWeightNode));
    ptaNode->children.emplace_back(std::move(ptaModelNode));
    ptaOpsAtenNode->children.emplace_back(std::move(ptaOpsAtenLeaksNode));
    ptaOpsNode->children.emplace_back(std::move(ptaOpsAtenNode));
    ptaNode->children.emplace_back(std::move(ptaOpsNode));
    expectTree->children.emplace_back(std::move(ptaNode));

    ExpectTreeEQ(tree.get(), expectTree.get());
}

/***
* 测试对trace进行Trim的三种策略
* ====Trim前如下====
*   |--------------------------------------func0---------------------------------------------|
*    |func01||func02||-------func03-------|               |--------------func04------------|
*                         |func031|
*/

/***
*    ====测试仅过滤  Trim后如下=====
*   |--------------------------------------func0---------------------------------------------|
*                   |-------func03-------|               |--------------func04--------------|
 *
*/
TEST_F(MemScopeServiceTest, TestTrimPythonTraceSlicesOnlyFilter) {
    // 构造测试数据
    MemScopePythonTrace trace;
    // 模拟时间范围在100000ns的情况
    trace.maxTimestamp = 100000;
    trace.minTimestamp = 0;
    trace.maxDepth = 2;
    trace.slices.emplace_back("func0", 0, 100, 0); // 0层单个较大块
    trace.slices.emplace_back("func01", 1, 2, 1); // 1层小块
    trace.slices.emplace_back("func02", 3, 4, 1); // 1层小间隙第二个小块
    trace.slices.emplace_back("func03", 4, 30, 1); // 1层小间隙较大块
    trace.slices.emplace_back("func04", 70, 99, 1); // 1层大间隙较大块
    trace.slices.emplace_back("func031", 10, 11, 2); // 2层小块

    // func01 func02 func031因为均为小块而被直接过滤
    trace.Trim(PythonTrimCompressStrategy::ONLY_FILTER_OUT_SMALL_FUNCTIONS);
    EXPECT_EQ(trace.slices.size(), 3);
    // 按照开始时间进行排序
    std::sort(trace.slices.begin(), trace.slices.end(),
        [](const PythonTraceSlice &a, const PythonTraceSlice &b) { return a.startTimestamp < b.startTimestamp; });
    EXPECT_EQ(trace.slices[0].func, "func0");
    EXPECT_EQ(trace.slices[1].func, "func03");
    EXPECT_EQ(trace.slices[2].func, "func04");
}

/***
*    ====测试仅合并同层级小块策略，不可合并小块被保留原样  Trim后如下=====
*   |--------------------------------------func0---------------------------------------------|
*    |---Merged---||-------func03-------|                 |--------------func04-------------|
*                       |func031|
*/
TEST_F(MemScopeServiceTest, TestTrimPythonTraceSlicesOnlyCompress) {
    // 构造测试数据
    MemScopePythonTrace trace;
    trace.maxTimestamp = 100000;
    trace.minTimestamp = 0;
    trace.maxDepth = 2;
    trace.slices.emplace_back("func0", 0, 100, 0); // 0层单个较大块
    trace.slices.emplace_back("func01", 1, 2, 1); // 1层小块
    trace.slices.emplace_back("func02", 3, 4, 1); // 1层小间隙第二个小块
    trace.slices.emplace_back("func03", 4, 30, 1); // 1层小间隙较大块
    trace.slices.emplace_back("func04", 70, 99, 1); // 1层大间隙较大块
    trace.slices.emplace_back("func031", 10, 11, 2); // 2层小块

    // func01 func02被合并 func031虽为小块但不可被合并因此保留
    trace.Trim(PythonTrimCompressStrategy::COMPRESS_SMALL_FUNCTIONS);
    EXPECT_EQ(trace.slices.size(), 5);
    // 按照开始时间进行排序
    std::sort(trace.slices.begin(), trace.slices.end(),
        [](const PythonTraceSlice &a, const PythonTraceSlice &b) { return a.startTimestamp < b.startTimestamp; });
    EXPECT_EQ(trace.slices[0].func, "func0");
    std::string MERGED_LINK = " -> ";
    EXPECT_EQ(trace.slices[1].func, StringUtil::StrJoin("Merged: ", "func01", MERGED_LINK, "func02"));
    EXPECT_EQ(trace.slices[2].func, "func03");
    EXPECT_EQ(trace.slices[3].func, "func031");
    EXPECT_EQ(trace.slices[4].func, "func04");
}

/***
*    ====测试仅合并同层级小块且过滤小块策略  Trim后如下=====
*   |--------------------------------------func0---------------------------------------------|
*    |---Merged---||-------func03-------|                 |--------------func14-------------|
*
*/
TEST_F(MemScopeServiceTest, TestTrimPythonTraceSlicesCompressAndFilter) {
    // 构造测试数据
    MemScopePythonTrace trace;
    trace.maxTimestamp = 100000;
    trace.minTimestamp = 0;
    trace.maxDepth = 2;
    trace.slices.emplace_back("func0", 0, 100, 0); // 0层单个较大块
    trace.slices.emplace_back("func01", 1, 2, 1); // 1层小块
    trace.slices.emplace_back("func02", 3, 4, 1); // 1层小间隙第二个小块
    trace.slices.emplace_back("func03", 4, 30, 1); // 1层小间隙较大块
    trace.slices.emplace_back("func04", 70, 99, 1); // 1层大间隙较大块
    trace.slices.emplace_back("func031", 10, 11, 2); // 2层小块

    // func01 func02被合并 func031为小块被过滤
    trace.Trim(PythonTrimCompressStrategy::COMPRESS_AND_FILTER_SMALL_FUNCTIONS);
    EXPECT_EQ(trace.slices.size(), 4);
    // 按照开始时间进行排序
    std::sort(trace.slices.begin(), trace.slices.end(),
        [](const PythonTraceSlice &a, const PythonTraceSlice &b) { return a.startTimestamp < b.startTimestamp; });
    EXPECT_EQ(trace.slices[0].func, "func0");
    std::string MERGED_LINK = " -> ";
    EXPECT_EQ(trace.slices[1].func, StringUtil::StrJoin("Merged: ", "func01", MERGED_LINK, "func02"));
    EXPECT_EQ(trace.slices[2].func, "func03");
    EXPECT_EQ(trace.slices[3].func, "func04");
}
