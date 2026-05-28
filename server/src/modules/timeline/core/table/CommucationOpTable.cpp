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
#include "CommucationOpTable.h"
namespace Dic::Module::Timeline {
void CommucationOpTable::OpNameHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.opName = resultSet->GetUint64(CommucationTaskOpColumn::OP_NAME);
}
void CommucationOpTable::TimestampHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.timestamp = resultSet->GetUint64(CommucationTaskOpColumn::TIMESTAMP);
}
void CommucationOpTable::EndTimeHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.endTime = resultSet->GetUint64(CommucationTaskOpColumn::ENDTIME);
}
void CommucationOpTable::ConnectionIdHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.connectionId = resultSet->GetInt64(CommucationTaskOpColumn::CONNECTION_ID);
}
void CommucationOpTable::GroupNameIdHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.groupName = resultSet->GetUint64(CommucationTaskOpColumn::GROUPNAME);
}
void CommucationOpTable::OpIdHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.opId = resultSet->GetUint64(CommucationTaskOpColumn::OP_ID);
}
void CommucationOpTable::RelayHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.relay = resultSet->GetUint64(CommucationTaskOpColumn::RELAY);
}
void CommucationOpTable::RetryHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.retry = resultSet->GetUint64(CommucationTaskOpColumn::RETRY);
}
void CommucationOpTable::DataTypeHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.dataType = resultSet->GetUint64(CommucationTaskOpColumn::DATA_TYPE);
}
void CommucationOpTable::AlgTypeHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.algType = resultSet->GetUint64(CommucationTaskOpColumn::ALG_TYPE);
}
void CommucationOpTable::CountHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.count = resultSet->GetUint64(CommucationTaskOpColumn::COUNT);
}
void CommucationOpTable::OpTypeHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.opType = resultSet->GetUint64(CommucationTaskOpColumn::OP_TYPE);
}
void CommucationOpTable::WaitTimeHandle(
    CommucationTaskOpPO &commucationTaskOpPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskOpPO.waitTime = resultSet->GetUint64(CommucationTaskOpColumn::WAIT_TIME);
}
}
