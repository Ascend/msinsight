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

#include "DbTraceDataBase.h"
#include "TrackInfoManager.h"
#include "TraceDatabaseHelper.h"
#include "CounterEventHelper.h"
#include "HardWareRepo.h"

namespace Dic::Module::FullDb {
using namespace Server;

std::vector<std::string> DbTraceDataBase::GetIdListByFuzzNameFromCache(
    const std::string &path, const std::string &fuzzName, const bool caseSensitive) {
    if (stringsCache.count(path) == 0) {
        return {};
    }
    std::vector<std::string> res;
    for (const auto &item : stringsCache.at(path)) {
        if (caseSensitive) {
            if (StringUtil::Contains(item.second, fuzzName)) {
                res.push_back(item.first);
            }
            continue;
        }
        if (StringUtil::ContainsIgnoreCase(item.second, fuzzName)) {
            res.push_back(item.first);
        }
    }
    return res;
}

bool DbTraceDataBase::QueryUnitCounter(
    Protocol::UnitCounterParams &params, uint64_t minTimestamp, std::vector<Protocol::UnitCounterData> &dataList) {
    auto stmt = CreatPreparedStatement();
    if (stmt == nullptr) {
        ServerLog::Error("Query_unit_counter. Failed to prepare sql.", sqlite3_errmsg(db));
        return false;
    }
    std::unique_ptr<SqliteResultSet> resultSet;
    const std::vector<PROCESS_TYPE> hostCounterEvents = {PROCESS_TYPE::CPU_USAGE, PROCESS_TYPE::HOST_DISK_USAGE,
        PROCESS_TYPE::HOST_MEM_USAGE, PROCESS_TYPE::HOST_NETWORK_USAGE};
    const std::vector<PROCESS_TYPE> deviceCounterEvents = {PROCESS_TYPE::AI_CORE, PROCESS_TYPE::ACC_PMU,
        PROCESS_TYPE::DDR, PROCESS_TYPE::STARS_SOC, PROCESS_TYPE::NPU_MEM, PROCESS_TYPE::HBM, PROCESS_TYPE::LLC,
        PROCESS_TYPE::SAMPLE_PMU, PROCESS_TYPE::NIC, PROCESS_TYPE::ROCE, PROCESS_TYPE::NETDEV_STATS, PROCESS_TYPE::PCIE,
        PROCESS_TYPE::HCCS, PROCESS_TYPE::QOS};
    if (std::find(hostCounterEvents.begin(), hostCounterEvents.end(),
            Timeline::TraceDatabaseHelper::GetProcessType(params.metaType)) != hostCounterEvents.end()) {
        try {
            resultSet = TraceDatabaseHelper::QueryHostUnitCounter(stmt, params, minTimestamp);
        } catch (DatabaseException &e) {
            ServerLog::Error("Query host unit counter failed, ", e.What());
            return false;
        }
    } else if (std::find(deviceCounterEvents.begin(), deviceCounterEvents.end(),
                   Timeline::TraceDatabaseHelper::GetProcessType(params.metaType)) != deviceCounterEvents.end()) {
        try {
            resultSet =
                TraceDatabaseHelper::QueryDeviceUnitCounter(stmt, params, minTimestamp, GetDeviceId(params.rankId));
        } catch (DatabaseException &e) {
            ServerLog::Error("Query device unit counter failed, ", e.What());
            return false;
        }
    } else {
        ServerLog::Error("Counter event type % is not supported.", params.metaType);
        return false;
    }
    std::string curArgs;
    UnitCounterData unitCounterData;
    while (resultSet->Next()) {
        unitCounterData.timestamp = resultSet->GetUint64("startTime");
        unitCounterData.valueJsonStr = resultSet->GetString("args");
        if (unitCounterData.valueJsonStr != curArgs) {
            dataList.emplace_back(unitCounterData);
            curArgs = unitCounterData.valueJsonStr;
        }
    }
    if (!dataList.empty()) {
        dataList = DownSampleUnitCounterData(dataList, counterSampleSize);
        dataList.emplace_back(unitCounterData);
    }
    return true;
}

void DbTraceDataBase::ProcessHostCounterEventsMetadata(
    const std::string &fileId, std::vector<std::unique_ptr<Protocol::UnitTrack>> &metaData) {
    CounterEventHelper helper;
    helper.RegisterHostMap();
    for (const auto &element : helper.hostCounterEventMap) {
        std::string progressName = element.second.processName;
        std::string metaDataSQL = helper.GenerateHostMetadataSQL(element.first);
        auto counter =
            GenerateBaseUnitTrack("label", fileId, progressName, progressName, ENUM_TO_STR(element.first).value());
        auto stmt = CreatPreparedStatement(metaDataSQL);
        if (!stmt) {
            ServerLog::Error(
                "Failed to query host counter metadata. metaType is %", ENUM_TO_STR(element.first).value());
            continue;
        }
        auto resultSet = stmt->ExecuteQuery();
        if (!resultSet) {
            ServerLog::Error("Failed to get result set of querying counter metadata. metaType is %.",
                ENUM_TO_STR(element.first).value());
            continue;
        }
        while (resultSet->Next()) {
            auto thread = GenerateBaseUnitTrack(
                "counter", fileId, progressName, progressName, ENUM_TO_STR(element.first).value());
            thread->metaData.threadId = resultSet->GetString("name");
            thread->metaData.threadName = thread->metaData.threadId;
            thread->metaData.dataType = StringUtil::Split(resultSet->GetString("types"), ",");
            counter->children.emplace_back(std::move(thread));
        }
        if (!counter->children.empty()) {
            GetOrCreateCpuMetricsUnitTrack(fileId, metaData)->children.emplace_back(std::move(counter));
        }
    }
}

void DbTraceDataBase::FillFlowDepth(const Protocol::UnitFlowsParams &requestParams, FlowLocation &location,
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint32_t>> &trackIdDepthCache) {
    SliceCacheManager &sliceCacheManager = SliceCacheManager::Instance();
    uint64_t trackId = TrackInfoManager::Instance().GetTrackId(requestParams.rankId, location.pid, location.tid);
    auto item = trackIdDepthCache.find(trackId);
    if (item != trackIdDepthCache.end()) {
        location.depth = item->second[NumberUtil::StringToLongLong(location.id)];
    } else {
        std::unordered_map<uint64_t, uint32_t> depthCache;
        sliceCacheManager.QueryDepthInfoWithoutTimeRange(std::to_string(trackId), requestParams.rankId, depthCache);
        trackIdDepthCache[trackId] = depthCache;
        location.depth = depthCache[NumberUtil::StringToLongLong(location.id)];
    }
}

std::vector<FlowLocation> DbTraceDataBase::ExecuteQueryUnitFlowsForTable(const Protocol::UnitFlowsParams &requestParams,
    const std::pair<std::string, std::string> &tableAndSql, uint64_t minTimestamp, const std::string &connectionId,
    const std::vector<uint64_t> &deviceIdList) {
    auto stmt = CreatPreparedStatement();
    std::string sql =
        "with constValue as (select ? as minTime, ? as connectionId) " + tableAndSql.second + " order by startTime ";
    std::unique_ptr<SqliteResultSet> resultSet;
    if (tableAndSql.first == "COMMUNICATION_OP" && deviceIdList.size() == 1) {
        resultSet = TraceDatabaseHelper::ExecuteQuery(stmt, sql, minTimestamp, connectionId, deviceIdList[0]);
    } else {
        resultSet = TraceDatabaseHelper::ExecuteQuery(stmt, sql, minTimestamp, connectionId);
    }

    std::vector<FlowLocation> flowLocations;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint32_t>> trackIdDepthCache;
    while (resultSet->Next()) {
        auto metaType = resultSet->GetString("metaType");
        auto rankId = resultSet->GetString("deviceId");
        for (const auto &item : QueryRankIdAndDeviceMap()) {
            if (rankId == item.second) {
                rankId = item.first;
            }
        }
        rankId = rankId.empty() ? path : QueryHostInfo() + rankId;
        FlowLocation location{.tid = resultSet->GetString("tid"),
            .id = resultSet->GetString("id"),
            .metaType = metaType,
            .rankId = rankId,
            .depth = 0,
            .timestamp = resultSet->GetUint64("startTime"),
            .duration = resultSet->GetUint64("duration"),
            .pid = resultSet->GetString("pid"),
            .name = stringsCache.at(path)[resultSet->GetString("name")]};
        FillFlowDepth(requestParams, location, trackIdDepthCache);
        if (tableAndSql.first == "TASK") {
            std::string domainId = resultSet->GetString("domainId");
            if (!domainId.empty()) {
                std::string streamId = resultSet->GetString("tid");
                location.tid = streamId + "_" + domainId;
            }
        }
        flowLocations.push_back(location);
    }
    for (auto &item : flowLocations) {
        if (item.rankId == path) {
            item.rankId = requestParams.rankId;
        }
    }
    return flowLocations;
}

// 先查找是否有MSTX类型连线，有的话一定是MSTX类型连线
// 然后查找TASK/COMMUNICATION_OP是否有该连线，如果有就是torch-cann-task-communication类型连线
// 否则就是async_task_queue fwd_bwd类型连线
bool DbTraceDataBase::QueryUnitFlows(const Protocol::UnitFlowsParams &requestParams,
    Protocol::UnitFlowsBody &responseBody, uint64_t minTimestamp, uint64_t trackId) {
    auto stmt = CreatPreparedStatement();
    auto connectionId = TraceDatabaseHelper::QueryConnectionId(stmt, requestParams);
    // connectionId为-1和UINT32_MAX都表示无效值，无连线，因此不能继续搜索
    if (!connectionId.has_value() || connectionId.value() == "-1" ||
        connectionId.value() == std::to_string(UINT32_MAX)) {
        ServerLog::Warn("Connection id of current operator is null or invalid value.");
        return false;
    }

    std::vector<uint64_t> deviceIdList = TraceDatabaseHelper::GetDeviceIdList(requestParams.rankId);
    std::string comSql = deviceIdList.size() == 1 ? COM_OP_UNIT_FLOW_SQL_UNIQUE_DEVICE : COM_OP_UNIT_FLOW_SQL;
    std::vector<FlowLocation> pytorchFlowLocationList = ExecuteQueryUnitFlowsForTable(
        requestParams, {"PYTORCH_API", PYTORCH_UNIT_FLOW_SQL}, minTimestamp, connectionId.value(), deviceIdList);
    std::vector<FlowLocation> cannFlowLocationList = ExecuteQueryUnitFlowsForTable(
        requestParams, {"CANN_API", CANN_UNIT_FLOW_SQL}, minTimestamp, connectionId.value(), deviceIdList);
    std::vector<FlowLocation> taskFlowLocationList = ExecuteQueryUnitFlowsForTable(
        requestParams, {"TASK", TASK_UNIT_FLOW_SQL}, minTimestamp, connectionId.value(), deviceIdList);
    UpdateAscendHardwareFlowLocationName(requestParams.rankId, taskFlowLocationList);
    std::vector<FlowLocation> communicationOpFlowLocationList = ExecuteQueryUnitFlowsForTable(
        requestParams, {"COMMUNICATION_OP", comSql}, minTimestamp, connectionId.value(), deviceIdList);
    std::vector<FlowLocation> mstxFlowLocationList = ExecuteQueryUnitFlowsForTable(
        requestParams, {"MSTX_EVENTS", MSTX_UNIT_FLOW_SQL}, minTimestamp, connectionId.value(), deviceIdList);

    if (AssembleUnitFlowOfTypeMSTX(mstxFlowLocationList, taskFlowLocationList, connectionId.value(), responseBody)) {
        return true;
    }
    if (AssembleUnitFlowOfTypePyTorchToCANNToAscendHardwareToCommunication(pytorchFlowLocationList,
            cannFlowLocationList, taskFlowLocationList, communicationOpFlowLocationList, connectionId.value(),
            responseBody)) {
        return true;
    }
    if (AssembleUnitFlowOfTypeAsyncTaskQueue(pytorchFlowLocationList, connectionId.value(), responseBody)) {
        return true;
    }
    if (AssembleUnitFlowOfTypeFwdBwd(pytorchFlowLocationList, connectionId.value(), responseBody)) {
        return true;
    }
    ServerLog::Warn("Failed to find relevant flow event for current operator.");
    return false;
}

bool DbTraceDataBase::AssembleUnitFlowOfTypeMSTX(const std::vector<FlowLocation> &mstxFlowLocationList,
    const std::vector<FlowLocation> &taskFlowLocationList, const std::string &connectionId,
    Protocol::UnitFlowsBody &responseBody) {
    if (mstxFlowLocationList.empty() || taskFlowLocationList.empty()) {
        return false;
    }
    // 要求rankId相等是为了解决PyTorch多device场景如下问题：
    // 某个Host侧算子只会和特定deviceId（比如0）的Device侧算子连线
    // 但是对于其它deviceId（比如1），QueryUnitFlows()的SQL也会查询出deviceId=0的该算子，导致界面上deviceId=1也显示连线，但连线末端无算子
    if (mstxFlowLocationList[0].rankId != taskFlowLocationList[0].rankId) {
        return false;
    }
    // MSTX类型连线只允许一对一
    UnitSingleFlow singleFlow{
        .cat = "MsTx", .id = connectionId, .from = mstxFlowLocationList[0], .to = taskFlowLocationList[0]};
    std::vector<UnitSingleFlow> flows{singleFlow};
    responseBody.unitAllFlows.push_back({.cat = singleFlow.cat, .flows = flows});
    return true;
}

bool DbTraceDataBase::AssembleUnitFlowOfTypePyTorchToCANNToAscendHardwareToCommunication(
    const std::vector<FlowLocation> &pytorchFlowLocationList, const std::vector<FlowLocation> &cannFlowLocationList,
    const std::vector<FlowLocation> &taskFlowLocationList,
    const std::vector<FlowLocation> &communicationOpFlowLocationList, const std::string &connectionId,
    Protocol::UnitFlowsBody &responseBody) {
    if (taskFlowLocationList.empty() && communicationOpFlowLocationList.empty()) {
        return false;
    }
    if (pytorchFlowLocationList.empty() && cannFlowLocationList.empty()) {
        return false;
    }
    if (!cannFlowLocationList.empty()) {
        // HostToDevice类型连线每个泳道只取第一个算子，避免出现aclgraph等场景TASK表同connectionId的算子过多导致返回前端连线过多，界面卡死的问题
        // 要求rankId相等是为了解决PyTorch多device场景如下问题：
        // 某个Host侧算子只会和特定deviceId（比如0）的Device侧算子连线
        // 但是对于其它deviceId（比如1），QueryUnitFlows()的SQL也会查询出deviceId=0的该算子，导致界面上deviceId=1也显示连线，但连线末端无算子
        std::vector<UnitSingleFlow> flows;
        if (!taskFlowLocationList.empty() && cannFlowLocationList[0].rankId == taskFlowLocationList[0].rankId) {
            UnitSingleFlow singleFlow{.cat = "HostToDevice",
                .id = connectionId,
                .from = cannFlowLocationList[0],
                .to = taskFlowLocationList[0]};
            flows.push_back(singleFlow);
        }
        if (!communicationOpFlowLocationList.empty() &&
            cannFlowLocationList[0].rankId == communicationOpFlowLocationList[0].rankId) {
            UnitSingleFlow singleFlow{.cat = "HostToDevice",
                .id = connectionId,
                .from = cannFlowLocationList[0],
                .to = communicationOpFlowLocationList[0]};
            flows.push_back(singleFlow);
        }
        if (!flows.empty()) {
            responseBody.unitAllFlows.push_back({.cat = "HostToDevice", .flows = flows});
        }
    }
    if (!pytorchFlowLocationList.empty()) {
        // async_npu类型连线每个泳道只取第一个算子，避免出现aclgraph等场景TASK表同connectionId的算子过多导致返回前端连线过多，界面卡死的问题
        // 要求rankId相等是为了解决PyTorch多device场景如下问题：
        // 某个Host侧算子只会和特定deviceId（比如0）的Device侧算子连线
        // 但是对于其它deviceId（比如1），QueryUnitFlows()的SQL也会查询出deviceId=0的该算子，导致界面上deviceId=1也显示连线，但连线末端无算子
        std::vector<UnitSingleFlow> flows;
        if (!taskFlowLocationList.empty() && pytorchFlowLocationList[0].rankId == taskFlowLocationList[0].rankId) {
            UnitSingleFlow singleFlow{.cat = "async_npu",
                .id = connectionId,
                .from = pytorchFlowLocationList[0],
                .to = taskFlowLocationList[0]};
            flows.push_back(singleFlow);
        }
        if (!communicationOpFlowLocationList.empty() &&
            pytorchFlowLocationList[0].rankId == communicationOpFlowLocationList[0].rankId) {
            UnitSingleFlow singleFlow{.cat = "async_npu",
                .id = connectionId,
                .from = pytorchFlowLocationList[0],
                .to = communicationOpFlowLocationList[0]};
            flows.push_back(singleFlow);
        }
        if (!flows.empty()) {
            responseBody.unitAllFlows.push_back({.cat = "async_npu", .flows = flows});
        }
    }
    if (responseBody.unitAllFlows.empty()) {
        return false;
    }
    return true;
}

bool DbTraceDataBase::AssembleUnitFlowOfTypeAsyncTaskQueue(const std::vector<FlowLocation> &pytorchFlowLocationList,
    const std::string &connectionId, Protocol::UnitFlowsBody &responseBody) {
    // async_task_queue是PyTorch和PyTorch连线，要求PyTorch同connectionId算子至少有2个
    if (pytorchFlowLocationList.size() < 2) {
        return false;
    }
    // async_task_queue类型连线只允许一对一
    if (!StringUtil::StartWith(pytorchFlowLocationList[0].name, "Enqueue")) {
        return false;
    }
    UnitSingleFlow singleFlow{.cat = "async_task_queue",
        .id = connectionId,
        .from = pytorchFlowLocationList[0],
        .to = pytorchFlowLocationList[1]};
    std::vector<UnitSingleFlow> flows{singleFlow};
    responseBody.unitAllFlows.push_back({.cat = singleFlow.cat, .flows = flows});
    return true;
}

bool DbTraceDataBase::AssembleUnitFlowOfTypeFwdBwd(const std::vector<FlowLocation> &pytorchFlowLocationList,
    const std::string &connectionId, Protocol::UnitFlowsBody &responseBody) {
    // async_task_queue是PyTorch和PyTorch连线，要求PyTorch同connectionId算子至少有2个
    if (pytorchFlowLocationList.size() < 2) {
        return false;
    }
    // fwdbwd类型连线只允许一对一
    UnitSingleFlow singleFlow{
        .cat = "fwdbwd", .id = connectionId, .from = pytorchFlowLocationList[0], .to = pytorchFlowLocationList[1]};
    std::vector<UnitSingleFlow> flows{singleFlow};
    responseBody.unitAllFlows.push_back({.cat = singleFlow.cat, .flows = flows});
    return true;
}

void DbTraceDataBase::UpdateAscendHardwareFlowLocationName(
    const std::string &rankId, std::vector<FlowLocation> &flowLocations) {
    // 搜索 name
    const std::vector<uint64_t> ids = std::accumulate(flowLocations.begin(), flowLocations.end(),
        std::vector<uint64_t>(), [](std::vector<uint64_t> &acc, const FlowLocation &item) {
            if (item.metaType == "Ascend Hardware") {
                acc.push_back(StringUtil::StringToUint32(item.id));
            }
            return acc;
        });
    if (ids.empty()) {
        return;
    }
    const std::shared_ptr<IBaseSliceRepo> hardwareRepo = std::make_unique<HardWareRepo>();
    SliceQuery sliceQuery;
    sliceQuery.rankId = rankId;
    std::vector<CompeteSliceDomain> competeSliceVec;
    hardwareRepo->QueryCompeteSliceByIds(sliceQuery, ids, competeSliceVec);
    // 创建 id 到 name 的映射
    std::unordered_map<std::string, std::string> nameMap;
    for (const auto &item : competeSliceVec) {
        nameMap[std::to_string(item.id)] = item.name;
    }

    // 更新 flowLocation 中的 name
    std::for_each(flowLocations.begin(), flowLocations.end(), [nameMap](FlowLocation &flow) {
        if (const auto it = nameMap.find(flow.id); it != nameMap.end()) {
            flow.name = it->second;
        }
    });
}

std::string DbTraceDataBase::GetDeviceIdFromMemoryTable() {
    auto queryDevice = [this](const std::string &table) -> std::string {
        sqlite3_stmt *stmt = nullptr;
        std::string sql = "SELECT deviceId FROM " + table + " limit 1";
        int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
        if (result != SQLITE_OK) {
            sqlite3_finalize(stmt);
            return "";
        }
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            std::string deviceId = sqlite3_column_string(stmt, resultStartIndex);
            sqlite3_finalize(stmt);
            return deviceId;
        }
        sqlite3_finalize(stmt);
        return "";
    };
    if (CheckTableExist(TABLE_MEMORY_RECORD)) {
        return queryDevice(TABLE_MEMORY_RECORD);
    }
    return "";
}

