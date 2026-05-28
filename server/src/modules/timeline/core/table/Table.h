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

#ifndef PROFILER_SERVER_TABLE_H
#define PROFILER_SERVER_TABLE_H
#include <string>
#include <memory>
#include <vector>
#include <variant>
#include <functional>
#include <unordered_map>
#include "SqlitePreparedStatement.h"
#include "SqliteResultSet.h"
#include "sqlite3.h"
#include "DataBaseManager.h"
#include "ServerLog.h"
namespace Dic::Module::Timeline {
using namespace Dic::Server;
enum class TableOrder {
    ASC,
    DESC,
};
struct SqlStruct {
    std::string conditionStr;
    std::string orderByStr;
    std::string groupByStr;
    std::string selectStr;
};
template <typename T> class Table {
  public:
    Table() noexcept = default;
    Table &Select(std::string_view str) {
        if (std::empty(SelectStr())) {
            SelectStr() = "SELECT " + std::string(str);
        } else {
            SelectStr() += "," + std::string(str);
        }
        auto it = GetAssignMap().find(str);
        if (it != GetAssignMap().end()) {
            AssignFuncs().emplace_back(it->second);
        } else {
            ServerLog::Warn("Select column is not exist");
        }
        return *this;
    }

    template <typename... Args> Table &Select(std::string_view str, const Args &...args) {
        if (std::empty(SelectStr())) {
            SelectStr() = "SELECT " + std::string(str);
        } else {
            SelectStr() += " , " + std::string(str);
        }
        auto it = GetAssignMap().find(str);
        if (it != GetAssignMap().end()) {
            AssignFuncs().emplace_back(it->second);
        } else {
            ServerLog::Warn("Select column is not exist");
        }
        Select(args...);
        return *this;
    }

    Table &Eq(std::string_view str, std::variant<uint32_t, uint64_t, std::string> value) {
        ConditionStr() += " AND " + std::string(str) + " = ? ";
        Values().emplace_back(value);
        return *this;
    }

    Table &NotEq(std::string_view str, std::variant<uint32_t, uint64_t, std::string> value) {
        ConditionStr() += " AND " + std::string(str) + " != ? ";
        Values().emplace_back(value);
        return *this;
    }

    Table &Less(std::string_view str, std::variant<uint32_t, uint64_t, std::string> value) {
        ConditionStr() += " AND " + std::string(str) + " < ? ";
        Values().emplace_back(value);
        return *this;
    }

    Table &LessEq(std::string_view str, std::variant<uint32_t, uint64_t, std::string> value) {
        ConditionStr() += " AND " + std::string(str) + " <= ? ";
        Values().emplace_back(value);
        return *this;
    }

    Table &Greater(std::string_view str, std::variant<uint32_t, uint64_t, std::string> value) {
        ConditionStr() += " AND " + std::string(str) + " > ? ";
        Values().emplace_back(value);
        return *this;
    }

    Table &GreaterEq(std::string_view str, std::variant<uint32_t, uint64_t, std::string> value) {
        ConditionStr() += " AND " + std::string(str) + " >= ? ";
        Values().emplace_back(value);
        return *this;
    }

    Table &Like(std::string_view str, std::string value) {
        ConditionStr() += " AND " + std::string(str) + " LIKE ? ";
        Values().emplace_back(value);
        return *this;
    }

    template <typename Y>
    static inline constexpr bool is_one_of_basic_types =
        std::disjunction_v<std::is_same<Y, uint32_t>, std::is_same<Y, uint64_t>, std::is_same<Y, std::string>>;

    template <typename Y> std::string SaveParamListAndGetPlaceholderStr(const std::vector<Y> &inputList) {
        static_assert(is_one_of_basic_types<Y>, "Fail to save param and get placeholder str, unknown type.");
        std::string res;
        for (size_t i = 0; i < inputList.size(); ++i) {
            if (i == 0) {
                res.append("?");
            } else {
                res.append(", ?");
            }
            Values().emplace_back(inputList[i]);
        }
        return res;
    }

    /**
     * 调用此函数需要先校验inputs不为空
     * @param str
     * @param inputs
     * @return
     */
    template <typename Y> Table &In(std::string_view str, const std::vector<Y> &inputs) {
        ConditionStr() += " AND " + std::string(str) + " IN ( ";
        std::string placeholderStr = SaveParamListAndGetPlaceholderStr(inputs);
        ConditionStr() += placeholderStr + " ) ";
        return *this;
    }

    template <typename Y> Table &NotIn(std::string_view str, const std::vector<Y> &inputs) {
        ConditionStr() += " AND " + std::string(str) + " NOT IN ( ";
        std::string placeholderStr = SaveParamListAndGetPlaceholderStr(inputs);
        ConditionStr() += placeholderStr + " ) ";
        return *this;
    }

    Table &OrderBy(std::string_view columnName, TableOrder order) {
        if (std::empty(OrderByStr())) {
            OrderByStr() = " ORDER BY " + std::string(columnName);
        } else {
            OrderByStr() += " , " + std::string(columnName);
        }
        if (order == TableOrder::DESC) {
            OrderByStr() += " DESC ";
        } else {
            OrderByStr() += " ASC ";
        }
        return *this;
    }

    Table &GroupBy(std::string_view columnName) {
        if (std::empty(GroupByStr())) {
            GroupByStr() = " GROUP BY " + std::string(columnName);
        } else {
            GroupByStr() += " , " + std::string(columnName);
        }
        return *this;
    }

    virtual std::vector<T> ExcuteQuery(const std::string &fileId) {
        std::vector<T> result;
        ExcuteQuery(fileId, result);
        return result;
    }

    virtual void ExcuteQuery(const std::string &fileId, std::vector<T> &result) {
        auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(fileId);
        if (database == nullptr) {
            ClearThreadLocal();
            return;
        }
        std::string sql =
            SelectStr() + " FROM " + GetTableName() + " WHERE 1 = 1 " + ConditionStr() + OrderByStr() + GroupByStr();
        auto stmt = database->CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            ServerLog::Warn(GetTableName() + " Failed to get stmt.");
            ClearThreadLocal();
            return;
        }
        for (const auto &item : Values()) {
            // 访问 variant 中存储的值
            if (std::holds_alternative<uint32_t>(item)) {
                stmt->BindParams(std::get<uint32_t>(item));
            } else if (std::holds_alternative<uint64_t>(item)) {
                stmt->BindParams(std::get<uint64_t>(item));
            } else if (std::holds_alternative<std::string>(item)) {
                stmt->BindParams(std::get<std::string>(item));
            }
        }
        auto resultSet = stmt->ExecuteQuery();
        if (resultSet == nullptr) {
            ServerLog::Warn(GetTableName() + " Failed to get result set.", stmt->GetErrorMessage());
            ClearThreadLocal();
            return;
        }
        while (resultSet->Next()) {
            T t;
            for (const auto &item : AssignFuncs()) {
                item(t, resultSet);
            }
            result.emplace_back(t);
        }
        ClearThreadLocal();
    }

