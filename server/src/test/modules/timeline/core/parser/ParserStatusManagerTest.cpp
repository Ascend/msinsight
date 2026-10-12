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
#include <map>
#include <shared_mutex>
#include "GlobalDefs.h"
#define private public
#include "ParserStatusManager.h"
#undef private
#include "ConstantDefs.h"
using namespace Dic::Module::Timeline;
namespace {
class ParserStatusManagerTest : public ::testing::Test {
  protected:
    ParserStatusManager &manager = ParserStatusManager::Instance();
    void SetUp() override {
        manager.ClearAllParserStatus();
        manager.NotifyStartParse();
    }
    void TearDown() override {
        manager.ClearAllParserStatus();
        manager.NotifyStartParse();
    }
};
TEST_F(ParserStatusManagerTest, FileTransitionsRejectMissingAndInvalidPredecessors) {
    EXPECT_EQ(manager.GetParserStatus("file"), ParserStatus::UN_KNOW);
    EXPECT_FALSE(manager.SetRunningStatus("file"));
    EXPECT_FALSE(manager.SetFinishStatus("file"));
    EXPECT_EQ(manager.SetTerminateStatus("file"), ParserStatus::UN_KNOW);
    manager.SetParserStatus("file", ParserStatus::INIT);
    EXPECT_FALSE(manager.SetFinishStatus("file"));
    EXPECT_TRUE(manager.SetRunningStatus("file"));
    EXPECT_FALSE(manager.SetRunningStatus("file"));
    EXPECT_TRUE(manager.SetFinishStatus("file"));
    EXPECT_EQ(manager.GetParserStatus("file"), ParserStatus::FINISH);
    EXPECT_FALSE(manager.SetFinishStatus("file"));
    EXPECT_EQ(manager.SetTerminateStatus("file"), ParserStatus::FINISH);
    EXPECT_EQ(manager.GetParserStatus("file"), ParserStatus::FINISH);
}
TEST_F(ParserStatusManagerTest, TerminationPreservesCompletedTasksAndCanFinishTerminatedTask) {
    manager.SetParserStatus("running", ParserStatus::RUNNING);
    manager.SetParserStatus("done", ParserStatus::FINISH_ALL);
    EXPECT_EQ(manager.SetTerminateStatus("running"), ParserStatus::RUNNING);
    EXPECT_EQ(manager.GetParserStatus("running"), ParserStatus::TERMINATE);
    EXPECT_TRUE(manager.SetFinishStatus("running"));
    manager.SetParserStatus("initial", ParserStatus::INIT);
    manager.SetAllTerminateStatus();
    EXPECT_EQ(manager.GetParserStatus("done"), ParserStatus::FINISH_ALL);
    EXPECT_EQ(manager.GetParserStatus("running"), ParserStatus::FINISH);
    EXPECT_EQ(manager.GetParserStatus("initial"), ParserStatus::TERMINATE);
}
TEST_F(ParserStatusManagerTest, ClusterLifecycleRejectsInvalidAndRepeatedTransitions) {
    EXPECT_TRUE(manager.IsClusterParserFinalState("cluster"));
    EXPECT_FALSE(manager.SetClusterParseStatus("cluster", ParserStatus::RUNNING));
    EXPECT_FALSE(manager.SetClusterParseStatus("cluster", ParserStatus::FINISH));
    EXPECT_FALSE(manager.SetClusterParseStatus("cluster", ParserStatus::UN_KNOW));
    EXPECT_TRUE(manager.SetClusterParseStatus("cluster", ParserStatus::INIT));
    EXPECT_FALSE(manager.SetClusterParseStatus("cluster", ParserStatus::INIT));
    EXPECT_FALSE(manager.IsClusterParserFinalState("cluster"));
    EXPECT_TRUE(manager.SetClusterParseStatus("cluster", ParserStatus::RUNNING));
    EXPECT_FALSE(manager.SetClusterParseStatus("cluster", ParserStatus::RUNNING));
    EXPECT_TRUE(manager.SetClusterParseStatus("cluster", ParserStatus::FINISH_ALL));
    EXPECT_FALSE(manager.SetClusterParseStatus("cluster", ParserStatus::TERMINATE));
    EXPECT_TRUE(manager.IsClusterParserFinalState("cluster"));
}
TEST_F(ParserStatusManagerTest, ClusterEraseOnlyRemovesMatchingKeysAndAllowsReinitialize) {
    manager.SetClusterParseStatus("rankA/one", ParserStatus::INIT);
    manager.SetClusterParseStatus("rankB/two", ParserStatus::INIT);
    manager.EraseClusterParserStatusByKeyWord("rankA");
    EXPECT_TRUE(manager.IsClusterParserFinalState("rankA/one"));
    EXPECT_FALSE(manager.IsClusterParserFinalState("rankB/two"));
    EXPECT_TRUE(manager.SetClusterParseStatus("rankA/one", ParserStatus::INIT));
    EXPECT_TRUE(manager.SetClusterParseStatus("rankA/one", ParserStatus::TERMINATE));
}
TEST_F(ParserStatusManagerTest, PendingPathsBlockCompletionAndTransitionsClearThem) {
    using Dic::ProjectTypeEnum;
    const auto type = static_cast<ProjectTypeEnum>(0);
    manager.SetPendingStatus("rank", {type, {"first.json", "second.json"}});
    EXPECT_EQ(manager.QueryPendingFilePath("rank").second, (std::vector<std::string>{"first.json", "second.json"}));
    std::string unfinished;
    EXPECT_FALSE(manager.IsAllFinished(unfinished));
    EXPECT_EQ(unfinished, "first.json");
    manager.SetParserStatus("rank", ParserStatus::INIT);
    EXPECT_TRUE(manager.QueryPendingFilePath("rank").second.empty());
    EXPECT_FALSE(manager.IsAllFinished(unfinished));
    EXPECT_EQ(unfinished, "rank");
    manager.ClearParserStatus("rank");
    EXPECT_TRUE(manager.IsAllFinished(unfinished));
    manager.SetClusterParseStatus("cluster", ParserStatus::INIT);
    EXPECT_FALSE(manager.IsAllFinished(unfinished));
    EXPECT_EQ(unfinished, "cluster");
    manager.SetClusterParseStatus("cluster", ParserStatus::FINISH);
    EXPECT_TRUE(manager.IsAllFinished(unfinished));
}
TEST_F(ParserStatusManagerTest, WaitAllFinishedIncludesKernelAndMemoryBeforeReturning) {
    const std::string file = "rank";
    manager.SetParserStatus(file, ParserStatus::FINISH);
    manager.SetParserStatus(Dic::KERNEL_PREFIX + file, ParserStatus::RUNNING);
    manager.SetParserStatus(Dic::MEMORY_PREFIX + file, ParserStatus::RUNNING);
    EXPECT_FALSE(manager.IsKernelAndMemoryFinished(file));
    std::promise<void> entered;
    auto done = std::async(std::launch::async, [&] {
        entered.set_value();
        manager.WaitAllFinished({file});
    });
    entered.get_future().wait();
    EXPECT_EQ(done.wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);
    manager.SetFinishStatus(Dic::KERNEL_PREFIX + file);
    EXPECT_EQ(done.wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);
    manager.SetFinishStatus(Dic::MEMORY_PREFIX + file);
    const auto status = done.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);
    if (status != std::future_status::ready) {
        // Release the worker even if the tested transition failed to finish or notify it.
        manager.ClearAllParserStatus();
        manager.parseCv.notify_all();
    }
    done.get();
    EXPECT_TRUE(manager.IsKernelAndMemoryFinished(file));
}
TEST_F(ParserStatusManagerTest, ImportNotificationReleasesWaiterAfterReset) {
    manager.ResetParse();
    std::promise<void> entered;
    auto done = std::async(std::launch::async, [&] {
        entered.set_value();
        manager.WaitStartParse();
    });
    entered.get_future().wait();
    EXPECT_EQ(done.wait_for(std::chrono::milliseconds(10)), std::future_status::timeout);
    manager.NotifyStartParse();
    const auto status = done.wait_for(std::chrono::seconds(5));
    EXPECT_EQ(status, std::future_status::ready);
    if (status != std::future_status::ready) {
        // Cleanup must not depend on NotifyStartParse, which is the method under test.
        {
            std::lock_guard lock(manager.importMutex);
            manager.importRes.store(true);
        }
        manager.importResCv.notify_all();
    }
    done.get();
    manager.WaitStartParse();
}
}
