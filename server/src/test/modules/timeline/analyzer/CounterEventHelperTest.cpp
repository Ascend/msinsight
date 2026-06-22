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
#include <gtest/gtest.h>
#include "CounterEventHelper.h"
using namespace Dic::Module::Timeline;
class CounterEventHelperTest : public testing::Test {};

TEST_F(CounterEventHelperTest, GenerateHostMetaDataSQLTest) {
    CounterEventHelper helper;
    helper.RegisterHostMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::CPU_USAGE;
    std::string sql = helper.GenerateHostMetadataSQL(type);
    const std::string cpuUsageSQL =
        "SELECT DISTINCT 'CPU ' || cpuId || '' AS name, 'Usage(%)' AS types FROM CPU_USAGE;";
    EXPECT_EQ(sql, cpuUsageSQL);
    type = Dic::Protocol::PROCESS_TYPE::HOST_DISK_USAGE;
    sql = helper.GenerateHostMetadataSQL(type);
    const std::string diskUsageSQL = "SELECT DISTINCT 'Disk Usage' AS name, 'Usage(%)' AS types FROM HOST_DISK_USAGE;";
    EXPECT_EQ(sql, diskUsageSQL);
    type = Dic::Protocol::PROCESS_TYPE::HOST_MEM_USAGE;
    sql = helper.GenerateHostMetadataSQL(type);
    const std::string memUsageSQL = "SELECT DISTINCT 'Memory Usage' AS name, 'Usage(%)' AS types FROM HOST_MEM_USAGE;";
    EXPECT_EQ(sql, memUsageSQL);
    type = Dic::Protocol::PROCESS_TYPE::HOST_NETWORK_USAGE;
    sql = helper.GenerateHostMetadataSQL(type);
    const std::string networkUsageSQL =
        "SELECT DISTINCT 'Network Usage' AS name, 'Usage(%)' AS types FROM HOST_NETWORK_USAGE;";
    EXPECT_EQ(sql, networkUsageSQL);
}

