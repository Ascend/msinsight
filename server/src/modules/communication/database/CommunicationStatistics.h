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
#ifndef PROFILER_SERVER_COMMUNICATION_STATISTICS_H
#define PROFILER_SERVER_COMMUNICATION_STATISTICS_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Dic::Module::Communication {
enum class StatisticsStatus { SUCCESS, UNSUPPORTED, INVALID_DATA, QUERY_FAILED };

struct RankStatisticsOptions {
    int64_t currentPage = 1;
    int64_t pageSize = 20;
    std::string orderBy = "rankId";
    std::string order = "ascend";
    std::string bandType;

    bool IsRankSortValid() const {
        return (orderBy == "rankId" || orderBy == "bandwidth") && (order == "ascend" || order == "descend") &&
            bandType.size() <= 4096 && bandType.find('\0') == std::string::npos &&
            (orderBy != "bandwidth" || !bandType.empty());
    }
};

struct GroupStatistic {
    std::string groupName;
    std::string groupId;
    std::optional<double> value;
};

struct OperatorFluctuation {
    std::string opName;
    double rangeTime = 0;
};

struct TransportBandwidth {
    std::string bandType;
    double bandwidth = 0;
};

struct RankStatistic {
    int64_t rankId = 0;
    std::vector<TransportBandwidth> bandwidths;
};

template <typename Row> struct StatisticsResult {
    StatisticsStatus status = StatisticsStatus::UNSUPPORTED;
    std::vector<Row> rows;
};

struct RankStatisticsResult {
    StatisticsStatus status = StatisticsStatus::UNSUPPORTED;
    std::vector<RankStatistic> rows;
    uint64_t total = 0;
};
}

#endif
