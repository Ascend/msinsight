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
#include "pch.h"
#include "CommunicationProtocol.h"
#include "CommunicationProtocolUtil.h"

namespace Dic {
namespace Protocol {
using namespace Dic::Server;
using namespace rapidjson;
#pragma region <<Response to json>>
template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response) {
    ServerLog::Warn("ToResponseJson is not implemented. command:", response.command);
    return std::nullopt;
}

template <> std::optional<document_t> ToResponseJson<OperatorDetailsResponse>(const OperatorDetailsResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t allOperators(kArrayType);
    for (const OperatorItem &operatorItem : response.body.allOperators) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "operatorName", operatorItem.operatorName, allocator);
        JsonUtil::AddMember(itemJson, "startTime", operatorItem.startTime, allocator);
        JsonUtil::AddMember(itemJson, "elapseTime", operatorItem.elapseTime, allocator);
        JsonUtil::AddMember(itemJson, "transitTime", operatorItem.transitTime, allocator);
        JsonUtil::AddMember(itemJson, "synchronizationTime", operatorItem.synchronizationTime, allocator);
        JsonUtil::AddMember(itemJson, "waitTime", operatorItem.waitTime, allocator);
        JsonUtil::AddMember(itemJson, "idleTime", operatorItem.idleTime, allocator);
        JsonUtil::AddMember(itemJson, "synchronizationTimeRatio", operatorItem.synchronizationTimeRatio, allocator);
        JsonUtil::AddMember(itemJson, "waitTimeRatio", operatorItem.waitTimeRatio, allocator);
        JsonUtil::AddMember(itemJson, "sdmaBw", operatorItem.sdmaBw, allocator);
        JsonUtil::AddMember(itemJson, "rdmaBw", operatorItem.rdmaBw, allocator);
        allOperators.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "currentPage", response.body.currentPage, allocator);
    JsonUtil::AddMember(body, "pageSize", response.body.pageSize, allocator);
    JsonUtil::AddMember(body, "count", response.body.count, allocator);
    JsonUtil::AddMember(body, "allOperators", allOperators, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<DistributionResponse>(const DistributionResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "distributionData", response.body.distributionData, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<BandwidthDataResponse>(const BandwidthDataResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t items(kArrayType);
    for (const BandwidthDataItem &bandwidthDataItem : response.body.items) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "transportType", bandwidthDataItem.transportType, allocator);
        JsonUtil::AddMember(itemJson, "transitSize", bandwidthDataItem.transitSize, allocator);
        JsonUtil::AddMember(itemJson, "transitTime", bandwidthDataItem.transitTime, allocator);
        JsonUtil::AddMember(itemJson, "bandwidth", bandwidthDataItem.bandwidth, allocator);
        JsonUtil::AddMember(itemJson, "largePacketRatio", bandwidthDataItem.largePacketRatio, allocator);
        items.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "items", items, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> IterOrRanksInfoToJson(
    const std::vector<IterationsOrRanksObject> &objs, Document::AllocatorType &allocator) {
    document_t iterationIdList(kArrayType);
    for (const IterationsOrRanksObject &ranksObject : objs) {
        iterationIdList.PushBack(json_t().SetString(ranksObject.iterationOrRankId.c_str(), allocator), allocator);
    }
    return std::optional<document_t>(std::move(iterationIdList));
}

template <>
std::optional<document_t> ToResponseJson<IterationsOrRanksResponse>(const IterationsOrRanksResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t iterationOrRankId(kObjectType);
    auto compareData = IterOrRanksInfoToJson(response.body.compare, allocator);
    JsonUtil::AddMember(iterationOrRankId, "compare", compareData, allocator);
    auto baselineData = IterOrRanksInfoToJson(response.body.baseline, allocator);
    JsonUtil::AddMember(iterationOrRankId, "baseline", baselineData, allocator);
    JsonUtil::AddMember(body, "iterationOrRankId", iterationOrRankId, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}
template <> std::optional<document_t> ToResponseJson<OperatorNamesResponse>(const OperatorNamesResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t operatorName(kArrayType);
    for (const OperatorNamesObject &object : response.body) {
        operatorName.PushBack(json_t().SetString(object.operatorName.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(body, "operatorName", operatorName, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<MatrixSortOpNamesResponse>(const MatrixSortOpNamesResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t operatorName(kArrayType);
    for (const OperatorNamesObject &object : response.body) {
        operatorName.PushBack(json_t().SetString(object.operatorName.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(body, "operatorName", operatorName, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> DurationDataToJson(const DurationData &durationData, Document::AllocatorType &allocator) {
    document_t res(kObjectType);
    JsonUtil::AddMember(res, "startTime", durationData.startTime, allocator);
    JsonUtil::AddMember(res, "elapseTime", durationData.elapseTime, allocator);
    JsonUtil::AddMember(res, "transitTime", durationData.transitTime, allocator);
    JsonUtil::AddMember(res, "synchronizationTime", durationData.synchronizationTime, allocator);
    JsonUtil::AddMember(res, "waitTime", durationData.waitTime, allocator);
    JsonUtil::AddMember(res, "idleTime", durationData.idleTime, allocator);
    JsonUtil::AddMember(res, "synchronizationTimeRatio", durationData.synchronizationTimeRatio, allocator);
    JsonUtil::AddMember(res, "waitTimeRatio", durationData.waitTimeRatio, allocator);
    JsonUtil::AddMember(res, "sdmaBw", durationData.sdmaBw, allocator);
    JsonUtil::AddMember(res, "rdmaBw", durationData.rdmaBw, allocator);
    return std::optional<document_t>(std::move(res));
}

template <> std::optional<document_t> ToResponseJson<DurationResponse>(const DurationResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t items(kArrayType);
    for (const Duration &duration : response.body.durationList) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "rankId", duration.rankId, allocator);
        JsonUtil::AddMember(itemJson, "dbPath", duration.dbPath, allocator);
        json_t compareData(kObjectType);
        auto compare = DurationDataToJson(duration.durationData.compare, allocator);
        JsonUtil::AddMember(compareData, "compare", compare, allocator);
        auto baseline = DurationDataToJson(duration.durationData.baseline, allocator);
        JsonUtil::AddMember(compareData, "baseline", baseline, allocator);
        auto diff = DurationDataToJson(duration.durationData.diff, allocator);
        JsonUtil::AddMember(compareData, "diff", diff, allocator);
        JsonUtil::AddMember(itemJson, "compareData", compareData, allocator);
        items.PushBack(itemJson, allocator);
    }
    json_t adviceJson(kArrayType);
    for (const auto &item : response.body.bwStatistics) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "type", item.type, allocator);
        JsonUtil::AddMember(itemJson, "max", item.maxBw, allocator);
        JsonUtil::AddMember(itemJson, "min", item.minBw, allocator);
        JsonUtil::AddMember(itemJson, "avg", item.avgBw, allocator);
        JsonUtil::AddMember(itemJson, "diff", item.diffBw, allocator);
        JsonUtil::AddMember(itemJson, "time", item.allTime, allocator);
        adviceJson.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "items", items, allocator);
    JsonUtil::AddMember(body, "advice", adviceJson, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> OpTimeListToJson(
    const std::vector<OperatorTimeItem> &opTimeList, Document::AllocatorType &allocator) {
    document_t res(kArrayType);
    for (auto item : opTimeList) {
        json_t opJson(kObjectType);
        JsonUtil::AddMember(opJson, "operatorName", item.operatorName, allocator);
        JsonUtil::AddMember(opJson, "startTime", item.startTime, allocator);
        JsonUtil::AddMember(opJson, "duration", item.elapseTime, allocator);
        res.PushBack(opJson, allocator);
    }
    return std::optional<document_t>(std::move(res));
}

template <> std::optional<document_t> ToResponseJson<OperatorListsResponse>(const OperatorListsResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t dataArray(kArrayType);
    for (size_t i = 0; i < response.body.rankLists.size(); ++i) {
        json_t oneRankJson(kObjectType);
        JsonUtil::AddMember(oneRankJson, "rankId", response.body.rankLists[i], allocator);
        JsonUtil::AddMember(oneRankJson, "dbPath", response.body.dbPathList[i], allocator);
        json_t compareOpList(kObjectType);
        auto compare = OpTimeListToJson(response.body.opLists[i].compare, allocator);
        JsonUtil::AddMember(compareOpList, "compare", compare, allocator);
        auto baseline = OpTimeListToJson(response.body.opLists[i].baseline, allocator);
        JsonUtil::AddMember(compareOpList, "baseline", baseline, allocator);
        JsonUtil::AddMember(oneRankJson, "lists", compareOpList, allocator);
        dataArray.PushBack(oneRankJson, allocator);
    }
    JsonUtil::AddMember(body, "data", dataArray, allocator);
    JsonUtil::AddMember(body, "minTime", response.body.minTime, allocator);
    JsonUtil::AddMember(body, "maxTime", response.body.maxTime, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<MatrixGroupResponse>(const MatrixGroupResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t data(kArrayType);
    for (const GroupInfo &groupInfo : response.body.groupList) {
        json_t groupJson(kObjectType);
        JsonUtil::AddMember(groupJson, "group", groupInfo.group, allocator);
        JsonUtil::AddMember(groupJson, "parallelStrategy", groupInfo.parallelStrategy, allocator);
        JsonUtil::AddMember(groupJson, "type", groupInfo.type, allocator);
        json_t groupIdHashJson(kObjectType);
        JsonUtil::AddMember(groupIdHashJson, "compare", groupInfo.groupIdHash.compare, allocator);
        JsonUtil::AddMember(groupIdHashJson, "baseline", groupInfo.groupIdHash.baseline, allocator);
        JsonUtil::AddMember(groupJson, "groupIdHash", groupIdHashJson, allocator);
        data.PushBack(groupJson, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

std::optional<document_t> MatrixDataToJson(const MatrixData &matrixData, Document::AllocatorType &allocator) {
    document_t res(kObjectType);
    JsonUtil::AddMember(res, "transportType", matrixData.transportType, allocator);
    JsonUtil::AddMember(res, "transitSize", matrixData.transitSize, allocator);
    JsonUtil::AddMember(res, "transitTime", matrixData.transitTime, allocator);
    JsonUtil::AddMember(res, "bandwidth", matrixData.bandwidth, allocator);
    JsonUtil::AddMember(res, "opName", matrixData.opName, allocator);
    return std::optional<document_t>(std::move(res));
}

template <> std::optional<document_t> ToResponseJson<MatrixListResponse>(const MatrixListResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t matrixList(kArrayType);
    for (const MatrixList &matrix : response.body.matrixList) {
        json_t itemJson(kObjectType);
        JsonUtil::AddMember(itemJson, "srcRank", matrix.srcRank, allocator);
        JsonUtil::AddMember(itemJson, "dstRank", matrix.dstRank, allocator);
        json_t compareMatrixData(kObjectType);
        auto compare = MatrixDataToJson(matrix.matrixData.compare, allocator);
        JsonUtil::AddMember(compareMatrixData, "compare", compare, allocator);
        auto baseline = MatrixDataToJson(matrix.matrixData.baseline, allocator);
        JsonUtil::AddMember(compareMatrixData, "baseline", baseline, allocator);
        auto diff = MatrixDataToJson(matrix.matrixData.diff, allocator);
        JsonUtil::AddMember(compareMatrixData, "diff", diff, allocator);
        JsonUtil::AddMember(itemJson, "matrixData", compareMatrixData, allocator);
        matrixList.PushBack(itemJson, allocator);
    }
    JsonUtil::AddMember(body, "matrixList", matrixList, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<CommunicationAdvisorResponse>(const CommunicationAdvisorResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    json_t items(kArrayType);
    for (const auto &item : response.body.items) {
        json_t singleAdvisor(kObjectType);
        JsonUtil::AddMember(singleAdvisor, "name", item.name, allocator);
        json_t statistics(kObjectType);
        for (const auto &column : item.statistics) {
            json_t values(kArrayType);
            std::string_view columnName = column.first;
            for (const auto &columnValue : column.second) {
                values.PushBack(json_t().SetString(columnValue.c_str(), columnValue.length(), allocator), allocator);
            }
            JsonUtil::AddMember(statistics, columnName, values, allocator);
        }
        JsonUtil::AddMember(singleAdvisor, "statistics", statistics, allocator);
        items.PushBack(singleAdvisor, allocator);
    }
    JsonUtil::AddMember(body, "items", items, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<CommunicationSlowRankAnalysisResponse>(
    const CommunicationSlowRankAnalysisResponse &response) {
    document_t json(kObjectType);
    auto &allocator = json.GetAllocator();
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    json_t body(kObjectType);
    JsonUtil::AddMember(body, "hasAdvice", response.body.hasAdvice, allocator);
    JsonUtil::AddMember(body, "fastRankId", response.body.fastRankId, allocator);
    JsonUtil::AddMember(body, "fastTotalElapseTime", response.body.fastTotalElapseTime, allocator);
    json_t data(kArrayType);
    for (const auto &slowRank : response.body.slowRankList) {
        json_t slowRankDetail(kObjectType);
        JsonUtil::AddMember(slowRankDetail, "rankId", slowRank.rankId, allocator);
        JsonUtil::AddMember(slowRankDetail, "totalDiffTime", slowRank.totalDiffTime, allocator);
        JsonUtil::AddMember(slowRankDetail, "totalElapseTime", slowRank.totalElapseTime, allocator);
        json_t opList(kArrayType);
        for (const auto &op : slowRank.opDetails) {
            json_t opDetail(kObjectType);
            JsonUtil::AddMember(opDetail, "name", op.name, allocator);
            JsonUtil::AddMember(opDetail, "startTime", op.startTime, allocator);
            JsonUtil::AddMember(opDetail, "diffTime", op.diffTime, allocator);
            JsonUtil::AddMember(opDetail, "elapseTime", op.elapseTime, allocator);
            JsonUtil::AddMember(opDetail, "maxTime", op.maxElapseTime, allocator);
            JsonUtil::AddMember(opDetail, "maxStartTime", op.maxStartTime, allocator);
            opList.PushBack(opDetail, allocator);
        }
        JsonUtil::AddMember(slowRankDetail, "opList", opList, allocator);
        data.PushBack(slowRankDetail, allocator);
    }
    JsonUtil::AddMember(body, "data", data, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

#pragma endregion
} // end of namespace Protocol
} // end of namespace Dic
