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

#ifndef PROFILER_SERVER_OPERATORPROTOCOLREQUEST_H
#define PROFILER_SERVER_OPERATORPROTOCOLREQUEST_H

#include <string>
#include <vector>
#include "pch.h"
#include "ProtocolDefs.h"
#include "OperatorProtocol.h"
#include "OperatorGroupConverter.h"

namespace Dic::Protocol {

enum class QueryType { CATEGORY, COMPUTE_UNIT };

inline bool CheckOrderOrFilterColumnValid(
    const std::string_view &colName, const std::set<std::string_view> &validCols) {
    if (colName.empty() || validCols.empty()) {
        return false;
    }
    return validCols.find(colName) != validCols.end();
}

// 算子视图饼图的请求参数
struct OperatorDurationReqParams {
    std::string rankId;
    std::string deviceId;
    std::string group; // Operator、Operator Type、Input Shape
    int64_t topK{0};

    bool CommonCheck(std::string &errorMsg) {
        if (!CheckStrParamValid(this->rankId, errorMsg)) {
            errorMsg = StringUtil::StrJoin("[Operator]Failed to check rankId in Query Compute Unit Info.", errorMsg);
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->deviceId, errorMsg)) {
            errorMsg = std::string("[Operator]Failed to check deviceId in Query Compute Unit Info.") + errorMsg;
            return false;
        }
        if (!CheckStrParamValid(this->group, errorMsg)) {
            errorMsg = std::string("[Operator]Failed to check group in Query Compute Unit Info.") + errorMsg;
            return false;
        }
        if (this->topK < -1) {
            errorMsg = std::string("[Operator]Failed to check topK in Query Compute Unit Info.") + errorMsg;
            return false;
        }
        return true;
    }
};

// 算子视图表格数据的请求参数
struct OperatorStatisticReqParams {
    bool isCompare{false};
    std::string rankId;
    std::string deviceId;
    std::string group; // Operator、Operator Type、Input Shape
    int64_t topK{0};
    int64_t current{1};
    int64_t pageSize{0};
    std::string orderBy;
    std::string order;
    std::vector<std::pair<std::string, std::string>> filters;
    std::vector<std::pair<std::string, std::vector<std::string>>> rangeFilters;
    bool CommonCheck(std::string &errorMsg) {
        // 查询值 小于-1,是异常值不需要再走查询，减少耗时
        if (this->topK < -1) {
            errorMsg = "[Operator]Failed to check topK in Query Op Statistic Info.";
            return false;
        }
        if (!CheckPageValid(this->pageSize, this->current, errorMsg)) {
            return false;
        }
        if (!CheckStrParamValid(rankId, errorMsg)) {
            errorMsg = std::string("[Operator]Failed to check rankId in Query Op Statistic Info.") + errorMsg;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(deviceId, errorMsg)) {
            errorMsg = std::string("[Operator]Failed to check deviceId in Query Op Statistic Info.") + errorMsg;
            return false;
        }
        if (!this->orderBy.empty() &&
            !CheckOrderOrFilterColumnValid(this->orderBy, OperatorStatisticView::VALID_ORDER_COLS) &&
            !CheckOrderOrFilterColumnValid(this->orderBy, OperatorDetailsView::VALID_ORDER_COLS)) {
            errorMsg = "[Operator]Failed to check orderBy in Query Op Statistic Info.";
            return false;
        }
        for (auto &filter : this->filters) {
            if (!CheckOrderOrFilterColumnValid(filter.first, OperatorStatisticView::VALID_FILTER_COLS) &&
                !CheckOrderOrFilterColumnValid(filter.first, OperatorDetailsView::VALID_FILTER_COLS)) {
                errorMsg = "[Operator]Failed to check filter column in Query Op Statistic Info.";
                return false;
            }
        }
        return true;
    }

    bool StatisticGroupCheck(std::string &errorMsg) {
        OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(this->group);
        if (operatorGroup != OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP &&
            operatorGroup != OperatorGroupConverter::OperatorGroup::COMMUNICATION_TYPE_GROUP &&
            operatorGroup != OperatorGroupConverter::OperatorGroup::OP_INPUT_SHAPE_GROUP) {
            errorMsg = "[Operator]Wrong group type in Query Op Statistic Info.";
            return false;
        }
        return true;
    }
};

// Operator Type、Operator Name + Input Shape类型时See More的请求参数
struct OperatorMoreInfoReqParams {
    std::string rankId;
    std::string deviceId;
    std::string group;
    int64_t topK{0};
    std::string opType;
    std::string opName;
    std::string shape;
    std::string accCore;
    int64_t current{1};
    int64_t pageSize{0};
    std::string orderBy;
    std::string order;
    std::vector<std::pair<std::string, std::string>> filters;

