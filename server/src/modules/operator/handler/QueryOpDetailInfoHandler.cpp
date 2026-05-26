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
#include <algorithm>
#include "pch.h"
#include "BaselineManager.h"
#include "DataBaseManager.h"
#include "OperatorProtocolRequest.h"
#include "OperatorProtocol.h"
#include "QueryOpDetailInfoHandler.h"

namespace {
using namespace Dic::Server;
using namespace Dic;
using DetailCmpRes = Dic::Protocol::OperatorDetailCmpInfoRes;

using DetailCmpFun = std::function<bool(const DetailCmpRes &, const DetailCmpRes &)>;
static std::unordered_map<std::string, DetailCmpFun> DetailDescCompareFunctions = {
    {"deviceId", [](const DetailCmpRes &a, const DetailCmpRes &b) { return a.diff.rankId > b.diff.rankId; }},
    {"step_id", [](const DetailCmpRes &a, const DetailCmpRes &b) { return a.diff.stepId > b.diff.stepId; }},
    {"name", [](const DetailCmpRes &a, const DetailCmpRes &b) { return a.diff.name > b.diff.name; }},
    {"type", [](const DetailCmpRes &a, const DetailCmpRes &b) { return a.diff.type > b.diff.type; }},
    {"accCore", [](const DetailCmpRes &a, const DetailCmpRes &b) { return a.diff.accCore > b.diff.accCore; }},
    {"startTime",
        [](const DetailCmpRes &a, const DetailCmpRes &b) {
            return NumberUtil::IsStr2DoubleDesc(a.diff.startTime, b.diff.startTime);
        }},
    {"duration",
        [](const DetailCmpRes &a, const DetailCmpRes &b) {
            return NumberUtil::IsStr2DoubleDesc(a.diff.duration, b.diff.duration);
        }},
    {"waitTime",
        [](const DetailCmpRes &a, const DetailCmpRes &b) {
            return NumberUtil::IsStr2DoubleDesc(a.diff.waitTime, b.diff.waitTime);
        }},
};
static std::unordered_map<std::string, DetailCmpFun> DetailAsceCompareFunctions = {
    {"deviceId", [](const DetailCmpRes &a, const DetailCmpRes &b) { return a.diff.rankId < b.diff.rankId; }},
    {"step_id", [](const DetailCmpRes &a, const DetailCmpRes &b) { return a.diff.stepId < b.diff.stepId; }},
    {"name", [](const DetailCmpRes &a, const DetailCmpRes &b) { return a.diff.name < b.diff.name; }},
    {"type", [](const DetailCmpRes &a, const DetailCmpRes &b) { return a.diff.type < b.diff.type; }},
    {"accCore", [](const DetailCmpRes &a, const DetailCmpRes &b) { return a.diff.accCore < b.diff.accCore; }},
    {"startTime",
        [](const DetailCmpRes &a, const DetailCmpRes &b) {
            return NumberUtil::IsStr2DoubleAsce(a.diff.startTime, b.diff.startTime);
        }},
    {"duration",
        [](const DetailCmpRes &a, const DetailCmpRes &b) {
            return NumberUtil::IsStr2DoubleAsce(a.diff.duration, b.diff.duration);
        }},
    {"waitTime",
        [](const DetailCmpRes &a, const DetailCmpRes &b) {
            return NumberUtil::IsStr2DoubleAsce(a.diff.waitTime, b.diff.waitTime);
        }},
};
bool DetailDescCmp(const DetailCmpRes &a, const DetailCmpRes &b, const std::string orderBy) {
    auto it = DetailDescCompareFunctions.find(orderBy);
    if (it != DetailDescCompareFunctions.end()) {
        return it->second(a, b);
    }
    return a.diff.duration > b.diff.duration;
}
bool DetailAsceCmp(const DetailCmpRes &a, const DetailCmpRes &b, const std::string orderBy) {
    auto it = DetailAsceCompareFunctions.find(orderBy);
    if (it != DetailAsceCompareFunctions.end()) {
        return it->second(a, b);
    }
    return a.diff.duration > b.diff.duration;
}
bool StaticCmp(const DetailCmpRes &a, const DetailCmpRes &b, const std::string order, const std::string orderBy) {
    if (order == "ascend") {
        return DetailAsceCmp(a, b, orderBy);
    } else {
        return DetailDescCmp(a, b, orderBy);
    }
}
};

