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

#include "pch.h"

#include <cctype>
#include <map>

#include "NumaMetricConverter.h"

namespace Dic::Module::Numa {
namespace {
constexpr double NANOSECONDS_PER_SECOND = 1000000000.0;

std::string NormalizeMetricName(const std::string &name) {
    std::string normalized;
    normalized.reserve(name.size());
    for (const unsigned char character : name) {
        if (std::isalnum(character) != 0) {
            normalized.push_back(static_cast<char>(std::tolower(character)));
        }
    }
    return normalized.empty() ? name : normalized;
}

std::string MetricKey(const std::string &name) {
    static const std::map<std::string, std::string> keys = {
        {"Cross Socket Read Bandwidth", "crossSocketRead"},
        {"Cross SCCL Read Bandwidth", "crossScclRead"},
        {"Total Read Bandwidth", "totalRead"},
        {"Inner Read Bandwidth", "innerRead"},
        {"DRAM Read Bandwidth", "dramRead"},
        {"DRAM Write Bandwidth", "dramWrite"},
        {"LLC Bandwidth", "llcTraffic"},
    };
    const auto iter = keys.find(name);
    return iter == keys.end() ? NormalizeMetricName(name) : iter->second;
}

std::string TotalUnit(const std::string &unit) {
    if (unit == "GB/s") {
        return "GB";
    }
    if (unit == "GOps/s") {
        return "GOps";
    }
    return unit;
}

std::string MetricLabel(const std::string &name) {
    static const std::map<std::string, std::string> labels = {
        {"Cross Socket Read Bandwidth", "Cross Socket Read Traffic"},
        {"Total Read Bandwidth", "Total Read Traffic"},
        {"Inner Read Bandwidth", "Inner Read Traffic"},
        {"DRAM Read Bandwidth", "DRAM Read Traffic"},
        {"DRAM Write Bandwidth", "DRAM Write Traffic"},
        {"Cross SCCL Read Bandwidth", "Cross SCCL Read Traffic"},
        {"LLC Bandwidth", "LLC Traffic"},
    };
    const auto iter = labels.find(name);
    return iter == labels.end() ? name : iter->second;
}

bool IsRateUnit(const std::string &unit) { return unit == "GB/s" || unit == "GOps/s"; }

bool CanIntegrateRate(const NumaMetricRecord &record) {
    return record.hasValue && record.sampleCount >= 2 && record.maxTimestamp > record.minTimestamp;
}

double IntegrateRate(const NumaMetricRecord &record) {
    const double intervalCount = static_cast<double>(record.sampleCount - 1);
    const double samplingInterval =
        static_cast<double>(record.maxTimestamp - record.minTimestamp) / intervalCount / NANOSECONDS_PER_SECOND;
    return record.value * samplingInterval;
}
} // namespace

Protocol::NumaMetricData ConvertNumaMetric(const NumaMetricRecord &record) {
    const bool isRate = IsRateUnit(record.measurementUnit);
    const bool hasValue = isRate ? CanIntegrateRate(record) : record.hasValue;
    const double value = isRate && hasValue ? IntegrateRate(record) : (hasValue ? record.value : 0.0);
    return {MetricKey(record.metricName), MetricLabel(record.metricName), record.description,
        TotalUnit(record.measurementUnit), value, hasValue};
}

} // namespace Dic::Module::Numa