    bool CommonCheck(std::string &errMsg) const {
        if (!CheckStrParamValid(this->rankId, errMsg)) {
            errMsg = "[Operator]Failed to check rankId in query op more info." + errMsg;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->deviceId, errMsg)) {
            errMsg = "[Operator]Failed to check deviceId in query op more info." + errMsg;
            return false;
        }
        if (!CheckStrParamValid(this->opName, errMsg) && !CheckStrParamValid(this->opType, errMsg)) {
            errMsg = "[Operator]Failed to check name and type in query op more info." + errMsg;
            return false;
        }
        OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(this->group);
        if (operatorGroup != OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP &&
            operatorGroup != OperatorGroupConverter::OperatorGroup::COMMUNICATION_TYPE_GROUP &&
            operatorGroup != OperatorGroupConverter::OperatorGroup::OP_INPUT_SHAPE_GROUP) {
            errMsg = "[Operator]Wrong group type in query op more info.";
            return false;
        }
        if (!this->orderBy.empty() &&
            !CheckOrderOrFilterColumnValid(this->orderBy, OperatorDetailsView::VALID_ORDER_COLS)) {
            errMsg = "[Operator]Failed to check orderBy in query Op more info.";
            return false;
        }
        for (auto &filter : this->filters) {
            if (!CheckOrderOrFilterColumnValid(filter.first, OperatorDetailsView::VALID_FILTER_COLS)) {
                errMsg = "[Operator]Failed to check filter column in query Op more info.";
                return false;
            }
        }
        return true;
    }
};

// 导出算子的请求参数
struct ExportOperatorDetailsReqParams {
    bool isCompare{false};
    std::string rankId;
    std::string deviceId;
    std::string group;
    int64_t topK{0};

    bool CommonCheck(std::string &errorMsg) {
        if (!CheckStrParamValid(this->rankId, errorMsg)) {
            errorMsg = std::string("[Operator]Failed to check rankId in export op detail.") + errorMsg;
            return false;
        }
        if (!CheckStrParamValidEmptyAllowed(this->deviceId, errorMsg)) {
            errorMsg = std::string("[Operator]Failed to check deviceId in export op detail.") + errorMsg;
            return false;
        }
        if (!CheckStrParamValid(this->group, errorMsg)) {
            errorMsg = std::string("[Operator]Failed to check group in export op detail.") + errorMsg;
            return false;
        }
        if (this->topK < -1) {
            errorMsg = std::string("[Operator]Failed to check topK in export op detail.") + errorMsg;
            return false;
        }
        return true;
    }
    bool IsStatisticGroup() {
        OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(this->group);
        if (operatorGroup != OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP &&
            operatorGroup != OperatorGroupConverter::OperatorGroup::COMMUNICATION_TYPE_GROUP &&
            operatorGroup != OperatorGroupConverter::OperatorGroup::OP_INPUT_SHAPE_GROUP) {
            return false;
        }
        return true;
    }
    bool IsNotStatisticGroup() {
        OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(this->group);
        if (operatorGroup != OperatorGroupConverter::OperatorGroup::OP_NAME_GROUP &&
            operatorGroup != OperatorGroupConverter::OperatorGroup::COMMUNICATION_NAME_GROUP) {
            return false;
        }
        return true;
    }
    bool StatisticGroupCheck(std::string &errorMsg) {
        if (!IsStatisticGroup() && !IsNotStatisticGroup()) {
            errorMsg = "[Operator]Wrong group type in export op detail.";
            return false;
        }
        return true;
    }
};

// 获取算子按类型耗时信息
struct OperatorCategoryInfoRequest : public Request {
    OperatorCategoryInfoRequest() : Request(REQ_RES_OPERATOR_CATEGORY_INFO) {};
    OperatorDurationReqParams params;
};

// 获取算子按计算单元耗时信息
struct OperatorComputeUnitInfoRequest : public Request {
    OperatorComputeUnitInfoRequest() : Request(REQ_RES_OPERATOR_COMPUTE_UNIT_INFO) {};
    OperatorDurationReqParams params;
};

// 获取算子按Op Type或者Op Name + Input Shape统计信息
struct OperatorStatisticInfoRequest : public Request {
    OperatorStatisticInfoRequest() : Request(REQ_RES_OPERATOR_STATISTIC_INFO) {};
    OperatorStatisticReqParams params;
};

// 获取算子全量信息
struct OperatorDetailInfoRequest : public Request {
    OperatorDetailInfoRequest() : Request(REQ_RES_OPERATOR_DETAIL_INFO) {};
    OperatorStatisticReqParams params;
};

// Op Type或者Op Name + Input Shape中See More请求
struct OperatorMoreInfoRequest : public Request {
    OperatorMoreInfoRequest() : Request(REQ_RES_OPERATOR_MORE_INFO) {};
    OperatorMoreInfoReqParams params;
};

// 导出算子全量信息请求
struct OperatorExportDetailsRequest : public Request {
    OperatorExportDetailsRequest() : Request(REQ_RES_OPERATOR_EXPORT_DETAILS) {};
    ExportOperatorDetailsReqParams params;
};
}

#endif // PROFILER_SERVER_OPERATORPROTOCOLREQUEST_H