namespace Dic::Module::Operator {
using namespace Dic::Server;

bool QueryOpDetailInfoHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    OperatorDetailInfoRequest &request = dynamic_cast<OperatorDetailInfoRequest &>(*requestPtr);
    std::unique_ptr<OperatorDetailInfoResponse> responsePtr = std::make_unique<OperatorDetailInfoResponse>();
    OperatorDetailInfoResponse &response = *responsePtr;
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        ServerLog::Error(errorMsg);
        SetOperatorError(ErrorCode::PARAMS_ERROR);
        SetBaseResponse(request, response);
        SetResponseResult(response, false);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    bool rst = request.params.isCompare
        ? HandleCompareDataRequest(request, dynamic_cast<OperatorDetailInfoResponse &>(*responsePtr))
        : HandleDetailDataRequest(request, dynamic_cast<OperatorDetailInfoResponse &>(*responsePtr));
    SetBaseResponse(request, response);
    SetResponseResult(response, rst);
    SendResponse(std::move(responsePtr), rst);
    return rst;
}

bool QueryOpDetailInfoHandler::HandleCompareDataRequest(
    OperatorDetailInfoRequest &request, OperatorDetailInfoResponse &response) {
    // topK是0，不用查询直接返回
    if (request.params.topK == 0) {
        return true;
    }
    std::string rankId = Summary::VirtualSummaryDataBase::GetFileIdFromCombinationId(request.params.rankId);
    auto database = Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId(rankId);
    std::string deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    if (deviceId.empty()) {
        return false;
    }
    request.params.deviceId = deviceId;
    std::vector<Protocol::OperatorDetailInfoRes> cmpRes;
    if (!database) {
        ServerLog::Warn("[Operator]Not exist operator database. Fail get op detail info.");
        return true;
    }
    if (!database->QueryAllOperatorDetailInfo(request.params, cmpRes, response.level)) {
        ServerLog::Error("[Operator]Failed to query current detail info by rankId.");
        SetOperatorError(ErrorCode::QUERY_ALL_DETAIL_FAILED);
        return false;
    }
    std::set<std::string> cmpPmuHeader = database->GetPmuColumns();

    std::string baselineId = Global::BaselineManager::Instance().GetBaselineId();
    if (baselineId == "") {
        ServerLog::Error("[Operator]Failed to get baseline id.");
        SetOperatorError(ErrorCode::GET_BASELINE_ID_FAILED);
        return false;
    }
    auto databaseBaseline = Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId(baselineId);
    std::vector<Protocol::OperatorDetailInfoRes> baselineRes;
    request.params.deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(baselineId);
    if (!databaseBaseline) {
        ServerLog::Warn("[Operator]Not exist baseline operator database. Fail get op detail info.");
        return true;
    }
    if (!databaseBaseline->QueryAllOperatorDetailInfo(request.params, baselineRes, response.level)) {
        ServerLog::Error("[Operator]Failed to query baseline detail Info by baselineId.");
        SetOperatorError(ErrorCode::QUERY_ALL_DETAIL_FAILED);
        return false;
    }
    std::set<std::string> basePmuHeader = databaseBaseline->GetPmuColumns();

    // 按照分组合并基线和compare数据，获取全量的比对数据
    std::vector<Protocol::OperatorDetailCmpInfoRes> fullCmpData;
    fullCmpData = GetCmpDataVec(baselineRes, cmpRes);
    constexpr int64_t MAX_INT64 = std::numeric_limits<int64_t>::max();
    const int64_t safeSize = std::min(static_cast<int64_t>(fullCmpData.size()), MAX_INT64);
    if (request.params.topK > 0) {
        response.total = std::min(request.params.topK, safeSize);
    } else {
        response.total = safeSize;
    }
    cmpPmuHeader.insert(basePmuHeader.begin(), basePmuHeader.end());
    response.pmuHeaders = cmpPmuHeader;
    // 计算比对数据的值，返回给data
    response.data = GetFixNumDiffCmpData(fullCmpData, request.params, response.total, basePmuHeader, cmpPmuHeader);
    return true;
}

bool QueryOpDetailInfoHandler::HandleDetailDataRequest(
    OperatorDetailInfoRequest &request, OperatorDetailInfoResponse &response) {
    std::string rankId = Summary::VirtualSummaryDataBase::GetFileIdFromCombinationId(request.params.rankId);
    auto database = Timeline::DataBaseManager::Instance().GetSummaryDatabaseByRankId(rankId);
    if (!database) {
        ServerLog::Warn("[Operator]Not exist operator database. Fail to get op detail info.");
        return true;
    }
    std::string deviceId = Timeline::DataBaseManager::Instance().GetDeviceIdFromRankId(rankId);
    if (deviceId.empty()) {
        SetOperatorError(ErrorCode::GET_DEVICE_ID_FAILED);
        return false;
    }
    request.params.deviceId = deviceId;
    if (!database->QueryOperatorDetailInfo(request.params, response)) {
        ServerLog::Error("[Operator]Failed to query detail Info by rankId");
        SetOperatorError(ErrorCode::QUERY_DETAIL_FAILED);
        return false;
    }
    return true;
}

