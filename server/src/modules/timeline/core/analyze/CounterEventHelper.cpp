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
namespace {
const std::string AI_CORE_FREQ_LEGACY_NAME = "AI Core Freq";
const std::string AI_CORE_FREQ_DIE_PREFIX = "AI Core Freq Die ";
}
const std::map<std::string, std::string> CounterEventHelper::displayNameToValueName = {{"AI Core Freq", "freq"},
    {"Read", "read"}, {"Write", "write"}, {"L2 Buffer Bw Level", "l2BufferBwLevel"}, {"Mata Bw Level", "mataBwLevel"},
    {"DDR", "ddr"}, {"HBM", "hbm"}, {"Bandwidth", "bandwidth"}, {"Hit Rate", "hitRate"}, {"Throughput", "throughput"},
    {"Freq", "freq"}, {"Usage", "usage"}, {"Total Cycle", "totalCycle"}, {"dat_rx", "rxDat"}, {"dat_tx", "txDat"},
    {"req_rx", "rxReq"}, {"req_tx", "txReq"}, {"rsp_rx", "rxRsp"}, {"rsp_tx", "txRsp"}, {"snp_rx", "rxSnp"},
    {"snp_tx", "txSnp"}};
void CounterEventHelper::RegisterHostMap() {
    hostCounterEventMap.insert({PROCESS_TYPE::CPU_USAGE,
        {"CPU Usage", PROCESS_TYPE_ES.at(PROCESS_TYPE::CPU_USAGE), "usage", "CPU {cpuId}", "Usage(%)"}});
    hostCounterEventMap.insert({PROCESS_TYPE::CPU_FREQ,
        {"CPU Freq", PROCESS_TYPE_ES.at(PROCESS_TYPE::CPU_FREQ), "freq", "CPU {cpuId}", "Frequency(KHz)"}});
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
    RegisterDeviceRoCEMap();
    RegisterDeviceNetDevStatsMap();
    RegisterDevicePCIeMap();
    RegisterDeviceHCCSMap();
    RegisterDeviceQOSMap();
    RegisterDeviceSIOMap();
    RegisterDeviceUBMap();
}

void CounterEventHelper::RegisterDeviceAICoreFreqMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::AI_CORE, {"AI Core Freq", "AICORE_FREQ", "freq", "AI Core Freq", "Frequency(Mhz)"}});
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
    deviceCounterEventMap.insert({PROCESS_TYPE::DDR, {"DDR", "DDR", "read", "Read", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::DDR, {"DDR", "DDR", "write", "Write", "Bandwidth(Byte/s)"}});
}

void CounterEventHelper::RegisterDeviceStarsSocMap() {
    deviceCounterEventMap.insert({PROCESS_TYPE::STARS_SOC,
        {"Stars Soc", "SOC_BANDWIDTH_LEVEL", "l2BufferBwLevel", "L2 Buffer Bw Level", "Level"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::STARS_SOC, {"Stars Soc", "SOC_BANDWIDTH_LEVEL", "mataBwLevel", "Mata Bw Level", "Level"}});
}

void CounterEventHelper::RegisterDeviceNPUMEMMap() {
    deviceCounterEventMap.insert({PROCESS_TYPE::NPU_MEM, {"NPU MEM", "NPU_MEM", "ddr", "{type:s}/DDR", "Byte"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NPU_MEM, {"NPU MEM", "NPU_MEM", "hbm", "{type:s}/HBM", "Byte"}});
}

void CounterEventHelper::RegisterDeviceHBMMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::HBM, {"HBM", "HBM", "bandwidth", "HBM {hbmId} {type:s}/Bandwidth", "Bandwidth(Byte/s)"}});
}

void CounterEventHelper::RegisterDeviceLLCMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::LLC, {"LLC", "LLC", "hitRate", "LLC {llcId} {mode:s}/Hit Rate", "Hit Rate(%)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::LLC, {"LLC", "LLC", "throughput", "LLC {llcId} {mode:s}/Throughput", "Throughput(Byte/s)"}});
}

