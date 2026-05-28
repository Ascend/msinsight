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

#include "CounterEventHelper.h"
#include "StringUtil.h"
#include "TableDefs.h"
namespace Dic::Module::Timeline {
using namespace Dic::Protocol;
const std::map<std::string, std::string> CounterEventHelper::displayNameToValueName = {{"AI Core Freq", "freq"},
    {"Read", "read"}, {"Write", "write"}, {"L2 Buffer Bw Level", "l2BufferBwLevel"}, {"Mata Bw Level", "mataBwLevel"},
    {"DDR", "ddr"}, {"HBM", "hbm"}, {"Bandwidth", "bandwidth"}, {"Hit Rate", "hitRate"}, {"Throughput", "throughput"},
    {"Freq", "freq"}, {"Usage", "usage"}, {"Total Cycle", "totalCycle"}};
void CounterEventHelper::RegisterHostMap() {
    hostCounterEventMap.insert({PROCESS_TYPE::CPU_USAGE,
        {"CPU Usage", PROCESS_TYPE_ES.at(PROCESS_TYPE::CPU_USAGE), "usage", "CPU {cpuId}", "Usage(%)"}});
    hostCounterEventMap.insert({PROCESS_TYPE::HOST_DISK_USAGE,
        {"Disk Usage", PROCESS_TYPE_ES.at(PROCESS_TYPE::HOST_DISK_USAGE), "usage", "Disk Usage", "Usage(%)"}});
    hostCounterEventMap.insert({PROCESS_TYPE::HOST_NETWORK_USAGE,
        {"Network Usage", PROCESS_TYPE_ES.at(PROCESS_TYPE::HOST_NETWORK_USAGE), "usage", "Network Usage", "Usage(%)"}});
    hostCounterEventMap.insert({PROCESS_TYPE::HOST_MEM_USAGE,
        {"Memory Usage", PROCESS_TYPE_ES.at(PROCESS_TYPE::HOST_MEM_USAGE), "usage", "Memory Usage", "Usage(%)"}});
}

void CounterEventHelper::RegisterDeviceMap() {
    RegisterDeviceAICoreFreqMap();
    RegisterDeviceAccPMUMap();
    RegisterDeviceDDRMap();
    RegisterDeviceStarsSocMap();
    RegisterDeviceNPUMEMMap();
    RegisterDeviceHBMMap();
    RegisterDeviceLLCMap();
    RegisterDeviceSamplePMUMap();
    RegisterDeviceNICMap();
    RegisterDevicePCIeMap();
    RegisterDeviceHCCSMap();
    RegisterDeviceQOSMap();
}

void CounterEventHelper::RegisterDeviceAICoreFreqMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::AI_CORE, {"AI Core Freq", "AICORE_FREQ", "freq", "AI Core Freq", "Mhz"}});
}

void CounterEventHelper::RegisterDeviceAccPMUMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::ACC_PMU, {"ACC PMU", "ACC_PMU", "readBwLevel", "Accelerator {accId}/readBwLevel", "Level"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::ACC_PMU, {"ACC PMU", "ACC_PMU", "writeBwLevel", "Accelerator {accId}/writeBwLevel", "Level"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::ACC_PMU, {"ACC PMU", "ACC_PMU", "readOstLevel", "Accelerator {accId}/readOstLevel", "Level"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::ACC_PMU, {"ACC PMU", "ACC_PMU", "writeOstLevel", "Accelerator {accId}/writeOstLevel", "Level"}});
}

