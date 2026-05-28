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

#ifndef PROFILER_SERVER_COMMUCATIONOPTABLE_H
#define PROFILER_SERVER_COMMUCATIONOPTABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct CommucationTaskOpPO {
    uint64_t opName = 0;
    uint64_t timestamp = 0;
    uint64_t endTime = 0;
    int64_t connectionId = 0;
    uint64_t groupName = 0;
    uint64_t opId = 0;
    uint64_t relay = 0;
    uint64_t retry = 0;
    uint64_t dataType = 0;
    uint64_t algType = 0;
    uint64_t count = 0;
    uint64_t opType = 0;
    uint64_t waitTime = 0;
};
class CommucationOpTable : public Table<CommucationTaskOpPO> {
  public:
    CommucationOpTable() = default;
    ~CommucationOpTable() = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {CommucationTaskOpColumn::OP_NAME, OpNameHandle}, {CommucationTaskOpColumn::TIMESTAMP, TimestampHandle},
            {CommucationTaskOpColumn::ENDTIME, EndTimeHandle},
            {CommucationTaskOpColumn::CONNECTION_ID, ConnectionIdHandle},
            {CommucationTaskOpColumn::GROUPNAME, GroupNameIdHandle}, {CommucationTaskOpColumn::OP_ID, OpIdHandle},
            {CommucationTaskOpColumn::RELAY, RelayHandle}, {CommucationTaskOpColumn::RETRY, RetryHandle},
            {CommucationTaskOpColumn::DATA_TYPE, DataTypeHandle}, {CommucationTaskOpColumn::ALG_TYPE, AlgTypeHandle},
            {CommucationTaskOpColumn::COUNT, CountHandle}, {CommucationTaskOpColumn::OP_TYPE, OpTypeHandle},
            {CommucationTaskOpColumn::WAIT_TIME, WaitTimeHandle}};

        return assignMap;
    }
    const std::string &GetTableName() override {
        static std::string tableName = "COMMUNICATION_OP";
        return tableName;
    }
    static void OpNameHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void TimestampHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void EndTimeHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void ConnectionIdHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void GroupNameIdHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void OpIdHandle(CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void RelayHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void RetryHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void DataTypeHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void AlgTypeHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void CountHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void OpTypeHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void WaitTimeHandle(
        CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_COMMUCATIONOPTABLE_H
