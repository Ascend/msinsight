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

#include "KernelDetailOverallDatabaseAccesser.h"

#include <algorithm>
#include <limits>
#include <map>

#include "Paginator.h"
#include "ServerLog.h"
#include "StringUtil.h"

namespace Dic::Module::Timeline {
namespace {
constexpr uint64_t QUERY_ALL_CURRENT = 1;
constexpr uint64_t QUERY_ALL_PAGE_SIZE = std::numeric_limits<uint32_t>::max();

std::string BuildOverallKey(const std::string &type, const std::string &acceleratorCore) {
    return std::to_string(type.size()) + ":" + type + "|" + std::to_string(acceleratorCore.size()) + ":" +
        acceleratorCore;
}

struct KernelDetailOverallFilter {
    std::string columnName;
    std::string value;
};

std::vector<KernelDetailOverallFilter> BuildKernelDetailOverallFilters(
    const std::vector<std::pair<std::string, std::string>> &filters) {
    std::vector<KernelDetailOverallFilter> overallFilters;
    overallFilters.reserve(filters.size());
    for (const auto &[columnName, value] : filters) {
        overallFilters.emplace_back(KernelDetailOverallFilter{columnName, StringUtil::ToLower(value)});
    }
    return overallFilters;
}

bool IsDetailMatchedFilters(
    const Protocol::KernelDetail &detail, const std::vector<KernelDetailOverallFilter> &filters) {
    for (const auto &[columnName, value] : filters) {
        if (columnName == "type" && !StringUtil::Contains(StringUtil::ToLower(detail.type), value)) {
            return false;
        }
        if (columnName == "acceleratorCore" &&
            !StringUtil::Contains(StringUtil::ToLower(detail.acceleratorCore), value)) {
            return false;
        }
    }
    return true;
}
}

bool KernelDetailOverallDatabaseAccesser::IsRelativeTimeValid(uint64_t relativeTime, uint64_t minTimestamp) {
    return relativeTime <= std::numeric_limits<uint64_t>::max() - minTimestamp;
}

bool KernelDetailOverallDatabaseAccesser::QueryAllKernelDetails(uint64_t startTime, uint64_t endTime,
    const std::string &deviceId, const std::string &coreType, Protocol::KernelDetailsBody &body) const {
    if (!database_ || fileId_.empty()) {
        return false;
    }

    const uint64_t minTimestamp = TraceTime::Instance().GetStartTime();
    if (!IsRelativeTimeValid(startTime, minTimestamp) || !IsRelativeTimeValid(endTime, minTimestamp)) {
        Server::ServerLog::Error("Time conversion overflow: relative time + base timestamp exceeds uint64_t limit");
        return false;
    }

    Protocol::KernelDetailsParams params;
    params.orderBy = "startTime";
    params.order = "ascend";
    params.current = QUERY_ALL_CURRENT;
    params.pageSize = QUERY_ALL_PAGE_SIZE;
    params.startTime = startTime;
    params.endTime = endTime;
    params.deviceId = deviceId;
    params.coreType = coreType;

    return database_->QueryKernelDetailData(params, body, minTimestamp);
}

bool KernelDetailOverallDatabaseAccesser::GetKernelDetailOverallRecords(uint64_t startTime, uint64_t endTime,
    const std::string &deviceId, const Protocol::KernelOverallRequest::Params &params,
    std::vector<KernelDetailOverallRecord> &records) const {
    Protocol::KernelDetailsBody body;
    if (!QueryAllKernelDetails(startTime, endTime, deviceId, "", body)) {
        return false;
    }

    std::map<std::string, KernelDetailOverallRecord> groupedRecords;
    std::vector<std::string> orderedKeys;
    BuildOverallRecords(body, params, groupedRecords, orderedKeys);

    records.clear();
    records.reserve(groupedRecords.size());
    for (const auto &key : orderedKeys) {
        auto &record = groupedRecords.at(key);
        record.avgDuration = record.count == 0 ? 0 : record.totalDuration / static_cast<double>(record.count);
        records.emplace_back(record);
    }
    if (!params.order.empty()) {
        SortOverallRecords(records, ParseOverallSortField(params.orderBy),
            params.order == "ascend" ? SortDirection::ASC : SortDirection::DESC);
    }
    return true;
}

void KernelDetailOverallDatabaseAccesser::SortOverallRecords(
    std::vector<KernelDetailOverallRecord> &records, OverallSortField field, SortDirection dir) {
    std::sort(records.begin(), records.end(), [field, dir](const auto &lhs, const auto &rhs) {
        const bool ascend = dir == SortDirection::ASC;
        bool equal = false;
        bool result = false;
        switch (field) {
        case OverallSortField::TYPE:
            equal = lhs.type == rhs.type;
            result = ascend ? lhs.type < rhs.type : lhs.type > rhs.type;
            break;
        case OverallSortField::ACCELERATOR_CORE:
            equal = lhs.acceleratorCore == rhs.acceleratorCore;
            result = ascend ? lhs.acceleratorCore < rhs.acceleratorCore : lhs.acceleratorCore > rhs.acceleratorCore;
            break;
        case OverallSortField::NUMBER:
            equal = lhs.count == rhs.count;
            result = ascend ? lhs.count < rhs.count : lhs.count > rhs.count;
            break;
        case OverallSortField::AVG_TIME:
            equal = lhs.avgDuration == rhs.avgDuration;
            result = ascend ? lhs.avgDuration < rhs.avgDuration : lhs.avgDuration > rhs.avgDuration;
            break;
        case OverallSortField::MIN_TIME:
            equal = lhs.minDuration == rhs.minDuration;
            result = ascend ? lhs.minDuration < rhs.minDuration : lhs.minDuration > rhs.minDuration;
            break;
        case OverallSortField::MAX_TIME:
            equal = lhs.maxDuration == rhs.maxDuration;
            result = ascend ? lhs.maxDuration < rhs.maxDuration : lhs.maxDuration > rhs.maxDuration;
            break;
        case OverallSortField::TOTAL_TIME:
        default:
            equal = lhs.totalDuration == rhs.totalDuration;
            result = ascend ? lhs.totalDuration < rhs.totalDuration : lhs.totalDuration > rhs.totalDuration;
            break;
        }
        return equal ? lhs.type < rhs.type : result;
    });
}

KernelDetailOverallDatabaseAccesser::OverallSortField KernelDetailOverallDatabaseAccesser::ParseOverallSortField(
    const std::string &orderBy) {
    if (orderBy == "type") {
        return OverallSortField::TYPE;
    }
    if (orderBy == "acceleratorCore") {
        return OverallSortField::ACCELERATOR_CORE;
    }
    if (orderBy == "number") {
        return OverallSortField::NUMBER;
    }
    if (orderBy == "avgTime") {
        return OverallSortField::AVG_TIME;
    }
    if (orderBy == "minTime") {
        return OverallSortField::MIN_TIME;
    }
    if (orderBy == "maxTime") {
        return OverallSortField::MAX_TIME;
    }
    return OverallSortField::TOTAL_TIME;
}

void KernelDetailOverallDatabaseAccesser::BuildOverallRecords(const Protocol::KernelDetailsBody &body,
    const Protocol::KernelOverallRequest::Params &params,
    std::map<std::string, KernelDetailOverallRecord> &groupedRecords, std::vector<std::string> &orderedKeys) {
    const auto filters = BuildKernelDetailOverallFilters(params.filters);
    for (const auto &detail : body.kernelDetails) {
        if (!IsDetailMatchedFilters(detail, filters)) {
            continue;
        }
        const std::string key = BuildOverallKey(detail.type, detail.acceleratorCore);
        auto &record = groupedRecords[key];
        if (record.count == 0) {
            orderedKeys.emplace_back(key);
            record.type = detail.type;
            record.acceleratorCore = detail.acceleratorCore;
            record.minDuration = detail.duration;
            record.maxDuration = detail.duration;
        }
        ++record.count;
        record.totalDuration += detail.duration;
        record.minDuration = std::min(record.minDuration, detail.duration);
        record.maxDuration = std::max(record.maxDuration, detail.duration);
    }
}
}