void CounterEventHelper::RegisterDeviceDDRMap() {
    deviceCounterEventMap.insert({PROCESS_TYPE::DDR, {"DDR", "DDR", "read", "Read", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::DDR, {"DDR", "DDR", "write", "Write", "Bandwidth(B/s)"}});
}

void CounterEventHelper::RegisterDeviceStarsSocMap() {
    deviceCounterEventMap.insert({PROCESS_TYPE::STARS_SOC,
        {"Stars Soc", "SOC_BANDWIDTH_LEVEL", "l2BufferBwLevel", "L2 Buffer Bw Level", "Level"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::STARS_SOC, {"Stars Soc", "SOC_BANDWIDTH_LEVEL", "mataBwLevel", "Mata Bw Level", "Level"}});
}

void CounterEventHelper::RegisterDeviceNPUMEMMap() {
    deviceCounterEventMap.insert({PROCESS_TYPE::NPU_MEM, {"NPU MEM", "NPU_MEM", "ddr", "{type:s}/DDR", "B"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NPU_MEM, {"NPU MEM", "NPU_MEM", "hbm", "{type:s}/HBM", "B"}});
}

void CounterEventHelper::RegisterDeviceHBMMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::HBM, {"HBM", "HBM", "bandwidth", "HBM {hbmId} {type:s}/Bandwidth", "Bandwidth(B/s)"}});
}

void CounterEventHelper::RegisterDeviceLLCMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::LLC, {"LLC", "LLC", "hitRate", "LLC {llcId} {mode:s}/Hit Rate", "Hit Rate(%)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::LLC, {"LLC", "LLC", "throughput", "LLC {llcId} {mode:s}/Throughput", "Throughput(B/s)"}});
}

void CounterEventHelper::RegisterDeviceSamplePMUMap() {
    deviceCounterEventMap.insert({PROCESS_TYPE::SAMPLE_PMU,
        {"SAMPLE PMU TIMELINE", "SAMPLE_PMU_TIMELINE", "freq", "{coreType:s} Core {coreId}/Freq", "Mhz"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::SAMPLE_PMU,
        {"SAMPLE PMU TIMELINE", "SAMPLE_PMU_TIMELINE", "usage", "{coreType:s} Core {coreId}/Usage", "Usage(%)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::SAMPLE_PMU,
        {"SAMPLE PMU TIMELINE", "SAMPLE_PMU_TIMELINE", "totalCycle", "{coreType:s} Core {coreId}/Total Cycle",
            "Cycle"}});
}

// NIC数据db格式的来源是NETDEV_STATS表，而不是NIC表
void CounterEventHelper::RegisterDeviceNICMap() {
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "roceTxPkt", "NIC/roceTxPkt", "Pkt"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "roceRxPkt", "NIC/roceRxPkt", "Pkt"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "roceTxErrPkt", "NIC/roceTxErrPkt", "Pkt"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "roceRxErrPkt", "NIC/roceRxErrPkt", "Pkt"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "roceTxCnpPkt", "NIC/roceTxCnpPkt", "Pkt"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "roceRxCnpPkt", "NIC/roceRxCnpPkt", "Pkt"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "roceNewPktRty", "NIC/roceNewPktRty", "Rty"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "nicTxByte", "NIC/nicTxByte", "Byte"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "nicTxBandwidth", "NIC/nicTxBandwidth", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "nicRxByte", "NIC/nicRxByte", "Byte"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "nicRxBandwidth", "NIC/nicRxBandwidth", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "macTxPfcPkt", "NIC/macTxPfcPkt", "Pkt"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "macRxPfcPkt", "NIC/macRxPfcPkt", "Pkt"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "macTxByte", "NIC/macTxByte", "Byte"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "macTxBandwidth", "NIC/macTxBandwidth", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "macRxByte", "NIC/macRxByte", "Byte"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "macRxBandwidth", "NIC/macRxBandwidth", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "macTxBadByte", "NIC/macTxBadByte", "Byte"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NIC, {"NIC", "NETDEV_STATS", "macRxBadByte", "NIC/macRxBadByte", "Byte"}});
}

