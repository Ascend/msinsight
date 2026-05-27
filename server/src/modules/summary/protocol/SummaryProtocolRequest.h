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

#ifndef DIC_SUMMARY_PROTOCOL_SUMMARY_REQUEST_H
#define DIC_SUMMARY_PROTOCOL_SUMMARY_REQUEST_H

#include <string>
#include <optional>
#include <vector>
#include "ProtocolDefs.h"
#include "ProtocolMessage.h"
#include "ClusterDef.h"

namespace Dic {
namespace Protocol {
struct SummaryTopRankParams {
    bool isCompare = false;
    std::string clusterPath;
    inline bool CheckParams(std::string &errMsg) const {
        std::string paramError;
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errMsg = "[Summary] Failed to check cluster";
            return false;
        }
        return true;
    }
};

struct SummaryTopRankRequest : public Request {
    SummaryTopRankRequest() : Request(REQ_RES_SUMMARY_QUERY_TOP_DATA) {};
    SummaryTopRankParams params;
};

struct SummaryStatisticParams {
    std::string rankId;
    std::string timeFlag;
    std::string stepId;
    std::string clusterPath;
    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValid(this->rankId, paramError)) {
            errorMsg = "[Summary] Failed to check rank id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->timeFlag, paramError)) {
            errorMsg = "[Summary] Failed to check time flag." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->stepId, paramError)) {
            errorMsg = "[Summary] Failed to check step id." + paramError;
            return false;
        }
        return true;
    }
};

struct SummaryStatisticRequest : public Request {
    SummaryStatisticRequest() : Request(REQ_RES_SUMMARY_STATISTIC) {};
    SummaryStatisticParams params;
};

struct PipelineStageTimeParam {
    std::string stepId;
    std::string stageId;
    std::string clusterPath;
    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(this->stepId, paramError)) {
            errorMsg = "[Summary] Failed to check step id." + paramError;
            return false;
        }
        if (!CheckStrParamValidWithoutLenLimit(this->stageId, paramError)) {
            errorMsg = "[Summary] Failed to check stage id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Summary] Failed to check cluster." + paramError;
            return false;
        }
        return true;
    }
};

struct PipelineFwdBwdTimelineParam {
    std::string stepId;
    std::string stageId;
    std::string clusterPath;
    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(this->stepId, paramError)) {
            errorMsg = "[Summary] Failed to check step id." + paramError;
            return false;
        }
        if (!CheckStrParamValidWithoutLenLimit(this->stageId, paramError)) {
            errorMsg = "[Summary] Failed to check stage id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Summary] Failed to check cluster." + paramError;
            return false;
        }
        return true;
    }
};

struct PipelineFwdBwdTimelineRequest : public Request {
    PipelineFwdBwdTimelineRequest() : Request(REQ_RES_PIPELINE_FWD_BWD_TIMELINE) {};
    PipelineFwdBwdTimelineParam params;
};

struct ParallelismArrangement {
    Module::ParallelStrategyConfig config;
    std::string dimension;
    std::string clusterPath;
    bool CheckParams(std::string &errorMsg) const {
        if (!config.CheckParams(errorMsg)) {
            return false;
        }
        if (std::find(Module::DIMENSIONS_ALLOWED.begin(), Module::DIMENSIONS_ALLOWED.end(), dimension) ==
            Module::DIMENSIONS_ALLOWED.end()) {
            errorMsg = "[Summary] Dimension is not allowed.";
            return false;
        }
        std::string paramErr;
        if (!CheckStrParamValid(clusterPath, paramErr)) {
            errorMsg = "[Summary] Failed to check cluster." + paramErr;
            return false;
        }
        return true;
    }
};

struct QueryParallelismArrangementRequest : public Request {
    QueryParallelismArrangementRequest() : Request(REQ_RES_PARALLELISM_ARRANGEMENT_ALL) {};
    ParallelismArrangement params;
};

struct ParallelismPerformance {
    Module::ParallelStrategyConfig config;
    std::string dimension;
    std::string orderBy;
    std::string step;
    bool isCompare = false;
    std::string baselineStep;
    std::vector<uint32_t> indexList;
    std::string clusterPath;
    bool CheckParams(std::string &errorMsg) const {
        if (!config.CheckParams(errorMsg)) {
            return false;
        }
        if (std::find(Module::DIMENSIONS_ALLOWED.begin(), Module::DIMENSIONS_ALLOWED.end(), dimension) ==
            Module::DIMENSIONS_ALLOWED.end()) {
            errorMsg = "[Summary] Dimension is not allowed.";
            return false;
        }
        std::string paramError;
        if (!CheckStrParamValidEmptyAllowed(this->orderBy, paramError)) {
            errorMsg = "[Summary] Failed to check orderBy." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->step, paramError)) {
            errorMsg = "[Summary] Failed to check step." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->baselineStep, paramError)) {
            errorMsg = "[Summary] Failed to check baselineStep." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Summary] Failed to check cluster." + paramError;
            return false;
        }
        return true;
    }
};

struct QueryParallelismPerformanceRequest : public Request {
    QueryParallelismPerformanceRequest() : Request(REQ_RES_PARALLELISM_PERFORMANCE_DATA) {};
    ParallelismPerformance params;
};

const std::string KEY_ORDERBY = "orderBy";
const std::string KEY_STEP = "step";
const std::string KEY_IS_COMPARE = "isCompare";
const std::string KEY_BASELINE_STEP = "baselineStep";

