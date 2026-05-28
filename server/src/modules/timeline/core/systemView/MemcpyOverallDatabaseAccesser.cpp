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

#include "ServerLog.h"
#include "Paginator.h"
#include "MemcpyOverallDatabaseAccesser.h"

namespace Dic::Module::Timeline {
DataType MemcpyOverallDatabaseAccesser::GetDatabaseType() const {
    if (fileId_.empty()) {
        // 如果fileId为空，返回默认类型
        return DataType::TEXT;
    }
    // 或者直接调用DataBaseManager的静态方法
    return DataBaseManager::Instance().GetDataType(fileId_);
}

bool MemcpyOverallDatabaseAccesser::GetMemcpyRecords(
    const uint64_t startTime, const uint64_t endTime, std::vector<MemcpyRecord> &records) const {
    if (!database_ || fileId_.empty()) {
        return false;
    }

    DataType dataType = GetDatabaseType();

    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    uint64_t absStart, absEnd;
    if (!SafeAddUint64(startTime, minTimestamp, absStart) || !SafeAddUint64(endTime, minTimestamp, absEnd)) {
        Server::ServerLog::Error("Time conversion overflow: relative time + base timestamp exceeds uint64_t limit");
        return false;
    }

    if (dataType == DataType::TEXT) {
        GetMemcpyRecordsFromText(absStart, absEnd, records);
    } else if (dataType == DataType::DB) {
        GetMemcpyRecordsFromDb(absStart, absEnd, records);
    } else {
        return false;
    }

    for (auto &record : records) {
        if (record.startTime < minTimestamp || record.endTime < minTimestamp) {
            ServerLog::Warn("Unexpected condition: slice time is less than min timestamp. "
                            "time: [",
                record.startTime, ", ", record.endTime, " Min time stamp: ", minTimestamp);
            return false;
        }
        record.startTime -= minTimestamp;
        record.endTime -= minTimestamp;
    }
    return true;
}

bool MemcpyOverallDatabaseAccesser::GetMemcpyDetailRecordsPaged(uint64_t startTime, uint64_t endTime,
    const std::string &tid, const std::optional<std::string> &memcpyType, uint32_t current, uint32_t pageSize,
    const OrderParam &orderParam, std::vector<MemcpyDetailRecord> &records, uint64_t &total) const {
    if (!database_ || fileId_.empty()) {
        return false;
    }

    DataType dataType = GetDatabaseType();

    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    uint64_t absStart, absEnd;
    if (!SafeAddUint64(startTime, minTimestamp, absStart) || !SafeAddUint64(endTime, minTimestamp, absEnd)) {
        Server::ServerLog::Error("Time conversion overflow: relative time + base timestamp exceeds uint64_t limit");
        return false;
    }

    if (dataType == DataType::TEXT) {
        auto [sortField, sortDir] = ParseSortParams(orderParam.orderBy, orderParam.GetNormalizeOrderType());
        GetMemcpyDetailRecordsPagedFromText(
            absStart, absEnd, tid, memcpyType, current, pageSize, sortField, sortDir, records, total);
    } else if (dataType == DataType::DB) {
        GetMemcpyDetailRecordsPagedFromDb(
            absStart, absEnd, tid, memcpyType, current, pageSize, orderParam.GenerateSql(), records, total);
    } else {
        return false;
    }

    for (auto &record : records) {
        if (record.timestamp < minTimestamp) {
            ServerLog::Warn("Unexpected condition: slice timestamp is less than min timestamp. "
                            "timestamp: ",
                record.timestamp, " Min time stamp: ", minTimestamp);
            return false;
        }
        record.timestamp -= minTimestamp;
    }
    return true;
}

bool MemcpyOverallDatabaseAccesser::GetMemcpyRecordsFromText(
    uint64_t startTime, uint64_t endTime, std::vector<MemcpyRecord> &records) const {
    if (!database_) {
        return false;
    }

    try {
        const bool useTimeSearch = startTime != endTime;
        // 构造SQL查询语句，使用参数化查询防止SQL注入
        std::string sql = "SELECT t.tid, t.thread_name, s.args, s.timestamp, s.end_time FROM slice s "
                          "LEFT JOIN thread t ON s.track_id = t.track_id "
                          "WHERE s.name = 'MEMCPY_ASYNC' ";
        if (useTimeSearch) {
            sql += "AND timestamp >= ? AND end_time <= ?";
        }

        auto stmt = database_->CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            // 记录错误日志
            Server::ServerLog::Error("Querying memcpy records from db has error, Fail to get stmt.");
            return false;
        }

        // 绑定参数
        if (useTimeSearch) {
            stmt->BindParams(startTime, endTime);
        }

        auto resultSet = stmt->ExecuteQuery();
        if (resultSet == nullptr) {
            // 记录错误日志
            Server::ServerLog::Error("Querying memcpy records from db has error, Fail to execute query.");
            return false;
        }

        while (resultSet->Next()) {
            MemcpyRecord record;

            // 获取tid
            record.threadId = resultSet->GetString("tid");
            record.threadName = resultSet->GetString("thread_name");

            // 获取args并解析operation
            std::string argsStr = resultSet->GetString("args");
            const auto parsed = ParseOperationAndSizeFromJson(argsStr);
            record.memcpyType = parsed.first;
            record.size = parsed.second;

            // 获取时间戳
            record.startTime = resultSet->GetUint64("timestamp");
            record.endTime = resultSet->GetUint64("end_time");
            record.duration = static_cast<double>(record.endTime - record.startTime);

            records.push_back(record);
        }

        return true;
    } catch (...) {
        return false;
    }
}

