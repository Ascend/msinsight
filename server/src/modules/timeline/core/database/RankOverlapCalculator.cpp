#include "RankOverlapCalculator.h"

#include <algorithm>

namespace Dic::Module::Timeline {
std::vector<RankInterval> RankOverlapCalculator::Merge(const std::vector<RankInterval> &intervals) {
    std::vector<RankInterval> valid;
    for (const auto &interval : intervals) {
        if (interval.endNs > interval.startNs) {
            valid.emplace_back(interval);
        }
    }
    std::sort(valid.begin(), valid.end(), [](const auto &left, const auto &right) {
        return left.startNs == right.startNs ? left.endNs < right.endNs : left.startNs < right.startNs;
    });
    std::vector<RankInterval> merged;
    for (const auto &interval : valid) {
        if (merged.empty() || interval.startNs > merged.back().endNs) {
            merged.emplace_back(interval);
        } else {
            merged.back().endNs = std::max(merged.back().endNs, interval.endNs);
        }
    }
    return merged;
}

std::vector<RankInterval> RankOverlapCalculator::Subtract(
    const std::vector<RankInterval> &source, const std::vector<RankInterval> &excluded) {
    std::vector<RankInterval> result;
    for (const auto &interval : source) {
        int64_t cursor = interval.startNs;
        for (const auto &block : excluded) {
            if (block.endNs <= cursor) {
                continue;
            }
            if (block.startNs >= interval.endNs) {
                break;
            }
            if (block.startNs > cursor) {
                result.push_back({cursor, std::min(block.startNs, interval.endNs)});
            }
            cursor = std::max(cursor, block.endNs);
            if (cursor >= interval.endNs) {
                break;
            }
        }
        if (cursor < interval.endNs) {
            result.push_back({cursor, interval.endNs});
        }
    }
    return result;
}

std::vector<FullDb::OVERLAP_INFO> RankOverlapCalculator::Calculate(const std::vector<RankInterval> &compute,
    const std::vector<RankInterval> &communication, const std::optional<RankInterval> &taskSpan) {
    const auto mergedCompute = Merge(compute);
    const auto mergedCommunication = Merge(communication);
    const auto communicationNotOverlapped = Subtract(mergedCommunication, mergedCompute);
    std::vector<RankInterval> activeInput = mergedCompute;
    activeInput.insert(activeInput.end(), mergedCommunication.begin(), mergedCommunication.end());
    const auto active = Merge(activeInput);
    std::vector<RankInterval> free;
    if (taskSpan.has_value() && taskSpan->endNs > taskSpan->startNs) {
        free = Subtract({taskSpan.value()}, active);
    }

    std::vector<FullDb::OVERLAP_INFO> result;
    auto append = [&result](const std::vector<RankInterval> &intervals, int64_t type) {
        for (const auto &interval : intervals) {
            result.emplace_back(interval.startNs, interval.endNs, type);
        }
    };
    append(mergedCompute, 0);
    append(mergedCommunication, 1);
    append(communicationNotOverlapped, 2);
    append(free, 3);
    std::sort(result.begin(), result.end(), [](const auto &left, const auto &right) {
        if (left.startNs != right.startNs) {
            return left.startNs < right.startNs;
        }
        if (left.endNs != right.endNs) {
            return left.endNs < right.endNs;
        }
        return left.type < right.type;
    });
    return result;
}
}
