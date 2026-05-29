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

#ifndef PROFILER_SERVER_ADVISORPROTOCOLREQUEST_H
#define PROFILER_SERVER_ADVISORPROTOCOLREQUEST_H

#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
namespace Dic::Protocol {
struct APITypeParams {
    std::string rankId;
    std::string deviceId;
    uint32_t currentPage{};
    uint32_t pageSize{};
    std::string orderBy;
    std::string orderType;
    uint64_t startTime = 0;
    uint64_t endTime = 0;
    bool Check(uint64_t minTimestamp, std::string &errorMsg) const {
        if (!CheckPageValid(this->pageSize, this->currentPage, errorMsg)) {
            return false;
        }
        std::string paramError;
        if (!CheckStrParamValid(this->rankId, paramError)) {
            errorMsg = "[Advisor] Failed to check rankId." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->deviceId, paramError)) {
            errorMsg = "[Advisor] Failed to check deviceId." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->orderBy, paramError)) {
            errorMsg = "[Advisor] Failed to check orderBy." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->orderType, paramError)) {
            errorMsg = "[Advisor] Failed to check OrderType." + paramError;
            return false;
        }
        if (startTime > endTime) {
            errorMsg = "[Advisor] start time is bigger than end time";
            return false;
        }
        if (endTime > UINT64_MAX - minTimestamp) {
            errorMsg = "[Advisor] end time is invalid";
            return false;
        }
        return true;
    }
};

struct AffinityOptimizerRequest : public Request {
    AffinityOptimizerRequest() : Request(REQ_RES_ADVISOR_AFFINITY_OPTIMIZER) {};
    APITypeParams params;
};

struct AffinityAPIRequest : public Request {
    AffinityAPIRequest() : Request(REQ_RES_ADVISOR_AFFINITY_API) {};
    APITypeParams params;
};

struct OperatorFusionRequest : public Request {
    OperatorFusionRequest() : Request(REQ_RES_ADVISOR_OPERATORS_FUSION) {};
    APITypeParams params;
};

struct AICpuOperatorRequest : public Request {
    AICpuOperatorRequest() : Request(REQ_RES_ADVISOR_AICPU_OPERATORS) {};
    APITypeParams params;
};

struct AclnnOperatorRequest : public Request {
    AclnnOperatorRequest() : Request(REQ_RES_ADVISOR_ACLNN_OPERATORS) {};
    APITypeParams params;
};

struct OperatorDispatchRequest : public Request {
    OperatorDispatchRequest() : Request(REQ_RES_ADVISOR_OPERATOR_DISPATCH) {};
    APITypeParams params;
};

}
#endif // PROFILER_SERVER_ADVISORPROTOCOLREQUEST_H
