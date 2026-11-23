/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy at http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_PMU_COUNTER_TYPES_H
#define PROFILER_SERVER_PMU_COUNTER_TYPES_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace Dic::Module::Timeline {
struct PmuCounterCapabilities {
    bool available = false;
    bool hasRawValues = false;
    bool hasUnit = false;
    std::string reason;
};

struct PmuCounterInfo {
    int64_t counterId = 0;
    int64_t eventId = 0;
    int64_t cpu = 0;
    uint64_t intervalNs = 0;
    std::string name;
    int source = 0;
    std::string unit = "unknown";
    bool hasSamples = false;
};

struct PmuCounterSample {
    int64_t timestamp = 0;
    int valid = 0;
    std::optional<int64_t> rawValue;
    std::optional<int64_t> timeEnabled;
    std::optional<int64_t> timeRunning;
};

struct PmuCounterQuery {
    uint64_t startTime = 0;
    uint64_t endTime = 0;
    uint64_t timeOrigin = 0;
    uint32_t targetBuckets = 1024;
};

struct PmuCounterSegment {
    uint64_t startTime = 0;
    uint64_t endTime = 0;
    std::optional<double> value;
};

struct PmuCounterResult {
    std::string counterId;
    std::string status = "empty";
    std::string nativeUnit = "unknown";
    std::vector<PmuCounterSegment> segments;
};
}
#endif
