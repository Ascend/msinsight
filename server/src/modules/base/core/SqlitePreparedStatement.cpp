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

#include "SqlitePreparedStatement.h"

namespace Dic {
namespace Module {
SqlitePreparedStatement::SqlitePreparedStatement(sqlite3 *db) : db(db) {}

SqlitePreparedStatement::~SqlitePreparedStatement() {
    if (stmt != nullptr) {
        sqlite3_finalize(stmt);
        stmt = nullptr;
    }
}

bool SqlitePreparedStatement::Prepare(std::string_view sql) {
    if (stmt != nullptr) {
        sqlite3_finalize(stmt);
        stmt = nullptr;
    }
    lastErrorCode = sqlite3_prepare_v2(db, sql.data(), sql.length(), &stmt, nullptr);
    if (lastErrorCode != SQLITE_OK) {
        return false;
    }
    return true;
}

int SqlitePreparedStatement::GetErrorCode() const { return lastErrorCode; }

std::string SqlitePreparedStatement::GetErrorMessage() const { return sqlite3_errmsg(db); }

void SqlitePreparedStatement::BindParam(int index, std::string_view value) {
    lastErrorCode = sqlite3_bind_text(stmt, index, value.data(), value.length(), SQLITE_TRANSIENT);
}

void SqlitePreparedStatement::BindParam(int index, int32_t value) {
    lastErrorCode = sqlite3_bind_int(stmt, index, value);
}

void SqlitePreparedStatement::BindParam(int index, int64_t value) {
    lastErrorCode = sqlite3_bind_int64(stmt, index, value);
}

void SqlitePreparedStatement::BindParam(int index, uint32_t value) {
    lastErrorCode = sqlite3_bind_int64(stmt, index, static_cast<int64_t>(value));
}

void SqlitePreparedStatement::BindParam(int index, uint64_t value) {
    if (value > INT64_MAX) {
        value = INT64_MAX;
    }
    lastErrorCode = sqlite3_bind_int64(stmt, index, static_cast<int64_t>(value));
}

void SqlitePreparedStatement::BindParam(int index, double value) {
    lastErrorCode = sqlite3_bind_double(stmt, index, value);
}

void SqlitePreparedStatement::Reset() {
    lastErrorCode = sqlite3_reset(stmt);
    bindIndex = 1;
}

bool SqlitePreparedStatement::Execute() {
    lastErrorCode = sqlite3_step(stmt);
    return lastErrorCode == SQLITE_DONE;
}

std::unique_ptr<SqliteResultSet> SqlitePreparedStatement::ExecuteQuery() {
    if (lastErrorCode != SQLITE_OK) {
        return nullptr;
    }
    return std::make_unique<SqliteResultSet>(stmt);
}

} // end of namespace Module
} // end of namespace Dic
