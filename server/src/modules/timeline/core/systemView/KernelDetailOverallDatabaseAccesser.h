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
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_KERNELDETAILOVERALLDATABASEACCESSER_H
#define PROFILER_SERVER_KERNELDETAILOVERALLDATABASEACCESSER_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "DataBaseManager.h"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::Timeline {
struct KernelDetailOverallRecord {
    std::string type;
    std::string acceleratorCore;
    uint64_t count{};
    double totalDuration{};
    double avgDuration{};
    double maxDuration{};
    double minDuration{};
};

class KernelDetailOverallDatabaseAccesser {
  public:
    explicit KernelDetailOverallDatabaseAccesser(
        const std::shared_ptr<VirtualTraceDatabase> &database, const std::string &fileId)
        : database_(database), fileId_(fileId) {}

    bool GetKernelDetailOverallRecords(uint64_t startTime, uint64_t endTime, const std::string &deviceId,
        const Protocol::KernelOverallRequest::Params &params, std::vector<KernelDetailOverallRecord> &records) const;

  private:
    enum class SortDirection { ASC, DESC };
    enum class OverallSortField { TYPE, ACCELERATOR_CORE, NUMBER, TOTAL_TIME, AVG_TIME, MIN_TIME, MAX_TIME };

    static bool IsRelativeTimeValid(uint64_t relativeTime, uint64_t minTimestamp);

    bool QueryAllKernelDetails(uint64_t startTime, uint64_t endTime, const std::string &deviceId,
        const std::string &coreType, Protocol::KernelDetailsBody &body) const;

    static void SortOverallRecords(
        std::vector<KernelDetailOverallRecord> &records, OverallSortField field, SortDirection dir);

    static OverallSortField ParseOverallSortField(const std::string &orderBy);

    static void BuildOverallRecords(const Protocol::KernelDetailsBody &body,
        const Protocol::KernelOverallRequest::Params &params,
        std::map<std::string, KernelDetailOverallRecord> &groupedRecords, std::vector<std::string> &orderedKeys);

    std::shared_ptr<VirtualTraceDatabase> database_;
    std::string fileId_;
};
}

#endif // PROFILER_SERVER_KERNELDETAILOVERALLDATABASEACCESSER_H
