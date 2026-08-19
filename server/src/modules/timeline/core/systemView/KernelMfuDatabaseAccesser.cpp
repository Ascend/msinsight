/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
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

#include "KernelMfuDatabaseAccesser.h"

#include <algorithm>
#include <array>
#include <sstream>

#include "ServerLog.h"

namespace Dic::Module::Timeline {
namespace {
constexpr char KERNEL_MFU_TABLE[] = "OperatorMFU";

struct KernelMfuColumn {
    const char *databaseName;
    const char *protocolName;
};

const std::array<KernelMfuColumn, 13> KERNEL_MFU_COLUMNS = {{
    {"rank_id", "rankId"},
    {"op_name", "opName"},
    {"kernel_name", "kernelName"},
    {"kernel_start(ns)", "kernelStartNs"},
    {"kernel_end(ns)", "kernelEndNs"},
    {"kernel_duration(ns)", "kernelDurationNs"},
    {"mfu", "mfu"},
    {"actual_tflops", "actualTflops"},
    {"chip_peak_tflops", "chipPeakTflops"},
    {"flops", "flops"},
    {"flops_op_name", "flopsOpName"},
    {"input_shapes", "inputShapes"},
    {"output_shapes", "outputShapes"},
}};

std::string QuoteColumn(const char *columnName) { return std::string("\"") + columnName + "\""; }

const KernelMfuColumn *FindColumn(const std::string &protocolName) {
    const auto it = std::find_if(KERNEL_MFU_COLUMNS.begin(), KERNEL_MFU_COLUMNS.end(),
        [&protocolName](const auto &column) { return protocolName == column.protocolName; });
    return it == KERNEL_MFU_COLUMNS.end() ? nullptr : &(*it);
}

std::string SelectColumns() {
    std::string columns;
    for (size_t i = 0; i < KERNEL_MFU_COLUMNS.size(); ++i) {
        if (i != 0) {
            columns.append(", ");
        }
        columns.append(QuoteColumn(KERNEL_MFU_COLUMNS[i].databaseName));
    }
    return columns;
}

std::string EscapeLikeValue(const std::string &value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        if (ch == '%' || ch == '_' || ch == '\\') {
            escaped.push_back('\\');
        }
        escaped.push_back(ch);
    }
    return escaped;
}

struct FilterSql {
    std::string condition;
    std::vector<std::string> values;
};

FilterSql BuildFilterSql(const Protocol::KernelMfuListParams &params) {
    FilterSql filter;
    if (!params.rankIds.empty()) {
        filter.condition = QuoteColumn("rank_id") + " IN (";
        for (size_t i = 0; i < params.rankIds.size(); ++i) {
            if (i != 0) {
                filter.condition.append(", ");
            }
            filter.condition.push_back('?');
            filter.values.emplace_back(params.rankIds[i]);
        }
        filter.condition.append(")");
    }
    if (!params.opName.empty()) {
        if (!filter.condition.empty()) {
            filter.condition.append(" AND ");
        }
        filter.condition.append("LOWER(").append(QuoteColumn("op_name")).append(") LIKE LOWER(?) ESCAPE '\\'");
        filter.values.emplace_back("%" + EscapeLikeValue(params.opName) + "%");
    }
    if (!params.kernelName.empty()) {
        if (!filter.condition.empty()) {
            filter.condition.append(" AND ");
        }
        filter.condition.append("LOWER(").append(QuoteColumn("kernel_name")).append(") LIKE LOWER(?) ESCAPE '\\'");
        filter.values.emplace_back("%" + EscapeLikeValue(params.kernelName) + "%");
    }
    return filter;
}

std::string AddWhere(const FilterSql &filter) { return filter.condition.empty() ? "" : " WHERE " + filter.condition; }

bool BindText(sqlite3_stmt *stmt, int &index, const std::string &value) {
    return sqlite3_bind_text(stmt, index++, value.c_str(), static_cast<int>(value.size()), SQLITE_TRANSIENT) ==
        SQLITE_OK;
}

bool BindFilter(SqlitePreparedStatement &stmt, const FilterSql &filter, int &index) {
    for (const auto &value : filter.values) {
        if (!BindText(stmt.stmt, index, value)) {
            return false;
        }
    }
    return true;
}

KernelMfuQueryStatus CheckSchema(const std::shared_ptr<VirtualClusterDatabase> &database) {
    if (database == nullptr || !database->IsOpen()) {
        Server::ServerLog::Error("Kernel MFU database is not open.");
        return KernelMfuQueryStatus::FAILED;
    }
    std::vector<std::string> tables;
    if (!database->GetTableList(tables)) {
        Server::ServerLog::Error("Failed to inspect Kernel MFU database tables.");
        return KernelMfuQueryStatus::FAILED;
    }
    if (std::find(tables.begin(), tables.end(), KERNEL_MFU_TABLE) == tables.end()) {
        return KernelMfuQueryStatus::UNAVAILABLE;
    }
    for (const auto &column : KERNEL_MFU_COLUMNS) {
        if (!database->CheckColumnExist(KERNEL_MFU_TABLE, column.databaseName)) {
            return KernelMfuQueryStatus::UNAVAILABLE;
        }
    }
    return KernelMfuQueryStatus::SUCCESS;
}

void ReadRow(const SqliteResultSet &result, Protocol::KernelMfuRow &row) {
    row.rankId = result.GetString(0);
    row.opName = result.GetString(1);
    row.kernelName = result.GetString(2);
    row.kernelStartNs = result.GetUint64(3);
    row.kernelEndNs = result.GetUint64(4);
    row.kernelDurationNs = result.GetUint64(5);
    row.mfu = result.GetDouble(6);
    row.actualTflops = result.GetDouble(7);
    row.chipPeakTflops = result.GetDouble(8);
    row.flops = result.GetDouble(9);
    row.flopsOpName = result.GetString(10);
    row.inputShapes = result.GetString(11);
    row.outputShapes = result.GetString(12);
}

bool ExecuteCount(const std::shared_ptr<VirtualClusterDatabase> &database, const FilterSql &filter, uint64_t &count) {
    const std::string sql = "SELECT COUNT(*) AS rowCount FROM " + QuoteColumn(KERNEL_MFU_TABLE) + AddWhere(filter);
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        return false;
    }
    int bindIndex = 1;
    if (!BindFilter(*stmt, filter, bindIndex)) {
        return false;
    }
    auto result = stmt->ExecuteQuery();
    if (result == nullptr || !result->Next()) {
        return false;
    }
    count = result->GetUint64(static_cast<int>(0));
    return result->GetErrorCode() == SQLITE_ROW;
}

