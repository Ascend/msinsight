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

#include "SqliteResultSet.h"

namespace Dic {
namespace Module {
SqliteResultSet::SqliteResultSet(sqlite3_stmt *stmt) : stmt(stmt) {
    if (stmt != nullptr) {
        int columnCount = sqlite3_column_count(stmt);
        for (int i = 0; i < columnCount; ++i) {
            columns.emplace(sqlite3_column_name(stmt, i), i);
        }
    }
}

int SqliteResultSet::GetErrorCode() const { return lastErrorCode; }

std::string SqliteResultSet::GetErrorMessage() const { return sqlite3_errmsg(sqlite3_db_handle(stmt)); }

const std::unordered_map<std::string, int> SqliteResultSet::GetColumns() const { return columns; }
} // end of namespace Module
} // end of namespace Dic
