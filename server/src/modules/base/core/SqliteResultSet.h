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

#ifndef PROFILER_SERVER_SQLITE_RESULT_SET_H
#define PROFILER_SERVER_SQLITE_RESULT_SET_H

#include <string>
#include <unordered_map>
#include <cstdint>
#include "sqlite3.h"

namespace Dic {
namespace Module {
class SqliteResultSet {
  public:
    explicit SqliteResultSet(sqlite3_stmt *stmt);
    SqliteResultSet(const SqliteResultSet &) = delete;
    SqliteResultSet(SqliteResultSet &&) = delete;
    SqliteResultSet &operator=(const SqliteResultSet &) = delete;
    SqliteResultSet &operator=(SqliteResultSet &&) = delete;
    ~SqliteResultSet() = default;
    int GetErrorCode() const;
    std::string GetErrorMessage() const;
    const std::unordered_map<std::string, int> GetColumns() const;

    /**
     * 将光标移动到下一行，如果没有下一行，返回false，需要先调用next()，再读数据
     * @return 下一行有数据，返回true，执行失败或到最后一行，返回false
     */
    // LCOV_EXCL_BR_START
    inline bool Next() {
        lastErrorCode = sqlite3_step(stmt);
        return lastErrorCode == SQLITE_ROW;
    }
    // LCOV_EXCL_BR_STOP

    inline std::string GetString(int index) const {
        const unsigned char *data = sqlite3_column_text(stmt, index);
        if (data == nullptr) {
            return "";
        }
        int len = sqlite3_column_bytes(stmt, index);
        if (len <= 0) {
            return "";
        }
        return std::string(reinterpret_cast<const char *>(data), len);
    }

    inline int32_t GetInt32(const int index) const { return sqlite3_column_int(stmt, index); }

    inline int64_t GetInt64(const int index) const { return sqlite3_column_int64(stmt, index); }

    inline uint32_t GetUint32(const int index) const {
        int32_t temp = sqlite3_column_int(stmt, index);
        if (temp < 0) {
            return 0;
        }
        return temp;
    }

    inline uint64_t GetUint64(const int index) const {
        int64_t temp = sqlite3_column_int64(stmt, index);
        if (temp < 0) {
            return 0;
        }
        return temp;
    }

    inline double GetDouble(const int index) const { return sqlite3_column_double(stmt, index); }

    inline std::string GetString(std::string_view columnName) { return GetString(columns.at(std::string(columnName))); }

    inline int32_t GetInt32(std::string_view columnName) { return GetInt32(columns.at(std::string(columnName))); }

    inline int64_t GetInt64(std::string_view columnName) { return GetInt64(columns.at(std::string(columnName))); }

    inline uint32_t GetUint32(std::string_view columnName) { return GetUint32(columns.at(std::string(columnName))); }

    inline uint64_t GetUint64(std::string_view columnName) { return GetUint64(columns.at(std::string(columnName))); }

    inline double GetDouble(std::string_view columnName) { return GetDouble(columns.at(std::string(columnName))); }

  private:
    sqlite3_stmt *stmt = nullptr;
    std::unordered_map<std::string, int> columns;
    int lastErrorCode = SQLITE_OK;
};
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_SQLITE_RESULT_SET_H
