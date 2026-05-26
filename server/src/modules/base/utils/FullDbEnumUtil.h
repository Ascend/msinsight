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

#ifndef DIC_PROTOCOL_ENUM_UTIL_H
#define DIC_PROTOCOL_ENUM_UTIL_H

#include <map>
#include <string>
#include <optional>
#include "DomainObject.h"
#include "ProtocolDefs.h"

namespace Dic {
namespace Protocol {
using namespace Module::Timeline;
template <typename ENUM> using EnumStrMap = std::map<ENUM, std::string>;
const EnumStrMap<PROCESS_TYPE> PROCESS_TYPE_ES = {{PROCESS_TYPE::ASCEND_HARDWARE, "Ascend Hardware"},
    {PROCESS_TYPE::HCCL, "HCCL"}, {PROCESS_TYPE::OVERLAP_ANALYSIS, "OVERLAP_ANALYSIS"},
    {PROCESS_TYPE::CANN_API, "CANN_API"}, {PROCESS_TYPE::API, "PYTORCH_API"}, {PROCESS_TYPE::OSRT_API, "OSRT_API"},
    {PROCESS_TYPE::LLC, "LLC"}, {PROCESS_TYPE::PCIE, "PCIE"}, {PROCESS_TYPE::NIC, "NIC"},
    {PROCESS_TYPE::SAMPLE_PMU, "SAMPLE_PMU_TIMELINE"}, {PROCESS_TYPE::HCCS, "HCCS"}, {PROCESS_TYPE::DDR, "DDR"},
    {PROCESS_TYPE::QOS, "QOS"}, {PROCESS_TYPE::ACC_PMU, "ACC_PMU"}, {PROCESS_TYPE::AI_CORE, "AICORE_FREQ"},
    {PROCESS_TYPE::NPU_MEM, "NPU_MEM"}, {PROCESS_TYPE::STARS_SOC, "SOC_BANDWIDTH_LEVEL"},
    {PROCESS_TYPE::CPU_USAGE, "CPU_USAGE"}, {PROCESS_TYPE::HOST_DISK_USAGE, "HOST_DISK_USAGE"},
    {PROCESS_TYPE::HOST_MEM_USAGE, "HOST_MEM_USAGE"}, {PROCESS_TYPE::HOST_NETWORK_USAGE, "HOST_NETWORK_USAGE"},
    {PROCESS_TYPE::MS_TX, "MSTX_EVENTS"}, {PROCESS_TYPE::HBM, "HBM"}, {PROCESS_TYPE::TEXT, "TEXT"},
    {PROCESS_TYPE::PYTHON_GC, "GC_RECORD"}, {PROCESS_TYPE::PYTHON_STACK, "PYTORCH_API_PYTHON_STACK"},
    {PROCESS_TYPE::PROCESS, "PROCESS"}};

template <typename E> std::optional<std::string> ENUM_TO_STR(const E &e) { return std::nullopt; }

template <typename E> std::optional<E> STR_TO_ENUM(const std::string &s) { return std::nullopt; }

template <typename E, typename M> std::optional<E> TryGetEnum(const M &map, const std::string &s) {
    for (const auto &iter : map) {
        if (iter.second == s) {
            return iter.first;
        }
    }
    return std::nullopt;
}

template <> inline std::optional<std::string> ENUM_TO_STR<PROCESS_TYPE>(const PROCESS_TYPE &e) {
    if (PROCESS_TYPE_ES.count(e) == 0) {
        return std::nullopt;
    }
    return PROCESS_TYPE_ES.at(e);
}

template <> inline std::optional<PROCESS_TYPE> STR_TO_ENUM<PROCESS_TYPE>(const std::string &s) {
    return TryGetEnum<PROCESS_TYPE>(PROCESS_TYPE_ES, s);
}

const std::map<std::string, PROCESS_TYPE> PROCESS_NAME_MAP_TO_PROCESS_TYPE = {
    {"Ascend Hardware", PROCESS_TYPE::ASCEND_HARDWARE}, {"CANN", PROCESS_TYPE::CANN_API}};

inline PROCESS_TYPE PROCESS_NAME_TO_TYPE(const std::string &processName) {
    if (PROCESS_NAME_MAP_TO_PROCESS_TYPE.count(processName) == 0) {
        return PROCESS_TYPE::NONE;
    }
    return PROCESS_NAME_MAP_TO_PROCESS_TYPE.at(processName);
}

#pragma endregion
} // end of namespace Protocol
} // end of namespace Dic

#endif // DIC_PROTOCOL_ENUM_UTIL_H
