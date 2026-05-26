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
#include <unordered_map>
#include <algorithm>
#include <functional>
#include "pch.h"
#include "BaselineManager.h"
#include "DataBaseManager.h"
#include "OperatorProtocolRequest.h"
#include "OperatorProtocolResponse.h"
#include "OperatorGroupConverter.h"
#include "OperatorProtocol.h"
#include "QueryOpStatisticInfoHandler.h"

namespace {
using namespace Dic;
using namespace Dic::Server;
using StatisticCmpRes = Dic::Protocol::OperatorStatisticCmpInfoRes;
using GetObjFunc = std::function<OperatorStatisticInfoRes(const StatisticCmpRes &)>;

using StatisticCmpFun = std::function<bool(const StatisticCmpRes &, const StatisticCmpRes &, GetObjFunc)>;
std::unordered_map<std::string, StatisticCmpFun> StatisticDescCompareFunctions = {
    {"opType",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return func(a).opType > func(b).opType;
        }},
    {"opName",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return func(a).opName > func(b).opName;
        }},
    {"inputShape",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return func(a).inputShape > func(b).inputShape;
        }},
    {"accCore",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return func(a).accCore > func(b).accCore;
        }},
    {"totalTime",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return NumberUtil::IsStr2DoubleDesc(func(a).totalTime, func(b).totalTime);
        }},
    {"count",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return NumberUtil::IsStr2DoubleDesc(func(a).count, func(b).count);
        }},
    {"avgTime",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return NumberUtil::IsStr2DoubleDesc(func(a).avgTime, func(b).avgTime);
        }},
    {"maxTime",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return NumberUtil::IsStr2DoubleDesc(func(a).maxTime, func(b).maxTime);
        }},
    {"minTime", [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
         return NumberUtil::IsStr2DoubleDesc(func(a).minTime, func(b).minTime);
     }}};
std::unordered_map<std::string, StatisticCmpFun> StatisticAsceCompareFunctions = {
    {"opType",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return func(a).opType < func(b).opType;
        }},
    {"opName",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return func(a).opName < func(b).opName;
        }},
    {"inputShape",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return func(a).inputShape < func(b).inputShape;
        }},
    {"accCore",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return func(a).accCore < func(b).accCore;
        }},
    {"totalTime",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return NumberUtil::IsStr2DoubleAsce(func(a).totalTime, func(b).totalTime);
        }},
    {"count",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return NumberUtil::IsStr2DoubleAsce(func(a).count, func(b).count);
        }},
    {"avgTime",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return NumberUtil::IsStr2DoubleAsce(func(a).avgTime, func(b).avgTime);
        }},
    {"maxTime",
        [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
            return NumberUtil::IsStr2DoubleAsce(func(a).maxTime, func(b).maxTime);
        }},
    {"minTime", [](const StatisticCmpRes &a, const StatisticCmpRes &b, GetObjFunc func) {
         return NumberUtil::IsStr2DoubleAsce(func(a).minTime, func(b).minTime);
     }}};
bool StatisticDescCmp(const StatisticCmpRes &a, const StatisticCmpRes &b, const std::string orderBy, GetObjFunc func) {
    auto it = StatisticDescCompareFunctions.find(orderBy);
    if (it != StatisticDescCompareFunctions.end()) {
        return it->second(a, b, func);
    }
    return func(a).totalTime > func(b).totalTime;
}
bool StatisticAsceCmp(const StatisticCmpRes &a, const StatisticCmpRes &b, const std::string orderBy, GetObjFunc func) {
    auto it = StatisticAsceCompareFunctions.find(orderBy);
    if (it != StatisticAsceCompareFunctions.end()) {
        return it->second(a, b, func);
    }
    return func(a).totalTime < func(b).totalTime;
}
bool StaticCmp(const StatisticCmpRes &a, const StatisticCmpRes &b, const std::string order, const std::string orderBy,
    GetObjFunc func) {
    if (order == "ascend") {
        return StatisticAsceCmp(a, b, orderBy, func);
    } else {
        return StatisticDescCmp(a, b, orderBy, func);
    }
}
};

