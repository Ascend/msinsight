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

#include "QueryAffinityOptimizerAdvice.h"
#include "QueryAffinityAPIAdvice.h"
#include "QueryAiCpuOpAdviceHandler.h"
#include "QueryAclnnOpAdvisorHandler.h"
#include "QueryFusedOpAdviceHandler.h"
#include "QueryOperatorDispatchHandler.h"
#include "ProtocolDefs.h"
#include "AdvisorModule.h"

namespace Dic::Module {
using namespace Dic::Module::Advisor;
AdvisorModule::AdvisorModule() { moduleName = MODULE_ADVISOR; }
AdvisorModule::~AdvisorModule() { requestHandlerMap.clear(); }

void AdvisorModule::RegisterRequestHandlers() {
    requestHandlerMap.clear();
    requestHandlerMap.emplace(REQ_RES_ADVISOR_AFFINITY_OPTIMIZER, std::make_unique<QueryAffinityOptimizerAdvice>());
    requestHandlerMap.emplace(REQ_RES_ADVISOR_AFFINITY_API, std::make_unique<QueryAffinityAPIAdvice>());
    requestHandlerMap.emplace(REQ_RES_ADVISOR_AICPU_OPERATORS, std::make_unique<QueryAiCpuOpAdviceHandler>());
    requestHandlerMap.emplace(REQ_RES_ADVISOR_ACLNN_OPERATORS, std::make_unique<QueryAclnnOpAdvisorHandler>());
    requestHandlerMap.emplace(REQ_RES_ADVISOR_OPERATORS_FUSION, std::make_unique<QueryFusedOpAdviceHandler>());
    requestHandlerMap.emplace(REQ_RES_ADVISOR_OPERATOR_DISPATCH, std::make_unique<QueryOperatorDispatchHandler>());
}

void AdvisorModule::OnRequest(std::unique_ptr<Protocol::Request> request) { BaseModule::OnRequest(std::move(request)); }
}
