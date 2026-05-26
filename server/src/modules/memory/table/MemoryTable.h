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
#ifndef PROFILER_SERVER_MEMORYTABLE_H
#define PROFILER_SERVER_MEMORYTABLE_H
#include <vector>
#include <string>
#include <cstdint>
#include "Table.h"
namespace Dic::Module::Memory {
template <typename T> class MemoryTable : public Timeline::Table<T> {
  public:
    virtual std::vector<T> ExcuteQuery(const std::string &fileId) {
        std::vector<T> result;
        ExcuteQuery(fileId, result);
        return result;
    }

    virtual void ExcuteQuery(const std::string &fileId, std::vector<T> &result) {
        auto database = FullDb::DataBaseManager::Instance().GetMemoryDatabaseByRankId(fileId);
        if (database == nullptr) {
            this->ClearThreadLocal();
            return;
        }
        std::string sql = this->SelectStr() + " FROM " + this->GetTableName() + " WHERE 1 = 1 " + this->ConditionStr() +
            this->OrderByStr() + this->GroupByStr();
        auto stmt = database->CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            Dic::Server::ServerLog::Warn(this->GetTableName() + " Failed to get stmt.");
            this->ClearThreadLocal();
            return;
        }
        for (const auto &item : this->Values()) {
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
            Dic::Server::ServerLog::Warn(this->GetTableName() + " Failed to get result set.", stmt->GetErrorMessage());
            this->ClearThreadLocal();
            return;
        }
        while (resultSet->Next()) {
            T t;
            for (const auto &item : this->AssignFuncs()) {
                item(t, resultSet);
            }
            result.emplace_back(t);
        }
        this->ClearThreadLocal();
    }

    virtual void ExcuteQuery(sqlite3 *db, std::vector<T> &result) {
        auto stmt = std::make_unique<SqlitePreparedStatement>(db);
        if (stmt == nullptr) {
            Dic::Server::ServerLog::Warn(this->GetTableName() + " Failed to get stmt.");
            this->ClearThreadLocal();
            return;
        }
        std::string sql = this->SelectStr() + " FROM " + this->GetTableName() + " WHERE 1 = 1 " + this->ConditionStr() +
            this->OrderByStr() + this->GroupByStr();
        if (!stmt->Prepare(sql)) {
            Dic::Server::ServerLog::Error("Failed prepare sql. ", stmt->GetErrorMessage());
            this->ClearThreadLocal();
            return;
        }
        for (const auto &item : this->Values()) {
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
            Dic::Server::ServerLog::Warn(this->GetTableName() + " Failed to get result set.", stmt->GetErrorMessage());
            this->ClearThreadLocal();
            return;
        }
        while (resultSet->Next()) {
            T t;
            for (const auto &item : this->AssignFuncs()) {
                item(t, resultSet);
            }
            result.emplace_back(t);
        }
        this->ClearThreadLocal();
    }

    virtual uint64_t Count(const std::string &fileId) {
        auto database = FullDb::DataBaseManager::Instance().GetMemoryDatabaseByRankId(fileId);
        if (database == nullptr) {
            this->ClearThreadLocal();
            return 0;
        }
        uint64_t count = 0;
        std::string sql =
            "SELECT COUNT(*) AS count FROM " + this->GetTableName() + " WHERE 1 = 1 " + this->ConditionStr();
        auto stmt = database->CreatPreparedStatement(sql);
        if (stmt == nullptr) {
            Dic::Server::ServerLog::Warn(this->GetTableName() + " Failed to get stmt.");
            this->ClearThreadLocal();
            return count;
        }
        for (const auto &item : this->Values()) {
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
            Dic::Server::ServerLog::Warn(this->GetTableName() + " Failed to get result set.", stmt->GetErrorMessage());
            this->ClearThreadLocal();
            return count;
        }
        if (resultSet->Next()) {
            count = resultSet->GetUint64("count");
        }
        this->ClearThreadLocal();
        return count;
    }
};
};
#endif // PROFILER_SERVER_MEMORYTABLE_H