void QueryOpDetailInfoHandler::SortDataBynameAndStartTime(
    std::vector<Protocol::OperatorDetailInfoRes> &baseDbData, std::vector<Protocol::OperatorDetailInfoRes> &cmpDbData) {
    sort(baseDbData.begin(), baseDbData.end(), [](OperatorDetailInfoRes &a, OperatorDetailInfoRes &b) {
        if (a.name != b.name) {
            return a.name < b.name;
        } else {
            return a.startTime < b.startTime;
        }
    });
    sort(cmpDbData.begin(), cmpDbData.end(), [](OperatorDetailInfoRes &a, OperatorDetailInfoRes &b) {
        if (a.name != b.name) {
            return a.name < b.name;
        } else {
            return a.startTime < b.startTime;
        }
    });
}

std::vector<Protocol::OperatorDetailCmpInfoRes> QueryOpDetailInfoHandler::GetCmpDataVec(
    std::vector<Protocol::OperatorDetailInfoRes> &baseDbData, std::vector<Protocol::OperatorDetailInfoRes> &cmpDbData) {
    SortDataBynameAndStartTime(baseDbData, cmpDbData);
    std::vector<Protocol::OperatorDetailInfoRes>::iterator baseIter = baseDbData.begin();
    std::vector<Protocol::OperatorDetailInfoRes>::iterator cmpIter = cmpDbData.begin();
    std::vector<Protocol::OperatorDetailCmpInfoRes> datailData;

    while (baseIter != baseDbData.end() && cmpIter != cmpDbData.end()) {
        while (baseIter != baseDbData.end() && cmpIter != cmpDbData.end() && baseIter->name == cmpIter->name) {
            OperatorDetailCmpInfoRes tmp;
            tmp.baseline = *baseIter;
            tmp.compare = *cmpIter;
            datailData.emplace_back(tmp);
            baseIter++;
            cmpIter++;
        }
        if (baseIter == baseDbData.end() || cmpIter == cmpDbData.end()) {
            break;
        }
        while (baseIter != baseDbData.end() && baseIter->name < cmpIter->name) {
            OperatorDetailCmpInfoRes tmp;
            tmp.baseline = *baseIter;
            datailData.emplace_back(tmp);
            baseIter++;
        }
        if (baseIter == baseDbData.end() || cmpIter == cmpDbData.end()) {
            break;
        }
        while (cmpIter != cmpDbData.end() && cmpIter->name < baseIter->name) {
            OperatorDetailCmpInfoRes tmp;
            tmp.compare = *cmpIter;
            datailData.emplace_back(tmp);
            cmpIter++;
        }
    }
    while (baseIter != baseDbData.end()) {
        OperatorDetailCmpInfoRes tmp;
        tmp.baseline = *baseIter;
        datailData.emplace_back(tmp);
        baseIter++;
    }
    while (cmpIter != cmpDbData.end()) {
        OperatorDetailCmpInfoRes tmp;
        tmp.compare = *cmpIter;
        datailData.emplace_back(tmp);
        cmpIter++;
    }
    return datailData;
}

std::string QueryOpDetailInfoHandler::CalPmuDataCompare(const std::string &comPmu, const std::string &basePmu) {
    // profiling给的数据不一定都是double且有可能为空，是double就计算，不是double原样呈现
    if (NumberUtil::IsDouble(comPmu) && NumberUtil::IsDouble(basePmu)) {
        return NumberUtil::StringDoubleMinus(comPmu, basePmu);
    } else {
        // 注意这里如果两个数据都是空，那么还是按照原数据呈现不要做处理，方便区分是未上报还是数据有问题
        return comPmu + "->" + basePmu;
    }
}

