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
#include "TrackInfoManager.h"
#include "DbFlowRepo.h"
namespace Dic::Module::Timeline {
void DbFlowRepo::QueryFlowPointByCategory(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    if (flowQuery.cat == "async_task_queue") {
        hostFlowRepo->QueryAsyncTaskQueue(flowQuery, flowPointVec);
    } else if (flowQuery.cat == "fwdbwd") {
        hostFlowRepo->QueryFwdbwd(flowQuery, flowPointVec);
    } else if (flowQuery.cat == "async_npu") {
        QueryAsyncNpu(flowQuery, flowPointVec);
    } else if (flowQuery.cat == "HostToDevice") {
        QueryHostToDevice(flowQuery, flowPointVec);
    } else if (flowQuery.cat == "MsTx") {
        QueryMsTx(flowQuery, flowPointVec);
    }
}

void DbFlowRepo::QueryFlowPointByTimeRange(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {}

void DbFlowRepo::QueryFlowPointByFlowId(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {}

void DbFlowRepo::QueryHostToDevice(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    hostFlowRepo->AddCANNFlowPoint(flowQuery, flowPointVec);
    deviceFlowRepo->AddDeviceFlowPoint(flowQuery, flowPointVec);
    std::sort(flowPointVec.begin(), flowPointVec.end());
}

void DbFlowRepo::QueryMsTx(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    std::vector<uint64_t> connectionIds = hostFlowRepo->AddMstxFlowPoint(flowQuery, flowPointVec);
    deviceFlowRepo->AddHardWareMstxFlowPoint(flowQuery, flowPointVec, connectionIds);
    std::sort(flowPointVec.begin(), flowPointVec.end());
}

void DbFlowRepo::QueryAsyncNpu(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) {
    hostFlowRepo->AddAsyncNpuFlowPoint(flowQuery, flowPointVec);
    deviceFlowRepo->AddDeviceFlowPoint(flowQuery, flowPointVec);
    std::sort(flowPointVec.begin(), flowPointVec.end());
}
}
