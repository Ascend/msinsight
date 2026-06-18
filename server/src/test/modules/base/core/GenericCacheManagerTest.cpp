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
#include "GenericCacheManager.h"
#include "ServerLog.h"
#include <thread>
#include <future>
#include <chrono>
#include <atomic>

class GenericCacheManagerTest : public ::testing::Test {
  protected:
    static void SetUpTestSuite() { Dic::Server::ServerLog::Initialize("./test_log", 10, "INFO", "0"); }
};

// 1. 基本插入和获取
TEST_F(GenericCacheManagerTest, BasicGetOrFetch) {
    GenericCacheManager<std::string, int> cache;

    int callCount = 0;
    auto fetchFunc = [&callCount]() {
        callCount++;
        return 42;
    };

    int val1 = cache.GetOrFetch("key1", fetchFunc);
    EXPECT_EQ(val1, 42);
    EXPECT_EQ(callCount, 1);

    // 第二次获取应该命中缓存，fetchFunc 不再被调用
    int val2 = cache.GetOrFetch("key1", fetchFunc);
    EXPECT_EQ(val2, 42);
    EXPECT_EQ(callCount, 1);

    EXPECT_EQ(cache.GetSize(), 1);
}

// 2. 缓存命中时返回缓存值，不重新计算
TEST_F(GenericCacheManagerTest, CacheHitReturnCachedValue) {
    GenericCacheManager<std::string, int> cache;

    int val1 = cache.GetOrFetch("key1", []() { return 100; });
    EXPECT_EQ(val1, 100);

    int val2 = cache.GetOrFetch("key1", []() { return 200; });
    EXPECT_EQ(val2, 100); // 仍然返回 100，而不是 200
}

// 3. 统计信息跟踪
TEST_F(GenericCacheManagerTest, CacheStatsTracking) {
    GenericCacheManager<std::string, int> cache;

    cache.GetOrFetch("key1", []() { return 1; }); // miss
    cache.GetOrFetch("key1", []() { return 1; }); // hit
    cache.GetOrFetch("key2", []() { return 2; }); // miss

    auto stats = cache.GetStats();
    EXPECT_EQ(stats.hits, 1);
    EXPECT_EQ(stats.misses, 2);
    EXPECT_EQ(stats.totalQueries, 3);
}

// 4. LRU 淘汰策略
TEST_F(GenericCacheManagerTest, LRUEviction) {
    typename GenericCacheManager<std::string, int>::Config config;
    config.maxSize = 2;
    config.enableLRU = true;
    GenericCacheManager<std::string, int> cache(config);

    cache.GetOrFetch("key1", []() { return 1; });
    cache.GetOrFetch("key2", []() { return 2; });
    cache.GetOrFetch("key1", []() { return 1; }); // key1 变成 MRU，key2 是 LRU

    // 插入 key3 应该淘汰 LRU=key2
    cache.GetOrFetch("key3", []() { return 3; });

    EXPECT_EQ(cache.GetSize(), 2);

    // key1 应该还在缓存中（key3 插入时淘汰的是 key2）
    int callCount = 0;
    cache.GetOrFetch("key1", [&callCount]() {
        callCount++;
        return 1;
    });
    EXPECT_EQ(callCount, 0); // fetch 未被调用

    // key2 已被淘汰，再次获取需要重新 fetch
    cache.GetOrFetch("key2", [&callCount]() {
        callCount++;
        return 2;
    });
    EXPECT_EQ(callCount, 1);
}

// 5. TTL 过期
TEST_F(GenericCacheManagerTest, TTLExpiration) {
    typename GenericCacheManager<std::string, int>::Config config;
    config.ttl = std::chrono::minutes(0); // 立即过期
    GenericCacheManager<std::string, int> cache(config);

    int callCount = 0;
    auto fetchFunc = [&callCount]() {
        callCount++;
        return 42;
    };

    cache.GetOrFetch("key1", fetchFunc);
    EXPECT_EQ(callCount, 1);

    // 由于 TTL=0，缓存已过期，再次获取会重新执行 fetch
    cache.GetOrFetch("key1", fetchFunc);
    EXPECT_EQ(callCount, 2);
}