void DbTraceDataBase::QueryDeviceIdInStepTraceTime(std::set<std::string> &deviceIds) {
    std::string sql = "SELECT DISTINCT deviceId from StepTraceTime";
    sqlite3_stmt *stmt = nullptr;
    int ret = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
    if (ret != SQLITE_OK) {
        ServerLog::Error("Failed to prepare sql for query deviceId in StepTraceTime. Error: ", sqlite3_errmsg(db));
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int col = resultStartIndex;
        std::string deviceId = sqlite3_column_string(stmt, col++);
        deviceIds.insert(deviceId);
    }
    sqlite3_finalize(stmt);
}

bool DbTraceDataBase::QueryThreadSameOperatorsDetails(const Protocol::UnitThreadsOperatorsParams &requestParams,
    Protocol::UnitThreadsOperatorsBody &responseBody, uint64_t minTimestamp, const std::vector<uint64_t> &trackIdList) {
    std::vector<std::string> pidList;
    std::vector<std::string> tidList;
    for (const auto &trackId : trackIdList) {
        TrackInfo trackInfo;
        TrackInfoManager::Instance().GetTrackInfo(trackId, trackInfo, requestParams.rankId);
        if (!StringUtil::CheckSqlValid(trackInfo.threadId)) {
            ServerLog::Error("There is an SQL injection attack in track id. Error param: % ", trackInfo.threadId);
            return false;
        }
        pidList.emplace_back(trackInfo.processId);
        tidList.emplace_back(trackInfo.threadId);
    }
    std::string orderByAndPage =
        " ORDER BY " + requestParams.orderBy + (requestParams.order == "descend" ? " DESC " : " ASC ");
    auto stmt = CreatPreparedStatement();
    std::unique_ptr<SqliteResultSet> resultSet;
    try {
        resultSet = TraceDatabaseHelper::QueryThreadSameOperatorsDetails(stmt, requestParams,
            {GetDeviceId(requestParams.rankId), minTimestamp, orderByAndPage, pidList, tidList,
                requestParams.isPythonStack, CheckColumnExist(TABLE_API, "type")});
    } catch (DatabaseException &e) {
        ServerLog::Error("Query thread same operators details fail, ", e.What());
        return false;
    }
    if (resultSet == nullptr) {
        return false;
    }
    ExecuteQueryDbThreadSameOperatorsDetails(resultSet, requestParams, responseBody, tidList);
    responseBody.currentPage = requestParams.current;
    responseBody.pageSize = requestParams.pageSize;
    return true;
}

