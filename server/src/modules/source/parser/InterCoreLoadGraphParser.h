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

#ifndef PROFILER_SERVER_INTERCORELOADGRAPHPARSER_H
#define PROFILER_SERVER_INTERCORELOADGRAPHPARSER_H

#include <string>
#include <vector>
#include "pch.h"
#include "NumberSafeUtil.h"
#include "SourceProtocolResponse.h"

namespace Dic {
namespace Module {
namespace Source {

struct InterCoreSubCoreDetail {
    std::string subCoreType;
    uint8_t subCoreIndex = 0;
    uint64_t cycles = 0;
    uint64_t throughput = 0;
    float hitRate = 0;
    int64_t simtVfInstructions{0};
    float simtVfInstructionPerCycle{0.0};
};

struct InterCoreOpDetail {
    uint8_t coreId = 0;
    std::vector<InterCoreSubCoreDetail> subCoreDetails = {};
};

struct InterCoreLoadAnalysisDetail {
    std::string soc;
    std::string opType;
    std::string advice;
    std::vector<InterCoreOpDetail> opDetails = {};
    std::map<std::string, uint64_t> minCycleMap;
    std::map<std::string, uint64_t> minThroughputMap;
    std::map<std::string, float> maxHitRateMap;

    void AddOpDetail(InterCoreOpDetail &&opDetail) { opDetails.emplace_back(std::move(opDetail)); }

    void SetMinCycle(const std::string &subCoreType, uint64_t cycle) {
        if (cycle == 0) {
            return;
        }
        uint64_t &minCycle = minCycleMap[subCoreType];
        if (minCycle == 0) {
            minCycle = cycle;
            return;
        }
        minCycle = std::min(minCycle, cycle);
    }

    void SetMinThroughput(const std::string &subCoreType, uint64_t throughput) {
        if (throughput == 0) {
            return;
        }
        uint64_t &minThroughput = minThroughputMap[subCoreType];
        if (minThroughput == 0) {
            minThroughput = throughput;
            return;
        }
        minThroughput = std::min(minThroughput, throughput);
    }

    void SetMaxHitRate(const std::string &subCoreType, float hitRate) {
        float &maxHitRate = maxHitRateMap[subCoreType];
        maxHitRate = (NumberUtil::IsGreater(hitRate, maxHitRate) ? hitRate : maxHitRate);
    }
};

/**
 * @brief  用于辅助计算核间负载的归一化值, 内部计算使用long double类型
 */
class SigmodStatHelper {
  public:
    long double average = {};
    int64_t count = {}; // normally the count
    long double sigma = 0.0;
    std::vector<long double> data; // copy the data
    void CalculateSigma() {
        if (data.empty()) {
            return;
        }
        count = static_cast<int64_t>(data.size());
        // 1. 计算均值
        const long double sum = std::accumulate(data.begin(), data.end(), static_cast<long double>(0));
        average = sum / static_cast<long double>(count);
        if (count <= 1) {
            sigma = 0.0;
            return;
        }

        // 2. 计算平方差之和
        auto sq_sum = static_cast<long double>(0);
        for (const long double x : data) {
            const long double diff = std::abs(x - average);
            sq_sum += diff * diff;
        }
        auto average_sq = sq_sum / static_cast<long double>(count - 1);
        // 3. 样本标准差 (n - 1)
        sigma = std::sqrt(average_sq);
    }
};

class InterCoreLoadGraphParser {
  public:
    bool GetInterCoreLoadAnalysisInfo(const std::string &json, Dic::Protocol::DetailsInterCoreLoadGraphBody &body);

  private:
    std::optional<InterCoreLoadAnalysisDetail> ParseInterCoreLoadAnalysisInfo(const std::string &json);
    void ParseJsonOpDetailArray(InterCoreLoadAnalysisDetail &analysisDetail, const json_t &jsonOpDetailArray);
    void TransformAnalysisDetail(InterCoreLoadAnalysisDetail &analysisDetail);
    void Try2MoveSubCoreDetails(InterCoreOpDetail &source, InterCoreOpDetail &dest, uint8_t subCoreIndex);

    void UpdateSigmodStats(const InterCoreSubCoreDetail &detail);
    void FinishSigmodStats();

    const uint8_t SUB_CORE_INDEX_0 = 0;
    const uint8_t SUB_CORE_INDEX_1 = 1;
    std::unordered_map<std::string, SigmodStatHelper> cacheHitRatioSigmodStats;
    std::unordered_map<std::string, SigmodStatHelper> cyclesSigmodStats;
    std::unordered_map<std::string, SigmodStatHelper> throughputSigmodStats;
    std::unordered_map<std::string, SigmodStatHelper> simtVfInstructionSigmodStats;
};

} // Dic
} // Module
} // Source

#endif // PROFILER_SERVER_INTERCORELOADGRAPHPARSER_H
