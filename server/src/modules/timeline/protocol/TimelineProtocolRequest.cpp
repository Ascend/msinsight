/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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

#include "TimelineProtocolRequest.h"

namespace Dic {
namespace Protocol {
void KernelDetailsParams::Check(uint64_t minTime, std::string &error) const {
    if (current == 0) {
        error = "current is invalid";
        return;
    }
    if (pageSize == 0) {
        error = "pageSize is invalid";
        return;
    }
    if (startTime > endTime) {
        error = "kernel detail start time is bigger than end time";
        return;
    }
    if (endTime > UINT64_MAX - minTime) {
        error = "kernel detail end time is invalid";
        return;
    }
}

bool SystemViewParams::CheckParams(uint64_t minTime, std::string &warnMsg) const {
    static const std::set<std::string> validLayerTypeSet = {
        "Python", "CANN", "Ascend Hardware", "HCCL", "COMMUNICATION", "Overlap Analysis"};
    if (validLayerTypeSet.find(layer) == validLayerTypeSet.end()) {
        warnMsg = "Layer is invalid";
        return false;
    }
    if (!CheckUnsignPageValid(pageSize, current, warnMsg)) {
        return false;
    }
    if (startTime > endTime) {
        warnMsg = "system view start time is bigger than end time";
        return false;
    }
    if (endTime > UINT64_MAX - minTime) {
        warnMsg = "system view end time is invalid";
        return false;
    }
    return true;
}

bool EventsViewParams::CheckParams(uint64_t minTime, std::string &warnMsg) const {
    // 检查入参合法性
    if (pid.empty()) {
        warnMsg = "Can not to query events view data while process id is empty.";
        return false;
    }
    if (!CheckUnsignPageValid(pageSize, currentPage, warnMsg)) {
        return false;
    }
    for (const auto &filter : filters) {
        if (!StringUtil::CheckSqlValid(filter.first)) {
            warnMsg = "filters exist invalid string value";
            return false;
        }
    }
    if (startTime > endTime) {
        warnMsg = "event view start time is bigger than end time";
        return false;
    }
    if (endTime > UINT64_MAX - minTime) {
        warnMsg = "event view end time is invalid";
        return false;
    }
    return true;
}

bool SystemViewOverallReqParam::CheckParams(uint64_t minTime, std::string &errMsg) const {
    if (page.pageSize == 0) {
        errMsg = "Failed to check page parameter. Page size cannot be zero.";
        return false;
    }
    if (page.current == 0) {
        errMsg = "Failed to check page parameter. Current cannot be zero.";
        return false;
    }
    if (startTime > endTime) {
        errMsg = "system view overall start time is bigger than end time";
        return false;
    }
    if (endTime > UINT64_MAX - minTime) {
        errMsg = "system view overall end time is invalid";
        return false;
    }
    return true;
}

bool UnitThreadsParams::CheckParams(uint64_t minTime, std::string &warnMsg) const {
    if (startTime > endTime) {
        warnMsg = "unit threads start time is bigger than end time";
        return false;
    }
    if (endTime > UINT64_MAX - minTime) {
        warnMsg = "unit threads end time is invalid";
        return false;
    }
    if (!CheckStrParamValidEmptyAllowed(startDepth, warnMsg)) {
        warnMsg = "unit threads start depth is invalid";
        return false;
    }
    if (!CheckStrParamValidEmptyAllowed(endDepth, warnMsg)) {
        warnMsg = "unit threads end depth is invalid";
        return false;
    }
    if (!startDepth.empty() && !endDepth.empty()) {
        if (NumberUtil::StringToUint32(startDepth) > NumberUtil::StringToUint32(endDepth)) {
            warnMsg = "unit threads start depth is bigger than end depth";
            return false;
        }
    }
    return true;
}

bool UnitThreadsOperatorsParams::CheckParams(uint64_t minTime, std::string &warnMsg) const {
    if (!StringUtil::CheckSqlValid(orderBy)) {
        warnMsg = "There is an SQL injection attack in request parameter orderBy in unit threads operators. ";
        return false;
    }
    if (startTime > endTime) {
        warnMsg = "unit threads operators start time is bigger than end time";
        return false;
    }
    if (endTime > UINT64_MAX - minTime) {
        warnMsg = "unit threads operators end time is invalid";
        return false;
    }
    if (!CheckStrParamValidEmptyAllowed(startDepth, warnMsg)) {
        warnMsg = "unit threads operators start depth is invalid";
        return false;
    }
    if (!CheckStrParamValidEmptyAllowed(endDepth, warnMsg)) {
        warnMsg = "unit threads operators end depth is invalid";
        return false;
    }
    if (!startDepth.empty() && !endDepth.empty()) {
        if (NumberUtil::StringToUint32(startDepth) > NumberUtil::StringToUint32(endDepth)) {
            warnMsg = "unit threads operators start depth is bigger than end depth";
            return false;
        }
    }
    if (processes.empty()) {
        warnMsg = "Failed to query threads same operator. process list is empty.";
        return false;
    }
    for (const auto &process : processes) {
        if (process.pid.empty() || process.tidList.empty()) {
            warnMsg = "Failed to query threads same operator. process list is invalid.";
            return false;
        }
    }
    return CheckUnsignPageValid(pageSize, current, warnMsg);
}

bool MemcpyOverallRequest::Params::CheckParams(uint64_t minTime, std::string &errMsg) const {
    if (page.pageSize == 0) {
        errMsg = "Failed to check page parameter. Page size cannot be zero.";
        return false;
    }
    if (page.current == 0) {
        errMsg = "Failed to check page parameter. Current cannot be zero.";
        return false;
    }
    if (startTime > endTime) {
        errMsg = "memcpy overall start time is bigger than end time";
        return false;
    }
    if (endTime > UINT64_MAX - minTime) {
        errMsg = "memcpy overall end time is invalid";
        return false;
    }
    return true;
}

bool KernelOverallRequest::Params::CheckParams(uint64_t minTime, std::string &errMsg) const {
    if (page.pageSize == 0) {
        errMsg = "Failed to check page parameter. Page size cannot be zero.";
        return false;
    }
    if (page.current == 0) {
        errMsg = "Failed to check page parameter. Current cannot be zero.";
        return false;
    }
    if (startTime > endTime) {
        errMsg = "kernel overall start time is bigger than end time";
        return false;
    }
    if (endTime > UINT64_MAX - minTime) {
        errMsg = "kernel overall end time is invalid";
        return false;
    }
    if (order.empty()) {
        return true;
    }
    if (order != "ascend" && order != "descend") {
        errMsg = "kernel overall order is invalid";
        return false;
    }
    if (orderBy != "type" && orderBy != "acceleratorCore" && orderBy != "number" && orderBy != "totalTime" &&
        orderBy != "avgTime" && orderBy != "minTime" && orderBy != "maxTime") {
        errMsg = "kernel overall orderBy is invalid";
        return false;
    }
    return true;
}
} // namespace Protocol
} // namespace Dic