void CounterEventHelper::RegisterDeviceSamplePMUMap() {
    deviceCounterEventMap.insert({PROCESS_TYPE::SAMPLE_PMU,
        {"SAMPLE PMU TIMELINE", "SAMPLE_PMU_TIMELINE", "freq", "{coreType:s} Core {coreId}/Freq", "Frequency(Mhz)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::SAMPLE_PMU,
        {"SAMPLE PMU TIMELINE", "SAMPLE_PMU_TIMELINE", "usage", "{coreType:s} Core {coreId}/Usage", "Usage(%)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::SAMPLE_PMU,
        {"SAMPLE PMU TIMELINE", "SAMPLE_PMU_TIMELINE", "totalCycle", "{coreType:s} Core {coreId}/Total Cycle",
            "Cycle"}});
}

void CounterEventHelper::RegisterDeviceNICMap() {
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "bandwidth", "bandwidth", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "rxPacketRate", "rxPacketRate", "Packet/s"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "rxByteRate", "rxByteRate", "Byte/s"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "rxPackets", "rxPackets", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "rxBytes", "rxBytes", "Byte"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "rxErrors", "rxErrors", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "rxDropped", "rxDropped", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "txPacketRate", "txPacketRate", "Packet/s"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "txByteRate", "txByteRate", "Byte/s"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "txPackets", "txPackets", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "txBytes", "txBytes", "Byte"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "txErrors", "txErrors", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "txDropped", "txDropped", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NIC, {"NIC", "NIC", "funcId", "funcId", "Port number"}});
}

void CounterEventHelper::RegisterDeviceRoCEMap() {
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "bandwidth", "bandwidth", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "rxPacketRate", "rxPacketRate", "Packet/s"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "rxByteRate", "rxByteRate", "Byte/s"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "rxPackets", "rxPackets", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "rxBytes", "rxBytes", "Byte"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "rxErrors", "rxErrors", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "rxDropped", "rxDropped", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "txPacketRate", "txPacketRate", "Packet/s"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "txByteRate", "txByteRate", "Byte/s"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "txPackets", "txPackets", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "txBytes", "txBytes", "Byte"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "txErrors", "txErrors", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "txDropped", "txDropped", "Packet"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::ROCE, {"RoCE", "ROCE", "funcId", "funcId", "Port number"}});
}

void CounterEventHelper::RegisterDeviceNetDevStatsMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "roceTxPkt", "roceTxPkt", "Packet"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "roceRxPkt", "roceRxPkt", "Packet"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "roceTxErrPkt", "roceTxErrPkt", "Packet"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "roceRxErrPkt", "roceRxErrPkt", "Packet"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "roceTxCnpPkt", "roceTxCnpPkt", "Packet"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "roceRxCnpPkt", "roceRxCnpPkt", "Packet"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "roceNewPktRty", "roceNewPktRty", "Retry"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "nicTxByte", "nicTxByte", "Byte"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NETDEV_STATS,
        {"NetDev Stats", "NETDEV_STATS", "nicTxBandwidth", "nicTxBandwidth", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "nicRxByte", "nicRxByte", "Byte"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NETDEV_STATS,
        {"NetDev Stats", "NETDEV_STATS", "nicRxBandwidth", "nicRxBandwidth", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "macTxPfcPkt", "macTxPfcPkt", "Frame"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "macRxPfcPkt", "macRxPfcPkt", "Frame"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "macTxByte", "macTxByte", "Byte"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NETDEV_STATS,
        {"NetDev Stats", "NETDEV_STATS", "macTxBandwidth", "macTxBandwidth", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "macRxByte", "macRxByte", "Byte"}});
    deviceCounterEventMap.insert({PROCESS_TYPE::NETDEV_STATS,
        {"NetDev Stats", "NETDEV_STATS", "macRxBandwidth", "macRxBandwidth", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "macTxBadByte", "macTxBadByte", "Byte"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::NETDEV_STATS, {"NetDev Stats", "NETDEV_STATS", "macRxBadByte", "macRxBadByte", "Byte"}});
}

