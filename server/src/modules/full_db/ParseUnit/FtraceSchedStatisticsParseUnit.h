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

#ifndef PROFILER_SERVER_FTRACE_SCHED_STATISTICS_PARSE_UNIT_H
#define PROFILER_SERVER_FTRACE_SCHED_STATISTICS_PARSE_UNIT_H

#include "AbstractParseUnit.h"
#include "TextTraceDatabase.h"

namespace Dic::Module::FullDb {

class FtraceSchedStatisticsParseUnit : public AbstractParseUnit<Timeline::TextTraceDatabase> {
  protected:
    std::string GetUnitName() const override;
    bool PreCheck(const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database,
        std::string &error) override;
    bool HandleParseProcess(const ParseUnitParams &params, const std::shared_ptr<Timeline::TextTraceDatabase> &database,
        std::string &error) override;
};

}
#endif // PROFILER_SERVER_FTRACE_SCHED_STATISTICS_PARSE_UNIT_H
