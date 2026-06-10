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
#include "TextTraceDatabase.h"
#include "TextTraceDatabaseFtraceSql.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;
using namespace Dic::Protocol;

// ==================== trace_task_summary 表 ====================

bool TextTraceDatabase::CreateTraceTaskSummaryTable() {
    if (!isOpen) {
        ServerLog::Error("Failed to create trace_task_summary table. Database is not open.");
        return false;
    }

    std::string sql = R"(
            CREATE TABLE IF NOT EXISTS trace_task_summary (
                comm TEXT NOT NULL,
                pid INTEGER NOT NULL,
                cpu_id INTEGER NOT NULL,
                running_ns INTEGER DEFAULT 0,
                sleeping_ns INTEGER DEFAULT 0,
                runnable_ns INTEGER DEFAULT 0,
                cs_count INTEGER DEFAULT 0,
                cs_involuntary_count INTEGER DEFAULT 0,
                soft_irq_count INTEGER DEFAULT 0,
                soft_irq_duration INTEGER DEFAULT 0,
                hard_irq_count INTEGER DEFAULT 0,
                hard_irq_duration INTEGER DEFAULT 0,
                PRIMARY KEY (comm, pid, cpu_id)
            );
        )";

    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!ExecSql(sql)) {
        return false;
    }

    // 补齐 IRQ 列
    static constexpr const char *IRQ_COLUMNS[] = {
        "soft_irq_count", "soft_irq_duration", "hard_irq_count", "hard_irq_duration"};
    for (const char *col : IRQ_COLUMNS) {
        if (!CheckColumnExist("trace_task_summary", col)) {
            std::string alterSql =
                "ALTER TABLE trace_task_summary ADD COLUMN " + std::string(col) + " INTEGER DEFAULT 0";
            if (!ExecSql(alterSql)) {
                ServerLog::Error("Failed to add column ", col, " to trace_task_summary.");
            }
        }
    }
    // lock 在函数结束时自动释放，无需手动 unlock
    return true;
}

bool TextTraceDatabase::InsertTraceTaskSummary(const TraceTaskSummaryData &data) {
    traceTaskSummaryCache.emplace_back(data);
    if (traceTaskSummaryCache.size() == CACHE_SIZE) {
        InsertTraceTaskSummaryList(traceTaskSummaryCache);
        traceTaskSummaryCache.clear();
    }
    return true;
}

std::unique_ptr<SqlitePreparedStatement> TextTraceDatabase::GetTraceTaskSummaryStmt(uint64_t paramLen) {
    std::string valuePlaceholders;
    for (uint64_t i = 0; i < paramLen; ++i) {
        if (i > 0) {
            valuePlaceholders += ", ";
        }
        valuePlaceholders += "(?, ?, ?, ?, ?, ?, ?, ?)";
    }
    std::string sql = "INSERT INTO trace_task_summary (comm, pid, cpu_id, running_ns, sleeping_ns, "
                      "runnable_ns, cs_count, cs_involuntary_count) VALUES " +
        valuePlaceholders;
    return CreatPreparedStatement(sql);
}

bool TextTraceDatabase::InsertTraceTaskSummaryList(const std::vector<TraceTaskSummaryData> &dataList) {
    std::unique_ptr<SqlitePreparedStatement> stmt = nullptr;
    std::unique_ptr<SqlitePreparedStatement> &refStmt =
        dataList.size() == CACHE_SIZE ? insertTraceTaskSummaryStmt : stmt;
    if (refStmt == nullptr) {
        refStmt = GetTraceTaskSummaryStmt(dataList.size());
    } else {
        refStmt->Reset();
    }

    if (refStmt == nullptr) {
        ServerLog::Error("Failed to create prepared statement for InsertTraceTaskSummaryList.");
        return false;
    }

    int bindIndex = bindStartIndex;
    for (const auto &data : dataList) {
        sqlite3_bind_text(refStmt->stmt, bindIndex++, data.comm.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.pid));
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.cpuId));
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.runningNs));
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.sleepingNs));
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.runnableNs));
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.csCount));
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.csInvoluntaryCount));
    }

    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!refStmt->Execute()) {
        ServerLog::Error("Insert trace task summary data fail. ", refStmt->GetErrorMessage());
        return false;
    }
    return true;
}

// ==================== trace_irq_detail 表 ====================