bool MemcpyOverallDatabaseAccesser::GetMemcpyRecordsFromDb(
    uint64_t startTime, uint64_t endTime, std::vector<MemcpyRecord> &records) const {
    if (!database_) {
        return false;
    }

    try {
        const bool useTimeSearch = startTime != endTime;
        // 构造SQL查询语句，关联task表和memcpy_info表，使用参数化查询
        // memcpy 的数据只可能存在 Ascend Hardware 泳道，因此 tid 使用 Ascend Hardware 泳道使用的 tid 定义，即 streamId
        std::string sql = "SELECT t.streamId AS tid, emo.name AS memcpyOperation, mi.size, t.startNs, t.endNs "
                          "FROM " +
            TABLE_TASK + " t JOIN " + TABLE_MEMCPY_INFO +
            " mi "
            "ON t.globalTaskId = mi.globalTaskId "
            "LEFT JOIN " +
            TABLE_ENUM_MEMCPY_OPERATION + " emo ON mi.memcpyOperation = emo.id ";
        if (useTimeSearch) {
            sql += "WHERE t.startNs >= ? AND t.endNs <= ?";
        }

        auto stmt = database_->CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            // 记录错误日志
            Server::ServerLog::Error("Querying memcpy records from db has error, Fail to get stmt.");
            return false;
        }

        // 绑定参数
        if (useTimeSearch) {
            stmt->BindParams(startTime, endTime);
        }

        auto resultSet = stmt->ExecuteQuery();
        if (resultSet == nullptr) {
            // 记录错误日志
            Server::ServerLog::Error("Querying memcpy records from db has error, Fail to execute query.");
            return false;
        }

        while (resultSet->Next()) {
            MemcpyRecord record;

            // 提取threadId
            record.threadId = resultSet->GetString("tid");
            record.threadName = "Stream " + record.threadId;

            // 获取memcpyOperation
            record.memcpyType = resultSet->GetString("memcpyOperation");

            // 获取size
            record.size = resultSet->GetUint64("size");

            // 获取时间戳
            record.startTime = resultSet->GetUint64("startNs");
            record.endTime = resultSet->GetUint64("endNs");
            record.duration = static_cast<double>(record.endTime - record.startTime);

            records.push_back(record);
        }

        return true;
    } catch (...) {
        return false;
    }
}

// ====== TEXT 库：构建基础查询（CTE 主体） ======
std::pair<std::string, std::vector<std::string>> MemcpyOverallDatabaseAccesser::BuildMemcpyDetailBaseQueryText(
    uint64_t startTime, uint64_t endTime, const std::string &tidFilter) {
    std::vector<std::string> params;
    std::ostringstream sql;

    sql << "SELECT s.id AS slice_id, s.name, s.args, s.timestamp, s.duration FROM slice s "
        << "LEFT JOIN thread t ON s.track_id = t.track_id "
        << "WHERE s.name = 'MEMCPY_ASYNC' ";

    // 时间过滤
    if (startTime != endTime) {
        sql << "AND s.timestamp >= ? AND s.end_time <= ? ";
        params.emplace_back(std::to_string(startTime));
        params.emplace_back(std::to_string(endTime));
    }

    // TID 过滤 (tid)
    if (!tidFilter.empty()) {
        sql << "AND t.tid = ? ";
        params.emplace_back(tidFilter);
    }

    return {sql.str(), params};
}

