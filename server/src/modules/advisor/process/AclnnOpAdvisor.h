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

#ifndef PROFILER_SERVER_ACLNNOPADVISOR_H
#define PROFILER_SERVER_ACLNNOPADVISOR_H

#include "AdvisorProtocolRequest.h"
#include "AdvisorProtocolResponse.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::Advisor {
const uint64_t ACLNN_OP_CNT_THRESHOLD = 20;
const std::vector<std::string> SINGLE_OP_ORDER_BY_NAME_LIST = {"startTime", "duration", "pid", "tid", "name"};
class AclnnOpAdvisor {
  public:
    static bool Process(const Protocol::APITypeParams &params, Protocol::AclnnOperatorResBody &resBody);

  private:
    static bool AclnnOpProcess(const std::shared_ptr<Timeline::VirtualTraceDatabase> &database,
        Protocol::KernelDetailsParams &param, Protocol::AclnnOperatorResBody &resBody);
};

} // Dic::Module::Advisor

#endif // PROFILER_SERVER_ACLNNOPADVISOR_H