    virtual void ExcuteQuery(sqlite3 *db, std::vector<T> &result) {
        auto stmt = std::make_unique<SqlitePreparedStatement>(db);
        if (stmt == nullptr) {
            ServerLog::Warn(GetTableName() + " Failed to get stmt.");
            ClearThreadLocal();
            return;
        }
        std::string sql =
            SelectStr() + " FROM " + GetTableName() + " WHERE 1 = 1 " + ConditionStr() + OrderByStr() + GroupByStr();
        if (!stmt->Prepare(sql)) {
            ServerLog::Error("Failed prepare sql. ", stmt->GetErrorMessage());
            ClearThreadLocal();
            return;
        }
        for (const auto &item : Values()) {
            // 访问 variant 中存储的值
            if (std::holds_alternative<uint32_t>(item)) {
                stmt->BindParams(std::get<uint32_t>(item));
            } else if (std::holds_alternative<uint64_t>(item)) {
                stmt->BindParams(std::get<uint64_t>(item));
            } else if (std::holds_alternative<std::string>(item)) {
                stmt->BindParams(std::get<std::string>(item));
            }
        }
        auto resultSet = stmt->ExecuteQuery();
        if (resultSet == nullptr) {
            ServerLog::Warn(GetTableName() + " Failed to get result set.", stmt->GetErrorMessage());
            ClearThreadLocal();
            return;
        }
        while (resultSet->Next()) {
            T t;
            for (const auto &item : AssignFuncs()) {
                item(t, resultSet);
            }
            result.emplace_back(t);
        }
        ClearThreadLocal();
    }

