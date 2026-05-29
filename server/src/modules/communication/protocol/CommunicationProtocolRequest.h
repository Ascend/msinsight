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

#ifndef DIC_COMMUNICATION_PROTOCOL_COMMUNICATION_REQUEST_H
#define DIC_COMMUNICATION_PROTOCOL_COMMUNICATION_REQUEST_H

#include <string>
#include <optional>
#include <vector>
#include "ProtocolMessage.h"
#include "ProtocolDefs.h"

namespace Dic {
namespace Protocol {
struct OperatorDetailsParam {
    std::string iterationId;
    std::string rankId;
    std::string orderBy;
    std::string order;
    std::string stage;
    std::string queryType = "Comparison";
    std::string pgName;
    std::string clusterPath;
    std::string groupIdHash;
    int pageSize{};
    int currentPage{};
    bool CheckParams(std::string &errorMsg) const {
        if (!CheckPageValid(this->pageSize, this->currentPage, errorMsg)) {
            return false;
        }
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(this->iterationId, paramError)) {
            errorMsg = "[Communication] Failed to check iteration id." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->rankId, paramError)) {
            errorMsg = "[Communication] Failed to check rank id." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->orderBy, paramError)) {
            errorMsg = "[Communication] Failed to check orderBy." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->order, paramError)) {
            errorMsg = "[Communication] Failed to check order type." + paramError;
            return false;
        }
        if (!CheckStrParamValidWithoutLenLimit(this->stage, paramError)) {
            errorMsg = "[Communication] Failed to check stage." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->queryType, paramError)) {
            errorMsg = "[Communication] Failed to check query type." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->pgName, paramError)) {
            errorMsg = "[Communication] Failed to check pg name." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Communication] Failed to check cluster." + paramError;
            return false;
        }
        if (!CheckStrParamValid(groupIdHash, paramError)) {
            errorMsg = "[Communication] Failed to check cluster." + paramError;
            return false;
        }
        return true;
    }

    OperatorDetailsParam() = default;
    OperatorDetailsParam(const OperatorDetailsParam &param) {
        this->iterationId = param.iterationId;
        this->rankId = param.rankId;
        this->orderBy = param.orderBy;
        this->order = param.order;
        this->stage = param.stage;
        this->pageSize = param.pageSize;
        this->currentPage = param.currentPage;
        this->pgName = param.pgName;
        this->groupIdHash = param.groupIdHash;
    }
    OperatorDetailsParam &operator=(const OperatorDetailsParam &param) = delete;
};

struct OperatorDetailsRequest : public Request {
    OperatorDetailsRequest() : Request(REQ_RES_COMMUNICATION_OPERATOR_DETAILS) {};
    OperatorDetailsParam params;
};

struct BandwidthDataParam {
    std::string iterationId;
    std::string rankId;
    std::string operatorName;
    std::string stage;
    std::string pgName;
    std::string clusterPath;
    std::string groupIdHash;
    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(this->iterationId, paramError)) {
            errorMsg = "[Communication] Failed to check iteration id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->rankId, paramError)) {
            errorMsg = "[Communication] Failed to check rank id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->operatorName, paramError)) {
            errorMsg = "[Communication] Failed to operator name." + paramError;
            return false;
        }
        if (!CheckStrParamValidWithoutLenLimit(this->stage, paramError)) {
            errorMsg = "[Communication] Failed to check stage." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->pgName, paramError)) {
            errorMsg = "[Communication] Failed to check pg name." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Communication] Failed to check cluster." + paramError;
            return false;
        }
        if (!CheckStrParamValid(groupIdHash, paramError)) {
            errorMsg = "[Communication] Failed to check group id hash." + paramError;
            return false;
        }
        return true;
    }
};

struct BandwidthDataRequest : public Request {
    BandwidthDataRequest() : Request(REQ_RES_COMMUNICATION_BANDWIDTH) {};
    BandwidthDataParam params;
};