void DbTraceDataBase::ExecuteQueryDbThreadSameOperatorsDetails(const std::unique_ptr<SqliteResultSet> &resultSet,
    const Protocol::UnitThreadsOperatorsParams &requestParams, Protocol::UnitThreadsOperatorsBody &responseBody,
    const std::vector<std::string> tidList) {
    uint64_t offset = (requestParams.current - 1) > UINT64_MAX / requestParams.pageSize
        ? 0
        : (requestParams.current - 1) * requestParams.pageSize;
    uint64_t count = 0;
    std::unordered_map<uint64_t, std::unordered_map<uint64_t, uint32_t>> trackIdDepthCache;
    while (resultSet->Next()) {
        int col = resultStartIndex;
        Protocol::SameOperatorsDetails sameOperatorsDetail{};
        sameOperatorsDetail.timestamp = resultSet->GetUint64(col++);
        sameOperatorsDetail.duration = resultSet->GetUint64(col++);
        sameOperatorsDetail.depth = resultSet->GetUint64(col++);
        sameOperatorsDetail.id = resultSet->GetString(col++);
        sameOperatorsDetail.tid = resultSet->GetString(col++);
        if (sameOperatorsDetail.tid.empty()) { // some process not have tid, use request.tid[0], ex:pytorch
            sameOperatorsDetail.tid = tidList[0];
        }
        sameOperatorsDetail.pid = resultSet->GetString(col++);
        uint64_t trackId = TrackInfoManager::Instance().GetTrackId(
            requestParams.rankId, sameOperatorsDetail.pid, sameOperatorsDetail.tid);
        uint64_t sliceId = NumberUtil::StringToLongLong(sameOperatorsDetail.id);
        if (requestParams.isPythonStack) {
            SliceQuery sliceQuery = CreateSliceQueryWithTimeRange({requestParams.rankId, sameOperatorsDetail.pid,
                Protocol::PYTHON_API_THREAD_ID, ENUM_TO_STR(PROCESS_TYPE::API).value_or(""),
                sameOperatorsDetail.timestamp, sameOperatorsDetail.duration});
            sliceQuery.isPythonStack = true;
            auto item = trackIdDepthCache.find(sliceQuery.trackId);
            if (item != trackIdDepthCache.end()) {
                sameOperatorsDetail.depth = item->second[sliceId];
            } else {
                std::unordered_map<uint64_t, uint32_t> depthCache;
                GetSliceDepthCacheForJump(sliceQuery, depthCache);
                trackIdDepthCache[sliceQuery.trackId] = depthCache;
                sameOperatorsDetail.depth = depthCache[sliceId];
            }
        } else {
            SliceCacheManager &sliceCacheManager = SliceCacheManager::Instance();
            auto item = trackIdDepthCache.find(trackId);
            if (item != trackIdDepthCache.end()) {
                sameOperatorsDetail.depth = item->second[sliceId];
            } else {
                std::unordered_map<uint64_t, uint32_t> depthCache;
                sliceCacheManager.QueryDepthInfoWithoutTimeRange(
                    std::to_string(trackId), requestParams.rankId, depthCache);
                trackIdDepthCache[trackId] = depthCache;
                sameOperatorsDetail.depth = depthCache[sliceId];
            }
        }
        if (!requestParams.startDepth.empty() && !requestParams.endDepth.empty() &&
            !(sameOperatorsDetail.depth >= NumberUtil::StringToUint32(requestParams.startDepth) &&
                sameOperatorsDetail.depth <= NumberUtil::StringToUint32(requestParams.endDepth))) {
            continue;
        }
        if (++count <= offset) {
            continue;
        }
        if (count > offset + requestParams.pageSize) {
            break;
        }
        responseBody.sameOperatorsDetails.emplace_back(sameOperatorsDetail);
    }
}
}
