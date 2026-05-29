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

#ifndef PROFILER_SERVER_ADVISORPROTOCOLRESPONSE_H
#define PROFILER_SERVER_ADVISORPROTOCOLRESPONSE_H

#include <vector>
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"

namespace Dic::Protocol {
struct BaseInfo {
    std::string id;
    std::string rankId;
    uint64_t startTime;
    uint64_t duration;
    std::string pid{};
    std::string tid{};
    uint64_t depth{};
};

struct AffinityOptimizerData {
    BaseInfo baseInfo;
    std::string originOptimizer;
    std::string replaceOptimizer;
};

struct AffinityOptimizerResBody {
    uint64_t size{};
    std::string dbPath;
    std::vector<AffinityOptimizerData> data;
};

struct AffinityOptimizerResponse : public Response {
    AffinityOptimizerResponse() : Response(REQ_RES_ADVISOR_AFFINITY_OPTIMIZER) {};
    AffinityOptimizerResBody body;
};

struct AffinityAPIData {
    BaseInfo baseInfo;
    std::string name; // 用来定位timeline上的算子
    std::string originAPI;
    std::string replaceAPI;
    std::string note;
};

struct AffinityAPIResBody {
    uint64_t size{};
    std::string dbPath;
    std::vector<AffinityAPIData> data;
};

struct AffinityAPIResponse : public Response {
    AffinityAPIResponse() : Response(REQ_RES_ADVISOR_AFFINITY_API) {};
    AffinityAPIResBody body;
};

struct OperatorFusionData {
    BaseInfo baseInfo;
    std::string name; // 用来定位timeline上的算子
    std::string originOpList; // 可融合的系列算子
    std::string fusedOp;
    std::string note;
};

struct OperatorFusionResBody {
    uint64_t size{};
    std::string dbPath;
    std::vector<OperatorFusionData> data;
};

struct OperatorFusionResponse : public Response {
    OperatorFusionResponse() : Response(REQ_RES_ADVISOR_OPERATORS_FUSION) {};
    OperatorFusionResBody body;
};

struct AICpuOperatorData {
    BaseInfo baseInfo;
    std::string opName;
    std::string note;
};

struct AICpuOperatorResBody {
    uint64_t size{};
    std::string dbPath;
    std::vector<AICpuOperatorData> data;
};

struct AICpuOperatorResponse : public Response {
    AICpuOperatorResponse() : Response(REQ_RES_ADVISOR_AICPU_OPERATORS) {};
    AICpuOperatorResBody body;
};

struct AclnnOperatorData {
    BaseInfo baseInfo;
    std::string opName;
    std::string note;
};

struct AclnnOperatorResBody {
    uint64_t size{};
    std::string dbPath;
    std::vector<AclnnOperatorData> data;
};

struct AclnnOperatorResponse : public Response {
    AclnnOperatorResponse() : Response(REQ_RES_ADVISOR_ACLNN_OPERATORS) {};
    AclnnOperatorResBody body;
};

struct OperatorDispatchData {
    BaseInfo baseInfo;
    std::string opName;
    std::string note;
};

struct OperatorDispatchResBody {
    uint64_t size{};
    std::string dbPath;
    std::vector<OperatorDispatchData> data;
};

struct OperatorDispatchResponse : public Response {
    OperatorDispatchResponse() : Response(REQ_RES_ADVISOR_OPERATOR_DISPATCH) {};
    OperatorDispatchResBody body;
};
}

#endif // PROFILER_SERVER_ADVISORPROTOCOLRESPONSE_H