struct DistributionDataParam {
    std::string iterationId;
    std::string rankId;
    std::string operatorName;
    std::string transportType;
    std::string stage;
    std::string pgName;
    std::string clusterPath;
    std::string groupIdHash;
    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(this->iterationId, paramError)) {
            errorMsg = "[Communication] Failed to check iteration id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->rankId, paramError)) {
            errorMsg = "[Communication] Failed to check rank id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->operatorName, paramError)) {
            errorMsg = "[Communication] Failed to check operator name." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->transportType, paramError)) {
            errorMsg = "[Communication] Failed to check transport type." + paramError;
            return false;
        }
        if (!CheckStrParamValidWithoutLenLimit(this->stage, paramError)) {
            errorMsg = "[Communication] Failed to check stage." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->pgName, paramError)) {
            errorMsg = "[Communication] Failed to check pg name." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Communication] Failed to check cluster." + paramError;
            return false;
        }
        if (!CheckStrParamValid(groupIdHash, paramError)) {
            errorMsg = "[Communication] Failed to check group id hash." + paramError;
            return false;
        }
        return true;
    }
};

struct DistributionDataRequest : public Request {
    DistributionDataRequest() : Request(REQ_RES_COMMUNICATION_DISTRIBUTION) {};
    DistributionDataParam params;
};

struct IterationsParams {
    bool isCompare = false;
    std::string clusterPath;

    inline bool CheckParams(std::string &errMsg) const {
        std::string paramErr;
        if (!CheckStrParamValid(clusterPath, paramErr)) {
            errMsg = "[Communication] Failed to check cluster." + paramErr;
            return false;
        }
        return true;
    }
};

struct IterationsRequest : public Request {
    IterationsRequest() : Request(REQ_RES_COMMUNICATION_ITERATIONS) {};
    IterationsParams params;
};

struct OperatorNamesParams {
    std::string iterationId;
    std::vector<std::string> rankList = {};
    std::string stage;
    std::string pgName;
    std::string clusterPath;
    std::string groupIdHash;
    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(this->iterationId, paramError)) {
            errorMsg = "[Communication] Failed to check iteration id." + paramError;
            return false;
        }
        if (!CheckStrParamValidWithoutLenLimit(this->stage, paramError)) {
            errorMsg = "[Communication] Failed to check stage." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->pgName, paramError)) {
            errorMsg = "[Communication] Failed to check pg name." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Communication] Failed to check cluster." + paramError;
            return false;
        }
        if (!CheckStrParamValid(groupIdHash, paramError)) {
            errorMsg = "[Communication] Failed to check group id hash." + paramError;
            return false;
        }
        return true;
    }

    OperatorNamesParams() = default;
    OperatorNamesParams(const OperatorNamesParams &params) {
        this->iterationId = params.iterationId;
        for (const auto &item : params.rankList) {
            this->rankList.push_back(item);
        }
        this->stage = params.stage;
        this->groupIdHash = params.groupIdHash;
    }
    OperatorNamesParams &operator=(const OperatorNamesParams &params) = delete;
};

struct OperatorNamesRequest : public Request {
    OperatorNamesRequest() : Request(REQ_RES_COMMUNICATION_OPERATORNAMES) {};
    OperatorNamesParams params;
};

struct MatrixSortOpNamesRequest : public Request {
    MatrixSortOpNamesRequest() : Request(REQ_RES_COMMUNICATION_SORT_OP) {};
    OperatorNamesParams params;
};

