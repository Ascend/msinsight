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
#ifndef PROFILER_SERVER_CLUSTER_STATISTICS_QUERY_H
#define PROFILER_SERVER_CLUSTER_STATISTICS_QUERY_H

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string_view>
#include "Database.h"
#include "SqlitePreparedStatement.h"

namespace Dic::Module::FullDb {
inline std::unique_ptr<SqlitePreparedStatement> PrepareStatisticsQuery(
    Database &database, const std::string &sql, std::initializer_list<std::string_view> params) {
    auto statement = database.CreatPreparedStatement(sql);
    if (statement == nullptr) {
        return nullptr;
    }
    for (auto value : params) {
        if (value.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
            return nullptr;
        }
        statement->BindParams(value);
        if (statement->GetErrorCode() != SQLITE_OK) {
            return nullptr;
        }
    }
    return statement;
}

inline bool HasStatisticsColumns(
    Database &database, const std::string &table, std::initializer_list<std::string_view> requiredColumns) {
    if (!database.CheckTableExist(table)) {
        return false;
    }
    const auto columns = database.QueryTableInfoByName(table);
    return std::all_of(requiredColumns.begin(), requiredColumns.end(), [&](std::string_view name) {
        return std::any_of(columns.begin(), columns.end(), [&](const auto &column) { return column.name == name; });
    });
}

inline bool ReadStatisticsText(sqlite3_stmt *statement, int column, std::string &value) {
    if (sqlite3_column_type(statement, column) != SQLITE_TEXT) {
        return false;
    }
    const auto *text = sqlite3_column_text(statement, column);
    if (text == nullptr) {
        return false;
    }
    value.assign(reinterpret_cast<const char *>(text), sqlite3_column_bytes(statement, column));
    return !value.empty() && value.find('\0') == std::string::npos;
}

inline bool ReadStatisticsMetric(sqlite3_stmt *statement, int column, double &value) {
    const int type = sqlite3_column_type(statement, column);
    if (type != SQLITE_FLOAT && type != SQLITE_INTEGER) {
        return false;
    }
    value = sqlite3_column_double(statement, column);
    return std::isfinite(value) && value >= 0;
}

inline bool ReadStatisticsRank(sqlite3_stmt *statement, int column, int64_t &rank) {
    if (sqlite3_column_type(statement, column) != SQLITE_INTEGER) {
        return false;
    }
    rank = sqlite3_column_int64(statement, column);
    return rank >= 0;
}
}
#endif