bool ExecuteRankOptions(
    const std::shared_ptr<VirtualClusterDatabase> &database, std::vector<std::string> &rankOptions) {
    const std::string sql = "SELECT DISTINCT " + QuoteColumn("rank_id") + " FROM " + QuoteColumn(KERNEL_MFU_TABLE) +
        " ORDER BY " + QuoteColumn("rank_id") + " ASC";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        return false;
    }
    auto result = stmt->ExecuteQuery();
    if (result == nullptr) {
        return false;
    }
    rankOptions.clear();
    while (result->Next()) {
        rankOptions.emplace_back(result->GetString(static_cast<int>(0)));
    }
    return result->GetErrorCode() == SQLITE_DONE;
}

std::string BuildOrderBy(const Protocol::KernelMfuListParams &params) {
    std::vector<std::string> orderItems;
    std::vector<std::string> orderedColumns;
    if (!params.orderBy.empty()) {
        const auto *column = FindColumn(params.orderBy);
        if (column == nullptr) {
            return "";
        }
        orderItems.emplace_back(QuoteColumn(column->databaseName) + (params.order == "descend" ? " DESC" : " ASC"));
        orderedColumns.emplace_back(column->databaseName);
    }
    constexpr std::array<const char *, 3> DEFAULT_ORDER = {"rank_id", "kernel_start(ns)", "kernel_end(ns)"};
    for (const auto *columnName : DEFAULT_ORDER) {
        if (std::find(orderedColumns.begin(), orderedColumns.end(), columnName) == orderedColumns.end()) {
            orderItems.emplace_back(QuoteColumn(columnName) + " ASC");
            orderedColumns.emplace_back(columnName);
        }
    }
    orderItems.emplace_back("rowid ASC");
    std::ostringstream orderBy;
    for (size_t i = 0; i < orderItems.size(); ++i) {
        if (i != 0) {
            orderBy << ", ";
        }
        orderBy << orderItems[i];
    }
    return orderBy.str();
}

bool ExecuteRows(const std::shared_ptr<VirtualClusterDatabase> &database, const Protocol::KernelMfuListParams &params,
    const FilterSql &filter, std::vector<Protocol::KernelMfuRow> &rows) {
    const std::string sql = "SELECT " + SelectColumns() + " FROM " + QuoteColumn(KERNEL_MFU_TABLE) + AddWhere(filter) +
        " ORDER BY " + BuildOrderBy(params) + " LIMIT ? OFFSET ?";
    auto stmt = database->CreatPreparedStatement(sql);
    if (stmt == nullptr) {
        return false;
    }
    int bindIndex = 1;
    if (!BindFilter(*stmt, filter, bindIndex)) {
        return false;
    }
    const uint64_t offset = (params.current - 1) * params.pageSize;
    if (sqlite3_bind_int64(stmt->stmt, bindIndex++, static_cast<sqlite3_int64>(params.pageSize)) != SQLITE_OK ||
        sqlite3_bind_int64(stmt->stmt, bindIndex, static_cast<sqlite3_int64>(offset)) != SQLITE_OK) {
        return false;
    }
    auto result = stmt->ExecuteQuery();
    if (result == nullptr) {
        return false;
    }
    while (result->Next()) {
        Protocol::KernelMfuRow row;
        ReadRow(*result, row);
        rows.emplace_back(std::move(row));
    }
    return result->GetErrorCode() == SQLITE_DONE;
}
} // namespace

KernelMfuQueryStatus KernelMfuDatabaseAccesser::CheckAvailability(
    const std::shared_ptr<VirtualClusterDatabase> &database) {
    return CheckSchema(database);
}

KernelMfuQueryStatus KernelMfuDatabaseAccesser::QueryList(const std::shared_ptr<VirtualClusterDatabase> &database,
    const Protocol::KernelMfuListParams &params, std::vector<Protocol::KernelMfuRow> &rows,
    std::vector<std::string> &rankOptions, uint64_t &count) {
    const auto schemaStatus = CheckSchema(database);
    if (schemaStatus != KernelMfuQueryStatus::SUCCESS) {
        return schemaStatus;
    }
    const FilterSql filter = BuildFilterSql(params);
    if (!ExecuteRankOptions(database, rankOptions) || !ExecuteCount(database, filter, count) ||
        !ExecuteRows(database, params, filter, rows)) {
        Server::ServerLog::Error("Failed to query Kernel MFU data.");
        return KernelMfuQueryStatus::FAILED;
    }
    return KernelMfuQueryStatus::SUCCESS;
}
} // namespace Dic::Module::Timeline
