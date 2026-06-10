/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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
#include "FtraceIrqAggregationParseUnit.h"
#include "ConstantDefs.h"
#include "ParseUnitManager.h"

namespace Dic::Module::FullDb {

std::string FtraceIrqAggregationParseUnit::GetUnitName() const { return Dic::FTRACE_IRQ_AGGREGATION_UNIT; }

bool FtraceIrqAggregationParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    // 确保 trace_task_summary 表存在且 IRQ 列已补齐
    if (!database->CreateTraceTaskSummaryTable()) {
        error = "Failed to ensure trace_task_summary table with IRQ columns.";
        return false;
    }
    // trace_irq_detail 表已在前面的 ParseUnit 中创建
    return true;
}

bool FtraceIrqAggregationParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database, std::string &error) {
    return database->AggregateIrqToTaskSummary();
}

// 注册到 ParseUnitManager，由 AbstractParseUnit::Handle 保证幂等性（status_info 表检查）
ParseUnitRegistrar<FtraceIrqAggregationParseUnit> unitRegFtraceIrqAgg(Dic::FTRACE_IRQ_AGGREGATION_UNIT);

}