bool TextTraceDatabase::CreateTraceIrqDetailTable() {
    if (!isOpen) {
        ServerLog::Error("Failed to create trace_irq_detail table. Database is not open.");
        return false;
    }

    std::string sql = R"(
            CREATE TABLE IF NOT EXISTS trace_irq_detail (
                comm TEXT NOT NULL,
                pid INTEGER NOT NULL,
                cpu_id INTEGER NOT NULL,
                irq_type TEXT NOT NULL,
                irq_name TEXT NOT NULL,
                count INTEGER DEFAULT 0,
                time_ns INTEGER DEFAULT 0,
                PRIMARY KEY (comm, pid, cpu_id, irq_type, irq_name)
            );
        )";

    std::unique_lock<std::recursive_mutex> lock(mutex);
    return ExecSql(sql);
}

bool TextTraceDatabase::InsertTraceIrqDetail(const TraceIrqDetailData &data) {
    traceIrqDetailCache.emplace_back(data);
    if (traceIrqDetailCache.size() == CACHE_SIZE) {
        InsertTraceIrqDetailList(traceIrqDetailCache);
        traceIrqDetailCache.clear();
    }
    return true;
}

std::unique_ptr<SqlitePreparedStatement> TextTraceDatabase::GetTraceIrqDetailStmt(uint64_t paramLen) {
    std::string valuePlaceholders;
    for (uint64_t i = 0; i < paramLen; ++i) {
        if (i > 0) {
            valuePlaceholders += ", ";
        }
        valuePlaceholders += "(?, ?, ?, ?, ?, ?, ?)";
    }
    std::string sql = "INSERT INTO trace_irq_detail (comm, pid, cpu_id, irq_type, irq_name, count, time_ns) VALUES " +
        valuePlaceholders;
    return CreatPreparedStatement(sql);
}

bool TextTraceDatabase::InsertTraceIrqDetailList(const std::vector<TraceIrqDetailData> &dataList) {
    std::unique_ptr<SqlitePreparedStatement> stmt = nullptr;
    std::unique_ptr<SqlitePreparedStatement> &refStmt = dataList.size() == CACHE_SIZE ? insertTraceIrqDetailStmt : stmt;
    if (refStmt == nullptr) {
        refStmt = GetTraceIrqDetailStmt(dataList.size());
    } else {
        refStmt->Reset();
    }

    if (refStmt == nullptr) {
        ServerLog::Error("Failed to create prepared statement for InsertTraceIrqDetailList.");
        return false;
    }

    int bindIndex = bindStartIndex;
    for (const auto &data : dataList) {
        sqlite3_bind_text(refStmt->stmt, bindIndex++, data.comm.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.pid));
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.cpuId));
        sqlite3_bind_text(refStmt->stmt, bindIndex++, data.irqType.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(refStmt->stmt, bindIndex++, data.irqName.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.count));
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.timeNs));
    }

    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!refStmt->Execute()) {
        ServerLog::Error("Insert trace irq detail data fail. ", refStmt->GetErrorMessage());
        return false;
    }
    return true;
}

/**
 * 累加更新指定 (comm, pid, cpu_id) 行的 cs_count 和 cs_involuntary_count。
 * 用于 FtraceSchedStatisticsParseUnit 在时间统计已写入的基础上追加上下文切换数据。
 */
bool TextTraceDatabase::UpdateTraceTaskSummaryCsCount(
    const std::string &comm, uint64_t pid, int32_t cpuId, uint64_t csCount, uint64_t csInvoluntaryCount) {
    const std::string sql = "UPDATE trace_task_summary SET "
                            "cs_count = cs_count + ?, cs_involuntary_count = cs_involuntary_count + ? "
                            "WHERE comm = ? AND pid = ? AND cpu_id = ?";

    std::lock_guard<std::recursive_mutex> lock(mutex);
    if (updateTraceTaskSummaryCsStmt == nullptr) {
        updateTraceTaskSummaryCsStmt = CreatPreparedStatement(sql);
        if (updateTraceTaskSummaryCsStmt == nullptr) {
            ServerLog::Error("Create prepared statement for UpdateTraceTaskSummaryCsCount fail.");
            return false;
        }
    }

    updateTraceTaskSummaryCsStmt->Reset();
    if (!updateTraceTaskSummaryCsStmt->Execute(static_cast<int64_t>(csCount), static_cast<int64_t>(csInvoluntaryCount),
            comm, static_cast<int64_t>(pid), static_cast<int64_t>(cpuId))) {
        ServerLog::Error("Update trace_task_summary cs_count fail. ", updateTraceTaskSummaryCsStmt->GetErrorMessage());
        return false;
    }
    return true;
}

