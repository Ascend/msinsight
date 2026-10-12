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
#include <future>
#include "ACLGraphDebugJsonFileParser.h"
#include "ClusterParseThreadPoolExecutor.h"
#include "EventNotifyThreadPoolExecutor.h"
#define private public
#include "JsonFileParserManager.h"
#undef private
using namespace Dic::Module::Timeline;
namespace {
class LifecycleProbeParser : public Dic::Module::FileParser {
  public:
    explicit LifecycleProbeParser(std::shared_ptr<Dic::ThreadPool> pool) : pool(std::move(pool)) {}
    std::shared_ptr<Dic::ThreadPool> pool;
    unsigned resets = 0;
    bool Parse(
        const std::vector<std::string> &, const std::string &, const std::string &, const std::string &) override {
        return false;
    }
    void Reset() override { ++resets; }
};
class SecondLifecycleProbeParser : public LifecycleProbeParser {
  public:
    using LifecycleProbeParser::LifecycleProbeParser;
};
class ScopedParserRegistry {
  public:
    explicit ScopedParserRegistry(JsonFileParserManager &value) : manager(value) {
        manager.globalThreadPool_->WaitForAllTasks();
        std::unique_lock lock(manager.mutex_);
        original.swap(manager.parsers);
    }
    ~ScopedParserRegistry() {
        std::unique_lock lock(manager.mutex_);
        manager.parsers.clear();
        original.swap(manager.parsers);
    }

  private:
    JsonFileParserManager &manager;
    decltype(JsonFileParserManager::parsers) original;
};
TEST(JsonParserLifecycleTest, ManagerReusesEachParserTypeAndSharesPoolAcrossTypes) {
    JsonFileParserManager manager;
    auto &first = manager.GetOrCreateParser<LifecycleProbeParser>();
    auto &second = manager.GetOrCreateParser<SecondLifecycleProbeParser>();
    EXPECT_EQ(&first, &manager.GetOrCreateParser<LifecycleProbeParser>());
    EXPECT_NE(static_cast<void *>(&first), static_cast<void *>(&second));
    EXPECT_EQ(first.pool, second.pool);
    EXPECT_EQ(first.pool->AddTask([] { return 42; }, "").get(), 42);
}
TEST(JsonParserLifecycleTest, ConcurrentGetReturnsOneParserInstance) {
    JsonFileParserManager manager;
    std::vector<std::future<LifecycleProbeParser *>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.push_back(
            std::async(std::launch::async, [&] { return &manager.GetOrCreateParser<LifecycleProbeParser>(); }));
    }
    auto *expected = futures[0].get();
    for (size_t i = 1; i < futures.size(); ++i) {
        EXPECT_EQ(futures[i].get(), expected);
    }
    EXPECT_EQ(manager.parsers.size(), 1U);
}
TEST(JsonParserLifecycleTest, ResetAllCallsEveryRegisteredParserAndPreservesIdentity) {
    auto &manager = JsonFileParserManager::Instance();
    ScopedParserRegistry registry(manager);
    auto &first = manager.GetOrCreateParser<LifecycleProbeParser>();
    auto &second = manager.GetOrCreateParser<SecondLifecycleProbeParser>();
    const auto beforeFirst = first.resets, beforeSecond = second.resets;
    JsonFileParserManager::ResetAll();
    EXPECT_EQ(first.resets, beforeFirst + 1);
    EXPECT_EQ(second.resets, beforeSecond + 1);
    EXPECT_EQ(&first, &manager.GetOrCreateParser<LifecycleProbeParser>());
    EXPECT_EQ(first.pool->AddTask([] { return 7; }, "").get(), 7);
}
TEST(JsonParserLifecycleTest, PublicParserGettersReuseSupportedTypes) {
    EXPECT_EQ(&JsonFileParserManager::GetTraceFileParser(), &JsonFileParserManager::GetTraceFileParser());
    EXPECT_EQ(&JsonFileParserManager::GetACLGraphDebugParser(), &JsonFileParserManager::GetACLGraphDebugParser());
}
TEST(JsonParserLifecycleTest, ExecutorPoolsRunOrderedTasksAndRemainUsableAfterReset) {
    const auto event = EventNotifyThreadPoolExecutor::Instance().GetThreadPool();
    const auto cluster = ClusterParseThreadPoolExecutor::Instance().GetThreadPool();
    ASSERT_NE(event, nullptr);
    ASSERT_NE(cluster, nullptr);
    EXPECT_NE(event, cluster);
    EXPECT_EQ(event, EventNotifyThreadPoolExecutor::Instance().GetThreadPool());
    EXPECT_EQ(cluster, ClusterParseThreadPoolExecutor::Instance().GetThreadPool());
    for (const auto &pool : {event, cluster}) {
        std::vector<int> order;
        auto first = pool->AddTask(
            [&] {
                order.push_back(1);
                return 1;
            },
            "");
        auto second = pool->AddTask(
            [&] {
                order.push_back(2);
                return 2;
            },
            "");
        EXPECT_EQ(first.get(), 1);
        EXPECT_EQ(second.get(), 2);
        EXPECT_EQ(order, (std::vector<int>{1, 2}));
        auto failed = pool->AddTask([]() -> int { throw std::runtime_error("test failure"); }, "");
        EXPECT_THROW(failed.get(), std::runtime_error);
        pool->Reset();
        EXPECT_EQ(pool->AddTask([] { return 3; }, "").get(), 3);
    }
}
}
