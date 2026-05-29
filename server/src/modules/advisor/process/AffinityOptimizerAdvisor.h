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

#ifndef PROFILER_SERVER_AFFINITYOPTIMIZER_H
#define PROFILER_SERVER_AFFINITYOPTIMIZER_H

#include <map>
#include "AdvisorProtocolRequest.h"
#include "AdvisorProtocolResponse.h"

namespace Dic::Module::Advisor {
const std::map<std::string, std::string> OPTIMIZER_MAP = {{"Optimizer.step#SGD.step", "torch_npu.optim.NpuFusedSGD"},
    {"Optimizer.step#Adadelta.step", "torch_npu.optim.NpuFusedAdadelta"},
    {"Optimizer.step#Lamb.step", "torch_npu.optim.NpuFusedLamb"},
    {"Optimizer.step#Adam.step", "torch_npu.optim.NpuFusedAdam"},
    {"Optimizer.step#AdamW.step", "torch_npu.optim.NpuFusedAdamW"},
    {"Optimizer.step#AdamP.step", "torch_npu.optim.NpuFusedAdamP"},
    {"Optimizer.step#BertAdam.step", "torch_npu.optim.NpuFusedBertAdam"},
    {"Optimizer.step#RMSprop.step", "torch_npu.optim.NpuFusedRMSprop"},
    {"Optimizer.step#RMSpropTF.step", "torch_npu.optim.NpuFusedRMSpropTF"}};
const std::vector<std::string> AFFINITY_OP_ORDER_BY_NAME_LIST = {
    "startTime", "duration", "pid", "tid", "originOptimizer"};

class AffinityOptimizerAdvisor {
  public:
    static bool Process(const Protocol::APITypeParams &params, Protocol::AffinityOptimizerResBody &resBody);
};
}

#endif // PROFILER_SERVER_AFFINITYOPTIMIZER_H