namespace Dic::Module::Operator {
using namespace Dic::Server;

bool QueryOpStatisticInfoHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    OperatorStatisticInfoRequest &request = dynamic_cast<OperatorStatisticInfoRequest &>(*requestPtr);
    std::unique_ptr<OperatorStatisticInfoResponse> responsePtr = std::make_unique<OperatorStatisticInfoResponse>();
    OperatorStatisticInfoResponse &response = *responsePtr;

    bool rst = false;
    std::string errorMsg;
    if ((request.params.topK != 0) && request.params.CommonCheck(errorMsg) &&
        request.params.StatisticGroupCheck(errorMsg)) {
        rst = request.params.isCompare
            ? HandleCompareDataRequest(request, dynamic_cast<OperatorStatisticInfoResponse &>(*responsePtr))
            : HandleStatisticcDataRequest(request, dynamic_cast<OperatorStatisticInfoResponse &>(*responsePtr));
    } else {
        SetOperatorError(ErrorCode::PARAMS_ERROR);
    }
    SetBaseResponse(request, response);
    SendResponse(std::move(responsePtr), rst);
    return rst;
}

bool QueryOpStatisticInfoHandler::HandleCompareDataRequest(
    OperatorStatisticInfoRequest &request, OperatorStatisticInfoResponse &response) {
    std::string rankId = Summary::VirtualSummaryDataBase::GetFileIdFromCombinationId(request.params.rankId);
    auto database = Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId(rankId);
    std::string deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    if (deviceId.empty()) {
        SetOperatorError(ErrorCode::GET_DEVICE_ID_FAILED);
        return false;
    }
    request.params.deviceId = deviceId;
    std::vector<Protocol::OperatorStatisticInfoRes> compareRes;
    if (!database) {
        ServerLog::Warn("[Operator]Not exist compare operator database. Fail get op statistic info.");
        return true;
    }
    if (!database->QueryAllOperatorStatisticInfo(request.params, compareRes)) {
        ServerLog::Error("[Operator]Failed to query current Statistic Info by rankId.");
        SetOperatorError(ErrorCode::QUERY_ALL_STATISTIC_FAILED);
        return false;
    }
    std::string baselineId = Global::BaselineManager::Instance().GetBaselineId();
    if (baselineId == "") {
        ServerLog::Error("[Operator]Failed to get baseline id.");
        SetOperatorError(ErrorCode::GET_BASELINE_ID_FAILED);
        return false;
    }
    auto databaseBaseline = Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId(baselineId);
    std::vector<Protocol::OperatorStatisticInfoRes> baselineRes;
    request.params.deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(baselineId);
    if (!databaseBaseline) {
        ServerLog::Warn("[Operator]Not exist baseline operator database. Fail get op statistic info.");
        return true;
    }
    if (!databaseBaseline->QueryAllOperatorStatisticInfo(request.params, baselineRes)) {
        ServerLog::Error("[Operator]Failed to query baseline Statistic Info by baselineId.");
        SetOperatorError(ErrorCode::QUERY_ALL_STATISTIC_FAILED);
        return false;
    }
    std::vector<Protocol::OperatorStatisticCmpInfoRes> res;
    res = GetCmpDataVec(request.params.group, baselineRes, compareRes);
    constexpr int64_t MAX_INT64 = std::numeric_limits<int64_t>::max();
    int64_t safeSize = (res.size() > MAX_INT64) ? MAX_INT64 : static_cast<int64_t>(res.size());
    if (request.params.topK > 0) {
        response.total = std::min(request.params.topK, safeSize);
    } else {
        response.total = safeSize;
    }
    response.data = GetFixNumDiffCmpData(res, request.params, response.total);
    return true;
}

