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
 * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_TEXTTRACEDATABASEFTRACESQL_H
#define PROFILER_SERVER_TEXTTRACEDATABASEFTRACESQL_H
#include <string>
#include <unordered_map>

namespace Dic::Module::Timeline {

// ==================== trace_task_summary 查询 SQL 常量 ====================

const std::string QUERY_TRACE_TASK_SUMMARY_SELECT_SQL =
    "SELECT comm, pid, cpu_id, runnable_ns, running_ns, sleeping_ns, "
    "cs_count, cs_involuntary_count, soft_irq_count, soft_irq_duration, "
    "hard_irq_count, hard_irq_duration FROM trace_task_summary";

// orderBy 字段映射（前端 dataIndex → SQL 列名）
const std::unordered_map<std::string, std::string> FTRACE_TASK_SUMMARY_ORDER_BY_MAP = {
    {"cpu", "cpu_id"},
    {"comm", "comm"},
    {"pid", "pid"},
    {"runnable", "runnable_ns"},
    {"running", "running_ns"},
    {"sleeping", "sleeping_ns"},
    {"context_switch_count", "cs_count"},
    {"soft_irq_count", "soft_irq_count"},
    {"soft_irq_duration", "soft_irq_duration"},
    {"hard_irq_count", "hard_irq_count"},
    {"hard_irq_duration", "hard_irq_duration"},
};

// IRQ 聚合 UPDATE SQL（Parse 阶段调用，将 IRQ 数据聚合后写入 trace_task_summary）
const std::string UPDATE_TASK_SUMMARY_SOFTIRQ_SQL =
    "UPDATE trace_task_summary SET "
    "soft_irq_count = soft_irq_count + ?, soft_irq_duration = soft_irq_duration + ? "
    "WHERE comm = ? AND pid = ? AND cpu_id = ?";

const std::string UPDATE_TASK_SUMMARY_HARDIRQ_SQL =
    "UPDATE trace_task_summary SET "
    "hard_irq_count = hard_irq_count + ?, hard_irq_duration = hard_irq_duration + ? "
    "WHERE comm = ? AND pid = ? AND cpu_id = ?";

// IRQ 聚合查询 SQL（Parse 阶段按 (comm, pid, cpu_id, irq_type) 聚合 trace_irq_detail）
const std::string QUERY_IRQ_AGGREGATE_BY_TASK_SQL =
    "SELECT comm, pid, cpu_id, irq_type, SUM(count) AS total_count, SUM(time_ns) AS total_time "
    "FROM trace_irq_detail GROUP BY comm, pid, cpu_id, irq_type";

} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_TEXTTRACEDATABASEFTRACESQL_H
