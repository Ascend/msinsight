/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#ifndef DIC_AICORE_FREQ_RANGE_H
#define DIC_AICORE_FREQ_RANGE_H

#include <algorithm>
#include <cmath>
#include <utility>
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"

namespace Dic::Module::Timeline {
// 频率图的边距为零，预处理将采样点对齐到首个包含它的像素桶起点，末点再向右绘制一像素。
// 使用相对 TraceTime 的坐标计算，避免绝对纳秒时间戳转换为浮点数时丢失精度。
inline std::pair<long double, long double> AICoreFreqPixelSpan(
    const Protocol::SystemViewAICoreFreqParams &params, uint64_t minTimestamp, uint64_t timestamp) {
    long double tailStart = timestamp >= minTimestamp ? static_cast<long double>(timestamp - minTimestamp) : -1;
    long double tailEnd = tailStart;
    const double relativeLast = static_cast<double>(tailStart);
    const double duration = params.viewEndTime - params.viewStartTime;
    const double step = params.chartWidth > 0 ? duration / params.chartWidth : 0;
    if (std::isfinite(params.chartWidth) && params.chartWidth > 0 && std::isfinite(params.viewStartTime) &&
        std::isfinite(params.viewEndTime) && std::isfinite(step) && step > 0 && relativeLast >= params.viewStartTime &&
        relativeLast <= params.viewEndTime) {
        double pixel = std::max(0.0, std::ceil((relativeLast - params.viewStartTime) / step) - 1);
        // 与绘图预处理的右边界比较保持一致，修正浮点临界值的桶归属。
        if (params.viewStartTime + step * (pixel + 1) < relativeLast) {
            pixel += 1;
        }
        if (pixel > 0 && params.viewStartTime + step * pixel >= relativeLast) {
            pixel -= 1;
        }
        tailStart = params.viewStartTime + step * pixel;
        tailEnd = std::min(params.viewEndTime, params.viewStartTime + step * (pixel + 1));
    }
    return {tailStart, tailEnd};
}

inline bool AICoreFreqPixelHit(
    const Protocol::SystemViewAICoreFreqParams &params, const std::pair<long double, long double> &span) {
    return params.startTime <= span.second && params.endTime >= span.first;
}

inline bool ResolveAICoreFreqRange(const Protocol::SystemViewAICoreFreqParams &params, uint64_t minTimestamp,
    uint64_t previous, uint64_t last, uint64_t &start, uint64_t &end, uint64_t first = UINT64_MAX) {
    if (first == last || first == UINT64_MAX) {
        if (!AICoreFreqPixelHit(params, AICoreFreqPixelSpan(params, minTimestamp, last))) {
            return false;
        }
        start = end = last; // 孤立点保留像素命中策略，不伪造持续时间。
        return true;
    }
    start = previous;
    end = params.endTime + minTimestamp;
    return params.startTime < params.endTime && params.startTime + minTimestamp < last && end > first;
}

struct AICoreFreqAggregate {
    uint64_t count = 0;
    uint64_t low = UINT64_MAX;
    uint64_t high = 0;
    long double sum = 0;
    long double duration = 0;
    void Add(uint64_t value, uint64_t weight = 1) {
        if (weight == 0) {
            return;
        }
        ++count;
        sum += static_cast<long double>(value) * weight;
        duration += weight;
        low = std::min(low, value);
        high = std::max(high, value);
    }
};

// 按时间顺序流式累计相邻点的有效持续时间；重复时间戳不重复计时，空白不进入分母。
class AICoreFreqSamples {
  public:
    AICoreFreqSamples(
        const Protocol::SystemViewAICoreFreqParams &params, uint64_t base, uint64_t first, uint64_t last, bool ranged)
        : start(ranged ? params.startTime + base : 0), end(ranged ? std::min(params.endTime + base, last) : UINT64_MAX),
          weighted(params.statisticsOnly), ranged(ranged), single(ranged && first == last) {}
    void Add(uint64_t timestamp, uint64_t frequency) {
        if (!weighted) {
            value.Add(frequency);
            return;
        } // 旧专家字段保留原算术统计语义。
        // 仅均值改为时长加权；范围内采样（含终点）的极值口径保持不变。
        if (timestamp >= start && timestamp <= end) {
            value.low = std::min(value.low, frequency);
            value.high = std::max(value.high, frequency);
        }
        if (seen && timestamp > previousTime) {
            multiple = true;
            AddSpan(timestamp);
        }
        previousTime = timestamp;
        previousFrequency = frequency;
        seen = true;
    }
    void Write(Protocol::ExpAnaAICoreFreqBody &body, uint64_t &high, uint64_t &low, bool statisticsOnly) {
        if (weighted && seen) {
            if (single || (!ranged && !multiple)) {
                value.Add(previousFrequency);
            } else if (ranged) {
                AddSpan(end);
            }
        }
        high = statisticsOnly ? value.high : std::max(high, value.high);
        low = statisticsOnly ? value.low : std::min(low, value.low);
        body.sampleCount = value.count;
        if (value.count == 0) {
            return;
        }
        body.avgFreq = static_cast<double>(value.sum / value.duration);
        body.minFreq = low;
        body.maxFreq = high;
    }

  private:
    void AddSpan(uint64_t until) {
        const uint64_t left = std::max(previousTime, start), right = std::min(until, end);
        if (right > left) {
            value.Add(previousFrequency, right - left);
        }
    }
    uint64_t start, end, previousTime = 0, previousFrequency = 0;
    bool weighted, ranged, single, seen = false, multiple = false;
    AICoreFreqAggregate value;
};
}
#endif