bool QueryOpStatisticInfoHandler::HandleStatisticcDataRequest(
    OperatorStatisticInfoRequest &request, OperatorStatisticInfoResponse &response) {
    std::string rankId = Summary::VirtualSummaryDataBase::GetFileIdFromCombinationId(request.params.rankId);
    auto database = Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId(rankId);
    if (!database) {
        ServerLog::Warn("[Operator]Not exist operator database. Fail get statistic info.");
        return true;
    }
    std::string deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    if (deviceId.empty()) {
        SetOperatorError(ErrorCode::GET_DEVICE_ID_FAILED);
        return false;
    }
    request.params.deviceId = deviceId;
    if (!database->QueryOperatorStatisticInfo(request.params, response)) {
        ServerLog::Error("[Operator]Failed to query Statistic Info by rankId.");
        SetOperatorError(ErrorCode::QUERY_STATISTIC_FAILED);
        return false;
    }
    GetObjFunc func = [](const StatisticCmpRes &stat) { return stat.compare; };
    std::sort(response.data.begin(), response.data.end(),
        [&request, func](OperatorStatisticCmpInfoRes &a, OperatorStatisticCmpInfoRes &b) {
            return StaticCmp(a, b, request.params.order, request.params.orderBy, func);
        });
    return true;
}

std::string QueryOpStatisticInfoHandler::GetGroup(const std::string &paramsGroup, OperatorStatisticInfoRes &data) {
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(paramsGroup);
    switch (operatorGroup) {
    case OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP:
        return data.opType + data.accCore;
    case OperatorGroupConverter::OperatorGroup::COMMUNICATION_TYPE_GROUP:
        // communication_type 计算算子类型场景 ACC_CORE固定为HCCL或COMMUNICATION，无实际意义，无需拼接
        return data.opType;
    case OperatorGroupConverter::OperatorGroup::OP_INPUT_SHAPE_GROUP:
        return data.opName + data.inputShape + data.accCore;
    default:
        return "";
    }
}

void QueryOpStatisticInfoHandler::GroupingData(const std::string &paramsGroup, OpStaticResVec &datFromDb,
    std::map<std::string, Protocol::OperatorStatisticCmpInfoRes> &groupMap, bool isBaselineData) {
    std::string group;
    for (auto &data : datFromDb) {
        group = GetGroup(paramsGroup, data);
        if (group.empty()) {
            continue;
        }
        if (isBaselineData) {
            groupMap[group].baseline = data;
        } else {
            groupMap[group].compare = data;
        }
    }
}
std::string QueryOpStatisticInfoHandler::CalDataCompare(const std::string &com, const std::string &base) {
    return NumberUtil::StringDoubleMinus(com, base);
}

void QueryOpStatisticInfoHandler::SetOpInputShapeGroupData(OperatorStatisticCmpInfoRes &data) {
    data.diff.opName = data.compare.opName.empty() ? data.baseline.opName : data.compare.opName;
    data.diff.inputShape = data.compare.inputShape.empty() ? data.baseline.inputShape : data.compare.inputShape;
    data.diff.opType = data.compare.opType + " -> " + data.compare.opType;
}

void QueryOpStatisticInfoHandler::SetOpOrHcclTypeGroupData(OperatorStatisticCmpInfoRes &data) {
    data.diff.opType = data.compare.opType.empty() ? data.baseline.opType : data.compare.opType;
    data.diff.opName = data.compare.opName + "->" + data.baseline.opName;
    data.diff.inputShape = data.compare.inputShape + "->" + data.baseline.inputShape;
}

