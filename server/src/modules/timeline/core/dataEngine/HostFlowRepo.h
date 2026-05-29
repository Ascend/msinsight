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

#ifndef PROFILER_SERVER_HOSTFLOWREPO_H
#define PROFILER_SERVER_HOSTFLOWREPO_H
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "PytorchApiTable.h"
#include "ConnectionIdsTable.h"
#include "StringIdsTable.h"
#include "MstxEventsTable.h"
#include "CANNApiTable.h"
namespace Dic::Module::Timeline {
class HostFlowRepo {
  public:
    void QueryFwdbwd(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec);
    void QueryAsyncTaskQueue(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec);
    void AddAsyncNpuFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec);
    void AddCANNFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec);
    std::vector<uint64_t> AddMstxFlowPoint(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec);

  private:
    const std::string pythonApiTid = "pytorch";
    std::unique_ptr<PytorchApiTable> pytorchApiTable = std::make_unique<PytorchApiTable>();
    std::unique_ptr<ConnectionIdsTable> connectionIdsTable = std::make_unique<ConnectionIdsTable>();
    std::unique_ptr<StringIdsTable> stringIdsTable = std::make_unique<StringIdsTable>();
    std::unique_ptr<MstxEventsTable> mstxEventsTable = std::make_unique<MstxEventsTable>();
    std::unique_ptr<CANNApiTable> cannApiTable = std::make_unique<CANNApiTable>();
    uint64_t QueryEnqueueNameId(const FlowQuery &flowQuery);
    std::vector<PytorchApiPO> QueryNotEnqueuePythonApi(const FlowQuery &flowQuery, uint64_t nameId);
    std::unordered_map<uint64_t, uint64_t> QueryConnectionIdMap(const FlowQuery &flowQuery);
    std::vector<uint64_t> QueryEnqueueFlowConnectionIds(const FlowQuery &flowQuery, uint64_t nameId);
    std::vector<uint64_t> QueryRealConnectionIds(
        const FlowQuery &flowQuery, const std::vector<uint64_t> &pythonConnectionIds);
    void QueryAllPythonConnectionIds(const FlowQuery &flowQuery, const std::vector<uint64_t> &realConnectionIds,
        std::unordered_map<uint64_t, uint64_t> &connectionIdMap, std::vector<uint64_t> &allPythonConnectionIds);
};
}
#endif // PROFILER_SERVER_HOSTFLOWREPO_H
