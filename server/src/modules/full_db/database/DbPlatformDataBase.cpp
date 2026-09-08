/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include "DbPlatformDataBase.h"
#include "DataBaseManager.h"
#include "JsonUtil.h"
#include "ServerLog.h"

namespace Dic {
namespace Module {
namespace FullDb {

using namespace Dic::Server;
using namespace Dic::Module::Timeline;

namespace {
const std::vector<std::string> PLATFORM_TIMELINE_TABLES = {
    "NUMA_TITLES_NAMES",
    "NUMA_LEVELS_HIERARCHY_NAMES",
    "NUMA_METRICS",
    "NUMA_SCALING_VALUES",
};
constexpr char EMBEDDED_PLATFORM_RANK_SUFFIX[] = "#platform";
} // namespace

bool HasPlatformTimelineData(const std::shared_ptr<Database> &database) {
    return database != nullptr && database->IsOpen() && database->CheckTablesExist(PLATFORM_TIMELINE_TABLES);
}

std::string BuildEmbeddedPlatformRankId(const std::string &traceRankId) {
    return traceRankId + EMBEDDED_PLATFORM_RANK_SUFFIX;
}

bool IsEmbeddedPlatformRankId(const std::string &rankId) {
    const size_t suffixLength = std::char_traits<char>::length(EMBEDDED_PLATFORM_RANK_SUFFIX);
    return rankId.size() >= suffixLength &&
        rankId.compare(rankId.size() - suffixLength, suffixLength, EMBEDDED_PLATFORM_RANK_SUFFIX) == 0;
}

std::string GetTraceRankIdFromEmbeddedPlatformRankId(const std::string &rankId) {
    if (!IsEmbeddedPlatformRankId(rankId)) {
        return rankId;
    }
    return rankId.substr(0, rankId.size() - std::char_traits<char>::length(EMBEDDED_PLATFORM_RANK_SUFFIX));
}

bool DbPlatformDataBase::QueryLevelData(LevelDataMap &levels) {
    const std::string sql =
        "SELECT rowid AS level_id, title0_id, title1_id, title2_id FROM NUMA_LEVELS_HIERARCHY_NAMES";

    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        return false;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        return false;
    }

    while (resultSet->Next()) {
        LevelData info;
        info.levelId = resultSet->GetInt64("level_id");
        info.title0Id = resultSet->GetInt64("title0_id");
        info.title1Id = resultSet->GetInt64("title1_id");
        info.title2Id = resultSet->GetInt64("title2_id");
        levels[info.levelId] = info;
    }
    return true;
}

bool DbPlatformDataBase::QueryTitleData(TitleDataMap &titles) {
    const std::string sql =
        "SELECT unique_id AS id, name, description, measurement_unit, summary_flag FROM NUMA_TITLES_NAMES";

    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        return false;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        return false;
    }

    while (resultSet->Next()) {
        TitleData info;
        info.id = resultSet->GetInt64("id");
        info.name = resultSet->GetString("name");
        info.description = resultSet->GetString("description");
        info.measurementUnit = resultSet->GetString("measurement_unit");
        info.summaryFlag = resultSet->GetInt64("summary_flag");
        titles[info.id] = info;
    }
    return true;
}

bool DbPlatformDataBase::QueryPlatformMetrics(std::vector<PlatformMetric> &metrics) {
    const std::string sql = "SELECT m.ts, m.levels_id, m.value, l.title0_id "
                            "FROM NUMA_METRICS m "
                            "JOIN NUMA_LEVELS_HIERARCHY_NAMES l ON m.levels_id = l.rowid "
                            "ORDER BY m.ts ASC";

    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        return false;
    }
    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        return false;
    }

    while (resultSet->Next()) {
        PlatformMetric metric;
        metric.ts = resultSet->GetInt64("ts");
        metric.levelId = resultSet->GetInt64("levels_id");
        metric.value = resultSet->GetDouble("value");
        metric.title0Id = resultSet->GetInt64("title0_id");
        metrics.emplace_back(metric);
    }

    return true;
}

bool DbPlatformDataBase::QueryPlatformCounterData(int64_t levelId, uint64_t startTime, uint64_t endTime,
    uint64_t minTimestamp, std::vector<PlatformCounterData> &dataList) {
    if (startTime > UINT64_MAX - minTimestamp || endTime > UINT64_MAX - minTimestamp) {
        ServerLog::Error("QueryPlatformCounterData - time range overflows timestamp origin");
        return false;
    }
    std::string sql = "SELECT metrics.ts, metrics.value FROM NUMA_METRICS metrics "
                      "WHERE metrics.levels_id = ? ";
    if (startTime != endTime) {
        sql += " AND metrics.ts >= ? AND metrics.ts <= ? ";
    }
    sql += " ORDER BY metrics.ts ASC";

    auto stmt = CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        ServerLog::Error("QueryPlatformCounterData - CreatPreparedStatement failed");
        return false;
    }

    if (startTime != endTime) {
        stmt->BindParams(levelId, minTimestamp + startTime, minTimestamp + endTime);
    } else {
        stmt->BindParams(levelId);
    }

    auto resultSet = stmt->ExecuteQuery();
    if (resultSet == nullptr) {
        ServerLog::Error("QueryPlatformCounterData - ExecuteQuery failed");
        return false;
    }

    while (resultSet->Next()) {
        PlatformCounterData data;
        data.timestamp = static_cast<uint64_t>(resultSet->GetInt64("ts"));
        data.value = resultSet->GetDouble("value");
        dataList.emplace_back(data);
    }

    ServerLog::Info("QueryPlatformCounterData - got ", dataList.size(), " rows");
    return true;
}

