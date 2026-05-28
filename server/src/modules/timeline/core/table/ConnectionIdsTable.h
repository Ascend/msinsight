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

#ifndef PROFILER_SERVER_CONNECTIONIDSTABLE_H
#define PROFILER_SERVER_CONNECTIONIDSTABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct ConnectionIdsPO {
    uint64_t id = 0;
    uint64_t connectionId = 0;
};
class ConnectionIdsTable : public Table<ConnectionIdsPO> {
  public:
    ConnectionIdsTable() = default;
    ~ConnectionIdsTable() override = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {ConnectionIdsColumn::ID, SetId},
            {ConnectionIdsColumn::CONNECTIONID, SetConnectionId},
        };
        return assignMap;
    }

    const std::string &GetTableName() override {
        static std::string tableName = "CONNECTION_IDS";
        return tableName;
    }
    static void SetId(ConnectionIdsPO &connectionIdsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetConnectionId(ConnectionIdsPO &connectionIdsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_CONNECTIONIDSTABLE_H
