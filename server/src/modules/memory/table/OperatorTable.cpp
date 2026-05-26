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
#include "OperatorTable.h"
namespace Dic::Module::Memory {
void OperatorTable::SetId(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.id = resultSet->GetUint64(OpMemoryColumn::ID);
}

void OperatorTable::SetName(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.name = resultSet->GetString(OpMemoryColumn::NAME);
}

void OperatorTable::SetSize(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.size = resultSet->GetDouble(OpMemoryColumn::SIZE);
}

void OperatorTable::SetAllocationTime(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.allocationTime = resultSet->GetUint64(OpMemoryColumn::ALLOCATION_TIME);
}

void OperatorTable::SetReleaseTime(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.releaseTime = resultSet->GetUint64(OpMemoryColumn::RELEASE_TIME);
}

void OperatorTable::SetDuration(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.duration = resultSet->GetDouble(OpMemoryColumn::DURATION);
}

void OperatorTable::SetActiveReleaseTime(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.activeReleaseTime = resultSet->GetUint64(OpMemoryColumn::ACTIVE_RELEASE_TIME);
}

void OperatorTable::SetActiveDuration(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.activeDuration = resultSet->GetDouble(OpMemoryColumn::ACTIVE_DURATION);
}

void OperatorTable::SetAllocationAllocated(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.allocationAllocated = resultSet->GetDouble(OpMemoryColumn::ALLOCATION_ALLOCATED);
}

void OperatorTable::SetAllocationReserve(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.allocationReserve = resultSet->GetDouble(OpMemoryColumn::ALLOCATION_RESERVE);
}

void OperatorTable::SetAllocationActive(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.allocationActive = resultSet->GetDouble(OpMemoryColumn::ALLOCATION_ACTIVE);
}

void OperatorTable::SetReleaseAllocated(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.releaseAllocated = resultSet->GetDouble(OpMemoryColumn::RELEASE_ALLOCATED);
}

void OperatorTable::SetReleaseReserve(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.releaseReserve = resultSet->GetDouble(OpMemoryColumn::RELEASE_RESERVE);
}

void OperatorTable::SetReleaseActive(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.releaseActive = resultSet->GetDouble(OpMemoryColumn::RELEASE_ACTIVE);
}

void OperatorTable::SetStream(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    operatorPO.stream = resultSet->GetString(OpMemoryColumn::STREAM);
}
}