void QueryOpDetailInfoHandler::FromatDatailData(Protocol::OperatorDetailCmpInfoRes &data,
    const std::set<std::string> &baseDiff, const std::set<std::string> &cmpDiff,
    const std::set<std::string> intersection) {
    data.diff.rankId = data.compare.rankId.empty() ? data.baseline.rankId : data.compare.rankId;
    data.diff.stepId = data.compare.stepId.empty() ? data.baseline.stepId : data.compare.stepId;
    data.diff.name = data.compare.name.empty() ? data.baseline.name : data.compare.name;

    data.diff.type = data.compare.type + "->" + data.baseline.type;
    data.diff.accCore = data.compare.accCore + "->" + data.baseline.accCore;

    data.diff.startTime = CalPmuDataCompare(data.compare.startTime, data.baseline.startTime);
    data.diff.duration = CalPmuDataCompare(data.compare.duration, data.baseline.duration);
    data.diff.waitTime = CalPmuDataCompare(data.compare.waitTime, data.baseline.waitTime);
    data.diff.blockNum = CalPmuDataCompare(data.compare.blockNum, data.baseline.blockNum);
    data.diff.inputShape = data.compare.inputShape + "->" + data.baseline.inputShape;
    data.diff.inputType = data.compare.inputType + "->" + data.baseline.inputType;
    data.diff.inputFormat = data.compare.inputFormat + "->" + data.baseline.inputFormat;
    data.diff.outputShape = data.compare.outputShape + "->" + data.baseline.outputShape;
    data.diff.outputType = data.compare.outputType + "->" + data.baseline.outputType;
    data.diff.outputFormat = data.compare.outputFormat + "->" + data.baseline.outputFormat;

    // 先遍历比对和基线都有的数据
    for (const auto &col : intersection) {
        data.diff.pmuDatas[col] = CalPmuDataCompare(data.compare.pmuDatas[col], data.baseline.pmuDatas[col]);
    }
    // 遍历只有base有的数据
    for (const auto &col : baseDiff) {
        data.diff.pmuDatas[col] = "->" + data.baseline.pmuDatas[col];
    }
    // 遍历只有compare有的数据
    for (const auto &col : cmpDiff) {
        data.diff.pmuDatas[col] = data.compare.pmuDatas[col] + "->";
    }
}

std::vector<Protocol::OperatorDetailCmpInfoRes> QueryOpDetailInfoHandler::GetFixNumDiffCmpData(
    std::vector<Protocol::OperatorDetailCmpInfoRes> &datailData, Protocol::OperatorStatisticReqParams &reqParams,
    const int64_t total, const std::set<std::string> &basePmuHeader, const std::set<std::string> &cmpPmuHeader) {
    if (datailData.empty()) {
        return datailData;
    }
    std::set<std::string> baseDiff;
    std::set_difference(basePmuHeader.begin(), basePmuHeader.end(), cmpPmuHeader.begin(), cmpPmuHeader.end(),
        std::inserter(baseDiff, baseDiff.begin()));
    std::set<std::string> cmpDiff;
    std::set_difference(cmpPmuHeader.begin(), cmpPmuHeader.end(), basePmuHeader.begin(), basePmuHeader.end(),
        std::inserter(cmpDiff, cmpDiff.begin()));

    std::set<std::string> intersection;
    std::set_intersection(basePmuHeader.begin(), basePmuHeader.end(), cmpPmuHeader.begin(), cmpPmuHeader.end(),
        std::inserter(intersection, intersection.begin()));
    for (auto &data : datailData) {
        FromatDatailData(data, baseDiff, cmpDiff, intersection);
    }
    std::string dbOrderBy = reqParams.orderBy;
    // 对差值排序
    const std::string order = reqParams.order;
    std::sort(datailData.begin(), datailData.end(),
        [&order, &dbOrderBy](Protocol::OperatorDetailCmpInfoRes &a, Protocol::OperatorDetailCmpInfoRes &b) {
            return StaticCmp(a, b, order, dbOrderBy);
        });

    std::vector<Protocol::OperatorDetailCmpInfoRes> topKDetailData;
    topKDetailData.assign(datailData.begin(), datailData.begin() + total);

    // 截取需要的部分 （偏移量） 到 （偏移量 + limit - 1） pageSize 默认是10条，此处防止除零操作
    uint64_t dataSize = topKDetailData.size();
    uint64_t pageSize = (reqParams.pageSize <= 0 ? 10 : static_cast<uint64_t>(reqParams.pageSize));
    uint64_t offset = pageSize * static_cast<uint64_t>(reqParams.current - 1);
    if (offset >= dataSize) {
        offset = dataSize - ((dataSize % pageSize) == 0 ? pageSize : (dataSize % pageSize));
    }
    std::vector<Protocol::OperatorDetailCmpInfoRes>::const_iterator start = topKDetailData.begin() + offset;
    std::vector<Protocol::OperatorDetailCmpInfoRes>::const_iterator end =
        topKDetailData.begin() + std::min(offset + pageSize, dataSize);
    std::vector<Protocol::OperatorDetailCmpInfoRes> result;
    result.assign(start, end);
    return result;
}
}
