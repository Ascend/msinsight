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
#include "NumberSafeUtil.h"
#include "AdvisorProcessUtil.h"
#include "DataBaseManager.h"
#include "TraceTime.h"
#include "AffinityAPIAdvisor.h"
#include "AdvisorErrorManager.h"

namespace Dic::Module::Advisor {
using namespace Dic::Server;
bool AffinityAPIAdvisor::Process(const Protocol::APITypeParams &params, Protocol::AffinityAPIResBody &resBody) {
    auto database = Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId(params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection for Affinity API query. fileId:", params.rankId);
        SetAdvisorError(ErrorCode::CONNECT_DATABASE_FAILED);
        return false;
    }
    std::vector<Protocol::FlowLocation> results = GetFlowLocationData(params);
    uint64_t start = params.pageSize * (params.currentPage - 1);
    for (uint64_t i = start; i < start + params.pageSize && i < results.size(); ++i) {
        auto item = results[i];
        Protocol::AffinityAPIData one{};
        one.name = item.name;
        one.baseInfo.id = item.id;
        one.baseInfo.rankId = params.rankId;
        one.baseInfo.pid = item.pid;
        one.baseInfo.tid = item.tid;
        one.baseInfo.startTime = item.timestamp;
        if (item.duration < item.timestamp) {
            ServerLog::Error("The original data seems to have an issue, as the end time is smaller than the timestamp."
                             "Please check the rationality of the data.");
            SetAdvisorError(ErrorCode::DATA_ANOMALY_END_TIME_SMALLER_TIMESTAMP);
            return false;
        }
        one.baseInfo.duration = item.duration - item.timestamp; // duration里存储的是end time，前端需要的是ns
        one.baseInfo.depth = item.depth;
        one.originAPI = item.type;
        one.replaceAPI = item.metaType;
        one.note = item.deviceId;
        resBody.data.emplace_back(one);
    }
    resBody.dbPath = database->GetDbPath();
    resBody.size = results.size();
    return true;
}

std::vector<Protocol::FlowLocation> AffinityAPIAdvisor::GetFlowLocationData(const Protocol::APITypeParams &params) {
    std::vector<Protocol::FlowLocation> results;
    auto database = Timeline::DataBaseManager::Instance().GetTraceDatabaseByRankId(params.rankId);
    if (database == nullptr) {
        ServerLog::Error("Failed to get connection for Affinity API query. fileId:", params.rankId);
        SetAdvisorError(ErrorCode::CONNECT_DATABASE_FAILED);
        return results;
    }
    Protocol::KernelDetailsParams param = {.orderBy = params.orderBy,
        .order = params.orderType,
        .current = params.currentPage,
        .pageSize = params.pageSize,
        .startTime = params.startTime,
        .endTime = params.endTime};
    param.order = params.orderType == "ascend" ? "ASC" : "DESC";
    if (std::count(AFFINITY_API_ORDER_BY_NAME_LIST.begin(), AFFINITY_API_ORDER_BY_NAME_LIST.end(), params.orderBy) ==
        0) {
        param.orderBy = "duration";
    }
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    std::map<uint64_t, std::vector<Protocol::FlowLocation>> dataMap{};
    std::map<uint64_t, std::vector<uint32_t>> indexMap{};
    if (!database->QueryAffinityAPIData(param, GetFirstApiList(AFFINITY_API_RULE), startTime, dataMap, indexMap)) {
        ServerLog::Error("Failed to Query Affinity API from database.");
        SetAdvisorError(ErrorCode::QUERY_AFFINITY_API_FAILED);
        return results;
    }
    for (const auto &it : dataMap) { // 获取某个泳道的数据
        uint64_t trackId = it.first;
        std::vector<Protocol::FlowLocation> data = it.second;
        std::vector<uint32_t> indexList = indexMap[trackId];
        FilterAffinityApiData(params, data, indexList, results);
    }
    AdvisorProcessUtil::SortFlowLocationData(results, param);
    return results;
}

std::set<std::string> AffinityAPIAdvisor::GetFirstApiList(const std::vector<AffinityApiData> &affinityApiData) {
    std::set<std::string> apiList{};
    if (affinityApiData.empty()) {
        return apiList;
    }
    for (const auto &item : affinityApiData) {
        if (item.apiList.empty()) {
            continue;
        }
        std::vector list = StringUtil::Split(item.apiList[0], "\\|"); // 按"|"分割api
        for (const auto &one : list) {
            apiList.insert(one);
        }
    }
    return apiList;
}

// 给定一个API，过滤所有rule.apiList[0]中包含给定API
std::vector<uint32_t> AffinityAPIAdvisor::FilterPossibleRules(const std::string &name) {
    std::vector<uint32_t> possible{};
    if (name.empty()) {
        return possible;
    }
    for (uint32_t i = 0; i < AFFINITY_API_RULE.size(); ++i) {
        std::vector<std::string> list = StringUtil::Split(AFFINITY_API_RULE[i].apiList[0], "\\|");
        if (std::find(list.begin(), list.end(), name) != list.end()) {
            possible.emplace_back(i);
        }
    }
    return possible;
}

// 匹配连续的api
void AffinityAPIAdvisor::FilterAffinityApiData(const Protocol::APITypeParams &params,
    std::vector<Protocol::FlowLocation> &dataList, const std::vector<uint32_t> &indexList,
    std::vector<Protocol::FlowLocation> &result) {
    if (dataList.empty() || indexList.empty()) {
        return;
    }
    auto dataListSize = dataList.size();
    auto ruleSize = AFFINITY_API_RULE.size();
    for (auto index : indexList) { // 遍历索引
        if (index >= dataListSize) {
            break;
        }
        std::vector<uint32_t> possible = FilterPossibleRules(dataList[index].name); // 先过滤可能的rule，以提高效率
        if (possible.empty()) {
            continue;
        }
        for (auto ruleIndex : possible) {
            if (ruleIndex >= ruleSize) {
                break;
            }
            auto rule = AFFINITY_API_RULE[ruleIndex];
            Protocol::AffinityAPIData one{};
            if (!CheckApiSeqWithRule(rule.apiList, dataList, index)) {
                continue;
            }
            dataList[index].type = StringUtil::join(rule.apiList, ", ");
            dataList[index].metaType = rule.affinityApi;
            dataList[index].deviceId = rule.note;
            result.emplace_back(dataList[index]);
        }
    }
}

// 给定匹配条件，检查api序列是否匹配相关条件
bool AffinityAPIAdvisor::CheckApiSeqWithRule(
    const std::vector<std::string> &rule, const std::vector<Protocol::FlowLocation> &dataList, uint32_t index) {
    std::string name = dataList[index].name;
    std::vector<std::string> list0 = StringUtil::Split(rule[0], "\\|");
    if (std::find(list0.begin(), list0.end(), name) == list0.end()) {
        return false; // 匹配rule中第一个API，不匹配规则时跳过
    }
    if (index >= NumberSafe::Sub(dataList.size(), rule.size())) {
        return false; // 真实数据长度 < 预期数据长度，无法匹配
    }

    for (size_t i = 1; i < rule.size(); ++i) { // 上文已匹配索引为0的数据
        std::string tmp = dataList[index + i].name;
        std::vector<std::string> list = StringUtil::Split(rule[i], "\\|");
        if (std::find(list.begin(), list.end(), tmp) == list.end()) { // 不完全匹配，则跳过
            return false;
        }
    }

    return true;
}
} // Dic::Module::Advisor
