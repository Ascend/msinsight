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

#ifndef PROFILER_SERVER_OPERATORTABLE_H
#define PROFILER_SERVER_OPERATORTABLE_H
#include "MemoryTableColum.h"
#include "MemoryTable.h"
namespace Dic::Module::Memory {
struct OperatorPO {
    uint64_t id = 0;
    std::string name;
    double size = 0;
    uint64_t allocationTime = 0;
    uint64_t releaseTime = 0;
    double duration = 0;
    uint64_t activeReleaseTime = 0;
    double activeDuration = 0;
    double allocationAllocated = 0;
    double allocationReserve = 0;
    double allocationActive = 0;
    double releaseAllocated = 0;
    double releaseReserve = 0;
    double releaseActive = 0;
    std::string stream;
};
class OperatorTable : public MemoryTable<OperatorPO> {
  public:
    OperatorTable() = default;
    ~OperatorTable() override = default;

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
        };

        return assignMap;
    }
    const std::string &GetTableName() override {
        static std::string tableName = "operator";
        return tableName;
    }
    static void SetId(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetName(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetSize(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetAllocationTime(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetReleaseTime(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetDuration(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetActiveReleaseTime(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetActiveDuration(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetAllocationAllocated(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetAllocationReserve(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetAllocationActive(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetReleaseAllocated(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetReleaseReserve(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetReleaseActive(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetStream(OperatorPO &operatorPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_OPERATORTABLE_H