bool MemcpyOverallDatabaseAccesser::GetMemcpyDetailRecordsPagedFromText(uint64_t startTime, uint64_t endTime,
    const std::string &tid, const std::optional<std::string> &memcpyType, uint32_t current, uint32_t pageSize,
    SortField orderByField, SortDirection orderDir, std::vector<MemcpyDetailRecord> &records, uint64_t &total) const {
    if (!database_) {
        return false;
    }

    try {
        auto [baseQuery, baseParams] = BuildMemcpyDetailBaseQueryText(startTime, endTime, tid);
        if (baseQuery.empty()) {
            return false;
        }

        auto stmt = database_->CreatPreparedStatement(baseQuery);
        if (!stmt) {
            Server::ServerLog::Error("Failed to create prepared statement for memcpy detail (TEXT)");
            return false;
        }
        for (const auto &param : baseParams) {
            stmt->BindParams(param);
        }

        auto resultSet = stmt->ExecuteQuery();
        if (!resultSet) {
            Server::ServerLog::Error("Failed to execute query for memcpy detail (TEXT)");
            return false;
        }

        std::vector<MemcpyDetailRecord> filtered;
        while (resultSet->Next()) {
            // C++层过滤memcpyType
            std::string argsStr = resultSet->GetString("args");
            auto [operation, size] = ParseOperationAndSizeFromJson(argsStr);

            if (memcpyType.has_value() && operation != memcpyType) {
                continue; // 跳过不匹配的类型
            }

            MemcpyDetailRecord record;
            record.timestamp = resultSet->GetUint64("timestamp");
            record.duration = resultSet->GetUint64("duration");
            record.size = size;
            record.name = resultSet->GetString("name");
            record.id = std::to_string(resultSet->GetUint64("slice_id"));

            filtered.push_back(record);
        }
        SortRecordsInMemory(filtered, orderByField, orderDir); // 排序
        const Paginator<MemcpyDetailRecord> paginator(filtered, pageSize); // 分页
        total = paginator.GetTotal();
        records = paginator.GetPage(current);
        return true;
    } catch (const std::exception &e) {
        Server::ServerLog::Error("Exception in GetMemcpyDetailRecordsPagedFromText: " + std::string(e.what()));
        return false;
    } catch (...) {
        Server::ServerLog::Error("Unknown exception in GetMemcpyDetailRecordsPagedFromText");
        return false;
    }
}

// ====== DB 库：构建基础查询（CTE 主体） ======
std::pair<std::string, std::vector<std::string>> MemcpyOverallDatabaseAccesser::BuildMemcpyDetailBaseQueryDb(
    uint64_t startTime, uint64_t endTime, const std::string &tidFilter,
    const std::optional<std::string> &memcpyTypeFilter) {
    std::vector<std::string> params;
    std::ostringstream sql;

    /**
     * @note 获取 name 的解释
     * 1. MSTX_EVENTS 事件目前不可能有 MEMCPY 的算子，忽略
     * 2. 非 MSTX_EVENTS 事件算子名称: DbSqlDefs.h 文件中的 ASCEND_THREADS_EXCLUDING_MSTX_BY_PID 语句获取名称的 SQL 是
     *      `coalesce(c.name, s.name, main.taskType) as name` c 表示 COMPUTE, s 表示 COMMUNICATION
     *      在业务逻辑上 MEMCPY 既不是 COMPUTE 也不是 COMMUNICATION，因此只能取 main.taskType, main 表示 TASK
     **/
    sql << "SELECT t.globalTaskId, t.streamId, emo.name AS memcpyOperation, si.value AS name, "
        << "mi.size AS size, t.startNs AS startTime, t.endNs - t.startNs AS duration FROM " << TABLE_TASK << " t "
        << "JOIN " << TABLE_MEMCPY_INFO << " mi ON t.globalTaskId = mi.globalTaskId "
        << "LEFT JOIN " << TABLE_ENUM_MEMCPY_OPERATION << " emo ON mi.memcpyOperation = emo.id "
        << "LEFT JOIN " << TABLE_STRING_IDS << " si ON si.id = t.taskType "
        << "WHERE 1=1 ";

    // 时间过滤
    if (startTime != endTime) {
        sql << "AND t.startNs >= ? AND t.endNs <= ? ";
        params.emplace_back(std::to_string(startTime));
        params.emplace_back(std::to_string(endTime));
    }

    // TID 过滤 (streamId)
    if (!tidFilter.empty()) {
        sql << "AND t.streamId = ? ";
        params.emplace_back(tidFilter);
    }

    // memcpyType 过滤
    if (memcpyTypeFilter.has_value()) {
        sql << "AND emo.name = ? ";
        params.emplace_back(memcpyTypeFilter.value());
    }

    return {sql.str(), params};
}