void CounterEventHelper::RegisterDevicePCIeMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txPostMin", "PCIE/txPostMin", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txPostMax", "PCIE/txPostMax", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txPostAvg", "PCIE/txPostAvg", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxPostMin", "PCIE/rxPostMin", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxPostMax", "PCIE/rxPostMax", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxPostAvg", "PCIE/rxPostAvg", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostMin", "PCIE/txNonpostMin", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostMax", "PCIE/txNonpostMax", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostAvg", "PCIE/txNonpostAvg", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxNonpostMin", "PCIE/rxNonpostMin", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxNonpostMax", "PCIE/rxNonpostMax", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxNonpostAvg", "PCIE/rxNonpostAvg", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txCplMin", "PCIE/txCplMin", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txCplMax", "PCIE/txCplMax", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txCplAvg", "PCIE/txCplAvg", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxCplMin", "PCIE/rxCplMin", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxCplMax", "PCIE/rxCplMax", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "rxCplAvg", "PCIE/rxCplAvg", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostLatencyMin", "PCIE/txNonpostLatencyMin", "Time(ns)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostLatencyMax", "PCIE/txNonpostLatencyMax", "Time(ns)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::PCIE, {"PCIE", "PCIE", "txNonpostLatencyAvg", "PCIE/txNonpostLatencyAvg", "Time(ns)"}});
}

void CounterEventHelper::RegisterDeviceHCCSMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::HCCS, {"HCCS", "HCCS", "txThroughput", "HCCS/txThroughput", "Bandwidth(Byte/s)"}});
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::HCCS, {"HCCS", "HCCS", "rxThroughput", "HCCS/rxThroughput", "Bandwidth(Byte/s)"}});
}

void CounterEventHelper::RegisterDeviceQOSMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::QOS, {"QOS", "QOS", "bandwidth", "{eventName:s}/Bandwidth", "Bandwidth(Byte/s)"}});
}

void CounterEventHelper::RegisterDeviceSIOMap() {
    const std::vector<std::pair<std::string, std::string>> metrics = {{"rxDat", "dat_rx"}, {"txDat", "dat_tx"},
        {"rxReq", "req_rx"}, {"txReq", "req_tx"}, {"rxRsp", "rsp_rx"}, {"txRsp", "rsp_tx"}, {"rxSnp", "snp_rx"},
        {"txSnp", "snp_tx"}};
    for (const auto &[columnName, displayName] : metrics) {
        deviceCounterEventMap.insert(
            {PROCESS_TYPE::SIO, {"SIO", "SIO", columnName, displayName + "/{name:s}", "Bandwidth(Byte/s)"}});
    }
}

