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

#ifndef PROFILER_SERVER_OPERATORDISPATCHADVISOR_H
#define PROFILER_SERVER_OPERATORDISPATCHADVISOR_H

#include <cstdint>
#include <string>
#include <vector>
#include "VirtualTraceDatabase.h"
#include "AdvisorProtocolRequest.h"
#include "AdvisorProtocolResponse.h"

namespace Dic::Module::Advisor {

const uint64_t OPERATOR_COMPILE_CNT_THRESHOLD = 20;
const std::vector<std::string> OPERATOR_DISPATCH_ORDER_BY_NAME_LIST = {"startTime", "duration", "pid", "tid", "name"};
const std::string SUGGESTION_NOTE = "Please use `torch_npu.npu.set_compile_mode(jit_compile=False)` "
                                    "to disable jit compile in dynamic shape usage.";

class OperatorDispatchAdvisor {
  public:
    static bool Process(const Protocol::APITypeParams &params, Protocol::OperatorDispatchResBody &resBody);
};

} // Dic::Module::Advisor

#endif // PROFILER_SERVER_OPERATORDISPATCHADVISOR_H
