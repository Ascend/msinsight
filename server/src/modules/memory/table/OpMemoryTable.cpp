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
#include "OpMemoryTable.h"
namespace Dic::Module::Memory {
void OpMemoryTable::SetId(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.id = resultSet->GetUint64(OpMemoryColumn::ID);
}

void OpMemoryTable::SetName(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.name = resultSet->GetUint64(OpMemoryColumn::NAME);
}

void OpMemoryTable::SetSize(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.size = resultSet->GetUint64(OpMemoryColumn::SIZE);
}

void OpMemoryTable::SetAllocationTime(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.allocationTime = resultSet->GetUint64(OpMemoryColumn::ALLOCATION_TIME);
}

void OpMemoryTable::SetReleaseTime(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.releaseTime = resultSet->GetUint64(OpMemoryColumn::RELEASE_TIME);
}

void OpMemoryTable::SetDuration(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.duration = resultSet->GetUint64(OpMemoryColumn::DURATION);
}

void OpMemoryTable::SetActiveReleaseTime(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.activeReleaseTime = resultSet->GetUint64(OpMemoryColumn::ACTIVE_RELEASE_TIME);
}

void OpMemoryTable::SetActiveDuration(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.activeDuration = resultSet->GetUint64(OpMemoryColumn::ACTIVE_DURATION);
}

void OpMemoryTable::SetAllocationAllocated(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.allocationAllocated = resultSet->GetUint64(OpMemoryColumn::ALLOCATION_ALLOCATED);
}

void OpMemoryTable::SetAllocationReserve(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.allocationReserve = resultSet->GetUint64(OpMemoryColumn::ALLOCATION_RESERVE);
}

void OpMemoryTable::SetAllocationActive(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.allocationActive = resultSet->GetUint64(OpMemoryColumn::ALLOCATION_ACTIVE);
}

void OpMemoryTable::SetReleaseAllocated(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.releaseAllocated = resultSet->GetUint64(OpMemoryColumn::RELEASE_ALLOCATED);
}

void OpMemoryTable::SetReleaseReserve(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.releaseReserve = resultSet->GetUint64(OpMemoryColumn::RELEASE_RESERVE);
}

void OpMemoryTable::SetReleaseActive(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.releaseActive = resultSet->GetUint64(OpMemoryColumn::RELEASE_ACTIVE);
}

void OpMemoryTable::SetStream(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.stream = resultSet->GetUint64(OpMemoryColumn::STREAM);
}

void OpMemoryTable::SetDevice(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    opMemoryPO.deviceId = resultSet->GetUint64(OpMemoryColumn::DEVICE_ID);
}
}