void CounterEventHelper::RegisterDevicePCIeMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txPostMin", "PCIE/txPostMin", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txPostMax", "PCIE/txPostMax", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txPostAvg", "PCIE/txPostAvg", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxPostMin", "PCIE/rxPostMin", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxPostMax", "PCIE/rxPostMax", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxPostAvg", "PCIE/rxPostAvg", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostMin", "PCIE/txNonpostMin", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostMax", "PCIE/txNonpostMax", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostAvg", "PCIE/txNonpostAvg", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxNonpostMin", "PCIE/rxNonpostMin", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxNonpostMax", "PCIE/rxNonpostMax", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxNonpostAvg", "PCIE/rxNonpostAvg", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txCplMin", "PCIE/txCplMin", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txCplMax", "PCIE/txCplMax", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txCplAvg", "PCIE/txCplAvg", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxCplMin", "PCIE/rxCplMin", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxCplMax", "PCIE/rxCplMax", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxCplAvg", "PCIE/rxCplAvg", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostLatencyMin", "PCIE/txNonpostLatencyMin", "Time(ns)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostLatencyMax", "PCIE/txNonpostLatencyMax", "Time(ns)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostLatencyAvg", "PCIE/txNonpostLatencyAvg", "Time(ns)"}});
}

void CounterEventHelper::RegisterDeviceHCCSMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::HCCS, {"HCCS", "HCCS", "txThroughput", "HCCS/txThroughput", "Bandwidth(B/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::HCCS, {"HCCS", "HCCS", "rxThroughput", "HCCS/rxThroughput", "Bandwidth(B/s)"}});
}

void CounterEventHelper::RegisterDeviceQOSMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::QOS, {"QOS", "QOS", "bandwidth", "{eventName:s}/Bandwidth", "Bandwidth(B/s)"}});
}

std::string CounterEventHelper::GenerateHostMetadataSQL(const PROCESS_TYPE type) {
    CounterEventConfig config = hostCounterEventMap.at(type);
    std::string sql = "SELECT DISTINCT ";
    std::vector<std::string> valueNamesToJoin;
    std::string substitutedFormat = SubstituteThreadNameFormat(config.threadNameFormat, valueNamesToJoin);
    sql += substitutedFormat;
    sql += " AS name, '" + config.type + "' AS types FROM " + config.tableName;
    for (size_t i = 0; i < valueNamesToJoin.size(); ++i) {
        sql += " INNER JOIN " + TABLE_STRING_IDS + " AS id" + std::to_string(i) + " ON " + config.tableName + "." +
            valueNamesToJoin[i] + " = id" + std::to_string(i) + ".id";
    }
    sql += ";";
    return sql;
}

std::string CounterEventHelper::GenerateHostCounterSQL(const Dic::Module::Timeline::PROCESS_TYPE type) {
    CounterEventConfig config = hostCounterEventMap.at(type);
    std::string sql = "SELECT timestampNs - ? AS startTime, '{\"" + config.type + "\":' || " + config.valueName +
        " || '}' AS args FROM " + config.tableName;
    std::vector<std::string> valueNamesToJoin;
    std::string substitutedFormat = SubstituteThreadNameFormat(config.threadNameFormat, valueNamesToJoin);
    for (size_t i = 0; i < valueNamesToJoin.size(); ++i) {
        sql += " INNER JOIN " + TABLE_STRING_IDS + " AS id" + std::to_string(i) + " ON " + config.tableName + "." +
            valueNamesToJoin[i] + " = id" + std::to_string(i) + ".id";
    }
    sql += " WHERE " + substitutedFormat;
    sql += " = ? AND startTime >= ? AND startTime <= ? ORDER BY startTime ASC;";
    return sql;
}

std::string CounterEventHelper::GetDeviceProcessName(const Dic::Module::Timeline::PROCESS_TYPE type) {
    auto it = deviceCounterEventMap.find(type);
    if (it == deviceCounterEventMap.end()) {
        return "";
    }
    return it->second.processName;
}

std::string CounterEventHelper::GetDeviceTableName(const Dic::Module::Timeline::PROCESS_TYPE type) {
    auto it = deviceCounterEventMap.find(type);
    if (it == deviceCounterEventMap.end()) {
        return "";
    }
    return it->second.tableName;
}