void QueryOpStatisticInfoHandler::CalDiffDataByGroup(
    const std::string &paramsGroup, OperatorStatisticCmpInfoRes &data) {
    data.diff.accCore = data.compare.accCore.empty() ? data.baseline.accCore : data.compare.accCore;
    data.diff.totalTime = CalDataCompare(data.compare.totalTime, data.baseline.totalTime);
    data.diff.count = CalDataCompare(data.compare.count, data.baseline.count);
    data.diff.avgTime = CalDataCompare(data.compare.avgTime, data.baseline.avgTime);
    data.diff.maxTime = CalDataCompare(data.compare.maxTime, data.baseline.maxTime);
    data.diff.minTime = CalDataCompare(data.compare.minTime, data.baseline.minTime);
    OperatorGroupConverter::OperatorGroup operatorGroup = Protocol::OperatorGroupConverter::ToEnum(paramsGroup);
    switch (operatorGroup) {
    case OperatorGroupConverter::OperatorGroup::OP_TYPE_GROUP:
    case OperatorGroupConverter::OperatorGroup::COMMUNICATION_TYPE_GROUP:
        SetOpOrHcclTypeGroupData(data);
        break;
    case OperatorGroupConverter::OperatorGroup::OP_INPUT_SHAPE_GROUP:
        SetOpInputShapeGroupData(data);
        break;
    default:
        break;
    }
}

std::vector<Protocol::OperatorStatisticCmpInfoRes> QueryOpStatisticInfoHandler::GetCmpDataVec(
    std::string &group, OpStaticResVec &base, OpStaticResVec &cmp) {
    std::map<std::string, Protocol::OperatorStatisticCmpInfoRes> groupMap;
    std::vector<Protocol::OperatorStatisticCmpInfoRes> cmpRes;
    // 处理base cmp放在map里
    GroupingData(group, base, groupMap, true);
    GroupingData(group, cmp, groupMap, false);
    for (auto &CmpInfo : groupMap) {
        CalDiffDataByGroup(group, CmpInfo.second);
        cmpRes.emplace_back(CmpInfo.second);
    }
    return cmpRes;
}

std::vector<Protocol::OperatorStatisticCmpInfoRes> QueryOpStatisticInfoHandler::GetFixNumDiffCmpData(
    std::vector<Protocol::OperatorStatisticCmpInfoRes> &statisticData, Protocol::OperatorStatisticReqParams &reqParams,
    const int64_t total) {
    if (statisticData.empty()) {
        return statisticData;
    }
    std::string dbOrderBy = reqParams.orderBy;
    // 对差值排序
    const std::string order = reqParams.order;
    GetObjFunc func = [](const StatisticCmpRes &stat) { return stat.diff; };
    std::sort(statisticData.begin(), statisticData.end(),
        [&order, &dbOrderBy, func](OperatorStatisticCmpInfoRes &a, OperatorStatisticCmpInfoRes &b) {
            return StaticCmp(a, b, order, dbOrderBy, func);
        });
    uint64_t MAX_UINT64 = std::numeric_limits<uint64_t>::max();
    uint64_t safeTotal = total < 0 ? MAX_UINT64 : static_cast<uint64_t>(total);
    safeTotal = std::min(safeTotal, static_cast<uint64_t>(statisticData.size()));
    std::vector<Protocol::OperatorStatisticCmpInfoRes> topKStatisticData(
        statisticData.begin(), statisticData.begin() + safeTotal);

    // 截取需要的部分 （偏移量） 到 （偏移量 + limit - 1） pagesize 默认是10条
    uint64_t dataSize = topKStatisticData.size();
    uint64_t pageSize =
        (reqParams.pageSize <= 0 ? 10
                                 : static_cast<uint64_t>(reqParams.pageSize)); // pageSize 默认是10条，此处防止除零操作
    uint64_t offset = pageSize * static_cast<uint64_t>(reqParams.current - 1);
    if (offset >= dataSize) {
        offset = dataSize - ((dataSize % pageSize) == 0 ? pageSize : (dataSize % pageSize));
    }
    std::vector<Protocol::OperatorStatisticCmpInfoRes>::const_iterator start = topKStatisticData.begin() + offset;
    std::vector<Protocol::OperatorStatisticCmpInfoRes>::const_iterator end =
        topKStatisticData.begin() + std::min(offset + pageSize, dataSize);
    std::vector<Protocol::OperatorStatisticCmpInfoRes> result;
    result.assign(start, end);
    return result;
}
}
