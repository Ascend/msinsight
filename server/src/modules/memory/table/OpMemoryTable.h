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

#ifndef PROFILER_SERVER_OPMEMORYTABLE_H
#define PROFILER_SERVER_OPMEMORYTABLE_H
#include "MemoryTableColum.h"
#include "MemoryTable.h"
namespace Dic::Module::Memory {
struct OpMemoryPO {
    uint64_t id = 0;
    uint64_t name = 0;
    uint64_t size = 0;
    uint64_t allocationTime = 0;
    uint64_t releaseTime = 0;
    uint64_t duration = 0;
    uint64_t activeReleaseTime = 0;
    uint64_t activeDuration = 0;
    uint64_t allocationAllocated = 0;
    uint64_t allocationReserve = 0;
    uint64_t allocationActive = 0;
    uint64_t releaseAllocated = 0;
    uint64_t releaseReserve = 0;
    uint64_t releaseActive = 0;
    uint64_t stream = 0;
    uint64_t deviceId = 0;
};
class OpMemoryTable : public MemoryTable<OpMemoryPO> {
  public:
    OpMemoryTable() = default;
    ~OpMemoryTable() override = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {OpMemoryColumn::ID, SetId},
            {OpMemoryColumn::NAME, SetName},
            {OpMemoryColumn::SIZE, SetSize},
            {OpMemoryColumn::ALLOCATION_TIME, SetAllocationTime},
            {OpMemoryColumn::RELEASE_TIME, SetReleaseTime},
            {OpMemoryColumn::DURATION, SetDuration},
            {OpMemoryColumn::ACTIVE_RELEASE_TIME, SetActiveReleaseTime},
            {OpMemoryColumn::ACTIVE_DURATION, SetActiveDuration},
            {OpMemoryColumn::ALLOCATION_ALLOCATED, SetAllocationAllocated},
            {OpMemoryColumn::ALLOCATION_RESERVE, SetAllocationReserve},
            {OpMemoryColumn::ALLOCATION_ACTIVE, SetAllocationActive},
            {OpMemoryColumn::RELEASE_ALLOCATED, SetReleaseAllocated},
            {OpMemoryColumn::RELEASE_RESERVE, SetReleaseReserve},
            {OpMemoryColumn::RELEASE_ACTIVE, SetReleaseActive},
            {OpMemoryColumn::STREAM, SetStream},
            {OpMemoryColumn::DEVICE_ID, SetDevice},
        };

        return assignMap;
    }
    const std::string &GetTableName() override {
        static std::string tableName = "OP_MEMORY";
        return tableName;
    }
    static void SetId(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetName(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetSize(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetAllocationTime(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetReleaseTime(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetDuration(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetActiveReleaseTime(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetActiveDuration(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetAllocationAllocated(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetAllocationReserve(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetAllocationActive(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetReleaseAllocated(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetReleaseReserve(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetReleaseActive(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetStream(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetDevice(OpMemoryPO &opMemoryPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_OPMEMORYTABLE_H
