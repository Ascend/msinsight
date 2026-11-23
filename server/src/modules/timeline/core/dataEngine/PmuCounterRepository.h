/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 * You may obtain a copy at http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_PMU_COUNTER_REPOSITORY_H
#define PROFILER_SERVER_PMU_COUNTER_REPOSITORY_H

#include "Database.h"
#include "PmuCounterTypes.h"
#include <functional>

namespace Dic::Module::Timeline {
class PmuCounterRepository {
  public:
    explicit PmuCounterRepository(Database &database) : database(database) {}
    PmuCounterCapabilities DetectCapabilities();
    bool ListCounters(std::vector<PmuCounterInfo> &counters, const PmuCounterCapabilities &capabilities);
    bool GetCounter(
        int64_t counterId, std::optional<PmuCounterInfo> &counter, const PmuCounterCapabilities &capabilities);
    bool ReadWindowWithNeighbors(int64_t counterId, uint64_t absoluteStart, uint64_t absoluteEnd,
        const PmuCounterCapabilities &capabilities, const std::function<void(const PmuCounterSample &)> &consume);

  private:
    Database &database;
    bool ReadCounters(std::vector<PmuCounterInfo> &counters, const PmuCounterCapabilities &capabilities,
        std::optional<int64_t> counterId);
};
}
#endif