// 6. Single-flight 防止缓存击穿（多线程场景）
TEST_F(GenericCacheManagerTest, SingleFlightConcurrentFetch) {
    GenericCacheManager<std::string, int> cache;
    std::atomic<int> fetchCount{0};

    auto fetchFunc = [&fetchCount]() -> int {
        fetchCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return 42;
    };

    // 启动多个线程并发获取同一个 key
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 5; ++i) {
        futures.push_back(std::async(
            std::launch::async, [&cache, &fetchFunc]() { return cache.GetOrFetch("shared_key", fetchFunc); }));
    }

    for (auto &f : futures) {
        EXPECT_EQ(f.get(), 42);
    }

    // 虽然 5 个线程并发获取，但 fetchFunc 应该只被调用一次
    EXPECT_EQ(fetchCount.load(), 1);
}

// 7. 异常处理：fetchFunc 抛出异常，异常应被传播
TEST_F(GenericCacheManagerTest, ExceptionHandling) {
    GenericCacheManager<std::string, int> cache;

    EXPECT_THROW(
        cache.GetOrFetch("key1", []() -> int { throw std::runtime_error("fetch failed"); }), std::runtime_error);

    // 异常后，pending 记录应被清理，下一次可以重新尝试
    int val = cache.GetOrFetch("key1", []() { return 42; });
    EXPECT_EQ(val, 42);
}

// 8. 清除缓存
TEST_F(GenericCacheManagerTest, ClearCache) {
    GenericCacheManager<std::string, int> cache;

    cache.GetOrFetch("key1", []() { return 1; });
    cache.GetOrFetch("key2", []() { return 2; });
    EXPECT_EQ(cache.GetSize(), 2);

    cache.Clear();
    EXPECT_EQ(cache.GetSize(), 0);

    // 清除后再次获取应重新执行 fetch
    int callCount = 0;
    cache.GetOrFetch("key1", [&callCount]() {
        callCount++;
        return 1;
    });
    EXPECT_EQ(callCount, 1);
}

// 9. 清理过期缓存
TEST_F(GenericCacheManagerTest, CleanupExpired) {
    typename GenericCacheManager<std::string, int>::Config config;
    config.ttl = std::chrono::minutes(0); // 立即过期
    GenericCacheManager<std::string, int> cache(config);

    cache.GetOrFetch("key1", []() { return 1; });
    cache.GetOrFetch("key2", []() { return 2; });
    EXPECT_EQ(cache.GetSize(), 2);

    cache.CleanupExpired();
    EXPECT_EQ(cache.GetSize(), 0);
}

// 10. 使用 int 作为 KeyType
TEST_F(GenericCacheManagerTest, IntKeyType) {
    GenericCacheManager<int, std::string> cache;

    std::string val1 = cache.GetOrFetch(100, []() { return std::string("value_100"); });
    EXPECT_EQ(val1, "value_100");

    std::string val2 = cache.GetOrFetch(100, []() { return std::string("other"); });
    EXPECT_EQ(val2, "value_100");

    EXPECT_EQ(cache.GetSize(), 1);
}

// 11. 禁用 LRU 时的简单淘汰
TEST_F(GenericCacheManagerTest, DisableLRUEviction) {
    typename GenericCacheManager<std::string, int>::Config config;
    config.maxSize = 2;
    config.enableLRU = false;
    GenericCacheManager<std::string, int> cache(config);

    cache.GetOrFetch("key1", []() { return 1; });
    cache.GetOrFetch("key2", []() { return 2; });
    cache.GetOrFetch("key3", []() { return 3; }); // 简单淘汰任意一个

    EXPECT_EQ(cache.GetSize(), 2);
}

// 12. 异常后 single-flight 等待者也能收到异常
TEST_F(GenericCacheManagerTest, SingleFlightExceptionPropagation) {
    GenericCacheManager<std::string, int> cache;
    std::atomic<int> fetchCount{0};

    auto fetchFunc = [&fetchCount]() -> int {
        fetchCount++;
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        throw std::runtime_error("network error");
    };

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 3; ++i) {
        futures.push_back(std::async(std::launch::async,
            [&cache, &fetchFunc]() { EXPECT_THROW(cache.GetOrFetch("fail_key", fetchFunc), std::runtime_error); }));
    }

    for (auto &f : futures) {
        f.wait();
    }

    EXPECT_EQ(fetchCount.load(), 1);
}
