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

#ifndef PROFILER_SERVER_FUSEDOPADVISOR_H
#define PROFILER_SERVER_FUSEDOPADVISOR_H

#include <memory>
#include <string>
#include <vector>
#include "VirtualTraceDatabase.h"
#include "AdvisorProtocolRequest.h"
#include "AdvisorProtocolResponse.h"

namespace Dic::Module::Advisor {
const std::vector<Timeline::FuseableOpRule> FUSEABLE_OPERATER_RULE_LIST = {
    {{"Cast", "LayerNorm", "Cast"}, "LayerNorm", ""},
    {{"Transpose", "Transpose", "GatherElement", "Transpose"}, "GatherElements", ""},
    {{"Mul", "Slice", "Neg", "Slice", "ConcatD", "Mul", "Add"}, "RotaryMul", ""},
    {{"Mul", "AsStrided", "Neg", "AsStrided", "ConcatD", "Mul", "Add"}, "RotaryMul", ""},
    {{"Mul", "Slice", "Neg", "Slice", "ConcatD", "Cast", "Mul", "Add"}, "RotaryMul", ""},
    {{"Cast", "Square", "MemSet", "ReduceMean", "Add", "Rsqrt", "Mul", "Cast", "Mul"}, "RMSNorm", ""},
    {{"Cast", "Square", "ReduceMeanD", "Add", "Rsqrt", "Cast", "Cast", "Mul", "Cast", "Cast", "Mul", "Cast"}, "RMSNORM",
        ""},
};

const std::vector<std::string> FUSED_OP_ORDER_BY_NAME_LIST = {"startTime", "duration", "pid", "tid", "name"};
class FusedOpAdvisor {
  public:
    static bool Process(const Protocol::APITypeParams &params, Protocol::OperatorFusionResBody &resBody);

  private:
    static std::shared_ptr<Timeline::VirtualTraceDatabase> GetDatabaseConnection(const std::string &rankId);
};

} // Dic::Module::Advisor

#endif // PROFILER_SERVER_FUSEDOPADVISOR_H
