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

#include "AdvisorProtocolUtil.h"
#include "AdvisorProtocolFromRequestJson.h"
#include "AdvisorProtocolToResponseJson.h"

namespace Dic::Protocol {
void AdvisorProtocolUtil::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(
        REQ_RES_ADVISOR_AFFINITY_OPTIMIZER, AdvisorProtocolFromRequestJson::ToAffinityOptimizerRequest);
    jsonToReqFactory.emplace(REQ_RES_ADVISOR_AFFINITY_API, AdvisorProtocolFromRequestJson::ToAffinityAPIRequest);
    jsonToReqFactory.emplace(REQ_RES_ADVISOR_OPERATORS_FUSION, AdvisorProtocolFromRequestJson::ToOperatorFusionRequest);
    jsonToReqFactory.emplace(REQ_RES_ADVISOR_AICPU_OPERATORS, AdvisorProtocolFromRequestJson::ToAICpuOperatorRequest);
    jsonToReqFactory.emplace(REQ_RES_ADVISOR_ACLNN_OPERATORS, AdvisorProtocolFromRequestJson::ToAclnnOperatorRequest);
    jsonToReqFactory.emplace(
        REQ_RES_ADVISOR_OPERATOR_DISPATCH, AdvisorProtocolFromRequestJson::ToOperatorDispatchRequest);
}

void AdvisorProtocolUtil::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(
        REQ_RES_ADVISOR_AFFINITY_OPTIMIZER, AdvisorProtocolToResponseJson::ToAffinityOptimizerResponse);
    resToJsonFactory.emplace(REQ_RES_ADVISOR_AFFINITY_API, AdvisorProtocolToResponseJson::ToAffinityAPIResponse);
    resToJsonFactory.emplace(REQ_RES_ADVISOR_OPERATORS_FUSION, AdvisorProtocolToResponseJson::ToOperatorFusionResponse);
    resToJsonFactory.emplace(REQ_RES_ADVISOR_AICPU_OPERATORS, AdvisorProtocolToResponseJson::ToAICpuOperatorResponse);
    resToJsonFactory.emplace(REQ_RES_ADVISOR_ACLNN_OPERATORS, AdvisorProtocolToResponseJson::ToAclnnOperatorResponse);
    resToJsonFactory.emplace(
        REQ_RES_ADVISOR_OPERATOR_DISPATCH, AdvisorProtocolToResponseJson::ToOperatorDispatchResponse);
}

void AdvisorProtocolUtil::RegisterEventToJsonFuncs() {}
}
