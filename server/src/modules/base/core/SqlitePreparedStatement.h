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

#ifndef PROFILER_SERVER_SQLITE_PREPARED_STATEMENT_H
#define PROFILER_SERVER_SQLITE_PREPARED_STATEMENT_H

#include <string>
#include <optional>
#include <tuple>
#include <memory>
#include "sqlite3.h"
#include "SqliteResultSet.h"

namespace Dic {
namespace Module {
class SqlitePreparedStatement {
  public:
    /**
     * Constructor, 不管理sqlite3* 的生命周期，此类不是线程安全的
     * @param db 已打开的数据库指针
     */
    explicit SqlitePreparedStatement(sqlite3 *db);
    SqlitePreparedStatement(const SqlitePreparedStatement &) = delete;
    SqlitePreparedStatement(SqlitePreparedStatement &&) = delete;
    SqlitePreparedStatement &operator=(const SqlitePreparedStatement &) = delete;
    SqlitePreparedStatement &operator=(SqlitePreparedStatement &&) = delete;
    ~SqlitePreparedStatement();
    int GetErrorCode() const;
    std::string GetErrorMessage() const;

    /**
     * prepare sql
     * @param sql sql语句
     * @return 成功return true，失败返回false，可使用GetErrorMessage()获取错误信息
     */
    bool Prepare(std::string_view sql);

    /**
     * reset stmt
     */
    void Reset();

    /**
     * 执行无返回值的sql语句，需要先执行Prepare，再执行execute
     * @return 成功返回true，失败返回false
     */
    bool Execute();

    /**
     * 执行有返回值的sql语句，需要先执行Prepare，再执行executeQuery，
     * @return SqliteResultSet
     */
    std::unique_ptr<SqliteResultSet> ExecuteQuery();

    /**
     * 绑定参数，多次调用绑定索引依次增加，索引从0开始，重新绑定时需要先调用Reset()
     * @tparam Args args
     * @param args args 需要绑定的参数
     */
    template <typename... Args>
    // LCOV_EXCL_BR_START
    void BindParams(Args &&...args) {
        std::tuple<Args &&...> tp(std::forward<Args>(args)...);
        BindParamsHelper(tp, std::make_index_sequence<sizeof...(Args)>{});
    }
    // LCOV_EXCL_BR_STOP

    /**
     * 和Execute()相同，先绑定参数再执行，第二次调用前需要先执行Reset()
     * @tparam Args
     * @param args 需要绑定的参数
     * @return
     */
    template <typename... Args> bool Execute(Args &&...args) {
        BindParams(std::forward<Args>(args)...);
        return Execute();
    }

    /**
     * 和ExecuteQuery() 相同，先绑定参数再执行，第二次调用前需要先执行Reset()
     * @tparam Args
     * @param args 需要绑定的参数
     * @return
     */
    template <typename... Args> std::unique_ptr<SqliteResultSet> ExecuteQuery(Args &&...args) {
        BindParams(std::forward<Args>(args)...);
        return ExecuteQuery();
    }

    sqlite3_stmt *stmt = nullptr;
    int bindIndex = 1; // bind index start with 1

  private:
    sqlite3 *db = nullptr;
    int lastErrorCode = SQLITE_OK;

    void BindParam(int index, std::string_view value);
    void BindParam(int index, int32_t value);
    void BindParam(int index, int64_t value);
    void BindParam(int index, uint32_t value);
    void BindParam(int index, uint64_t value);
    void BindParam(int index, double value);
    template <class T> inline void BindParam(int index, std::optional<T> value) {
        if (value.has_value()) {
            BindParam(index, value.value());
        } else {
            lastErrorCode = sqlite3_bind_null(stmt, index);
        }
    }

    // LCOV_EXCL_BR_START
    template <typename Tuple, size_t... I> void BindParamsHelper(Tuple &tp, std::index_sequence<I...>) {
        (BindParam(bindIndex++, std::get<I>(tp)), ...);
    }
    // LCOV_EXCL_BR_STOP
};
} // end of namespace Module
} // end of namespace Dic

#endif // PROFILER_SERVER_SQLITE_PREPARED_STATEMENT_H
