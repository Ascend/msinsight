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
#ifndef TIMELINE_MEMORY_DATABASE_FIXTURE_H
#define TIMELINE_MEMORY_DATABASE_FIXTURE_H
#include <gtest/gtest.h>
#include <algorithm>
#include "DataBaseManager.h"
#include "TrackInfoManager.h"

namespace Dic::Module::Timeline::Test {
class TimelineMemoryDatabaseTestBase : public ::testing::Test {
  protected:
    std::string rank;
    std::shared_ptr<VirtualTraceDatabase> database;
    void SetUp() override {
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        rank = std::string("timeline_memory_") + info->test_suite_name() + "_" + info->name();
        ASSERT_TRUE(DataBaseManager::Instance().CreateDerivedTraceConnectionPool(
            rank, "file:msinsight_overlap_" + rank + "?mode=memory&cache=shared", true));
        database = DataBaseManager::Instance().GetTraceDatabaseByRankId(rank);
        ASSERT_NE(database, nullptr);
        ASSERT_TRUE(database->ExecSql(
            "CREATE TABLE STRING_IDS(id INTEGER, value TEXT);"
            "CREATE TABLE CONNECTION_IDS(id INTEGER, connectionId INTEGER);"
            "CREATE TABLE PYTORCH_API(startNs INTEGER,endNs INTEGER,globalTid INTEGER,type INTEGER,"
            "connectionId INTEGER,name INTEGER,depth INTEGER);"
            "CREATE TABLE CANN_API(startNs INTEGER,endNs INTEGER,globalTid INTEGER,type INTEGER,"
            "connectionId INTEGER,name INTEGER,depth INTEGER);"
            "CREATE TABLE TASK(startNs INTEGER,endNs INTEGER,connectionId INTEGER,streamId INTEGER,"
            "deviceId INTEGER,globalTaskId INTEGER,taskType INTEGER,depth INTEGER);"
            "CREATE TABLE COMPUTE_TASK_INFO(globalTaskId INTEGER,name INTEGER);"
            "CREATE TABLE COMMUNICATION_TASK_INFO(globalTaskId INTEGER,opId INTEGER);"
            "CREATE TABLE COMMUNICATION_OP(opId INTEGER,startNs INTEGER,connectionId INTEGER,groupName INTEGER);"
            "CREATE TABLE HOST_INFO(hostName TEXT,hostUid TEXT);"
            "CREATE TABLE NPU_INFO(id INTEGER,name TEXT);"
            "CREATE TABLE GC_RECORD(startNs INTEGER,endNs INTEGER,globalTid INTEGER);"
            "CREATE TABLE MSTX_EVENTS(startNs INTEGER,endNs INTEGER,globalTid INTEGER,domainId INTEGER,"
            "connectionId INTEGER,message INTEGER,depth INTEGER);"));
    }
    void TearDown() override {
        database.reset();
        DataBaseManager::Instance().ReleaseDatabaseByFileId(rank);
        TrackInfoManager::Instance().Reset();
    }
};
}
#endif
