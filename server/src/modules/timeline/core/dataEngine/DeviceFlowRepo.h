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

#ifndef PROFILER_SERVER_DEVICEFLOWREPO_H
#define PROFILER_SERVER_DEVICEFLOWREPO_H
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "CommucationOpTable.h"
#include "TaskTable.h"
#include "HostInfoTable.h"
#include "NpuInfoRepo.h"
#include "CommucationTaskInfoTable.h"
namespace Dic::Module::Timeline {
class DeviceFlowRepo {
  public:
    void AddDeviceFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec);
    void AddHardWareMstxFlowPoint(
        const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec, const std::vector<uint64_t> &connectionIds);
    void SetTaskTable(std::unique_ptr<TaskTable> taskTablePtr);
    void SetCommucationOpTable(std::unique_ptr<CommucationOpTable> commucationOpTablePtr);
    void SetNpuInfoRepo(std::unique_ptr<NpuInfoRepo> npuInfoRepoPtr);

  protected:
    void AddHardWareMstxFlowPointExecuteSQL(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec,
        const std::vector<uint64_t> &connectionIds, std::shared_ptr<VirtualTraceDatabase> database);

  private:
    const std::string hcclPid = "HCCL";
    const std::string hardWarePid = "Ascend Hardware";
    std::unique_ptr<CommucationOpTable> commucationOpTable = std::make_unique<CommucationOpTable>();
    std::unique_ptr<TaskTable> taskTable = std::make_unique<TaskTable>();
    std::unique_ptr<HostInfoTable> hostInfoTable = std::make_unique<HostInfoTable>();
    std::unique_ptr<CommucationTaskInfoTable> commucationTaskInfoTable = std::make_unique<CommucationTaskInfoTable>();
    std::unique_ptr<NpuInfoRepo> npuInfoRepo = std::make_unique<NpuInfoRepo>();
    std::unordered_map<uint64_t, uint64_t> QueryOpIdMap(const FlowQuery &flowQuery);
    std::unordered_map<uint64_t, uint64_t> QueryDeviceMap(const FlowQuery &flowQuery);
    std::unordered_set<int64_t> AddGroupHcclFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec,
        const std::unordered_map<uint64_t, uint64_t> &opIdMap, const std::unordered_map<uint64_t, uint64_t> &deviceMap,
        const std::string &host);
    void AddHardWareFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec, const std::string &host,
        const std::unordered_set<int64_t> &hcclConnectionIdSet);
};
}
#endif // PROFILER_SERVER_DEVICEFLOWREPO_H