void CounterEventHelper::RegisterDeviceUBMap() {
    deviceCounterEventMap.insert(
        {PROCESS_TYPE::UB, {"UB", "UB", "", "UB Port{portId:03d}", "Rx Bandwidth(Byte/s),Tx Bandwidth(Byte/s)"}});
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

std::string CounterEventHelper::GenerateDeviceMetadataSQL(
    const Dic::Module::Timeline::PROCESS_TYPE type, bool hasDieId) {
    std::string sql;
    for (auto [beg, end] = deviceCounterEventMap.equal_range(type); beg != end; ++beg) {
        CounterEventConfig config = beg->second;
        if (beg != deviceCounterEventMap.lower_bound(type)) {
            sql += " UNION ALL ";
        }
        sql += "SELECT DISTINCT ";
        std::vector<std::string> valueNamesToJoin;
        std::string substitutedFormat = SubstituteThreadNameFormat(config.threadNameFormat, valueNamesToJoin);
        if (type == PROCESS_TYPE::AI_CORE && hasDieId) {
            substitutedFormat = "CASE WHEN COALESCE(AICORE_FREQ.dieId, -1) = -1 THEN '" + AI_CORE_FREQ_LEGACY_NAME +
                "' ELSE '" + AI_CORE_FREQ_DIE_PREFIX + "' || AICORE_FREQ.dieId END";
        } else if (type == PROCESS_TYPE::QOS && hasDieId) {
            substitutedFormat = "CASE WHEN COALESCE(QOS.dieId, -1) = -1 THEN " + substitutedFormat +
                " ELSE id0.value || '/Die ' || QOS.dieId || '/Bandwidth' END";
        }
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
    const bool isAICoreFreqDie =
        type == PROCESS_TYPE::AI_CORE && StringUtil::StartWith(threadId, AI_CORE_FREQ_DIE_PREFIX);
    const bool isQosDie = type == PROCESS_TYPE::QOS && threadId.find("/Die ") != std::string::npos;
    std::string expectedDisplayName;
    size_t index = type == PROCESS_TYPE::SIO ? threadId.find_first_of('/') : threadId.find_last_of('/');
    if (index == std::string::npos) {
        expectedDisplayName = threadId;
    } else if (type == PROCESS_TYPE::SIO) {
        expectedDisplayName = threadId.substr(0, index);
    } else {
        expectedDisplayName = threadId.substr(index + 1);
    }
    std::string expectedValueName;
    if (isAICoreFreqDie) {
        expectedValueName = "freq";
    } else if (displayNameToValueName.find(expectedDisplayName) == displayNameToValueName.end()) {
        expectedValueName = expectedDisplayName;
    } else {
        expectedValueName = displayNameToValueName.at(expectedDisplayName);
    }
    auto beg = deviceCounterEventMap.lower_bound(type);
    auto end = deviceCounterEventMap.upper_bound(type);
    if (type != PROCESS_TYPE::UB) {
        for (; beg != end; ++beg) {
            if (beg->second.valueName == expectedValueName) {
                break;
            }
        }
    }
    if (beg == end) {
        return "";
    }

    CounterEventConfig config = beg->second;
    std::string sql;
    if (type == PROCESS_TYPE::UB) {
        sql = "SELECT timestampNs - ? AS startTime, "
              "'{\"Rx Bandwidth(Byte/s)\":' || rxUdmaBind || "
              "',\"Tx Bandwidth(Byte/s)\":' || txUdmaBind || '}' AS args FROM " +
            config.tableName;
    } else {
        sql = "SELECT timestampNs - ? AS startTime, '{\"" + config.type + "\":' || " + config.valueName +
            " || '}' AS args FROM " + config.tableName;
    }
    std::vector<std::string> valueNamesToJoin;
    std::string queryIdentity = SubstituteThreadNameFormat(config.threadNameFormat, valueNamesToJoin);
    if (isAICoreFreqDie) {
        queryIdentity = "'" + AI_CORE_FREQ_DIE_PREFIX + "' || AICORE_FREQ.dieId";
    } else if (isQosDie) {
        queryIdentity = "id0.value || '/Die ' || QOS.dieId || '/Bandwidth'";
    }
    for (size_t i = 0; i < valueNamesToJoin.size(); ++i) {
        sql += " INNER JOIN " + TABLE_STRING_IDS + " AS id" + std::to_string(i) + " ON " + config.tableName + "." +
            valueNamesToJoin[i] + " = id" + std::to_string(i) + ".id";
    }
    sql += " WHERE " + queryIdentity;
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
        } else if (StringUtil::EndWith(contentInBrace, ":03d")) {
            const std::string columnName = contentInBrace.substr(0, contentInBrace.size() - 4); // 4 is :03d
            substitutedFormat += "printf('%03d', " + columnName + ")";
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
