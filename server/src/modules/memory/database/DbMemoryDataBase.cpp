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
#include "WsSender.h"
#include "TraceTime.h"
#include "TableDefs.h"
#include "DataBaseManager.h"
#include "ProtocolDefs.h"
#include "TrackInfoManager.h"
#include "DbMemoryDataBase.h"

namespace Dic {
namespace Module {
namespace FullDb {
using namespace Dic::Server;
using namespace Dic::Module::Timeline;

std::map<std::string, Protocol::MemorySuccess> FullDb::DbMemoryDataBase::ranks = {};

bool DbMemoryDataBase::OpenDb(const std::string &dbPath, bool clearAllTable) {
    auto result = Database::OpenDb(dbPath, clearAllTable) && QueryMetaVersion();
    deviceIdColumnName = "deviceId";
    return result;
}

bool DbMemoryDataBase::QueryMemoryType(std::string &type, std::vector<std::string> &graphId) {
    return ExecuteMemoryType(graphId, type);
}

bool DbMemoryDataBase::QueryMemoryResourceType(std::string &type) {
    type = "Pytorch";
    return true;
}

std::string DbMemoryDataBase::BuildOperatorDetailSql(const uint64_t baseTimestamp) {
    std::string selectColumns = GetSelectOperatorMemoryFullColumnsWithCount(baseTimestamp);
    std::string nameJoinStringIdsAlias = GetJoinStringIDSAlias(OpMemoryColumn::NAME);
    std::string sql = StringUtil::FormatString("SELECT {} FROM {} JOIN STRING_IDS AS {} ON {}.id = {} WHERE {} = ? ",
        selectColumns, TABLE_OPERATOR_MEMORY, nameJoinStringIdsAlias, nameJoinStringIdsAlias, OpMemoryColumn::NAME,
        OpMemoryColumn::DEVICE_ID);
    return sql;
}

int64_t DbMemoryDataBase::QueryOperatorDetail(
    Protocol::MemoryOperatorParams &requestParams, std::vector<Protocol::MemoryOperator> &opDetails) {
    if (!GetMemoryDbContext().withOperatorMemory) {
        ServerLog::Warn("Missing table % on querying operator detail, nothing will be done.", TABLE_OPERATOR_MEMORY);
        return 0;
    }
    std::string sql;
    const FileType type = DataBaseManager::Instance().GetFileType(path);
    const uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    const uint64_t offsetTime =
        Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp(requestParams.rankId);
    // 溢出防护
    if (startTime > std::numeric_limits<uint64_t>::max() - offsetTime) {
        ServerLog::Error("Failed to calculate relative to the reference time due to integer overflow.");
        return -1;
    }
    if (type == FileType::PYTORCH) {
        sql = DbMemoryDataBase::BuildOperatorDetailSql(startTime + offsetTime);
    } else {
        ServerLog::Error("Memory tab does not support msprof data.");
        return -1;
    }
    AddOperatorSql(requestParams, sql);
    return ExecuteOperatorDetail(requestParams, opDetails, sql);
}

bool DbMemoryDataBase::QueryEntireOperatorTable(Protocol::MemoryOperatorParams &requestParams,
    std::vector<Protocol::MemoryOperator> &opDetails, uint64_t offsetTime) {
    if (!GetMemoryDbContext().withOperatorMemory) {
        ServerLog::Warn(
            "Missing table % on querying entire operator table, nothing will be done.", TABLE_OPERATOR_MEMORY);
        return true;
    }
    std::string sql;
    FileType type = DataBaseManager::Instance().GetFileType(path);
    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    std::string startTimeStr = std::to_string(startTime);
    std::string offsetTimeStr = std::to_string(offsetTime);
    if (type == FileType::PYTORCH) {
        sql = BuildOperatorDetailSql(NumberSafe::Add(startTime, offsetTime));
    } else {
        ServerLog::Error("Memory tab does not support msprof data.");
        return false;
    }
    return ExecuteQueryEntireOperatorTable(requestParams, opDetails, sql);
}

bool DbMemoryDataBase::QueryComponentDetail(Protocol::MemoryComponentParams &requestParams,
    std::vector<Protocol::MemoryTableColumnAttr> &columnAttr,
    std::vector<Protocol::MemoryComponent> &componentDetails) {
    if (!GetMemoryDbContext().withNpuModuleMem) {
        ServerLog::Warn("Missing table % on querying component detail, nothing will be done.", TABLE_NPU_MODULE_MEM);
        return true;
    }
    std::string sql;
    FileType type = DataBaseManager::Instance().GetFileType(path);
    if (type == FileType::PYTORCH) {
        uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
        uint64_t offsetTime = Timeline::TraceTime::Instance().GetOffsetByFileIdUsingMinTimestamp(requestParams.rankId);
        // 最内层SQL根据组件编号分组取出内存占用峰值大于100M的那些组件编号
        // 中间层SQL和最内层SQL的查询结果根据组件编号和内存占用值做一次连接
        // 做分组的原因是可能有多个时刻内存占用为峰值，只需要时刻最小的那个
        // 最外层SQL根据组件编号和组件名做一次连接
        sql = "SELECT t4.name AS componentColumn, ROUND(t3.size / (1024.0 * 1024.0), 2) AS totalReservedColumn,"
              " t3.timestamp_maxsize AS timestampColumn FROM "
              "(SELECT t1.moduleId AS id, t1.totalReserved AS size, MIN(ROUND((t1.timestampNs - " +
            std::to_string(NumberSafe::Add(startTime, offsetTime)) +
            ") / (1000.0 * 1000.0), 3)) AS timestamp_maxsize FROM " + TABLE_NPU_MODULE_MEM + " AS t1 JOIN " +
            "(SELECT moduleId, MAX(totalReserved) AS max_total_reserved FROM " + TABLE_NPU_MODULE_MEM +
            " GROUP BY moduleId HAVING max_total_reserved >= " + std::to_string(componentThresholdByte) +
            ") AS t2 ON t1.moduleId = t2.moduleId AND t1.totalReserved = t2.max_total_reserved "
            " WHERE t1.deviceId = ? "
            "GROUP BY t1.moduleId, t1.totalReserved) AS t3 JOIN ENUM_MODULE AS t4 ON t3.id = t4.id";
        if (!requestParams.order.empty() && !requestParams.orderBy.empty()) {
            sql += " ORDER BY " + requestParams.orderBy + "Column";
            if (requestParams.order == "ascend") {
                sql += " ASC ";
            } else {
                sql += " DESC ";
            }
        }
        sql += " LIMIT ? OFFSET ? ";
    } else {
        ServerLog::Error("Failed to query component detail: Memory tab does not support msprof data.");
        return false;
    }
    return ExecuteComponentDetail(requestParams, columnAttr, componentDetails, sql);
}

bool DbMemoryDataBase::QueryEntireComponentTable(Protocol::MemoryComponentParams &requestParams,
    std::vector<Protocol::MemoryComponent> &componentDetails, uint64_t offsetTime) {
    if (!GetMemoryDbContext().withNpuModuleMem) {
        ServerLog::Warn("Missing table % on querying entire component, nothing will be done.", TABLE_NPU_MODULE_MEM);
        return true;
    }
    std::string sql;
    FileType type = DataBaseManager::Instance().GetFileType(path);
    if (type == FileType::PYTORCH) {
        uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
        sql = "SELECT t4.name, ROUND(t3.size / (1024.0 * 1024.0), 2), t3.timestamp_maxsize FROM "
              "(SELECT t1.moduleId AS id, t1.totalReserved AS size, MIN(ROUND((t1.timestampNs - " +
            std::to_string(NumberSafe::Add(startTime, offsetTime)) +
            ") / (1000.0 * 1000.0), 3)) AS timestamp_maxsize FROM " + TABLE_NPU_MODULE_MEM + " AS t1 JOIN " +
            "(SELECT moduleId, MAX(totalReserved) AS max_total_reserved FROM " + TABLE_NPU_MODULE_MEM +
            " GROUP BY moduleId HAVING max_total_reserved >= " + std::to_string(componentThresholdByte) +
            ") AS t2 ON t1.moduleId = t2.moduleId AND t1.totalReserved = t2.max_total_reserved "
            "WHERE t1.deviceId = ? "
            "GROUP BY t1.moduleId, t1.totalReserved) AS t3 JOIN ENUM_MODULE AS t4 ON t3.id = t4.id ";
    } else {
        ServerLog::Error("Failed to query entire component table: Memory tab does not support msprof data.");
        return false;
    }
    return ExecuteQueryEntireComponentTable(requestParams, componentDetails, sql);
}

bool DbMemoryDataBase::QueryMemoryView(
    Protocol::MemoryViewParams &requestParams, Protocol::MemoryViewData &operatorBody, uint64_t offsetTime) {
    std::string sql = "";
    FileType type = DataBaseManager::Instance().GetFileType(path);
    if (!GetMemoryDbContext().withMemoryRecord) {
        ServerLog::Warn("Missing table % on querying memory view, nothing will be done.", TABLE_MEMORY_RECORD);
        return true;
    }

    uint64_t startTime = Timeline::TraceTime::Instance().GetStartTime();
    if (type == FileType::PYTORCH) {
        sql += "select * from ( ";
        sql += "SELECT NAME.value AS component, ROUND((timestamp - " + std::to_string(startTime) + " - " +
            std::to_string(offsetTime) +
            ") / (1000.0 * 1000.0), 3) as timestamp, "
            "ROUND(totalAllocated / (1024.0 * 1024.0), 2) as totalAllocated, "
            " ROUND(totalReserved / (1024.0 * 1024.0), 2) as totalReserve, "
            "ROUND(totalActive / (1024.0 * 1024.0), 2) as totalActive, streamPtr as stream, " +
            deviceIdColumnName + " FROM ";
        sql += TABLE_MEMORY_RECORD + " JOIN STRING_IDS AS NAME ON NAME.id = MEMORY_RECORD.component ";
        if (GetMemoryDbContext().withNpuMem) {
            sql += " UNION ALL select 'APP' as component, ROUND((timestampNs - " + std::to_string(startTime) +
                " ) / (1000.0 * 1000.0), 2) as timestampNs, "
                " 0 as totalAllocated,  ROUND((hbm + ddr) / (1024.0 * 1024.0), 2) as totalReserve, "
                " 0 as totalActive, '' as stream, deviceId from NPU_MEM join STRING_IDS as ids on ids.id = type "
                " where value = 'app' ";
        }
        sql += " ) WHERE " + deviceIdColumnName + " = ? ";
    } else {
        ServerLog::Error("Memory tab does not support msprof data.");
        return false;
    }
    std::vector<Protocol::ComponentDto> componentDtoVec;
    std::vector<std::string> streams;
    if (!ExecuteQueryMemoryViewExecuteSql(requestParams, componentDtoVec, streams, sql, deviceIdColumnName)) {
        return false;
    }
    return ExecuteQueryMemoryViewGetGraph(requestParams, componentDtoVec, streams, operatorBody);
}

bool DbMemoryDataBase::QueryComponentsTotalNum(Protocol::MemoryComponentParams &requestParams, int64_t &totalNum) {
    if (!GetMemoryDbContext().withNpuModuleMem) {
        ServerLog::Warn("Missing table % on querying component detail, nothing will be done.", TABLE_NPU_MODULE_MEM);
        return true;
    }
    std::string sql;
    FileType type = DataBaseManager::Instance().GetFileType(path);
    if (type == FileType::PYTORCH) {
        sql = "SELECT count(*) FROM (SELECT t2.name FROM " + TABLE_NPU_MODULE_MEM +
            " AS t1 JOIN ENUM_MODULE AS t2 ON t1.moduleId = t2.id WHERE deviceId = ? "
            " GROUP BY t2.name HAVING MAX(t1.totalReserved) >= " +
            std::to_string(componentThresholdByte) + ") AS t3";
    } else {
        ServerLog::Error("Failed to query components total num: Memory tab does not support msprof data.");
        return false;
    }
    return ExecuteComponentTotalNum(requestParams, totalNum, sql);
}

bool DbMemoryDataBase::QueryOperatorSize(Protocol::MemoryOperatorSizeParams &requestParams, double &min, double &max) {
    FileType type = DataBaseManager::Instance().GetFileType(path);
    std::string sql = "";
    if (!GetMemoryDbContext().withOperatorMemory) {
        ServerLog::Warn("Missing table % on querying operator size, nothing will be done.", TABLE_OPERATOR_MEMORY);
        return true;
    }
    if (type == FileType::PYTORCH) {
        sql += "SELECT ROUND(min(size)/ 1024.0, 2) as minSize, "
               " ROUND(max(size)/ 1024.0, 2) as maxSize FROM " +
            TABLE_OPERATOR_MEMORY + " WHERE " + deviceIdColumnName + " = ? ";
    } else {
        ServerLog::Error("Memory tab does not support msprof data.");
        return false;
    }
    return ExecuteOperatorSize(requestParams, min, max, sql);
}

// DB格式不支持静态图内存数据
bool DbMemoryDataBase::QueryStaticOperatorSize(
    Protocol::StaticOperatorSizeParams &requestParams, double &min, double &max) {
    return false;
}

// DB格式不支持静态图内存数据
int64_t DbMemoryDataBase::QueryStaticOperatorList(
    Protocol::StaticOperatorListParams &requestParams, std::vector<Protocol::StaticOperatorItem> &opDetails) {
    return -1;
}

// DB格式不支持静态图内存数据
bool DbMemoryDataBase::QueryEntireStaticOperatorTable(
    Protocol::StaticOperatorListParams &requestParams, std::vector<Protocol::StaticOperatorItem> &opDetails) {
    return false;
}

// DB格式不支持静态图内存数据
bool DbMemoryDataBase::QueryStaticOperatorGraph(
    Protocol::StaticOperatorGraphParams &requestParams, Protocol::StaticOperatorGraphItem &graphItem) {
    return false;
}

void DbMemoryDataBase::ParserEnd(std::string rankId, bool result, std::string fileId) {
    if (!result) {
        return;
    }

    Server::ServerLog::Info("[Memory]Parser ends, Rank ID: ", rankId);
    if (ranks.count(rankId) == 0) {
        Protocol::MemorySuccess success;
        success.rankId = rankId;
        success.parseSuccess = true;
        success.hasFile = true;
        success.fileId = fileId;
        auto rankInfos = TrackInfoManager::Instance().GetRankListByFileId(fileId, rankId);
        if (!rankInfos.empty()) {
            success.rankInfo = rankInfos[0];
        }
        ranks.emplace(rankId, success);
    } else {
        ranks[rankId].parseSuccess = true;
        ranks[rankId].hasFile = true;
    }
}

// 输入rankId为空时，会清空历史结果
void DbMemoryDataBase::ParseCallBack(
    const std::string &rankId, const std::string &fileId, bool result, const std::string &msg) {
    if (rankId.empty()) {
        ranks.clear();
        auto event = std::make_unique<Protocol::ModuleResetEvent>();
        event->moduleName = Protocol::MODULE_MEMORY;
        event->result = true;
        event->reset = true;
        SendEvent(std::move(event));
    } else {
        auto event = std::make_unique<Protocol::ParseMemoryCompletedEvent>();
        event->moduleName = Protocol::MODULE_TIMELINE;
        event->result = result;
        event->isCluster = true;
        event->fileId = fileId;
        std::vector<Protocol::MemorySuccess> memoryResult;
        memoryResult.push_back(ranks[rankId]);
        event->memoryResult = memoryResult;
        SendEvent(std::move(event));
    }
}

std::map<std::string, Protocol::MemorySuccess> DbMemoryDataBase::GetRanks() { return ranks; }

void DbMemoryDataBase::Reset() {
    ServerLog::Info("Memory reset. Wait task completed.");
    ranks.clear();
    ServerLog::Info("Memory task completed.");
    auto databaseList = Timeline::DataBaseManager::Instance().GetAllMemoryDatabase();
    for (auto &db : databaseList) {
        auto database = dynamic_cast<DbMemoryDataBase *>(db);
        if (database != nullptr) {
            database->CloseDb();
        }
    }
    Timeline::DataBaseManager::Instance().Clear(Timeline::DatabaseType::MEMORY);
}

void DbMemoryDataBase::GetSelectOperatorMemoryColumnAndAlias(
    std::string_view columnKey, uint64_t baseTimestamp, std::string &column, std::string &alias) {
    // id列，从db中的rowid查出并别名为id
    if (columnKey == "id") {
        column = StringUtil::FormatString("{}.{}", TABLE_OPERATOR_MEMORY, OpMemoryColumn::ID);
        alias = columnKey;
        return;
    }
    // 注意此处会将所有列别名前缀_, 用于避免计算列where的判断条件时使用原值而不是计算值
    alias = StringUtil::FormatString("_{}", columnKey);
    // Bytes转为MBytes的列
    if (OPERATOR_MEMORY_ARA_SIZE_COLUMNS.find(columnKey) != OPERATOR_MEMORY_ARA_SIZE_COLUMNS.end()) {
        column = StringUtil::FormatString("ROUND({}/(1024.0*1024.0), 2)", columnKey);
        return;
    }
    // ns转换为ms的列
    std::string baseTimestampStr;
    if (OPERATOR_MEMORY_TIMESTAMP_NS_COLUMNS_SET.find(columnKey) != OPERATOR_MEMORY_TIMESTAMP_NS_COLUMNS_SET.end()) {
        if (columnKey == OpMemoryColumn::DURATION || columnKey == OpMemoryColumn::ACTIVE_DURATION) {
            baseTimestampStr = "0";
        } else {
            baseTimestampStr = std::to_string(baseTimestamp);
        }
        column = StringUtil::FormatString("ROUND(({} - {})/(1000.0*1000.0), 3)", columnKey, baseTimestampStr);
        return;
    }
    // KBytes转为MBytes的列
    if (columnKey == OpMemoryColumn::SIZE) {
        column = StringUtil::FormatString("ROUND({}/1024.0, 2)", columnKey);
        return;
    }
    // 需要JOIN STRING_IDS的列，默认需要取列SI_{原列名}.value
    if (columnKey == OpMemoryColumn::NAME) {
        column = StringUtil::FormatString("{}.value", GetJoinStringIDSAlias(columnKey));
        return;
    }
    // 需要指定rowid的
    if (columnKey == OpMemoryColumn::ID) {
        column = StringUtil::FormatString("{}.rowid", TABLE_OPERATOR_MEMORY);
        return;
    }
    // 缺省不计算
    column = std::string(columnKey);
}

std::string DbMemoryDataBase::GetJoinStringIDSAlias(std::string_view joinCol) {
    return StringUtil::FormatString("SI_{}", joinCol);
}

MemoryDataBaseContext DbMemoryDataBase::GetMemoryDbContext() {
    if (!initContextFlag) {
        memDbContext.withMemoryRecord = CheckTableExist(TABLE_MEMORY_RECORD);
        memDbContext.withOperatorMemory = CheckTableExist(TABLE_OPERATOR_MEMORY);
        memDbContext.withNpuModuleMem = CheckTableExist(TABLE_NPU_MODULE_MEM);
        memDbContext.withNpuMem = CheckTableExist(TABLE_NPU_MEM);
        initContextFlag = true;
    }
    return memDbContext;
}

}
}
}