std::string CounterEventHelper::GenerateDeviceMetadataSQL(const Dic::Module::Timeline::PROCESS_TYPE type) {
    std::string sql;
    for (auto [beg, end] = deviceCounterEventMap.equal_range(type); beg != end; ++beg) {
        CounterEventConfig config = beg->second;
        if (beg != deviceCounterEventMap.lower_bound(type)) {
            sql += " UNION ALL ";
        }
        sql += "SELECT DISTINCT ";
        std::vector<std::string> valueNamesToJoin;
        std::string substitutedFormat = SubstituteThreadNameFormat(config.threadNameFormat, valueNamesToJoin);
        sql += substitutedFormat;
        sql += " AS name, '" + config.type + "' AS types FROM " + config.tableName;
        for (size_t i = 0; i < valueNamesToJoin.size(); ++i) {
            sql += " INNER JOIN " + TABLE_STRING_IDS + " AS id" + std::to_string(i) + " ON " + config.tableName + "." +
                valueNamesToJoin[i] + " = id" + std::to_string(i) + ".id";
        }
        sql += " WHERE deviceId = ?";
    }
    sql += ";";
    return sql;
}

std::string CounterEventHelper::GenerateDeviceCounterSQL(
    const Dic::Module::Timeline::PROCESS_TYPE type, const std::string &threadId) {
    std::string expectedDisplayName;
    size_t index = threadId.find_last_of('/');
    if (index == std::string::npos) {
        expectedDisplayName = threadId;
    } else {
        expectedDisplayName = threadId.substr(index + 1);
    }
    std::string expectedValueName;
    if (displayNameToValueName.find(expectedDisplayName) == displayNameToValueName.end()) {
        expectedValueName = expectedDisplayName;
    } else {
        expectedValueName = displayNameToValueName.at(expectedDisplayName);
    }
    auto beg = deviceCounterEventMap.lower_bound(type);
    auto end = deviceCounterEventMap.upper_bound(type);
    for (; beg != end; ++beg) {
        if (beg->second.valueName == expectedValueName) {
            break;
        }
    }
    if (beg == end) {
        return "";
    }

    CounterEventConfig config = beg->second;
    std::string sql = "SELECT timestampNs - ? AS startTime, '{\"" + config.type + "\":' || " + config.valueName +
        " || '}' AS args FROM " + config.tableName;
    std::vector<std::string> valueNamesToJoin;
    std::string substitutedFormat = SubstituteThreadNameFormat(config.threadNameFormat, valueNamesToJoin);
    for (size_t i = 0; i < valueNamesToJoin.size(); ++i) {
        sql += " INNER JOIN " + TABLE_STRING_IDS + " AS id" + std::to_string(i) + " ON " + config.tableName + "." +
            valueNamesToJoin[i] + " = id" + std::to_string(i) + ".id";
    }
    sql += " WHERE " + substitutedFormat;
    sql += " = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    return sql;
}

std::string CounterEventHelper::SubstituteThreadNameFormat(
    const std::string &format, std::vector<std::string> &valueNamesToJoin) {
    std::string substitutedFormat = "'";
    size_t index = 0;
    while (format.find("{", index) != std::string::npos) {
        size_t nextIndex = format.find("{", index);
        substitutedFormat += format.substr(index, nextIndex - index) + "' || ";
        size_t nextBackBraceIndex = format.find("}", index);
        std::string contentInBrace = format.substr(nextIndex + 1, nextBackBraceIndex - nextIndex - 1);
        if (StringUtil::EndWith(contentInBrace, ":s")) {
            substitutedFormat += "id" + std::to_string(valueNamesToJoin.size()) + ".value";
            valueNamesToJoin.emplace_back(contentInBrace.substr(0, contentInBrace.size() - 2)); // 2 is the size of :s
        } else {
            substitutedFormat += contentInBrace;
        }
        substitutedFormat += " || '";
        index = nextBackBraceIndex + 1;
    }
    substitutedFormat += format.substr(index, format.size() - index) + "'";
    return substitutedFormat;
}
}
