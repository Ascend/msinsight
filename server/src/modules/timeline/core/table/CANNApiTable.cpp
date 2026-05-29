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
#include "CANNApiTable.h"
namespace Dic::Module::Timeline {
void CANNApiTable::SetId(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    cannApiPO.id = resultSet->GetUint64(CANNApiColumn::ID);
}

void CANNApiTable::SetTimestamp(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    cannApiPO.timestamp = resultSet->GetUint64(CANNApiColumn::TIMESTAMP);
}

void CANNApiTable::SetEndTime(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    cannApiPO.endTime = resultSet->GetUint64(CANNApiColumn::ENDTIME);
}

void CANNApiTable::SetType(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    cannApiPO.type = resultSet->GetUint64(CANNApiColumn::TYPE);
}

void CANNApiTable::SetGlobalTid(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    cannApiPO.globalTid = resultSet->GetUint64(CANNApiColumn::GLOBAL_TID);
}

void CANNApiTable::SetName(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    cannApiPO.name = resultSet->GetUint64(CANNApiColumn::NAME);
}

void CANNApiTable::SetDepth(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    cannApiPO.depth = resultSet->GetUint64(CANNApiColumn::DEPTH);
}
}
