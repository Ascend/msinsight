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

#ifndef PROFILER_SERVER_DBFLOWREPO_H
#define PROFILER_SERVER_DBFLOWREPO_H
#include <memory>
#include "HostFlowRepo.h"
#include "DeviceFlowRepo.h"
namespace Dic::Module::Timeline {
class DbFlowRepo : public FlowRepoInterface {
  public:
    void QueryFlowPointByCategory(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override;
    void QueryFlowPointByTimeRange(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override;
    void QueryFlowPointByFlowId(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec) override;

  private:
    std::unique_ptr<HostFlowRepo> hostFlowRepo = std::make_unique<HostFlowRepo>();
    std::unique_ptr<DeviceFlowRepo> deviceFlowRepo = std::make_unique<DeviceFlowRepo>();
    void QueryHostToDevice(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec);
    void QueryMsTx(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec);
    void QueryAsyncNpu(const FlowQuery &flowQuery, std::vector<FlowPoint> &flowPointVec);
};
}
#endif // PROFILER_SERVER_DBFLOWREPO_H
