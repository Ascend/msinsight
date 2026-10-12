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
#include <vector>
#include "DBConnectionPool.h"
using namespace Dic::Module::Timeline;
namespace {
struct PoolCounters {
    int created = 0, destroyed = 0, opened = 0, shared = 0, fail = 0;
    std::string path;
};
struct PoolConnection {
    PoolCounters &counts;
    int id;
    explicit PoolConnection(PoolCounters &value) : counts(value), id(++value.created) {}
    ~PoolConnection() { ++counts.destroyed; }
    bool OpenDb(const std::string &path, bool) {
        counts.path = path;
        ++counts.opened;
        return counts.opened > counts.fail;
    }
    bool OpenSharedMemoryDb(const std::string &path) {
        ++counts.shared;
        return OpenDb(path, false);
    }
};
TEST(DBConnectionPoolTest, ReusesReleasedConnectionAndStopDestroysExactlyOnce) {
    PoolCounters counts;
    DBConnectionPool<PoolConnection> pool("db", [&] { return new PoolConnection(counts); });
    auto first = pool.GetConnection();
    ASSERT_NE(first, nullptr);
    const int id = first->id;
    first.reset();
    auto reused = pool.GetConnection();
    ASSERT_NE(reused, nullptr);
    EXPECT_EQ(reused->id, id);
    EXPECT_EQ(counts.created, 1);
    EXPECT_EQ(pool.GetDbPath(), "db");
    reused.reset();
    pool.Stop();
    pool.Stop();
    EXPECT_EQ(counts.destroyed, 1);
    EXPECT_EQ(pool.GetConnection(), nullptr);
    EXPECT_TRUE(pool.GetDbPath().empty());
}
TEST(DBConnectionPoolTest, RetriesFailedOpenAndDeletesEveryFailedConnection) {
    PoolCounters counts;
    counts.fail = 2;
    DBConnectionPool<PoolConnection> pool("db", [&] { return new PoolConnection(counts); });
    auto connection = pool.GetConnection();
    ASSERT_NE(connection, nullptr);
    EXPECT_EQ(counts.opened, 3);
    EXPECT_EQ(counts.destroyed, 2);
    connection.reset();
    pool.Stop();
    EXPECT_EQ(counts.destroyed, 3);
}
TEST(DBConnectionPoolTest, FailedOpenExhaustsThreeAttemptsWithoutLeaking) {
    PoolCounters counts;
    counts.fail = 10;
    DBConnectionPool<PoolConnection> pool("missing", [&] { return new PoolConnection(counts); });
    EXPECT_EQ(pool.GetConnection(), nullptr);
    EXPECT_EQ(counts.opened, 3);
    EXPECT_EQ(counts.destroyed, 3);
}
TEST(DBConnectionPoolTest, SharedMemoryUsesSharedOpenAndPropagatesPath) {
    PoolCounters counts;
    DBConnectionPool<PoolConnection> pool("file:pool?mode=memory", [&] { return new PoolConnection(counts); }, true);
    auto connection = pool.GetConnection();
    ASSERT_NE(connection, nullptr);
    EXPECT_EQ(counts.shared, 1);
    EXPECT_EQ(counts.path, "file:pool?mode=memory");
    connection.reset();
}
TEST(DBConnectionPoolTest, CapacityCanGrowAndConcurrentBorrowersGetDistinctConnections) {
    PoolCounters counts;
    DBConnectionPool<PoolConnection> pool("db", [&] { return new PoolConnection(counts); });
    pool.SetMaxActiveCount(31);
    std::vector<std::shared_ptr<PoolConnection>> held;
    for (int i = 0; i < 31; ++i) {
        held.push_back(pool.GetConnection());
        ASSERT_NE(held.back(), nullptr);
        EXPECT_EQ(held.back()->id, i + 1);
    }
    held.clear();
    pool.Stop();
    EXPECT_EQ(counts.destroyed, 31);
}
TEST(DBConnectionPoolTest, StopWaitsForOutstandingBorrowerBeforeDestroyingIt) {
    PoolCounters counts;
    DBConnectionPool<PoolConnection> pool("db", [&] { return new PoolConnection(counts); });
    auto held = pool.GetConnection();
    ASSERT_NE(held, nullptr);
    std::promise<void> entered;
    auto stopped = std::async(std::launch::async, [&] {
        entered.set_value();
        pool.Stop();
    });
    entered.get_future().wait();
    EXPECT_EQ(stopped.wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);
    EXPECT_EQ(held->id, 1);
    held.reset();
    EXPECT_EQ(stopped.wait_for(std::chrono::seconds(5)), std::future_status::ready);
    stopped.get();
    EXPECT_EQ(counts.destroyed, 1);
    EXPECT_EQ(pool.GetConnection(), nullptr);
}
}
