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

#ifndef PROFILER_SERVER_RANKOVERLAPCALCULATOR_H
#define PROFILER_SERVER_RANKOVERLAPCALCULATOR_H

#include <cstdint>
#include <optional>
#include <vector>

#include "DbTraceDataBase.h"

namespace Dic::Module::Timeline {
struct RankInterval {
    int64_t startNs = 0;
    int64_t endNs = 0;
};

class RankOverlapCalculator {
  public:
    static std::vector<FullDb::OVERLAP_INFO> Calculate(const std::vector<RankInterval> &compute,
        const std::vector<RankInterval> &communication, const std::optional<RankInterval> &taskSpan);

  private:
    static std::vector<RankInterval> Merge(const std::vector<RankInterval> &intervals);
    static std::vector<RankInterval> Subtract(
        const std::vector<RankInterval> &source, const std::vector<RankInterval> &excluded);
};
}

#endif