void MemcpyOverallDatabaseAccesser::GetMemcpyDetailTotalFromDb(
    const std::string &baseQuery, const std::vector<std::string> &baseParams, uint64_t &total) const {
    const std::string totalSql = "WITH filtered AS (" + baseQuery + ") SELECT COUNT(*) AS total FROM filtered";

    auto totalStmt = database_->CreatPreparedStatement(totalSql);
    if (!totalStmt) {
        Server::ServerLog::Error("Failed to create prepared statement for memcpy detail total (DB)");
        return;
    }
    for (const auto &param : baseParams) {
        totalStmt->BindParams(param);
    }
    auto resultSet = totalStmt->ExecuteQuery();
    if (!resultSet || !resultSet->Next()) {
        Server::ServerLog::Error("Failed to execute query for memcpy detail total (DB)");
        return;
    }
    total = resultSet->GetUint64("total");
}

// ====== DB 库：分页查询主逻辑 ======
bool MemcpyOverallDatabaseAccesser::GetMemcpyDetailRecordsPagedFromDb(uint64_t startTime, uint64_t endTime,
    const std::string &tid, const std::optional<std::string> &memcpyType, uint32_t current, uint32_t pageSize,
    const std::string &orderSql, std::vector<MemcpyDetailRecord> &records, uint64_t &total) const {
    if (!database_) {
        return false;
    }

    try {
        auto [baseQuery, baseParams] = BuildMemcpyDetailBaseQueryDb(startTime, endTime, tid, memcpyType);
        if (baseQuery.empty()) {
            return false;
        }
        // 查到总数
        GetMemcpyDetailTotalFromDb(baseQuery, baseParams, total);
        // 查到分页数据
        std::string dataSql =
            "WITH filtered AS (" + baseQuery + ") SELECT * FROM filtered " + orderSql + "LIMIT ? OFFSET ?";

        if (current - 1 != 0 && pageSize > UINT64_MAX / (current - 1)) {
            Server::ServerLog::Error("Pagination overflow, it exceeds uint64_t limit");
            return false;
        }
        uint64_t offset = static_cast<uint64_t>(current - 1) * pageSize;
        auto dataStmt = database_->CreatPreparedStatement(dataSql);
        if (!dataStmt) {
            Server::ServerLog::Error("Failed to create prepared statement for memcpy detail (DB)");
            return false;
        }
        for (const auto &param : baseParams) {
            dataStmt->BindParams(param);
        }
        dataStmt->BindParams(pageSize);
        dataStmt->BindParams(offset);

        auto resultSet = dataStmt->ExecuteQuery();
        if (!resultSet) {
            Server::ServerLog::Error("Failed to execute query for memcpy detail (DB)");
            return false;
        }

        while (resultSet->Next()) {
            MemcpyDetailRecord record;
            record.timestamp = resultSet->GetUint64("startTime");
            record.duration = resultSet->GetUint64("duration");
            record.size = resultSet->GetUint64("size");
            record.name = resultSet->GetString("name");
            record.id = std::to_string(resultSet->GetUint64("globalTaskId"));

            records.push_back(record);
        }
        return true;
    } catch (const std::exception &e) {
        Server::ServerLog::Error("Exception in GetMemcpyDetailRecordsPagedFromDb: " + std::string(e.what()));
        return false;
    } catch (...) {
        Server::ServerLog::Error("Unknown exception in GetMemcpyDetailRecordsPagedFromDb");
        return false;
    }
}

std::pair<std::string, uint64_t> MemcpyOverallDatabaseAccesser::ParseOperationAndSizeFromJson(
    const std::string &jsonStr) {
    std::string error;
    const auto json = JsonUtil::TryParse(jsonStr, error);

    if (!json.has_value() || !error.empty()) {
        return {"", 0}; // 解析失败，返回空字符串
    }

    std::string operation;
    // 检查并获取operation字段（支持多种可能的键名）
    if (JsonUtil::IsJsonKeyValid(json.value(), "operation")) {
        operation = JsonUtil::GetString(json.value(), "operation");
    } else if (JsonUtil::IsJsonKeyValid(json.value(), "Operation")) {
        operation = JsonUtil::GetString(json.value(), "Operation");
    } else if (JsonUtil::IsJsonKeyValid(json.value(), "OPERATION")) {
        operation = JsonUtil::GetString(json.value(), "OPERATION");
    }

    uint64_t size = 0;
    // 检查并获取 size(B) 字段
    if (JsonUtil::IsJsonKeyValid(json.value(), "size(B)")) {
        size = JsonUtil::GetInteger(json.value(), "size(B)");
    }

    return {operation, size};
}
}
