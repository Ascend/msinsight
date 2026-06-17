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

#ifndef DATA_INSIGHT_CORE_CONSTANTS_DEFS_H
#define DATA_INSIGHT_CORE_CONSTANTS_DEFS_H

#include <string>
#include <vector>
#include <stdint.h>

namespace Dic {
static const std::string FINISH_STATUS = "FINISH";
static const std::string NOT_FINISH_STATUS = "NOT_FINISH";
static const std::string KERNEL_PREFIX = "[Kernel]";
static const std::string MEMORY_PREFIX = "[Memory]";
// cluster parse status
static const std::string PARSE_RESULT_NONE = "none";
static const std::string PARSE_RESULT_OK = "ok";
static const std::string PARSE_RESULT_FAIL = "fail";

// dbTrace database status key
static const std::string OVERLAP_ANALYSIS_UNIT = "OVERLAP_ANALYSIS";
// FEAT: 支持上游提供 OVERLAP_ANALYSIS 表
// OVERLAP_ANALYSIS_SOURCE: 记录 OVERLAP_ANALYSIS 表的数据来源，不在 DB_STATUS_LIST 中，版本变更时不会被覆盖
// 值为 UPSTREAM 表示上游数据库提供，值为 LOCAL 表示本地生成
static const std::string OVERLAP_ANALYSIS_SOURCE = "OVERLAP_ANALYSIS_SOURCE";
static const std::string OVERLAP_ANALYSIS_SOURCE_UPSTREAM = "UPSTREAM";
static const std::string OVERLAP_ANALYSIS_SOURCE_LOCAL = "LOCAL";
static const std::string WAIT_TIME_UNIT = "WAIT_TIME";
static const std::string CONNECTION_UNIT = "CONNECTION_CATEGORY";
static const std::string FTRACE_TIME_STATISTICS_UNIT = "FTRACE_TIME_STATISTICS";
static const std::string FTRACE_IRQ_STATISTICS_UNIT = "FTRACE_IRQ_STATISTICS";
static const std::string FTRACE_SCHED_STATISTICS_UNIT = "FTRACE_SCHED_STATISTICS";
static const std::string FTRACE_IRQ_AGGREGATION_UNIT = "FTRACE_IRQ_AGGREGATION";
static std::vector<std::string> DB_STATUS_LIST = {OVERLAP_ANALYSIS_UNIT, WAIT_TIME_UNIT, CONNECTION_UNIT};
static std::vector<std::string> FTRACE_STATUS_LIST = {
    FTRACE_TIME_STATISTICS_UNIT, FTRACE_IRQ_STATISTICS_UNIT, FTRACE_SCHED_STATISTICS_UNIT, FTRACE_IRQ_AGGREGATION_UNIT};

// att model
static const std::string ATT_MODEL_DEFAULT = "all";
static const std::string ATT_MODEL_TIME = "communication_time";
static const std::string ATT_MODEL_MATRIX = "communication_matrix";

// db wrong data
static const std::string WRONG_DATA = std::to_string(UINT32_MAX);

static const std::string COMPARE = "compare";
static const std::string BASELINE = "baseline";

} // end of namespace Dic

#endif // DATA_INSIGHT_CORE_CONSTANTS_DEFS_H
