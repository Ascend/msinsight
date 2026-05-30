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

#include "DatabaseTestConst.h"

namespace Dic::Global::PROFILER::MockUtil {
// db
const std::string CREATE_TABLE_DB_STRING_IDS_SQL =
    "CREATE TABLE IF NOT EXISTS STRING_IDS(id INTEGER primary key, value TEXT);";
const std::string CREATE_TABLE_DB_PYTORCH_API_SQL =
    "CREATE TABLE IF NOT EXISTS PYTORCH_API(startNs TEXT, endNs TEXT,globalTid INTEGER, "
    " connectionId INTEGER, name INTEGER, sequenceNumber INTEGER, fwdThreadId INTEGER,inputDtypes INTEGER, "
    " inputShapes INTEGER, callchainId INTEGER, type INTEGER, depth integer);  ";
const std::string CREATE_TABLE_DB_CONNECTION_IDS_SQL =
    "CREATE TABLE IF NOT EXISTS CONNECTION_IDS( id INTEGER, connectionId INTEGER);  ";
const std::string CREATE_TABLE_DB_PYTORCH_CALLCHAINS_SQL =
    "CREATE TABLE IF NOT EXISTS PYTORCH_CALLCHAINS(id INTEGER, stack INTEGER"
    " , stackDepth INTEGER );";
const std::string CREATE_TABLE_DB_CANN_API_SQL =
    "CREATE TABLE IF NOT EXISTS CANN_API(startNs INTEGER, endNs INTEGER, type INTEGER, "
    " globalTid INTEGER, connectionId INTEGER primary key, name INTEGER, depth integer);  ";
const std::string CREATE_TABLE_DB_MSTX_EVENTS_SQL =
    "CREATE TABLE IF NOT EXISTS MSTX_EVENTS(startNs INTEGER,endNs INTEGER, "
    " eventType INTEGER,rangeId INTEGER, category INTEGER, message INTEGER, globalTid INTEGER, "
    " endGlobalTid INTEGER, domainId INTEGER, connectionId INTEGER, depth integer); ";
const std::string CREATE_TABLE_DB_OSRT_API_SQL = "CREATE TABLE IF NOT EXISTS OSRT_API(name INTEGER, globalTid NUMERIC,"
                                                 " startNs INTEGER, endNs INTEGER);";
const std::string CREATE_TABLE_DB_TASK_SQL =
    "create TABLE IF NOT EXISTS TASK( startNs INTEGER, endNs INTEGER, deviceId INTEGER, "
    " connectionId INTEGER, globalTaskId INTEGER, globalPid INTEGER, taskType INTEGER,contextId INTEGER, "
    " streamId INTEGER, taskId INTEGER, modelId INTEGER, depth integer);";
const std::string CREATE_TABLE_DB_COMPUTE_TASK_INFO_SQL =
    "CREATE TABLE IF NOT EXISTS COMPUTE_TASK_INFO( name INTEGER, "
    " globalTaskId INTEGER primary key, blockNum INTEGER, mixBlockDim INTEGER, taskType INTEGER,"
    " opType INTEGER, inputFormats INTEGER, inputDataTypes INTEGER, inputShapes INTEGER, "
    " outputFormats INTEGER, outputDataTypes INTEGER, outputShapes INTEGER, attrInfo INTEGER, "
    " waitNs INTEGER);  ";
const std::string CREATE_TABLE_DB_COMMUNICATION_OP_SQL =
    "CREATE TABLE IF NOT EXISTS COMMUNICATION_OP( opName INTEGER, startNs INTEGER, "
    " endNs INTEGER, connectionId INTEGER, groupName INTEGER, opId INTEGER primary key, "
    " relay INTEGER, retry INTEGER, dataType INTEGER, algType INTEGER, count NUMERIC, opType INTEGER, waitNs INTEGER,"
    " opConnectionId TEXT);";
const std::string CREATE_TABLE_DB_COMMUNICATION_TASK_INFO_SQL =
    "CREATE TABLE IF NOT EXISTS COMMUNICATION_TASK_INFO( name INTEGER, "
    " globalTaskId INTEGER, taskType INTEGER, planeId INTEGER, groupName INTEGER, notifyId INTEGER,"
    " rdmaType INTEGER, srcRank INTEGER, dstRank INTEGER, transportType INTEGER, size INTEGER, "
    " dataType INTEGER, linkType INTEGER, opId INTEGER);  ";
const std::string CREATE_TABLE_DB_COMMUNICATION_SCHEDULE_TASK_INFO_SQL =
    "CREATE TABLE IF NOT EXISTS COMMUNICATION_SCHEDULE_TASK_INFO("
    "name INTEGER, globalTaskId INTEGER primary key, taskType INTEGER, "
    "opType INTEGER);";
const std::string CREATE_TABLE_DB_NPU_INFO_SQL =
    "CREATE TABLE IF NOT EXISTS NPU_INFO(id INTEGER primary key, name TEXT);";
const std::string CREATE_TABLE_DB_RANK_DEVICE_MAP_SQL =
    "CREATE TABLE RANK_DEVICE_MAP (rankId INTEGER, deviceId INTEGER);";
const std::string CREATE_TABLE_DB_ENUM_API_TYPE_SQL =
    "CREATE TABLE IF NOT EXISTS ENUM_API_TYPE( id INTEGER primary key, name TEXT);  ";

// db counter
const std::string CREATE_TABLE_DB_AICORE_FREQ_SQL =
    "CREATE TABLE AICORE_FREQ (type INTEGER,ddr NUMERIC,hbm NUMERIC,timestampNs INTEGER,deviceId INTEGER);";
const std::string CREATE_TABLE_DB_ACC_PMU_SQL =
    "CREATE TABLE ACC_PMU (accId INTEGER,readBwLevel INTEGER,writeBwLevel INTEGER,readOstLevel "
    "INTEGER,writeOstLevel INTEGER,timestampNs NUMERIC,deviceId INTEGER);";
const std::string CREATE_TABLE_DB_SOC_BANDWIDTH_LEVEL_SQL =
    "CREATE TABLE SOC_BANDWIDTH_LEVEL (l2BufferBwLevel INTEGER,mataBwLevel INTEGER,timestampNs "
    "NUMERIC,deviceId INTEGER);";
const std::string CREATE_TABLE_DB_NPU_MEM_SQL =
    "CREATE TABLE NPU_MEM (type INTEGER,ddr NUMERIC,hbm NUMERIC,timestampNs INTEGER,deviceId INTEGER);";
const std::string CREATE_TABLE_DB_HCCS_SQL =
    "CREATE TABLE HCCS (type INTEGER,ddr NUMERIC,hbm NUMERIC,timestampNs INTEGER,deviceId INTEGER);";
const std::string CREATE_TABLE_DB_PCIE_SQL =
    "CREATE TABLE PCIE (type INTEGER,ddr NUMERIC,hbm NUMERIC,timestampNs INTEGER,deviceId INTEGER);";

// db Insight创建的辅助表
const std::string CREATE_TABLE_DB_OVERLAP_ANALYSIS_SQL =
    "CREATE TABLE OVERLAP_ANALYSIS (id INTEGER PRIMARY KEY AUTOINCREMENT, deviceId integer, "
    "startNs integer, endNs integer, type integer);";
const std::string CREATE_TABLE_DB_CONNECTION_CATS_SQL = "CREATE TABLE connectionCats(connectionId INT,cat);";

const std::map<TableName, std::string> CREATE_TABLE_SQL_MAP = {
    {TableName::DB_STRING_IDS, CREATE_TABLE_DB_STRING_IDS_SQL},
    {TableName::DB_PYTORCH_API, CREATE_TABLE_DB_PYTORCH_API_SQL},
    {TableName::DB_CONNECTION_IDS, CREATE_TABLE_DB_CONNECTION_IDS_SQL},
    {TableName::DB_PYTORCH_CALLCHAINS, CREATE_TABLE_DB_PYTORCH_CALLCHAINS_SQL},
    {TableName::DB_CANN_API, CREATE_TABLE_DB_CANN_API_SQL},
    {TableName::DB_MSTX_EVENTS, CREATE_TABLE_DB_MSTX_EVENTS_SQL},
    {TableName::DB_OSRT_API, CREATE_TABLE_DB_OSRT_API_SQL}, {TableName::DB_TASK, CREATE_TABLE_DB_TASK_SQL},
    {TableName::DB_COMPUTE_TASK_INFO, CREATE_TABLE_DB_COMPUTE_TASK_INFO_SQL},
    {TableName::DB_COMMUNICATION_OP, CREATE_TABLE_DB_COMMUNICATION_OP_SQL},
    {TableName::DB_COMMUNICATION_TASK_INFO, CREATE_TABLE_DB_COMMUNICATION_TASK_INFO_SQL},
    {TableName::DB_COMMUNICATION_SCHEDULE_TASK_INFO, CREATE_TABLE_DB_COMMUNICATION_SCHEDULE_TASK_INFO_SQL},
    {TableName::DB_NPU_INFO, CREATE_TABLE_DB_NPU_INFO_SQL},
    {TableName::DB_RANK_DEVICE_MAP, CREATE_TABLE_DB_RANK_DEVICE_MAP_SQL},
    {TableName::DB_ENUM_API_TYPE, CREATE_TABLE_DB_ENUM_API_TYPE_SQL},
    {TableName::DB_OVERLAP_ANALYSIS, CREATE_TABLE_DB_OVERLAP_ANALYSIS_SQL},
    {TableName::DB_AICORE_FREQ, CREATE_TABLE_DB_AICORE_FREQ_SQL}, {TableName::DB_ACC_PMU, CREATE_TABLE_DB_ACC_PMU_SQL},
    {TableName::DB_SOC_BANDWIDTH_LEVEL, CREATE_TABLE_DB_SOC_BANDWIDTH_LEVEL_SQL},
    {TableName::DB_NPU_MEM, CREATE_TABLE_DB_NPU_MEM_SQL}, {TableName::DB_HCCS, CREATE_TABLE_DB_HCCS_SQL},
    {TableName::DB_PCIE, CREATE_TABLE_DB_PCIE_SQL},
    {TableName::DB_CONNECTION_CATS, CREATE_TABLE_DB_CONNECTION_CATS_SQL}};
}