struct DurationListParams {
    std::string iterationId;
    std::vector<std::string> rankList = {};
    std::string operatorName;
    std::string stage;
    std::string targetOperatorName;
    bool isCompare = false;
    std::string baselineIterationId;
    std::string pgName;
    std::string clusterPath;
    std::string groupIdHash;
    std::string baselineGroupIdHash;
    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(this->iterationId, paramError)) {
            errorMsg = "[Communication] Failed to check iteration id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->operatorName, paramError)) {
            errorMsg = "[Communication] Failed to operator name." + paramError;
            return false;
        }
        if (!CheckStrParamValidWithoutLenLimit(this->stage, paramError)) {
            errorMsg = "[Communication] Failed to check stage." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->baselineIterationId, paramError)) {
            errorMsg = "[Communication] Failed to check baseline iteration id." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->pgName, paramError)) {
            errorMsg = "[Communication] Failed to check pg name." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Communication] Failed to check cluster." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->baselineGroupIdHash, paramError)) {
            errorMsg = "[Communication] Failed to check baseline group id hash." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->groupIdHash, paramError)) {
            errorMsg = "[Communication] Failed to check group id hash." + paramError;
            return false;
        }
        return true;
    }

    DurationListParams() = default;
    DurationListParams(const DurationListParams &params) {
        this->iterationId = params.iterationId;
        this->operatorName = params.operatorName;
        this->stage = params.stage;
        this->targetOperatorName = params.targetOperatorName;
        this->pgName = params.pgName;
        for (const auto &item : params.rankList) {
            this->rankList.push_back(item);
        }
        this->groupIdHash = params.groupIdHash;
    }
    DurationListParams &operator=(const DurationListParams &params) = delete;
};

struct DurationListRequest : public Request {
    DurationListRequest() : Request(REQ_RES_COMMUNICATION_LIST) {};
    DurationListParams params;
};

struct MatrixGroupParam {
    std::string iterationId;
    std::string baselineIterationId;
    bool isCompare = false;
    std::string clusterPath;
    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(this->iterationId, paramError)) {
            errorMsg = "[Communication] Failed to check iteration id." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->baselineIterationId, paramError)) {
            errorMsg = "[Communication] Failed to check baseline iteration id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Communication] Failed to check cluster." + paramError;
            return false;
        }
        return true;
    }
};

struct MatrixGroupRequest : public Request {
    MatrixGroupRequest() : Request(REQ_RES_COMMUNICATION_MATRIX_GROUP) {};
    MatrixGroupParam params;
};

struct MatrixBandwidthParam {
    std::string stage;
    std::string operatorName;
    std::string iterationId;
    std::string pgName;
    std::string groupIdHash;
    bool isCompare = false;
    std::string baselineIterationId;
    std::string clusterPath;
    std::string baselineGroupIdHash;
    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(this->iterationId, paramError)) {
            errorMsg = "[Communication] Failed to check iteration id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->operatorName, paramError)) {
            errorMsg = "[Communication] Failed to operator name." + paramError;
            return false;
        }
        if (!CheckStrParamValidWithoutLenLimit(this->stage, paramError)) {
            errorMsg = "[Communication] Failed to check stage." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->baselineIterationId, paramError)) {
            errorMsg = "[Communication] Failed to check baseline iteration id." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->pgName, paramError)) {
            errorMsg = "[Communication] Failed to check pg name." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Communication] Failed to check cluster." + paramError;
            return false;
        }
        return true;
    }
};

struct MatrixBandwidthRequest : public Request {
    MatrixBandwidthRequest() : Request(REQ_RES_COMMUNICATION_MATRIX_BANDWIDTH) {};
    MatrixBandwidthParam params;
};

struct CommunicationAdvisorParam {
    std::string clusterPath;

    inline bool CheckParams(std::string &errMsg) const {
        std::string paramErr;
        if (!CheckStrParamValid(clusterPath, paramErr)) {
            errMsg = "[Communication] Failed to check cluster." + paramErr;
            return false;
        }
        return true;
    }
};
struct CommunicationAdvisorRequest : public Request {
    CommunicationAdvisorRequest() : Request(REQ_RES_COMMUNICATION_ADVISOR) {};
    CommunicationAdvisorParam params;
};
} // end of namespace Protocol
} // end of namespace Dic

#endif // DIC_COMMUNICATION_PROTOCOL_COMMUNICATION_REQUEST_H