TEST_F(CounterEventHelperTest, GenerateHostCounterSQLTest) {
    CounterEventHelper helper;
    helper.RegisterHostMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::CPU_USAGE;
    std::string sql = helper.GenerateHostCounterSQL(type);
    const std::string cpuUsageSQL =
        "SELECT timestampNs - ? AS startTime, '{\"Usage(%)\":' || usage || '}' AS args FROM CPU_USAGE"
        " WHERE 'CPU ' || cpuId || '' = ? AND startTime >= ? AND startTime <= ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, cpuUsageSQL);
    type = Dic::Protocol::PROCESS_TYPE::HOST_DISK_USAGE;
    sql = helper.GenerateHostCounterSQL(type);
    const std::string diskUsageSQL =
        "SELECT timestampNs - ? AS startTime, '{\"Usage(%)\":' || usage || '}' AS args FROM HOST_DISK_USAGE"
        " WHERE 'Disk Usage' = ? AND startTime >= ? AND startTime <= ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, diskUsageSQL);
    type = Dic::Protocol::PROCESS_TYPE::HOST_MEM_USAGE;
    sql = helper.GenerateHostCounterSQL(type);
    const std::string memUsageSQL =
        "SELECT timestampNs - ? AS startTime, '{\"Usage(%)\":' || usage || '}' AS args FROM HOST_MEM_USAGE"
        " WHERE 'Memory Usage' = ? AND startTime >= ? AND startTime <= ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, memUsageSQL);
    type = Dic::Protocol::PROCESS_TYPE::HOST_NETWORK_USAGE;
    sql = helper.GenerateHostCounterSQL(type);
    const std::string networkUsageSQL =
        "SELECT timestampNs - ? AS startTime, '{\"Usage(%)\":' || usage || '}' AS args FROM HOST_NETWORK_USAGE"
        " WHERE 'Network Usage' = ? AND startTime >= ? AND startTime <= ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, networkUsageSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForAICoreFreqTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::AI_CORE;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string aiCoreFreqSQL =
        "SELECT DISTINCT 'AI Core Freq' AS name, 'Frequency(Mhz)' AS types FROM AICORE_FREQ WHERE deviceId = ?;";
    EXPECT_EQ(sql, aiCoreFreqSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForAICoreFreqTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::AI_CORE;
    std::string threadId = "AI Core Freq/freq";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string aiCoreFreqSQL =
        "SELECT timestampNs - ? AS startTime, '{\"Frequency(Mhz)\":' || freq || '}' AS args FROM AICORE_FREQ "
        "WHERE 'AI Core Freq' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, aiCoreFreqSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForAccPMUTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::ACC_PMU;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string accPMUSQL =
        "SELECT DISTINCT 'Accelerator ' || accId || '/readBwLevel' AS name, 'Level' AS types "
        "FROM ACC_PMU WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'Accelerator ' || accId || '/writeBwLevel' AS name, 'Level' AS types "
        "FROM ACC_PMU WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'Accelerator ' || accId || '/readOstLevel' AS name, 'Level' AS types "
        "FROM ACC_PMU WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'Accelerator ' || accId || '/writeOstLevel' AS name, 'Level' AS types "
        "FROM ACC_PMU WHERE deviceId = ?;";
    EXPECT_EQ(sql, accPMUSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForAccPMUTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::ACC_PMU;
    std::string threadId = "Accelerator 0/readBwLevel";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string accPMUSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Level\":' || readBwLevel || '}' AS args FROM ACC_PMU "
        "WHERE 'Accelerator ' || accId || '/readBwLevel' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, accPMUSQL1);
    threadId = "Accelerator 0/writeBwLevel";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string accPMUSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Level\":' || writeBwLevel || '}' AS args FROM ACC_PMU "
        "WHERE 'Accelerator ' || accId || '/writeBwLevel' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, accPMUSQL2);
    threadId = "Accelerator 0/readOstLevel";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string accPMUSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Level\":' || readOstLevel || '}' AS args FROM ACC_PMU "
        "WHERE 'Accelerator ' || accId || '/readOstLevel' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, accPMUSQL3);
    threadId = "Accelerator 0/writeOstLevel";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string accPMUSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Level\":' || writeOstLevel || '}' AS args FROM ACC_PMU "
        "WHERE 'Accelerator ' || accId || '/writeOstLevel' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, accPMUSQL4);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForDDRTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::DDR;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string ddrSQL =
        "SELECT DISTINCT 'Read' AS name, 'Bandwidth(Byte/s)' AS types FROM DDR WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'Write' AS name, 'Bandwidth(Byte/s)' AS types FROM DDR WHERE deviceId = ?;";
    EXPECT_EQ(sql, ddrSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForDDRTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::DDR;
    std::string threadId = "DDR/read";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string ddrSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || read || '}' AS args FROM DDR "
        "WHERE 'Read' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, ddrSQL1);
    threadId = "DDR/write";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string ddrSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || write || '}' AS args FROM DDR "
        "WHERE 'Write' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, ddrSQL2);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForStarsSocTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::STARS_SOC;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string starSocSQL = "SELECT DISTINCT 'L2 Buffer Bw Level' AS name, 'Level' AS types "
                                   "FROM SOC_BANDWIDTH_LEVEL WHERE deviceId = ? UNION ALL "
                                   "SELECT DISTINCT 'Mata Bw Level' AS name, 'Level' AS types "
                                   "FROM SOC_BANDWIDTH_LEVEL WHERE deviceId = ?;";
    EXPECT_EQ(sql, starSocSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForStarsSocTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::STARS_SOC;
    std::string threadId = "Stars Soc/l2BufferBwLevel";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string starsSocSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Level\":' || l2BufferBwLevel || '}' AS args FROM SOC_BANDWIDTH_LEVEL "
        "WHERE 'L2 Buffer Bw Level' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, starsSocSQL1);
    threadId = "Stars Soc/mataBwLevel";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string starsSocSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Level\":' || mataBwLevel || '}' AS args FROM SOC_BANDWIDTH_LEVEL "
        "WHERE 'Mata Bw Level' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, starsSocSQL2);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForNPUMEMTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::NPU_MEM;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string npuMemSQL = "SELECT DISTINCT '' || id0.value || '/DDR' AS name, 'Byte' AS types FROM NPU_MEM "
                                  "INNER JOIN STRING_IDS AS id0 ON NPU_MEM.type = id0.id WHERE deviceId = ? UNION ALL "
                                  "SELECT DISTINCT '' || id0.value || '/HBM' AS name, 'Byte' AS types FROM NPU_MEM "
                                  "INNER JOIN STRING_IDS AS id0 ON NPU_MEM.type = id0.id WHERE deviceId = ?;";
    EXPECT_EQ(sql, npuMemSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForNPUMEMTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::NPU_MEM;
    std::string threadId = "app/DDR";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string npuMemSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || ddr || '}' AS args FROM NPU_MEM "
        "INNER JOIN STRING_IDS AS id0 ON NPU_MEM.type = id0.id WHERE '' || id0.value || '/DDR' = ? AND startTime >= ? "
        "AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, npuMemSQL1);
    threadId = "device/DDR";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string npuMemSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || ddr || '}' AS args FROM NPU_MEM "
        "INNER JOIN STRING_IDS AS id0 ON NPU_MEM.type = id0.id WHERE '' || id0.value || '/DDR' = ? AND startTime >= ? "
        "AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, npuMemSQL2);
    threadId = "app/HBM";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string npuMemSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || hbm || '}' AS args FROM NPU_MEM "
        "INNER JOIN STRING_IDS AS id0 ON NPU_MEM.type = id0.id WHERE '' || id0.value || '/HBM' = ? AND startTime >= ? "
        "AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, npuMemSQL3);
    threadId = "device/HBM";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string npuMemSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || hbm || '}' AS args FROM NPU_MEM "
        "INNER JOIN STRING_IDS AS id0 ON NPU_MEM.type = id0.id WHERE '' || id0.value || '/HBM' = ? AND startTime >= ? "
        "AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, npuMemSQL4);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForHBMTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::HBM;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string hbmSQL =
        "SELECT DISTINCT 'HBM ' || hbmId || ' ' || id0.value || '/Bandwidth' AS name, 'Bandwidth(Byte/s)' AS types "
        "FROM HBM INNER JOIN STRING_IDS AS id0 ON HBM.type = id0.id WHERE deviceId = ?;";
    EXPECT_EQ(sql, hbmSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForHBMTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::HBM;
    std::string threadId = "HBM 0 read/Bandwidth";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string hbmSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || bandwidth || '}' AS args FROM HBM "
        "INNER JOIN STRING_IDS AS id0 ON HBM.type = id0.id "
        "WHERE 'HBM ' || hbmId || ' ' || id0.value || '/Bandwidth' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, hbmSQL1);
    threadId = "HBM 0 write/Bandwidth";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string hbmSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || bandwidth || '}' AS args FROM HBM "
        "INNER JOIN STRING_IDS AS id0 ON HBM.type = id0.id "
        "WHERE 'HBM ' || hbmId || ' ' || id0.value || '/Bandwidth' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, hbmSQL2);
    threadId = "HBM 1 read/Bandwidth";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string hbmSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || bandwidth || '}' AS args FROM HBM "
        "INNER JOIN STRING_IDS AS id0 ON HBM.type = id0.id "
        "WHERE 'HBM ' || hbmId || ' ' || id0.value || '/Bandwidth' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, hbmSQL3);
    threadId = "HBM 1 write/Bandwidth";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string hbmSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || bandwidth || '}' AS args FROM HBM "
        "INNER JOIN STRING_IDS AS id0 ON HBM.type = id0.id "
        "WHERE 'HBM ' || hbmId || ' ' || id0.value || '/Bandwidth' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, hbmSQL4);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForLLCTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::LLC;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string llcSQL =
        "SELECT DISTINCT 'LLC ' || llcId || ' ' || id0.value || '/Hit Rate' AS name, 'Hit Rate(%)' AS types FROM LLC "
        "INNER JOIN STRING_IDS AS id0 ON LLC.mode = id0.id WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'LLC ' || llcId || ' ' || id0.value || '/Throughput' AS name, 'Throughput(Byte/s)' AS types "
        "FROM LLC INNER JOIN STRING_IDS AS id0 ON LLC.mode = id0.id WHERE deviceId = ?;";
    EXPECT_EQ(sql, llcSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForLLCTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::LLC;
    std::string threadId = "LLC 0 read/Hit Rate";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string llcSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Hit Rate(%)\":' || hitRate || '}' AS args FROM LLC "
        "INNER JOIN STRING_IDS AS id0 ON LLC.mode = id0.id "
        "WHERE 'LLC ' || llcId || ' ' || id0.value || '/Hit Rate' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, llcSQL1);
    threadId = "LLC 0 write/Hit Rate";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string llcSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Hit Rate(%)\":' || hitRate || '}' AS args FROM LLC "
        "INNER JOIN STRING_IDS AS id0 ON LLC.mode = id0.id "
        "WHERE 'LLC ' || llcId || ' ' || id0.value || '/Hit Rate' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, llcSQL2);
    threadId = "LLC 0 read/Throughput";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string llcSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Throughput(Byte/s)\":' || throughput || '}' AS args FROM LLC "
        "INNER JOIN STRING_IDS AS id0 ON LLC.mode = id0.id "
        "WHERE 'LLC ' || llcId || ' ' || id0.value || '/Throughput' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, llcSQL3);
    threadId = "LLC 0 write/Throughput";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string llcSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Throughput(Byte/s)\":' || throughput || '}' AS args FROM LLC "
        "INNER JOIN STRING_IDS AS id0 ON LLC.mode = id0.id "
        "WHERE 'LLC ' || llcId || ' ' || id0.value || '/Throughput' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, llcSQL4);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForSamplePMUTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::SAMPLE_PMU;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string samplePmuSQL =
        "SELECT DISTINCT '' || id0.value || ' Core ' || coreId || '/Freq' AS name, 'Frequency(Mhz)' AS types "
        "FROM SAMPLE_PMU_TIMELINE INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT '' || id0.value || ' Core ' || coreId || '/Usage' AS name, 'Usage(%)' AS types "
        "FROM SAMPLE_PMU_TIMELINE INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT '' || id0.value || ' Core ' || coreId || '/Total Cycle' AS name, 'Cycle' AS types "
        "FROM SAMPLE_PMU_TIMELINE INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE deviceId = ?;";
    EXPECT_EQ(sql, samplePmuSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForSamplePMUTestPartOne) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::SAMPLE_PMU;
    std::string threadId = "AIC Core 0/Freq";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string samplePmuSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Frequency(Mhz)\":' || freq || '}' AS args FROM SAMPLE_PMU_TIMELINE "
        "INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE '' || id0.value || ' Core ' || coreId || '/Freq' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, samplePmuSQL1);
    threadId = "AIV Core 0/Freq";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string samplePmuSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Frequency(Mhz)\":' || freq || '}' AS args FROM SAMPLE_PMU_TIMELINE "
        "INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE '' || id0.value || ' Core ' || coreId || '/Freq' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, samplePmuSQL2);
    threadId = "AIC Core 10/Freq";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string samplePMUSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Frequency(Mhz)\":' || freq || '}' AS args FROM SAMPLE_PMU_TIMELINE "
        "INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE '' || id0.value || ' Core ' || coreId || '/Freq' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, samplePMUSQL3);
    threadId = "AIC Core 0/Usage";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string samplePmuSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Usage(%)\":' || usage || '}' AS args FROM SAMPLE_PMU_TIMELINE "
        "INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE '' || id0.value || ' Core ' || coreId || '/Usage' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, samplePmuSQL4);
    threadId = "AIV Core 0/Usage";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string samplePmuSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Usage(%)\":' || usage || '}' AS args FROM SAMPLE_PMU_TIMELINE "
        "INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE '' || id0.value || ' Core ' || coreId || '/Usage' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, samplePmuSQL5);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForSamplePMUTestPartTwo) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::SAMPLE_PMU;
    std::string threadId = "AIC Core 10/Usage";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string samplePmuSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Usage(%)\":' || usage || '}' AS args FROM SAMPLE_PMU_TIMELINE "
        "INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE '' || id0.value || ' Core ' || coreId || '/Usage' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, samplePmuSQL1);
    threadId = "AIC Core 0/Total Cycle";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string samplePmuSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Cycle\":' || totalCycle || '}' AS args FROM SAMPLE_PMU_TIMELINE "
        "INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE '' || id0.value || ' Core ' || coreId || '/Total Cycle' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, samplePmuSQL2);
    threadId = "AIV Core 0/Total Cycle";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string samplePMUSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Cycle\":' || totalCycle || '}' AS args FROM SAMPLE_PMU_TIMELINE "
        "INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE '' || id0.value || ' Core ' || coreId || '/Total Cycle' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, samplePMUSQL3);
    threadId = "AIC Core 10/Total Cycle";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string samplePmuSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Cycle\":' || totalCycle || '}' AS args FROM SAMPLE_PMU_TIMELINE "
        "INNER JOIN STRING_IDS AS id0 ON SAMPLE_PMU_TIMELINE.coreType = id0.id "
        "WHERE '' || id0.value || ' Core ' || coreId || '/Total Cycle' = ? AND startTime >= ? AND startTime <= ? "
        "AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, samplePmuSQL4);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForNICTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::NIC;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string nicSQL =
        "SELECT DISTINCT 'bandwidth' AS name, 'Bandwidth(Byte/s)' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxPacketRate' AS name, 'Packet/s' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxByteRate' AS name, 'Byte/s' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxPackets' AS name, 'Packet' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxBytes' AS name, 'Byte' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxErrors' AS name, 'Packet' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxDropped' AS name, 'Packet' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txPacketRate' AS name, 'Packet/s' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txByteRate' AS name, 'Byte/s' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txPackets' AS name, 'Packet' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txBytes' AS name, 'Byte' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txErrors' AS name, 'Packet' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txDropped' AS name, 'Packet' AS types FROM NIC WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'funcId' AS name, 'Port number' AS types FROM NIC WHERE deviceId = ?;";
    EXPECT_EQ(sql, nicSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForNICTestPartOne) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::NIC;
    std::string threadId = "bandwidth";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || bandwidth || '}' AS args FROM NIC "
        "WHERE 'bandwidth' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL1);
    threadId = "rxPacketRate";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet/s\":' || rxPacketRate || '}' AS args FROM NIC "
        "WHERE 'rxPacketRate' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL2);
    threadId = "rxByteRate";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte/s\":' || rxByteRate || '}' AS args FROM NIC "
        "WHERE 'rxByteRate' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL3);
    threadId = "rxPackets";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || rxPackets || '}' AS args FROM NIC "
        "WHERE 'rxPackets' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL4);
    threadId = "rxBytes";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || rxBytes || '}' AS args FROM NIC "
        "WHERE 'rxBytes' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL5);
    threadId = "rxErrors";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL6 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || rxErrors || '}' AS args FROM NIC "
        "WHERE 'rxErrors' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL6);
    threadId = "rxDropped";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL7 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || rxDropped || '}' AS args FROM NIC "
        "WHERE 'rxDropped' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL7);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForNICTestPartTwo) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::NIC;
    std::string threadId = "funcId";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Port number\":' || funcId || '}' AS args FROM NIC "
        "WHERE 'funcId' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL1);
    threadId = "txPacketRate";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet/s\":' || txPacketRate || '}' AS args FROM NIC "
        "WHERE 'txPacketRate' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL2);
    threadId = "txByteRate";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte/s\":' || txByteRate || '}' AS args FROM NIC "
        "WHERE 'txByteRate' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL3);
    threadId = "txPackets";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || txPackets || '}' AS args FROM NIC "
        "WHERE 'txPackets' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL4);
    threadId = "txBytes";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || txBytes || '}' AS args FROM NIC "
        "WHERE 'txBytes' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL5);
    threadId = "txErrors";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL6 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || txErrors || '}' AS args FROM NIC "
        "WHERE 'txErrors' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL6);
    threadId = "txDropped";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string nicSQL7 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || txDropped || '}' AS args FROM NIC "
        "WHERE 'txDropped' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, nicSQL7);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForROCETest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::ROCE;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string roceSQL =
        "SELECT DISTINCT 'bandwidth' AS name, 'Bandwidth(Byte/s)' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxPacketRate' AS name, 'Packet/s' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxByteRate' AS name, 'Byte/s' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxPackets' AS name, 'Packet' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxBytes' AS name, 'Byte' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxErrors' AS name, 'Packet' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'rxDropped' AS name, 'Packet' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txPacketRate' AS name, 'Packet/s' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txByteRate' AS name, 'Byte/s' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txPackets' AS name, 'Packet' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txBytes' AS name, 'Byte' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txErrors' AS name, 'Packet' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'txDropped' AS name, 'Packet' AS types FROM ROCE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'funcId' AS name, 'Port number' AS types FROM ROCE WHERE deviceId = ?;";
    EXPECT_EQ(sql, roceSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForROCETestPartOne) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::ROCE;
    std::string threadId = "bandwidth";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || bandwidth || '}' AS args FROM ROCE "
        "WHERE 'bandwidth' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL1);
    threadId = "rxPacketRate";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet/s\":' || rxPacketRate || '}' AS args FROM ROCE "
        "WHERE 'rxPacketRate' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL2);
    threadId = "rxByteRate";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte/s\":' || rxByteRate || '}' AS args FROM ROCE "
        "WHERE 'rxByteRate' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL3);
    threadId = "rxPackets";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || rxPackets || '}' AS args FROM ROCE "
        "WHERE 'rxPackets' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL4);
    threadId = "rxBytes";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || rxBytes || '}' AS args FROM ROCE "
        "WHERE 'rxBytes' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL5);
    threadId = "rxErrors";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL6 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || rxErrors || '}' AS args FROM ROCE "
        "WHERE 'rxErrors' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL6);
    threadId = "rxDropped";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL7 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || rxDropped || '}' AS args FROM ROCE "
        "WHERE 'rxDropped' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL7);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForROCETestPartTwo) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::ROCE;
    std::string threadId = "funcId";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Port number\":' || funcId || '}' AS args FROM ROCE "
        "WHERE 'funcId' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL1);
    threadId = "txPacketRate";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet/s\":' || txPacketRate || '}' AS args FROM ROCE "
        "WHERE 'txPacketRate' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL2);
    threadId = "txByteRate";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte/s\":' || txByteRate || '}' AS args FROM ROCE "
        "WHERE 'txByteRate' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL3);
    threadId = "txPackets";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || txPackets || '}' AS args FROM ROCE "
        "WHERE 'txPackets' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL4);
    threadId = "txBytes";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || txBytes || '}' AS args FROM ROCE "
        "WHERE 'txBytes' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL5);
    threadId = "txErrors";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL6 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || txErrors || '}' AS args FROM ROCE "
        "WHERE 'txErrors' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL6);
    threadId = "txDropped";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string roceSQL7 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || txDropped || '}' AS args FROM ROCE "
        "WHERE 'txDropped' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, roceSQL7);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForNetDevStatsTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::NETDEV_STATS;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string netDevStatsSQL =
        "SELECT DISTINCT 'roceTxPkt' AS name, 'Packet' AS types FROM NETDEV_STATS WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'roceRxPkt' AS name, 'Packet' AS types FROM NETDEV_STATS WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'roceTxErrPkt' AS name, 'Packet' AS types FROM NETDEV_STATS WHERE deviceId = ? "
        "UNION ALL "
        "SELECT DISTINCT 'roceRxErrPkt' AS name, 'Packet' AS types FROM NETDEV_STATS WHERE deviceId = ? "
        "UNION ALL "
        "SELECT DISTINCT 'roceTxCnpPkt' AS name, 'Packet' AS types FROM NETDEV_STATS WHERE deviceId = ? "
        "UNION ALL "
        "SELECT DISTINCT 'roceRxCnpPkt' AS name, 'Packet' AS types FROM NETDEV_STATS WHERE deviceId = ? "
        "UNION ALL "
        "SELECT DISTINCT 'roceNewPktRty' AS name, 'Retry' AS types FROM NETDEV_STATS WHERE deviceId = ? "
        "UNION ALL "
        "SELECT DISTINCT 'nicTxByte' AS name, 'Byte' AS types FROM NETDEV_STATS WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'nicTxBandwidth' AS name, 'Bandwidth(Byte/s)' AS types "
        "FROM NETDEV_STATS WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'nicRxByte' AS name, 'Byte' AS types FROM NETDEV_STATS WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'nicRxBandwidth' AS name, 'Bandwidth(Byte/s)' AS types "
        "FROM NETDEV_STATS WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'macTxPfcPkt' AS name, 'Frame' AS types FROM NETDEV_STATS WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'macRxPfcPkt' AS name, 'Frame' AS types FROM NETDEV_STATS WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'macTxByte' AS name, 'Byte' AS types FROM NETDEV_STATS WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'macTxBandwidth' AS name, 'Bandwidth(Byte/s)' AS types "
        "FROM NETDEV_STATS WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'macRxByte' AS name, 'Byte' AS types FROM NETDEV_STATS WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'macRxBandwidth' AS name, 'Bandwidth(Byte/s)' AS types "
        "FROM NETDEV_STATS WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'macTxBadByte' AS name, 'Byte' AS types FROM NETDEV_STATS WHERE deviceId = ? "
        "UNION ALL "
        "SELECT DISTINCT 'macRxBadByte' AS name, 'Byte' AS types FROM NETDEV_STATS WHERE deviceId = ?;";
    EXPECT_EQ(sql, netDevStatsSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForNetDevStatsTestPartOne) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::NETDEV_STATS;
    std::string threadId = "macTxPfcPkt";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Frame\":' || macTxPfcPkt || '}' AS args FROM NETDEV_STATS "
        "WHERE 'macTxPfcPkt' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY "
        "startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL1);
    threadId = "macRxPfcPkt";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Frame\":' || macRxPfcPkt || '}' AS args FROM NETDEV_STATS "
        "WHERE 'macRxPfcPkt' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY "
        "startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL2);
    threadId = "macTxByte";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || macTxByte || '}' AS args FROM NETDEV_STATS "
        "WHERE 'macTxByte' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime "
        "ASC;";
    EXPECT_EQ(sql, netDevStatsSQL3);
    threadId = "macTxBandwidth";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || macTxBandwidth || '}' AS args "
        "FROM NETDEV_STATS WHERE 'macTxBandwidth' = ? AND startTime >= ? AND startTime <= ? AND deviceId "
        "= ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL4);
    threadId = "macRxByte";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || macRxByte || '}' AS args FROM NETDEV_STATS "
        "WHERE 'macRxByte' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime "
        "ASC;";
    EXPECT_EQ(sql, netDevStatsSQL5);
    threadId = "macRxBandwidth";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL6 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || macRxBandwidth || '}' AS args "
        "FROM NETDEV_STATS WHERE 'macRxBandwidth' = ? AND startTime >= ? AND startTime <= ? AND deviceId "
        "= ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL6);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForNetDevStatsTestPartTwo) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::NETDEV_STATS;
    std::string threadId = "macTxBadByte";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || macTxBadByte || '}' AS args FROM NETDEV_STATS "
        "WHERE 'macTxBadByte' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY "
        "startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL1);
    threadId = "macRxBadByte";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || macRxBadByte || '}' AS args FROM NETDEV_STATS "
        "WHERE 'macRxBadByte' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY "
        "startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL2);
    threadId = "nicTxByte";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || nicTxByte || '}' AS args FROM NETDEV_STATS "
        "WHERE 'nicTxByte' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime "
        "ASC;";
    EXPECT_EQ(sql, netDevStatsSQL3);
    threadId = "nicTxBandwidth";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || nicTxBandwidth || '}' AS args "
        "FROM NETDEV_STATS WHERE 'nicTxBandwidth' = ? AND startTime >= ? AND startTime <= ? AND deviceId "
        "= ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL4);
    threadId = "nicRxByte";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Byte\":' || nicRxByte || '}' AS args FROM NETDEV_STATS "
        "WHERE 'nicRxByte' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime "
        "ASC;";
    EXPECT_EQ(sql, netDevStatsSQL5);
    threadId = "nicRxBandwidth";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL6 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || nicRxBandwidth || '}' AS args "
        "FROM NETDEV_STATS WHERE 'nicRxBandwidth' = ? AND startTime >= ? AND startTime <= ? AND deviceId "
        "= ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL6);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForNetDevStatsTestPartThree) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::NETDEV_STATS;
    std::string threadId = "roceTxPkt";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || roceTxPkt || '}' AS args FROM NETDEV_STATS "
        "WHERE 'roceTxPkt' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime "
        "ASC;";
    EXPECT_EQ(sql, netDevStatsSQL1);
    threadId = "roceRxPkt";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || roceRxPkt || '}' AS args FROM NETDEV_STATS "
        "WHERE 'roceRxPkt' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime "
        "ASC;";
    EXPECT_EQ(sql, netDevStatsSQL2);
    threadId = "roceTxErrPkt";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || roceTxErrPkt || '}' AS args FROM NETDEV_STATS "
        "WHERE 'roceTxErrPkt' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY "
        "startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL3);
    threadId = "roceRxErrPkt";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || roceRxErrPkt || '}' AS args FROM NETDEV_STATS "
        "WHERE 'roceRxErrPkt' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY "
        "startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL4);
    threadId = "roceTxCnpPkt";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || roceTxCnpPkt || '}' AS args FROM NETDEV_STATS "
        "WHERE 'roceTxCnpPkt' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY "
        "startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL5);
    threadId = "roceRxCnpPkt";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL6 =
        "SELECT timestampNs - ? AS startTime, '{\"Packet\":' || roceRxCnpPkt || '}' AS args FROM NETDEV_STATS "
        "WHERE 'roceRxCnpPkt' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY "
        "startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL6);
    threadId = "roceNewPktRty";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string netDevStatsSQL7 =
        "SELECT timestampNs - ? AS startTime, '{\"Retry\":' || roceNewPktRty || '}' AS args FROM NETDEV_STATS "
        "WHERE 'roceNewPktRty' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY "
        "startTime ASC;";
    EXPECT_EQ(sql, netDevStatsSQL7);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForPCIeTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::PCIE;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string pcieSQL =
        "SELECT DISTINCT 'PCIE/txPostMin' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/txPostMax' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/txPostAvg' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/rxPostMin' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/rxPostMax' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/rxPostAvg' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/txNonpostMin' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'PCIE/txNonpostMax' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'PCIE/txNonpostAvg' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'PCIE/rxNonpostMin' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'PCIE/rxNonpostMax' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'PCIE/rxNonpostAvg' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'PCIE/txCplMin' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/txCplMax' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/txCplAvg' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/rxCplMin' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/rxCplMax' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/rxCplAvg' AS name, 'Bandwidth(Byte/s)' AS types FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/txNonpostLatencyMin' AS name, 'Time(ns)' AS types "
        "FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/txNonpostLatencyMax' AS name, 'Time(ns)' AS types "
        "FROM PCIE WHERE deviceId = ? UNION ALL "
        "SELECT DISTINCT 'PCIE/txNonpostLatencyAvg' AS name, 'Time(ns)' AS types FROM PCIE WHERE deviceId = ?;";
    EXPECT_EQ(sql, pcieSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForPCIeTestPartOne) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::PCIE;
    std::string threadId = "PCIE/txPostMin";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || txPostMin || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txPostMin' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL1);
    threadId = "PCIE/txPostMax";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || txPostMax || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txPostMax' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL2);
    threadId = "PCIE/txPostAvg";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || txPostAvg || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txPostAvg' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL3);
    threadId = "PCIE/rxPostMin";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || rxPostMin || '}' AS args FROM PCIE "
        "WHERE 'PCIE/rxPostMin' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL4);
    threadId = "PCIE/rxPostMax";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || rxPostMax || '}' AS args FROM PCIE "
        "WHERE 'PCIE/rxPostMax' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL5);
    threadId = "PCIE/rxPostAvg";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL6 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || rxPostAvg || '}' AS args FROM PCIE "
        "WHERE 'PCIE/rxPostAvg' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL6);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForPCIeTestPartTwo) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::PCIE;
    std::string threadId = "PCIE/txNonpostMin";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || txNonpostMin || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txNonpostMin' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL1);
    threadId = "PCIE/txNonpostMax";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || txNonpostMax || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txNonpostMax' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL2);
    threadId = "PCIE/txNonpostAvg";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || txNonpostAvg || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txNonpostAvg' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL3);
    threadId = "PCIE/rxNonpostMin";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || rxNonpostMin || '}' AS args FROM PCIE "
        "WHERE 'PCIE/rxNonpostMin' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL4);
    threadId = "PCIE/rxNonpostMax";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || rxNonpostMax || '}' AS args FROM PCIE "
        "WHERE 'PCIE/rxNonpostMax' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL5);
    threadId = "PCIE/rxNonpostAvg";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL6 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || rxNonpostAvg || '}' AS args FROM PCIE "
        "WHERE 'PCIE/rxNonpostAvg' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL6);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForPCIeTestPartThree) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::PCIE;
    std::string threadId = "PCIE/txCplMin";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || txCplMin || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txCplMin' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL1);
    threadId = "PCIE/txCplMax";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || txCplMax || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txCplMax' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL2);
    threadId = "PCIE/txCplAvg";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || txCplAvg || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txCplAvg' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL3);
    threadId = "PCIE/rxCplMin";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL4 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || rxCplMin || '}' AS args FROM PCIE "
        "WHERE 'PCIE/rxCplMin' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL4);
    threadId = "PCIE/rxCplMax";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL5 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || rxCplMax || '}' AS args FROM PCIE "
        "WHERE 'PCIE/rxCplMax' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL5);
    threadId = "PCIE/rxCplAvg";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL6 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || rxCplAvg || '}' AS args FROM PCIE "
        "WHERE 'PCIE/rxCplAvg' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL6);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForPCIeTestPartFour) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::PCIE;
    std::string threadId = "PCIE/txNonpostLatencyMin";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Time(ns)\":' || txNonpostLatencyMin || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txNonpostLatencyMin' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL1);
    threadId = "PCIE/txNonpostLatencyMax";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Time(ns)\":' || txNonpostLatencyMax || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txNonpostLatencyMax' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL2);
    threadId = "PCIE/txNonpostLatencyAvg";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string pcieSQL3 =
        "SELECT timestampNs - ? AS startTime, '{\"Time(ns)\":' || txNonpostLatencyAvg || '}' AS args FROM PCIE "
        "WHERE 'PCIE/txNonpostLatencyAvg' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? "
        "ORDER BY startTime ASC;";
    EXPECT_EQ(sql, pcieSQL3);
}

TEST_F(CounterEventHelperTest, GenerateDeviceMetaDataSQLForHCCSTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::HCCS;
    std::string sql = helper.GenerateDeviceMetadataSQL(type);
    const std::string hccsSQL =
        "SELECT DISTINCT 'HCCS/txThroughput' AS name, 'Bandwidth(Byte/s)' AS types FROM HCCS WHERE deviceId = ? UNION "
        "ALL "
        "SELECT DISTINCT 'HCCS/rxThroughput' AS name, 'Bandwidth(Byte/s)' AS types FROM HCCS WHERE deviceId = ?;";
    EXPECT_EQ(sql, hccsSQL);
}

TEST_F(CounterEventHelperTest, GenerateDeviceCounterSQLForHCCSTest) {
    CounterEventHelper helper;
    helper.RegisterDeviceMap();
    Dic::Protocol::PROCESS_TYPE type = Dic::Protocol::PROCESS_TYPE::HCCS;
    std::string threadId = "HCCS/txThroughput";
    std::string sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string hccsSQL1 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || txThroughput || '}' AS args FROM HCCS "
        "WHERE 'HCCS/txThroughput' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, hccsSQL1);
    threadId = "HCCS/rxThroughput";
    sql = helper.GenerateDeviceCounterSQL(type, threadId);
    const std::string hccsSQL2 =
        "SELECT timestampNs - ? AS startTime, '{\"Bandwidth(Byte/s)\":' || rxThroughput || '}' AS args FROM HCCS "
        "WHERE 'HCCS/rxThroughput' = ? AND startTime >= ? AND startTime <= ? AND deviceId = ? ORDER BY startTime ASC;";
    EXPECT_EQ(sql, hccsSQL2);
}