TraceIrqDetailResult TextTraceDatabase::QueryTraceIrqDetail(
    uint64_t offset, uint64_t limit, const std::string &orderBy, bool desc) {
    TraceIrqDetailResult result;

    if (!isOpen) {
        ServerLog::Error("Failed to query trace_irq_detail. Database is not open.");
        return result;
    }

    std::string sql = "SELECT comm, pid, cpu_id, irq_type, irq_name, count, time_ns, "
                      "COUNT(*) OVER () AS total_count "
                      "FROM trace_irq_detail";
    if (!orderBy.empty()) {
        sql += " ORDER BY " + orderBy + (desc ? " DESC" : " ASC");
    }
    sql += " LIMIT ? OFFSET ?";

    std::unique_lock<std::recursive_mutex> lock(mutex);

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare statement: ", sqlite3_errmsg(db));
        return result;
    }

    int bindIndex = bindStartIndex;
    sqlite3_bind_int64(stmt, bindIndex++, static_cast<int64_t>(limit));
    sqlite3_bind_int64(stmt, bindIndex++, static_cast<int64_t>(offset));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        TraceIrqDetailData data;
        data.comm = sqlite3_column_string(stmt, 0);
        data.pid = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
        data.cpuId = static_cast<int32_t>(sqlite3_column_int64(stmt, 2));
        data.irqType = sqlite3_column_string(stmt, 3);
        data.irqName = sqlite3_column_string(stmt, 4);
        data.count = static_cast<uint64_t>(sqlite3_column_int64(stmt, 5));
        data.timeNs = static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));
        result.totalCount = sqlite3_column_int64(stmt, 7);

        result.data.push_back(data);
    }

    sqlite3_finalize(stmt);
    return result;
}

// ==================== Handler 查询方法（纯单表 SELECT，无 JOIN）====================

static void AppendFtraceFilter(std::string &sql, const std::string &columnName, const std::string &value) {
    std::string sqlColumn;
    if (columnName == "cpu") {
        sqlColumn = "CAST(cpu_id AS TEXT)";
    } else if (columnName == "comm") {
        sqlColumn = "comm";
    } else if (columnName == "pid") {
        sqlColumn = "CAST(pid AS TEXT)";
    } else {
        return; // 不支持的 columnName，忽略
    }
    if (!sql.empty()) {
        sql += " AND ";
    }
    sql += sqlColumn + " LIKE '%" + value + "%'";
}

std::vector<TraceTaskSummaryData> TextTraceDatabase::QueryTraceTaskSummary(
    const SystemViewFtraceStatParams &params, uint64_t offset, uint64_t limit, uint64_t &totalCount) {
    std::vector<TraceTaskSummaryData> result;

    if (!isOpen) {
        ServerLog::Error("Failed to query trace_task_summary. Database is not open.");
        return result;
    }

    // 拼接 WHERE 子句
    std::string whereClause;
    for (const auto &filter : params.filters) {
        AppendFtraceFilter(whereClause, filter.first, filter.second);
    }
    if (!whereClause.empty()) {
        whereClause = " WHERE " + whereClause;
    }

    // ORDER BY 子句
    std::string orderDir = (params.order == "ascend") ? " ASC" : " DESC";
    auto orderByIt = FTRACE_TASK_SUMMARY_ORDER_BY_MAP.find(params.orderBy);
    std::string orderColumn = (orderByIt != FTRACE_TASK_SUMMARY_ORDER_BY_MAP.end()) ? orderByIt->second : "running_ns";
    std::string orderClause = " ORDER BY " + orderColumn + orderDir;

    // count 查询
    std::string countSql = "SELECT COUNT(*) FROM trace_task_summary" + whereClause;

    // 数据查询（纯单表 SELECT）
    std::string dataSql = QUERY_TRACE_TASK_SUMMARY_SELECT_SQL + whereClause + orderClause + " LIMIT ? OFFSET ?";

    std::unique_lock<std::recursive_mutex> lock(mutex);

    // 先查询 totalCount
    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(db, countSql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare count statement: ", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return result;
        }
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            totalCount = static_cast<uint64_t>(sqlite3_column_int64(stmt, 0));
        }
        sqlite3_finalize(stmt);
    }

    // 查询数据
    {
        sqlite3_stmt *stmt = nullptr;
        if (sqlite3_prepare_v2(db, dataSql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare data statement: ", sqlite3_errmsg(db));
            return result;
        }

        int bindIndex = bindStartIndex;
        sqlite3_bind_int64(stmt, bindIndex++, static_cast<int64_t>(limit));
        sqlite3_bind_int64(stmt, bindIndex++, static_cast<int64_t>(offset));

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            TraceTaskSummaryData data;
            data.comm = sqlite3_column_string(stmt, 0);
            data.pid = static_cast<uint64_t>(sqlite3_column_int64(stmt, 1));
            data.cpuId = static_cast<int32_t>(sqlite3_column_int64(stmt, 2));
            data.runnableNs = static_cast<uint64_t>(sqlite3_column_int64(stmt, 3));
            data.runningNs = static_cast<uint64_t>(sqlite3_column_int64(stmt, 4));
            data.sleepingNs = static_cast<uint64_t>(sqlite3_column_int64(stmt, 5));
            data.csCount = static_cast<uint64_t>(sqlite3_column_int64(stmt, 6));
            data.csInvoluntaryCount = static_cast<uint64_t>(sqlite3_column_int64(stmt, 7));
            data.softIrqCount = static_cast<uint64_t>(sqlite3_column_int64(stmt, 8));
            data.softIrqDuration = static_cast<uint64_t>(sqlite3_column_int64(stmt, 9));
            data.hardIrqCount = static_cast<uint64_t>(sqlite3_column_int64(stmt, 10));
            data.hardIrqDuration = static_cast<uint64_t>(sqlite3_column_int64(stmt, 11));

            result.push_back(data);
        }

        sqlite3_finalize(stmt);
    }

    return result;
}

