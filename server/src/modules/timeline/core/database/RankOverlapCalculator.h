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
