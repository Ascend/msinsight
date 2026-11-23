/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy at http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_PMU_COUNTER_CALCULATOR_H
#define PROFILER_SERVER_PMU_COUNTER_CALCULATOR_H

#include "PmuCounterTypes.h"
#include <memory>

namespace Dic::Module::Timeline {
class PmuCounterCalculator {
  public:
    PmuCounterCalculator(
        const PmuCounterInfo &counter, const PmuCounterCapabilities &capabilities, const PmuCounterQuery &query);
    ~PmuCounterCalculator();
    PmuCounterCalculator(const PmuCounterCalculator &) = delete;
    PmuCounterCalculator &operator=(const PmuCounterCalculator &) = delete;
    void Consume(const PmuCounterSample &sample);
    PmuCounterResult Finish();
    static PmuCounterResult Calculate(const std::vector<PmuCounterSample> &samples, const PmuCounterInfo &counter,
        const PmuCounterCapabilities &capabilities, const PmuCounterQuery &query);

  private:
    class Impl;
    std::unique_ptr<Impl> impl;
};
}
#endif
