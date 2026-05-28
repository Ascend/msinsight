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
#include "PytorchApiTable.h"
namespace Dic::Module::Timeline {
void PytorchApiTable::SetId(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.id = resultSet->GetUint64(PytorchApiColumn::ID);
}

void PytorchApiTable::SetTimestamp(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.timestamp = resultSet->GetUint64(PytorchApiColumn::TIMESTAMP);
}

void PytorchApiTable::SetEndTime(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.endTime = resultSet->GetUint64(PytorchApiColumn::ENDTIME);
}

void PytorchApiTable::SetGlobalTid(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.globalTid = resultSet->GetUint64(PytorchApiColumn::GLOBAL_TID);
}

void PytorchApiTable::SetConnectionId(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.connectionId = resultSet->GetUint64(PytorchApiColumn::CONNECTIONID);
}

void PytorchApiTable::SetName(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.name = resultSet->GetUint64(PytorchApiColumn::NAME);
}

void PytorchApiTable::SetSequenceNumber(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.sequenceNumber = resultSet->GetString(PytorchApiColumn::SEQUENCE_NUMBER);
}

void PytorchApiTable::SetFwdThreadId(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.fwdThreadId = resultSet->GetString(PytorchApiColumn::FWD_THREAD_ID);
}

void PytorchApiTable::SetInputDtypes(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.inputDtypes = resultSet->GetString(PytorchApiColumn::INPUT_DTYPES);
}

void PytorchApiTable::SetInputShapes(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.inputShapes = resultSet->GetString(PytorchApiColumn::INPUT_SHAPES);
}

void PytorchApiTable::SetCallchainId(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.callchainId = resultSet->GetString(PytorchApiColumn::CALL_CHAIN_ID);
}

void PytorchApiTable::SetType(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    pytorchApiPO.type = resultSet->GetUint64(PytorchApiColumn::TYPE);
}
}
