/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "PmuCounterCalculator.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Dic::Module::Timeline {
namespace {
constexpr uint32_t MAX_BUCKETS = 4096;
constexpr uint64_t MAX_SAFE_TIMESTAMP = 9007199254740991ULL;
constexpr size_t MAX_RUNS_PER_BUCKET = 8;

bool SameSample(const PmuCounterSample &left, const PmuCounterSample &right) {
    return left.timestamp == right.timestamp && left.valid == right.valid && left.rawValue == right.rawValue &&
        left.timeEnabled == right.timeEnabled && left.timeRunning == right.timeRunning;
}

bool IsValid(const PmuCounterSample &sample) {
    return sample.valid == 1 && sample.rawValue && sample.timeEnabled && sample.timeRunning && *sample.rawValue >= 0 &&
        *sample.timeEnabled >= 0 && *sample.timeRunning >= 0 && *sample.timeRunning <= *sample.timeEnabled;
}

struct Interval {
    uint64_t start = 0;
    uint64_t end = 0;
    std::optional<long double> value;
};

struct Bucket {
    std::vector<Interval> intervals;
    bool fragmented = false;
};
}

class PmuCounterCalculator::Impl {
  public:
    Impl(const PmuCounterInfo &counter, const PmuCounterCapabilities &capabilities, const PmuCounterQuery &request)
        : query(request) {
        query.targetBuckets = std::clamp(query.targetBuckets, 1u, MAX_BUCKETS);
        buckets.resize(query.targetBuckets);
        result.counterId = std::to_string(counter.counterId);
        result.nativeUnit = counter.unit == "count" || counter.unit == "ns" ? counter.unit : "unknown";
        supported = capabilities.available && capabilities.hasRawValues;
        if (!supported) {
            result.status = "unsupported";
        } else if (query.endTime < query.startTime || query.endTime > MAX_SAFE_TIMESTAMP ||
            query.timeOrigin > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) ||
            query.endTime > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) - query.timeOrigin) {
            supported = false;
            result.status = "invalid-data";
        }
    }

    void Consume(const PmuCounterSample &sample) {
        if (finished || !supported || query.startTime == query.endTime) {
            return;
        }
        if (pending && pending->timestamp == sample.timestamp) {
            if (!SameSample(*pending, sample)) {
                pending->valid = 0;
            }
            return;
        }
        if (pending) {
            Process(*pending);
        }
        pending = sample;
    }

    PmuCounterResult Finish() {
        if (finished) {
            return result;
        }
        finished = true;
        if (!supported) {
            return result;
        }
        if (pending) {
            Process(*pending);
            pending.reset();
        }
        if (invalidTimeline) {
            result.status = "invalid-data";
            return result;
        }
        for (const auto &bucket : buckets) {
            for (const auto &interval : bucket.intervals) {
                const std::optional<double> value =
                    interval.value ? std::optional<double>(static_cast<double>(*interval.value)) : std::nullopt;
                result.segments.push_back({interval.start, interval.end, value});
            }
        }
        result.status = hasValidVisible ? "ok" : hasInvalidVisible ? "invalid-data" : "empty";
        return result;
    }

  private:
    PmuCounterQuery query;
    PmuCounterResult result;
    std::optional<PmuCounterSample> pending;
    std::optional<PmuCounterSample> previous;
    std::vector<Bucket> buckets;
    bool supported = false;
    bool finished = false;
    bool invalidTimeline = false;
    bool hasValidVisible = false;
    bool hasInvalidVisible = false;

    size_t BucketIndex(uint64_t timestamp) const {
        if (timestamp <= query.startTime || query.endTime <= query.startTime) {
            return 0;
        }
        if (timestamp >= query.endTime) {
            return query.targetBuckets - 1;
        }
        const long double fraction = static_cast<long double>(timestamp - query.startTime) /
            static_cast<long double>(query.endTime - query.startTime);
        return std::min(
            static_cast<size_t>(fraction * query.targetBuckets), static_cast<size_t>(query.targetBuckets - 1));
    }

    void Process(const PmuCounterSample &sample) {
        if (sample.timestamp < 0 || static_cast<uint64_t>(sample.timestamp) < query.timeOrigin) {
            previous.reset();
            return;
        }
        const auto timestamp = static_cast<uint64_t>(sample.timestamp) - query.timeOrigin;
        if (timestamp > MAX_SAFE_TIMESTAMP || (previous && sample.timestamp <= previous->timestamp)) {
            invalidTimeline = true;
            previous.reset();
            return;
        }
        if (timestamp >= query.startTime && timestamp <= query.endTime && !IsValid(sample)) {
            hasInvalidVisible = true;
        }
        if (previous) {
            const auto start = static_cast<uint64_t>(previous->timestamp) - query.timeOrigin;
            // Retain whole measurements, but discard pure context intervals before aggregation.
            if (start < query.endTime && timestamp > query.startTime) {
                Interval interval{start, timestamp, ComputeDelta(*previous, sample)};
                hasValidVisible = hasValidVisible || interval.value.has_value();
                hasInvalidVisible = hasInvalidVisible || !interval.value.has_value();
                Append(std::move(interval));
            }
        }
        previous = sample;
    }

    static std::optional<long double> ComputeDelta(const PmuCounterSample &left, const PmuCounterSample &right) {
        if (!IsValid(left) || !IsValid(right) || *right.rawValue < *left.rawValue ||
            *right.timeEnabled <= *left.timeEnabled || *right.timeRunning <= *left.timeRunning) {
            return std::nullopt;
        }
        // Subtract integers before floating conversion. Never difference the upstream scaled cumulative value.
        const int64_t raw = *right.rawValue - *left.rawValue;
        const int64_t enabled = *right.timeEnabled - *left.timeEnabled;
        const int64_t running = *right.timeRunning - *left.timeRunning;
        if (running > enabled) {
            return std::nullopt;
        }
        const long double value = static_cast<long double>(raw) * enabled / running;
        return std::isfinite(static_cast<double>(value)) ? std::optional<long double>(value) : std::nullopt;
    }

    void Append(Interval interval) {
        auto &bucket = buckets[BucketIndex(interval.end)];
        if (bucket.fragmented) {
            bucket.intervals.front().end = interval.end;
            return;
        }
        if (!bucket.intervals.empty()) {
            auto &last = bucket.intervals.back();
            if (last.end == interval.start && last.value.has_value() == interval.value.has_value()) {
                last.end = interval.end;
                if (last.value) {
                    *last.value += *interval.value;
                    if (!std::isfinite(static_cast<double>(*last.value))) {
                        last.value.reset();
                    }
                }
                return;
            }
        }
        bucket.intervals.emplace_back(std::move(interval));
        if (bucket.intervals.size() > MAX_RUNS_PER_BUCKET) {
            // An overfull bucket remains a gap; never connect or invent values across invalid reads.
            const Interval missing{bucket.intervals.front().start, bucket.intervals.back().end, std::nullopt};
            bucket.intervals.assign(1, missing);
            bucket.fragmented = true;
        }
    }
};

PmuCounterCalculator::PmuCounterCalculator(
    const PmuCounterInfo &counter, const PmuCounterCapabilities &capabilities, const PmuCounterQuery &query)
    : impl(std::make_unique<Impl>(counter, capabilities, query)) {}

PmuCounterCalculator::~PmuCounterCalculator() = default;

void PmuCounterCalculator::Consume(const PmuCounterSample &sample) { impl->Consume(sample); }

PmuCounterResult PmuCounterCalculator::Finish() { return impl->Finish(); }

PmuCounterResult PmuCounterCalculator::Calculate(const std::vector<PmuCounterSample> &samples,
    const PmuCounterInfo &counter, const PmuCounterCapabilities &capabilities, const PmuCounterQuery &query) {
    PmuCounterCalculator calculator(counter, capabilities, query);
    for (const auto &sample : samples) {
        calculator.Consume(sample);
    }
    return calculator.Finish();
}
}