struct ComputeDetailParams {
    std::string rankId;
    std::string dbPath;
    std::string timeFlag;
    int64_t currentPage = 0;
    int64_t pageSize = 0;
    std::string orderBy;
    std::string order;
    std::string clusterPath;
    bool CheckParams(std::string &errorMsg) const {
        if (!CheckPageValid(this->pageSize, this->currentPage, errorMsg)) {
            return false;
        }
        std::string paramError;
        if (!CheckStrParamValid(this->rankId, paramError)) {
            errorMsg = "[Summary] Failed to check rank id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->timeFlag, paramError)) {
            errorMsg = "[Summary] Failed to check time flag." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->orderBy, paramError)) {
            errorMsg = "[Summary] Failed to check orderBy." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->order, paramError)) {
            errorMsg = "[Summary] Failed to check order." + paramError;
            return false;
        }
        return true;
    }
};

struct ComputeDetailRequest : public Request {
    ComputeDetailRequest() : Request(REQ_RES_COMPUTE_DETAIL) {};
    ComputeDetailParams params;
};

struct CommunicationDetailParams {
    std::string rankId;
    std::string timeFlag = "HCCL";
    int64_t currentPage = 0;
    int64_t pageSize = 0;
    std::string orderBy;
    std::string order;
    std::string clusterPath;
    bool CheckParams(std::string &errorMsg) const {
        if (!CheckPageValid(this->pageSize, this->currentPage, errorMsg)) {
            return false;
        }
        std::string paramError;
        if (!CheckStrParamValid(this->rankId, paramError)) {
            errorMsg = "[Summary] Failed to check rank id." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->timeFlag, paramError)) {
            errorMsg = "[Summary] Failed to check time flag." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->orderBy, paramError)) {
            errorMsg = "[Summary] Failed to check orderBy." + paramError;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->order, paramError)) {
            errorMsg = "[Summary] Failed to check order." + paramError;
            return false;
        }
        return true;
    }
};

struct CommunicationDetailRequest : public Request {
    CommunicationDetailRequest() : Request(REQ_RES_COMMUNICATION_DETAIL) {};
    CommunicationDetailParams params;
};

struct ParallelStrategyParam {
    std::string clusterPath;

    inline bool CheckParams(std::string &errMsg) const {
        std::string paramErr;
        if (!CheckStrParamValid(clusterPath, paramErr)) {
            errMsg = "[Summary] Failed to check cluster." + paramErr;
            return false;
        }
        return true;
    }
};

struct QueryParallelStrategyRequest : public Request {
    QueryParallelStrategyRequest() : Request(REQ_RES_SUMMARY_QUERY_PARALLEL_STRATEGY) {};
    ParallelStrategyParam params;
};

struct SetParallelStrategyParam {
    Module::ParallelStrategyConfig config;
    std::string clusterPath;
    bool CheckParams(std::string &errMsg) const {
        if (!config.CheckParams(errMsg)) {
            return false;
        }
        if (!CheckStrParamValid(clusterPath, errMsg)) {
            return false;
        }
        return true;
    }
};
struct SetParallelStrategyRequest : public Request {
    SetParallelStrategyRequest() : Request(REQ_RES_SUMMARY_SET_PARALLEL_STRATEGY) {};
    SetParallelStrategyParam params;
};

struct ImportExpertDataParams {
    std::string filePath;
    std::string version;
    std::string clusterPath;

    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValid(this->filePath, paramError)) {
            errorMsg = "[Summary] Failed to check file path." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->version, paramError)) {
            errorMsg = "[Summary] Failed to check version." + paramError;
            return false;
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Summary] Failed to check cluster." + paramError;
            return false;
        }
        return true;
    }
};

struct ImportExpertDataRequest : public Request {
    ImportExpertDataRequest() : Request(REQ_RES_IMPORT_EXPERT_DATA) {};
    ImportExpertDataParams params;
};

struct ModelInfoParam {
    std::string clusterPath;
    inline bool CheckParams(std::string &errMsg) const {
        std::string paramErr;
        if (!CheckStrParamValid(clusterPath, paramErr)) {
            errMsg = "[Summary] Failed to check cluster." + paramErr;
            return false;
        }
        return true;
    }
};

struct QueryModelInfoRequest : public Request {
    QueryModelInfoRequest() : Request(REQ_RES_QUERY_MODEL_INFO) {};
    ModelInfoParam params;
};

struct QueryExpertHotspotParams {
    std::string modelStage;
    std::string version;
    std::vector<int> denseLayerList;
    int layerNum = 0;
    int expertNum = 0;
    std::string clusterPath;

    bool CheckParams(std::string &errorMsg) const {
        std::string paramError;
        if (!CheckStrParamValid(this->modelStage, paramError)) {
            errorMsg = "[Summary] Failed to check model stage." + paramError;
            return false;
        }
        if (!CheckStrParamValid(this->version, paramError)) {
            errorMsg = "[Summary] Failed to check version." + paramError;
            return false;
        }
        if (layerNum <= 0 || expertNum <= 0) {
            errorMsg = "[Summary] The number of layer and the number of expert must be greater than zero.";
            return false;
        }
        for (const auto &item : denseLayerList) {
            if (item >= layerNum) {
                errorMsg = "[Summary] The range of dense layer is out of layer number.";
                return false;
            }
        }
        if (!CheckStrParamValid(clusterPath, paramError)) {
            errorMsg = "[Summary] Failed to check cluster." + paramError;
            return false;
        }
        return true;
    }
};

struct QueryExpertHotspotRequest : public Request {
    QueryExpertHotspotRequest() : Request(REQ_RES_QUERY_EXPERT_HOTSPOT) {};
    QueryExpertHotspotParams params;
};

} // end of namespace Protocol
} // end of namespace Dic

#endif // DIC_SUMMARY_PROTOCOL_SUMMARY_REQUEST_H
