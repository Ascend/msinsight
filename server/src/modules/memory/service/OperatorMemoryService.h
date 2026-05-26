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

#ifndef PROFILER_SERVER_OPERATORMEMORYSERVICE_H
#define PROFILER_SERVER_OPERATORMEMORYSERVICE_H
#include "OperatorTable.h"
#include "OpMemoryTable.h"
namespace Dic::Module::Memory {
struct OperatorDomain {
    std::string metaType;
    uint64_t allocationTime = 0;
};
using MemoryOperatorComparator = std::function<bool(const MemoryOperator &, const MemoryOperator &)>;
using CommonStringNumberComparator = std::function<bool(const std::string &numStr1, const std::string &numStr2)>;
class OperatorMemoryService {
  public:
    explicit OperatorMemoryService(std::shared_ptr<OperatorTable> operatorTablePtr = std::make_shared<OperatorTable>(),
        std::shared_ptr<OpMemoryTable> opMemoryTablePtr = std::make_shared<OpMemoryTable>());
    OperatorDomain ComputeAllocationTimeById(const std::string &rankId, const std::string &id);
    static MemoryOperatorComparator GetComparatorByColumn(std::string_view orderBy, bool desc);

  private:
    std::shared_ptr<OperatorTable> operatorTable = nullptr;
    std::shared_ptr<OpMemoryTable> opMemoryTable = nullptr;
    // 数值类字符串通用比较器
    static CommonStringNumberComparator DefaultStringNumberComparator;

    // 释放时间戳字符串通用比较器
    static CommonStringNumberComparator DefaultReleaseTimeComparator;

    inline static std::map<std::string_view, MemoryOperatorComparator> AscComparatorMap = {
        {OpMemoryColumn::NAME,
            [](const MemoryOperator &op1, const MemoryOperator &op2) { return op1.name < op2.name; }},
        {OpMemoryColumn::SIZE,
            [](const MemoryOperator &op1, const MemoryOperator &op2) { return op1.size < op2.size; }},
        {OpMemoryColumn::ALLOCATION_TIME,
            [](const MemoryOperator &op1, const MemoryOperator &op2) {
                return DefaultStringNumberComparator(op1.allocationTime, op2.allocationTime);
            }},
        {OpMemoryColumn::RELEASE_TIME,
            [](const MemoryOperator &op1, const MemoryOperator &op2) {
                return DefaultReleaseTimeComparator(op1.releaseTime, op2.releaseTime);
            }},
        {OpMemoryColumn::DURATION,
            [](const MemoryOperator &op1, const MemoryOperator &op2) { return op1.duration < op2.duration; }},
        {OpMemoryColumn::ACTIVE_RELEASE_TIME,
            [](const MemoryOperator &op1, const MemoryOperator &op2) {
                return DefaultReleaseTimeComparator(op1.activeReleaseTime, op2.activeReleaseTime);
            }},
        {OpMemoryColumn::ACTIVE_DURATION,
            [](const MemoryOperator &op1, const MemoryOperator &op2) {
                return op1.activeDuration < op2.activeDuration;
            }},
        {OpMemoryColumn::ALLOCATION_ALLOCATED,
            [](const MemoryOperator &op1, const MemoryOperator &op2) {
                return op1.allocationAllocated < op2.allocationAllocated;
            }},
        {OpMemoryColumn::ALLOCATION_RESERVE,
            [](const MemoryOperator &op1, const MemoryOperator &op2) {
                return op1.allocationReserved < op2.allocationReserved;
            }},
        {OpMemoryColumn::ALLOCATION_ACTIVE,
            [](const MemoryOperator &op1, const MemoryOperator &op2) {
                return op1.allocationActive < op2.allocationActive;
            }},
        {OpMemoryColumn::RELEASE_ALLOCATED,
            [](const MemoryOperator &op1, const MemoryOperator &op2) {
                return op1.releaseAllocated < op2.releaseAllocated;
            }},
        {OpMemoryColumn::RELEASE_RESERVE,
            [](const MemoryOperator &op1, const MemoryOperator &op2) {
                return op1.releaseReserved < op2.releaseReserved;
            }},
        {OpMemoryColumn::RELEASE_ACTIVE,
            [](const MemoryOperator &op1, const MemoryOperator &op2) { return op1.releaseActive < op2.releaseActive; }},
        {OpMemoryColumn::STREAM,
            [](const MemoryOperator &op1, const MemoryOperator &op2) { return op1.streamId < op2.streamId; }},
        {OpMemoryColumn::DEVICE_ID,
            [](const MemoryOperator &op1, const MemoryOperator &op2) { return op1.deviceType < op2.deviceType; }}};
};
}
#endif // PROFILER_SERVER_OPERATORMEMORYSERVICE_H
