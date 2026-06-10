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

#include "TextTraceDatabase.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;
using namespace Dic::Protocol;

// ==================== TextTraceDatabase 方法实现 ====================

bool TextTraceDatabase::CreateFtraceTable() {
    if (!isOpen) {
        ServerLog::Error("Failed to create ftrace table. Database is not open.");
        return false;
    }

    std::string sql = R"(
            CREATE TABLE IF NOT EXISTS ftrace_analysis (
                track_id INTEGER NOT NULL,
                data_type INTEGER NOT NULL,
                args TEXT,
                PRIMARY KEY (track_id, data_type)
            );
        )";

    std::unique_lock<std::recursive_mutex> lock(mutex);
    return ExecSql(sql);
}

bool TextTraceDatabase::InsertFtraceStat(const FtraceStatisticsData &event) {
    ftraceStatCache.emplace_back(event);
    if (ftraceStatCache.size() == CACHE_SIZE) {
        InsertFtraceStatList(ftraceStatCache);
        ftraceStatCache.clear();
    }
    return true;
}

std::unique_ptr<SqlitePreparedStatement> TextTraceDatabase::GetFtraceStmt(uint64_t paramLen) {
    std::string valuePlaceholders;
    for (uint64_t i = 0; i < paramLen; ++i) {
        if (i > 0) {
            valuePlaceholders += ", ";
        }
        valuePlaceholders += "(?, ?, ?)";
    }
    std::string sql = "INSERT INTO ftrace_analysis (track_id, data_type, args) VALUES " + valuePlaceholders;
    return CreatPreparedStatement(sql);
}

bool TextTraceDatabase::InsertFtraceStatList(const std::vector<FtraceStatisticsData> &dataList) {
    std::unique_ptr<SqlitePreparedStatement> stmt = nullptr;
    std::unique_ptr<SqlitePreparedStatement> &refStmt = dataList.size() == CACHE_SIZE ? insertFtraceStatStmt : stmt;
    if (refStmt == nullptr) {
        refStmt = GetFtraceStmt(dataList.size());
    } else {
        refStmt->Reset();
    }

    int bindIndex = bindStartIndex;
    for (const auto &data : dataList) {
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int64_t>(data.trackId));
        sqlite3_bind_int64(refStmt->stmt, bindIndex++, static_cast<int8_t>(data.dataType));
        sqlite3_bind_text(refStmt->stmt, bindIndex++, data.GetArgs().c_str(), -1, SQLITE_TRANSIENT);
    }

    std::unique_lock<std::recursive_mutex> lock(mutex);
    if (!refStmt->Execute()) {
        ServerLog::Error("Insert ftrace stat data fail. ", refStmt->GetErrorMessage());
        return false;
    }
    return true;
}

FtraceStatistics TextTraceDatabase::QueryFtraceStatistics(FtraceDataType dataType, uint64_t offset, uint64_t limit) {
    FtraceStatistics result;

    if (!isOpen) {
        ServerLog::Error("Failed to query ftrace statistics. Database is not open.");
        return result;
    }

    std::unique_lock<std::recursive_mutex> lock(mutex);

    std::string sql = "SELECT track_id, data_type, args, COUNT(*) OVER () AS total_count FROM "
                      "ftrace_analysis WHERE data_type = ? LIMIT ? OFFSET ?";
    sqlite3_stmt *stmt = nullptr;

    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        ServerLog::Error("Failed to prepare statement: ", sqlite3_errmsg(db));
        return result;
    }
    int bindIndex = bindStartIndex;
    sqlite3_bind_int64(stmt, bindIndex++, static_cast<int64_t>(dataType));
    sqlite3_bind_int64(stmt, bindIndex++, static_cast<int64_t>(limit));
    sqlite3_bind_int64(stmt, bindIndex++, static_cast<int64_t>(offset));

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        FtraceStatisticsData data;
        data.trackId = sqlite3_column_int64(stmt, 0);
        data.dataType = static_cast<FtraceDataType>(sqlite3_column_int(stmt, 1));

        std::string argsStr = sqlite3_column_string(stmt, 2);
        if (!argsStr.empty()) {
            data.SetArgs(argsStr);
        }
        result.totalCount = sqlite3_column_int64(stmt, 3);

        result.data.push_back(data);
    }

    sqlite3_finalize(stmt);
    return result;
}

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
                PRIMARY KEY (comm, pid, cpu_id)
            );
        )";

    std::unique_lock<std::recursive_mutex> lock(mutex);
    return ExecSql(sql);
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

}