// ==================== IRQ 聚合方法（Parse 阶段调用）====================

bool TextTraceDatabase::AggregateIrqToTaskSummary() {
    if (!isOpen) {
        ServerLog::Error("Failed to aggregate IRQ to task summary. Database is not open.");
        return false;
    }

    std::unique_lock<std::recursive_mutex> lock(mutex);

    // 查询 trace_irq_detail 按 (comm, pid, cpu_id, irq_type) 聚合的结果
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, QUERY_IRQ_AGGREGATE_BY_TASK_SQL.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare IRQ aggregate statement: ", sqlite3_errmsg(db));
        return false;
    }

    int updatedCount = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        std::string comm = sqlite3_column_string(stmt, 0);
        int64_t pid = sqlite3_column_int64(stmt, 1);
        int32_t cpuId = static_cast<int32_t>(sqlite3_column_int64(stmt, 2));
        std::string irqType = sqlite3_column_string(stmt, 3);
        uint64_t totalCountVal = static_cast<uint64_t>(sqlite3_column_int64(stmt, 4));
        uint64_t totalTimeVal = static_cast<uint64_t>(sqlite3_column_int64(stmt, 5));

        // 选择对应的 UPDATE SQL
        const std::string &updateSql =
            (irqType == "softirq") ? UPDATE_TASK_SUMMARY_SOFTIRQ_SQL : UPDATE_TASK_SUMMARY_HARDIRQ_SQL;

        sqlite3_stmt *updateStmt = nullptr;
        if (sqlite3_prepare_v2(db, updateSql.c_str(), -1, &updateStmt, nullptr) != SQLITE_OK) {
            ServerLog::Error("Failed to prepare IRQ update statement: ", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return false;
        }

        sqlite3_bind_int64(updateStmt, 1, static_cast<int64_t>(totalCountVal));
        sqlite3_bind_int64(updateStmt, 2, static_cast<int64_t>(totalTimeVal));
        sqlite3_bind_text(updateStmt, 3, comm.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(updateStmt, 4, static_cast<int64_t>(pid));
        sqlite3_bind_int64(updateStmt, 5, static_cast<int64_t>(cpuId));

        if (sqlite3_step(updateStmt) != SQLITE_DONE) {
            ServerLog::Warn("Failed to update IRQ aggregate for comm=", comm, " pid=", pid, " cpu_id=", cpuId);
        } else {
            updatedCount++;
        }
        sqlite3_finalize(updateStmt);
    }

    sqlite3_finalize(stmt);
    ServerLog::Info("IRQ aggregation complete. Updated ", updatedCount, " rows in trace_task_summary.");
    return true;
}
}