    virtual uint64_t Count(const std::string &fileId) {
        auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(fileId);
        if (database == nullptr) {
            ClearThreadLocal();
            return 0;
        }
        uint64_t count = 0;
        std::string sql = "SELECT COUNT(*) AS count FROM " + GetTableName() + " WHERE 1 = 1 " + ConditionStr();
        auto stmt = database->CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            ServerLog::Warn(GetTableName() + " Failed to get stmt.");
            ClearThreadLocal();
            return count;
        }
        for (const auto &item : Values()) {
            // 访问 variant 中存储的值
            if (std::holds_alternative<uint32_t>(item)) {
                stmt->BindParams(std::get<uint32_t>(item));
            } else if (std::holds_alternative<uint64_t>(item)) {
                stmt->BindParams(std::get<uint64_t>(item));
            } else if (std::holds_alternative<std::string>(item)) {
                stmt->BindParams(std::get<std::string>(item));
            }
        }
        auto resultSet = stmt->ExecuteQuery();
        if (resultSet == nullptr) {
            ServerLog::Warn(GetTableName() + " Failed to get result set.", stmt->GetErrorMessage());
            ClearThreadLocal();
            return count;
        }
        if (resultSet->Next()) {
            count = resultSet->GetUint64("count");
        }
        ClearThreadLocal();
        return count;
    }

    virtual std::string GetDbPath(const std::string &fileId) {
        auto database = DataBaseManager::Instance().GetTraceDatabaseByRankId(fileId);
        if (database == nullptr) {
            std::string empty;
            return empty;
        }
        const std::string nameKey = database->GetDbPath();
        return nameKey;
    }

    virtual ~Table() = default;

  protected:
    using assign = std::function<void(T &, const std::unique_ptr<SqliteResultSet> &)>;

    inline std::string &SelectStr() const { return GetSqlStruct().selectStr; }

    inline std::string &ConditionStr() const { return GetSqlStruct().conditionStr; }

    inline std::string &OrderByStr() const { return GetSqlStruct().orderByStr; }

    inline std::string &GroupByStr() const { return GetSqlStruct().groupByStr; }

    inline SqlStruct &GetSqlStruct() const {
        thread_local SqlStruct sqlStruct;
        return sqlStruct;
    }

    std::vector<std::variant<uint32_t, uint64_t, std::string>> &Values() {
        thread_local std::vector<std::variant<uint32_t, uint64_t, std::string>> values;
        return values;
    }

    std::vector<assign> &AssignFuncs() {
        thread_local std::vector<assign> assignFuncs;
        return assignFuncs;
    }

    /* *
     * 每次调用完成需要重置threadlocal变量
     */
    void ClearThreadLocal() {
        AssignFuncs().clear();
        Values().clear();
        ConditionStr().clear();
        SelectStr().clear();
        OrderByStr().clear();
        GroupByStr().clear();
    }

    virtual const std::unordered_map<std::string_view, assign> &GetAssignMap() = 0;
    virtual const std::string &GetTableName() = 0;
};
}

#endif // PROFILER_SERVER_TABLE_H