bool DbPlatformDataBase::QueryMeasurementUnit(int64_t levelId, std::string &measurementUnit) {
    const std::string sql = "SELECT titles.measurement_unit "
                            "FROM NUMA_LEVELS_HIERARCHY_NAMES as levels "
                            "JOIN NUMA_TITLES_NAMES as titles ON titles.unique_id = COALESCE( "
                            "NULLIF(levels.title2_id, 0), "
                            "NULLIF(levels.title1_id, 0), "
                            "NULLIF(levels.title0_id, 0)) "
                            "WHERE levels.rowid = ?";

    auto stmt = CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("QueryMeasurementUnit - CreatPreparedStatement failed");
        return false;
    }

    stmt->BindParams(levelId);

    auto resultSet = stmt->ExecuteQuery();
    if (!resultSet) {
        ServerLog::Error("QueryMeasurementUnit - ExecuteQuery failed");
        return false;
    }

    if (resultSet->Next()) {
        measurementUnit = resultSet->GetString("measurement_unit");
    } else {
        return false;
    }

    return true;
}

bool DbPlatformDataBase::QueryScalingValuesData(ScalingValueMap &scalingValueMap) {
    std::string sql = "SELECT level_id, max_value FROM NUMA_SCALING_VALUES";

    auto stmt = CreatPreparedStatement(sql);
    if (!stmt) {
        ServerLog::Error("QueryScalingValuesData - CreatPreparedStatement failed");
        return false;
    }

    auto resultSet = stmt->ExecuteQuery();
    if (!resultSet) {
        ServerLog::Error("QueryScalingValuesData - ExecuteQuery failed");
        return false;
    }

    while (resultSet->Next()) {
        int64_t level_id = resultSet->GetInt64("level_id");
        double max_value = resultSet->GetDouble("max_value");
        scalingValueMap[level_id] = max_value;
    }

    return true;
}

void DbPlatformDataBase::Reset() {
    ServerLog::Info("DbPlatformDataBase::Reset - started");

    auto databaseList = Timeline::DataBaseManager::Instance().GetAllPlatformDatabase();
    for (auto &db : databaseList) {
        auto database = dynamic_cast<DbPlatformDataBase *>(db);
        if (database != nullptr) {
            try {
                database->CloseDb();
            } catch (...) {
                ServerLog::Error("CloseDB failed during DbPlatformDataBase::Reset");
            }
        }
    }

    ServerLog::Info("DbPlatformDataBase::Reset - DBs closed");

    Timeline::DataBaseManager::Instance().Clear(Timeline::DatabaseType::PLATFORM);

    ServerLog::Info("DbPlatformDataBase::Reset - completed");
}

bool DbPlatformDataBase::QueryUnitCounter(Dic::Protocol::UnitCounterParams &params, uint64_t minTimestamp,
    std::vector<Dic::Protocol::UnitCounterData> &dataList) {
    ServerLog::Info("DbPlatformDataBase::QueryUnitCounter - metric: ", params.threadName,
        ", threadId: ", params.threadId, ", startTime: ", params.startTime, ", endTime: ", params.endTime);

    int64_t levelId = -1;
    if (!params.threadId.empty()) {
        try {
            levelId = std::stoll(params.threadId);
        } catch (...) {
            ServerLog::Info(
                "DbPlatformDataBase::QueryUnitCounter - invalid threadId, treating as ALL: ", params.threadId);
            return false;
        }
    }

    std::vector<PlatformCounterData> counterData;
    if (!QueryPlatformCounterData(levelId, params.startTime, params.endTime, minTimestamp, counterData)) {
        ServerLog::Error("DbPlatformDataBase::QueryUnitCounter - QueryPlatformCounterData failed");
        return false;
    }

    std::string measurementUnit;
    if (!QueryMeasurementUnit(levelId, measurementUnit)) {
        ServerLog::Error("DbPlatformDataBase::QueryUnitCounter - QueryMeasurementUnit failed");
        measurementUnit = "Value"; // default value
    }

    if (measurementUnit.empty()) {
        measurementUnit = "Value"; // default value
    }

    document_t measurementUnitJson;
    measurementUnitJson.SetString(measurementUnit.c_str(), static_cast<rapidjson::SizeType>(measurementUnit.size()),
        measurementUnitJson.GetAllocator());
    const std::string escapedMeasurementUnit = JsonUtil::JsonDump(measurementUnitJson);

    double lastValue = 0;
    for (const auto &item : counterData) {
        double curValue = item.value;
        if (!dataList.empty() && curValue == lastValue) {
            continue;
        }
        lastValue = curValue;
        Dic::Protocol::UnitCounterData data;
        data.timestamp = item.timestamp >= minTimestamp ? item.timestamp - minTimestamp : 0;
        data.valueJsonStr = "{" + escapedMeasurementUnit + ":" + std::to_string(curValue) + "}";
        dataList.emplace_back(data);
    }

    ServerLog::Info("DbPlatformDataBase::QueryUnitCounter - returned ", dataList.size(), " data points");
    return true;
}

} // namespace FullDb
} // namespace Module
} // namespace Dic
