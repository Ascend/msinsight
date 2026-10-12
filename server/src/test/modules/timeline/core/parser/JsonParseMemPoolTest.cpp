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
#include <thread>
#include <future>
#include "JsonParseMemPool.h"
using Dic::Module::JsonParseMemPool;
namespace {
class JsonParseMemPoolTest : public ::testing::Test {
  protected:
    void SetUp() override { JsonParseMemPool::Instance().Clear(); }
    void TearDown() override { JsonParseMemPool::Instance().Clear(); }
};
TEST_F(JsonParseMemPoolTest, ReusesAllocatorPerThreadAndReleaseClearsAllocations) {
    auto &pool = JsonParseMemPool::Instance();
    auto allocator = pool.GetMemBuff(std::this_thread::get_id());
    EXPECT_EQ(allocator, pool.GetMemBuff(std::this_thread::get_id()));
    ASSERT_NE(allocator->Malloc(128), nullptr);
    EXPECT_GE(allocator->Size(), 128U);
    pool.ReleaseMemBuff(allocator);
    EXPECT_EQ(allocator->Size(), 0U);
    EXPECT_EQ(allocator, pool.GetMemBuff(std::this_thread::get_id()));
}
TEST_F(JsonParseMemPoolTest, DifferentLiveThreadsHaveIndependentAllocators) {
    auto &pool = JsonParseMemPool::Instance();
    auto main = pool.GetMemBuff(std::this_thread::get_id());
    auto worker = std::async(std::launch::async, [&] { return pool.GetMemBuff(std::this_thread::get_id()); }).get();
    EXPECT_NE(main, worker);
    main->Malloc(64);
    EXPECT_EQ(worker->Size(), 0U);
    pool.Clear();
    auto replacement = pool.GetMemBuff(std::this_thread::get_id());
    EXPECT_NE(main, replacement);
    EXPECT_GE(main->Size(), 64U);
    EXPECT_EQ(replacement->Size(), 0U);
}
}
