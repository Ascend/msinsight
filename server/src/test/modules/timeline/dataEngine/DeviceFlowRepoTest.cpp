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
#include "TaskTable.h"
#include "NpuInfoRepo.h"
#include "TrackInfoManager.h"
#include "DeviceFlowRepo.h"
#include "CommucationOpTable.h"
#include "../../../DatabaseTestCaseMockUtil.h"
using namespace Dic::Global::PROFILER::MockUtil;
using namespace Dic::Module::FullDb;
class DeviceFlowRepoTest : public DeviceFlowRepo, public ::testing::Test {
  protected:
    void SetUp() override { TrackInfoManager::Instance().Reset(); }

    void TearDown() override { TrackInfoManager::Instance().Reset(); }

    std::string taskCreate =
        "CREATE TABLE TASK (startNs INTEGER,endNs INTEGER,deviceId INTEGER,connectionId "
        "INTEGER,globalTaskId INTEGER,globalPid INTEGER,taskType INTEGER,contextId INTEGER,streamId "
        "INTEGER,taskId INTEGER,modelId INTEGER, depth integer);";
    std::string mstxCreate = "CREATE TABLE MSTX_EVENTS (startNs INTEGER,endNs INTEGER,eventType INTEGER,rangeId "
                             "INTEGER,category INTEGER,message INTEGER,globalTid INTEGER,endGlobalTid "
                             "INTEGER,domainId INTEGER,connectionId INTEGER, depth integer);";
    std::string taskInsert =
        "INSERT INTO TASK(startNs, endNs, deviceId, connectionId, globalTaskId, "
        "globalPid, taskType, contextId, streamId, taskId, modelId, depth) "
        "VALUES (1742699319641107170, 1742699319641107190, 0, 4294967295, 7480, 1984976, 1, 4294967295, 2, 12658, "
        "4294967295, 0),"
        "(1729733883833924932, 1729733883833924952, 0, 4000000002, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0),"
        "(1729733883833924952, 1729733883833924992, 0, 4000000001, 82550, 511284, 221, 4294967295, 2, 40, 4294967295, "
        "0);";
    std::string mstxInsert =
        "INSERT INTO MSTX_EVENTS (startNs, endNs, eventType, rangeId, category, message, globalTid, endGlobalTid, "
        "domainId, connectionId, depth) VALUES "
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 447, "
        "4754301164515056, 4754301164515056, 239, 4000000001, 0),"
        "(1729733883833924932, 1729733883833924952, 2, 4294967295, 4294967295, 448, "
        "4754301164515056, 4754301164515056, 240, 4000000002, 0);";
};

DeviceFlowRepo GetDeviceFlowRepoMock() {
    class TaskMock : public Dic::Module::Timeline::TaskTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<TaskPO> &result) override {
            TaskPO taskPO1 = {0, 0, 0, 0, 0, 33, 0, 0, 0, 0, 0, 0};
            result.emplace_back(taskPO1);
            ClearThreadLocal();
        }
    };
    class CommucationOpTableMock : public Dic::Module::Timeline::CommucationOpTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<CommucationTaskOpPO> &result) override {
            CommucationTaskOpPO commucationTaskOpPO1 = {1, 22, 45, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0};
            result.emplace_back(commucationTaskOpPO1);
            ClearThreadLocal();
        }
    };
    class NpuInfoTableMock : public Dic::Module::Timeline::NpuInfoTable {
      public:
        void ExcuteQuery(const std::string &fileId, std::vector<NpuInfoPo> &result) override {
            NpuInfoPo po = {"device1", 0};
            result.push_back(po);
            ClearThreadLocal();
        }
    };
    std::unique_ptr<Dic::Module::Timeline::TaskTable> tPtr = std::make_unique<TaskMock>();
    std::unique_ptr<Dic::Module::Timeline::CommucationOpTable> copPtr = std::make_unique<CommucationOpTableMock>();
    std::unique_ptr<Dic::Module::Timeline::NpuInfoTable> niPtr = std::make_unique<NpuInfoTableMock>();
    DeviceFlowRepo deviceFlowRepo;
    deviceFlowRepo.SetTaskTable(std::move(tPtr));
    deviceFlowRepo.SetCommucationOpTable(std::move(copPtr));
    std::unique_ptr<NpuInfoRepo> npr = std::make_unique<NpuInfoRepo>();
    npr->SetNpuInfoTable(std::move(niPtr));
    deviceFlowRepo.SetNpuInfoRepo(std::move(npr));
    return deviceFlowRepo;
}

TEST_F(DeviceFlowRepoTest, test_AddDeviceFlowPoint) {
    DeviceFlowRepo deviceFlowRepo = GetDeviceFlowRepoMock();
    std::vector<FlowPoint> flowPointVec;
    FlowQuery flowQuery;
    deviceFlowRepo.AddDeviceFlowPoint(flowQuery, flowPointVec);
    int expectCount = 1;
    EXPECT_EQ(flowPointVec.size(), expectCount);
}

TEST_F(DeviceFlowRepoTest, AddHardWareMstxFlowPointExecuteSQLTest) {
    std::string currPath = Dic::FileUtil::GetCurrPath();
    int index = currPath.find("server");
    currPath = currPath.substr(0, index);
    std::string dbPath = R"(test/data/msprof/)";
    std::string completePath = currPath + dbPath + "DeviceFlowRepoTest.db";
    std::recursive_mutex mutex;
    std::shared_ptr<VirtualTraceDatabase> database = std::make_shared<DbTraceDataBase>(mutex);
    database->OpenDb(completePath, false);
    database->ExecSql(taskCreate);
    database->ExecSql(mstxCreate);
    database->ExecSql(taskInsert);
    database->ExecSql(mstxInsert);

    FlowQuery flowQuery;
    flowQuery.minTimestamp = 0;
    std::vector<uint64_t> connectionIds = {4000000001, 4000000002};
    std::vector<FlowPoint> flowPointVec;
    AddHardWareMstxFlowPointExecuteSQL(flowQuery, flowPointVec, connectionIds, database);
    const size_t expectedSize = 2;
    ASSERT_EQ(flowPointVec.size(), expectedSize);
    EXPECT_EQ(flowPointVec[0].flowId, "4000000002");
    EXPECT_EQ(flowPointVec[0].id, 2); // 2
    EXPECT_EQ(flowPointVec[1].flowId, "4000000001");
    EXPECT_EQ(flowPointVec[1].id, 3); // 3
    database->CloseDb();
    std::remove(completePath.c_str());
}
