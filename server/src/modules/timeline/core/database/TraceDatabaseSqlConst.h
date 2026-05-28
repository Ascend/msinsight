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

#ifndef PROFILER_SERVER_TRACEDATABASESQLCONST_H
#define PROFILER_SERVER_TRACEDATABASESQLCONST_H

#include <string>
#include "TableDefs.h"
#include "TraceDatabaseDef.h"
namespace Dic::Module::Timeline {
const std::string TEMP_FWDBWD_FLOW_TABLE = "tmpFwdbwdFlow";
const std::string CREATE_TEMP_FWDBWD_FLOW_TABLE_TEXT_SQL =
    "DROP TABLE IF EXISTS tmpFwdbwdFlow;"
    "CREATE TEMPORARY TABLE tmpFwdbwdFlow AS "
    "WITH combined AS ( \n"
    "    SELECT f.flow_id, f.type, f.timestamp AS slice_begin, s.end_time AS slice_end \n"
    "    FROM flow f JOIN slice s ON f.track_id = s.track_id AND f.timestamp = s.timestamp \n"
    "    WHERE f.cat = 'fwdbwd' AND f.type IN ('s', 'f') \n"
    "    ORDER by f.flow_id \n"
    "), " // 过滤flow表里前反向数据，并与slice表join，以获取每个元素的结束时间
    "fwd AS ( \n"
    "    SELECT flow_id, slice_begin, slice_end FROM combined WHERE type = 's' \n"
    "), \n"
    "bwd AS ( \n"
    "    SELECT flow_id, slice_begin, slice_end FROM combined WHERE type = 'f' \n"
    "), \n"
    "flowsAscByFwd AS ( \n"
    "    SELECT fwd.slice_begin AS fwdStart, fwd.slice_end AS fwdEnd, \n"
    "    bwd.slice_begin AS bwdStart, bwd.slice_end AS bwdEnd, \n"
    "    ROW_NUMBER() OVER (ORDER BY fwd.slice_begin) AS rowNum \n"
    "    FROM fwd JOIN bwd ON fwd.flow_id = bwd.flow_id \n"
    ") " // 将前反向联系起来，并按前向起始时间升序排列，以方便后续识别前反向边界
    "SELECT fwdStart, fwdEnd, bwdStart, bwdEnd, rowNum FROM flowsAscByFwd;";

const std::string CREATE_TEMP_FWDBWD_FLOW_TABLE_DB_SQL =
    "DROP TABLE IF EXISTS tmpFwdbwdFlow;"
    "CREATE TEMPORARY TABLE tmpFwdbwdFlow AS "
    "with type as ( \n"
    "    SELECT id FROM ENUM_API_TYPE WHERE name = 'op' \n"
    "), " // 过滤cpu_op对应的类型值
    "flowCat as ( \n"
    "    SELECT connectionId \n"
    "    FROM connectionCats WHERE cat = 'fwdbwd' ORDER BY connectionId \n"
    "), " // 过滤所有的前反向连线id
    "flowTable as ( \n"
    "    SELECT ids.id as flowId, ids.connectionId as connectionId \n"
    "    FROM flowCat cats JOIN CONNECTION_IDS ids \n"
    "    ON cats.connectionId = ids.connectionId ORDER by ids.id ASC \n"
    "), " // 过滤前反向连线flowId和connectId数据
    "apiTable as ( \n"
    "    SELECT CAST(startNs AS INTEGER) AS startNs, CAST(endNs AS INTEGER) AS endNs, connectionId FROM PYTORCH_API \n"
    "    WHERE connectionId IS NOT NULL AND type in type \n"
    "    ORDER BY connectionId \n"
    "), " // 过滤出所有的cpu_op数据
    "combined as ( \n"
    "    SELECT startNs, endNs, flow.connectionId FROM flowTable flow join apiTable api \n"
    "    ON flow.flowId = api.connectionId ORDER BY flow.connectionId ASC \n"
    "), " // 建立起连线与界面元素起始时间的联系
    "flowsAscByFwd as (\n"
    "    SELECT s.startNs as fwdStart, s.endNs as fwdEnd, f.startNs as bwdStart, f.endNs as bwdEnd, \n"
    "    ROW_NUMBER() OVER (ORDER BY s.startNs) AS rowNum \n"
    "    FROM combined s JOIN combined f \n"
    "    ON s.connectionId = f.connectionId AND s.startNs < f.startNs \n"
    ") " // 将前反向联系起来，并按前向起始时间升序排列，以方便后续识别前反向边界
    "SELECT fwdStart, fwdEnd, bwdStart, bwdEnd, rowNum FROM flowsAscByFwd;";

const std::string QUERY_FWDBWD_FLOW_DATA_SQL =
    "WITH increaseEndIndex AS ( \n"
    "    SELECT CASE WHEN d1.rowNum != 1 THEN d1.rowNum END as endIndex \n"
    "    FROM tmpFwdbwdFlow d1 LEFT JOIN tmpFwdbwdFlow d2 ON d2.rowNum = d1.rowNum + 1\n"
    "    WHERE d2.bwdStart > d1.bwdStart OR d2.bwdStart IS NULL OR d1.rowNum = 1\n"
    "), " // 按前向开始时间升序处理，前向递增，反向递减，前反向边界，反向有个增大的突变(vpp场景下，vpp的边界是例外，后续需要特殊处理)
    "flowsAscByBwd AS ( \n"
    "    SELECT fwdStart, bwdStart, rowNum as oldRowNum, \n"
    "    ROW_NUMBER() OVER (ORDER BY bwdStart) AS rowNum \n"
    "    FROM tmpFwdbwdFlow \n"
    "), " // 按反向起始时间升序排列，以方便后续识别前反向边界
    "decreaseEndIndex AS ( \n"
    "    SELECT d1.oldRowNum as endIndex \n"
    "    FROM flowsAscByBwd d1 LEFT JOIN flowsAscByBwd d2 ON d2.rowNum = d1.rowNum - 1 \n"
    "    WHERE d1.fwdStart > d2.fwdStart OR d2.fwdStart IS NULL OR d1.rowNum = 1 \n"
    "), " // 按反向开始时间升序排列，过滤出潜在的突变点，作为正向处理的补充，以解决vpp场景下
    "possibleIndex AS ( \n"
    "    SELECT * FROM increaseEndIndex WHERE endIndex is NOT NULL\n"
    "    UNION \n"
    "    SELECT * FROM decreaseEndIndex WHERE endIndex is NOT NULL\n"
    "    ORDER BY endIndex \n"
    "), " // 将前向开始时间升序识别的边界与后向开始时间升序识别的边界进行合并
    "possibleData AS ( \n"
    "    SELECT \n"
    "        CASE WHEN d1.rowNum != 1 THEN d1.fwdEnd ELSE 0 END as nextFpEnd, \n"
    "        CASE WHEN d1.rowNum != 1 THEN d1.bwdStart ELSE 0 END as nextBpStart, \n"
    "        COALESCE(d2.fwdStart, 0) AS prevFpStart, \n"
    "        COALESCE(d2.bwdEnd, 0) AS prevBpEnd, \n"
    "        ROW_NUMBER() OVER (ORDER BY d1.rowNum) AS rowNum \n"
    "    FROM tmpFwdbwdFlow d1 LEFT JOIN tmpFwdbwdFlow d2 ON d2.rowNum = d1.rowNum + 1 \n"
    "    WHERE d1.rowNum = 1 OR d1.rowNum in possibleIndex \n"
    ") \n"
    "SELECT d1.prevFpStart as fpStart, d1.prevBpEnd as bpEnd, d2.nextFpEnd as fpEnd, d2.nextBpStart as bpStart, \n"
    "d2.nextFpEnd - d1.prevFpStart as fpDuration, d1.prevBpEnd - d2.nextBpStart as bpDuration \n"
    "FROM possibleData d1 JOIN possibleData d2 ON d2.rowNum = d1.rowNum + 1 \n"
    "WHERE d1.prevFpStart >= ? AND d1.prevFpStart <= ?";
// LCOV_EXCL_BR_START
const std::string QUERY_HOST_METADATA_CANN_SQL =
    " select EAL.name, globalTid, type, max(depth) as maxDepth from CANN_API"
    " a join ENUM_API_TYPE EAL on a.type = EAL.id "
    " group by type, globalTid order by globalTid, type desc";
const std::string QUERY_HOST_METADATA_PYTORCH_SQL = " select 'PyTorch' as name, globalTid, 'pytorch' as type,"
                                                    " max(depth) as maxDepth from PYTORCH_API"
                                                    " a group by globalTid order by globalTid";
const std::string QUERY_HOST_METADATA_OSRT_SQL =
    "SELECT 'OS Runtime API' AS name, globalTid, 'OSRT_API' AS type, 0 AS maxDepth FROM OSRT_API"
    " a GROUP BY globalTid ORDER BY globalTid";
const std::string QUERY_HOST_METADATA_MSTX_SQL =
    "select coalesce(b.value, 'MSTX') as name, a.globalTid, a.domainId as type, max(a.depth) as maxDepth "
    "from MSTX_EVENTS a left join STRING_IDS b on a.domainId = b.id "
    "group by a.globalTid, a.domainId order by a.globalTid, a.domainId";
const std::string QUERY_HOST_METADATA_PYTHONGC_SQL =
    "select 'Python GC' as name, globalTid,'Python GC' as type,  0 as maxDepth from GC_RECORD a "
    " group by globalTid order by globalTid";

const std::string QUERY_KERNEL_SQL =
    "select info.ROWID as id, groupName||'group' as tid, opName as name, 'HCCL' as pid,"
    " 0 as depth, 'HCCL' as metaType from COMMUNICATION_OP info "
    " where name = (select id from STRING_IDS where value = ?) and abs(startNs - ?) <= 500 "
    " UNION all "
    " select T.ROWID as id, groupName || '_' || planeId as tid, info.taskType as name, 'HCCL' as pid, 0 AS depth,"
    " 'HCCL' as metaType from COMMUNICATION_TASK_INFO info join TASK T on info.globalTaskId = T.globalTaskId "
    " where info.taskType = (select id from STRING_IDS where value = ?) and abs(startNs - ?) <= 500"
    " UNION all "
    " select T.ROWID as id, T.streamId as tid, name, 'Ascend Hardware' as pid, depth, "
    " 'Ascend Hardware' as metaType from COMPUTE_TASK_INFO info join TASK T on info.globalTaskId = T.globalTaskId"
    " where name = (select id from STRING_IDS where value = ?) and abs(startNs - ?) <= 500"
    " UNION all "
    " select T.ROWID as id, T.streamId as tid, T.taskType AS name, 'Ascend Hardware' as pid, depth, "
    " 'Ascend Hardware' as metaType from MEMCPY_INFO info join TASK T on info.globalTaskId = T.globalTaskId"
    " where name = (select id from STRING_IDS where value = ?) and abs(startNs - ?) <= 500"
    " UNION all "
    " select info.ROWID as id, domainId as tid, message as name, globalTid AS pid,"
    " depth, 'MSTX_EVENTS' as metaType from MSTX_EVENTS info"
    " where name = (select id from STRING_IDS where value = ?) and abs(startNs - ?) <= 500"
    " UNION all "
    " select ca.ROWID as id, ca.type AS tid, ca.name as name, ca.globalTid AS pid,"
    " depth, 'CANN_API' as metaType from CANN_API ca"
    " where name = (select id from STRING_IDS where value = ?) and abs(startNs - ?) <= 500"
    " UNION all "
    " SELECT pa.ROWID AS id, 'pytorch' AS tid, name, globalTid AS pid, depth, "
    " 'PYTORCH_API' as metaType from PYTORCH_API pa "
    " where name = (select id from STRING_IDS where value = ?) and abs(startNs - ?) <= 500"
    " UNION ALL "
    " SELECT osrt.ROWID AS id, 'OSRT_API' AS tid, name, globalTid AS pid, 0 AS depth, "
    " 'OSRT_API' as metaType FROM OSRT_API osrt "
    " WHERE name = (SELECT id FROM STRING_IDS WHERE value = ?) AND abs(startNs - ?) <= 500";

// 兼容老版本（1.0.0）
const std::string QUERY_COMMUNICATION_GROUP_MAP_DB_1_0_SQL =
    "SELECT groupName, planeId, 'Plane ' || planeId as threadName FROM " + TABLE_COMMUNICATION_TASK_INFO +
    " cti "
    "JOIN " +
    TABLE_TASK +
    " task ON cti.globalTaskId = task.globalTaskId WHERE task.deviceId = ? "
    "GROUP BY groupName || planeId "
    "UNION "
    "SELECT op.groupName, -1 as planeId, 'Group ' || row_num || ' Communication' as threadName "
    "FROM " +
    TABLE_COMMUNICATION_OP +
    " op JOIN ( "
    "    SELECT groupName, row_number() OVER (ORDER BY groupName ASC) - 1 as row_num "
    "    FROM " +
    TABLE_COMMUNICATION_OP +
    " GROUP BY groupName "
    ") grp ON op.groupName = grp.groupName "
    "GROUP BY op.groupName";
const std::string QUERY_COMMUNICATION_GROUP_MAP_DB_SQL =
    "    SELECT groupName, planeId, 'Plane ' || planeId as threadName FROM COMMUNICATION_TASK_INFO cti "
    "JOIN " +
    TABLE_TASK +
    " task ON cti.globalTaskId = task.globalTaskId WHERE task.deviceId = ? "
    "    GROUP BY groupName || planeId  "
    "    UNION "
    "    SELECT op.groupName, -1 as planeId, 'Group ' || strGroup.value || ' Communication' as threadName "
    "    FROM COMMUNICATION_OP op JOIN ( "
    "        SELECT groupName "
    "        FROM  COMMUNICATION_OP  GROUP BY groupName "
    "    ) grp ON op.groupName = grp.groupName "
    "    JOIN STRING_IDS strGroup ON op.groupName = strGroup.id "
    "    GROUP BY op.groupName ";

// 兼容老版本（1.0.0）
const std::string QUERY_COMMUNICATION_SUMMARY_DB_1_0_SQL =
    "WITH data AS ("
    "    SELECT name, start_time - ? as start_time, duration, end_time - ? as end_time, groupName, planeId, "
    "    thread_name, type, row_number() OVER (ORDER BY groupName ASC, planeId ASC, start_time ASC) as row_num "
    "    FROM ("
    "        SELECT str1.value as name, task.startNs as start_time, task.endNs - task.startNs as duration, "
    "        task.endNs as end_time, groupName, planeId, 'Plane ' || planeId as thread_name, 0 as type "
    "        FROM " +
    TABLE_COMMUNICATION_TASK_INFO +
    " info "
    "        JOIN " +
    TABLE_STRING_IDS +
    " str1 ON info.taskType = str1.id "
    "        JOIN " +
    TABLE_TASK +
    " task ON info.globalTaskId = task.globalTaskId "
    "        WHERE task.deviceId = ? "
    "        UNION "
    "        SELECT str2.value as name, startNs as start_time, endNs - startNs as duration, "
    "        endNs as end_time, op.groupName, -1 as planeId, "
    "        'Group ' || row_num || ' Communication' as thread_name, 1 as type "
    "        FROM " +
    TABLE_COMMUNICATION_OP +
    " op "
    "        JOIN " +
    TABLE_STRING_IDS +
    " str2 ON op.opName = str2.id "
    "        JOIN ( "
    "            SELECT groupName, row_number() OVER (ORDER BY groupName ASC) - 1 as row_num "
    "            FROM " +
    TABLE_COMMUNICATION_OP +
    " GROUP BY groupName "
    "        ) grp ON op.groupName = grp.groupName "
    "    ) "
    ") ";
const std::string QUERY_COMMUNICATION_SUMMARY_DB_SQL =
    "  WITH data AS ("
    "      SELECT name, start_time - ? as start_time, duration, end_time - ? as end_time, groupName, planeId, "
    "      thread_name, type, row_number() OVER (ORDER BY groupName ASC, planeId ASC, start_time ASC) as row_num "
    "  FROM ("
    "      SELECT str1.value as name, task.startNs as start_time, task.endNs - task.startNs as duration, "
    "      task.endNs as end_time, groupName, planeId, 'Plane ' || planeId as thread_name, 0 as type "
    "      FROM COMMUNICATION_TASK_INFO info "
    "      JOIN STRING_IDS str1 ON info.taskType = str1.id "
    "      JOIN TASK task ON info.globalTaskId = task.globalTaskId "
    "      WHERE task.deviceId = ? "
    "      UNION "
    "      SELECT str2.value as name, startNs as start_time, endNs - startNs as duration, "
    "      endNs as end_time, op.groupName, -1 as planeId,  "
    "      'Group ' || strGroup.value || ' Communication'  as thread_name,"
    "       1 as type "
    "      FROM COMMUNICATION_OP op "
    "      JOIN STRING_IDS str2 ON op.opName = str2.id "
    "      JOIN STRING_IDS strGroup ON op.groupName = strGroup.id "
    "      JOIN ( "
    "          SELECT groupName, row_number() OVER (ORDER BY groupName ASC) - 1 as row_num "
    "          FROM COMMUNICATION_OP GROUP BY groupName "
    "          )"
    "      grp ON op.groupName = grp.groupName "
    "      )"
    " ) ";

// 兼容老版本（1.0.0）
const std::string QUERY_COMMUNICATION_GROUP_ID_DB_1_0_SQL =
    "SELECT groupId, 'Group ' || row_num || ' Communication' as groupName "
    "FROM ( "
    "    SELECT co.groupName as groupId, row_number() OVER (ORDER BY co.groupName ASC) -1 as row_num "
    "    FROM " +
    TABLE_COMMUNICATION_OP +
    " co"
    "    JOIN " +
    TABLE_COMMUNICATION_TASK_INFO +
    " cti ON co.opId = cti.opId"
    "    JOIN " +
    TABLE_TASK +
    " t ON cti.globalTaskId = t.globalTaskId"
    "    WHERE t.deviceId = ?"
    " GROUP BY co.groupName )";

const std::string QUERY_COMMUNICATION_GROUP_ID_DB_SQL =
    "SELECT op.groupName as groupId, 'Group ' || str.value || ' Communication' as groupName "
    "FROM ( "
    "    SELECT co.groupName FROM " +
    TABLE_COMMUNICATION_OP +
    " co"
    "    JOIN " +
    TABLE_COMMUNICATION_TASK_INFO +
    " cti ON co.opId = cti.opId"
    "    JOIN " +
    TABLE_TASK +
    " t ON cti.globalTaskId = t.globalTaskId"
    "    WHERE t.deviceId = ?"
    " GROUP BY co.groupName ORDER BY co.groupName ASC "
    ") op JOIN " +
    TABLE_STRING_IDS + " str on op.groupName = str.id";
// LCOV_EXCL_BR_STOP
class TraceDatabaseSqlConst {
    // LCOV_EXCL_BR_START
  public:
    static std::string GetOverlapAnalysisDbSqlByType(const SystemViewOverallReqParam &params) {
        std::string timeCondSql = AppendDbTimeRangeConditionSql(params.startTime, params.endTime);
        return "SELECT deviceId as name, startNs - ? as startNs, endNs - ? as endNs, endNs - startNs as duration "
               "FROM " +
            TABLE_OVERLAP_ANALYSIS + " WHERE deviceId = ? AND type = ? " + timeCondSql +
            " ORDER BY deviceId ASC, startNs ASC";
    }

    static std::string GetCommunicationOpDbSqlByGroupId(const SystemViewOverallReqParam &params) {
        std::string timeCondSql;
        if (params.startTime != params.endTime) { // time range analysis
            timeCondSql += " AND op.endNs >= ? AND op.startNs <= ? ";
        }
        std::string nameCondSql;
        if (!params.name.empty()) {
            nameCondSql += " AND str.value LIKE ? ";
        }
        return "SELECT DISTINCT op.opId as id, str.value as name, op.startNs - ? as startNs, "
               "op.endNs - op.startNs as duration, op.endNs - ? as endNs "
               "FROM " +
            TABLE_COMMUNICATION_OP + " op JOIN " + TABLE_STRING_IDS +
            " str ON op.opName = str.id "
            "JOIN " +
            TABLE_COMMUNICATION_TASK_INFO +
            " cti ON op.opId = cti.opId "
            "JOIN " +
            TABLE_TASK +
            " t ON cti.globalTaskId = t.globalTaskId "
            "WHERE t.deviceId = ? AND op.groupName = ? " +
            nameCondSql + timeCondSql + " ORDER BY op.startNs ASC";
    }

    static std::string GenerateAffinityApiDbSql(const Protocol::KernelDetailsParams &params) {
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND py.endNs >= ? AND py.startNs <= ? ";
        }
        return "SELECT py.ROWID as id, str.value as name, py.startNs - ? as startTime, "
               "py.endNs - ? as endTime, py.globalTid as pid, 'pytorch' as tid, py.depth as depth "
               "FROM " +
            TABLE_API + " py JOIN " + TABLE_STRING_IDS +
            " str ON py.name = str.id "
            "WHERE (str.value LIKE 'aten::%' OR str.value LIKE 'npu::%') " +
            timeCondSql + "ORDER BY py.globalTid ASC, py.startNs ASC ";
    }

    static std::string GenerateAclnnQueryDbSql(const Protocol::KernelDetailsParams &params) {
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND task.endNs >= ? AND task.startNs <= ? ";
        }
        return "SELECT task.ROWID as id, s1.value as name, s2.value as op_type, task.taskType, "
               "task.startNs - ? as startTime, (task.endNs - task.startNs) as duration, 'Ascend Hardware' as pid, "
               "task.streamId as tid, task.depth as depth "
               "FROM " +
            TABLE_COMPUTE_TASK_INFO +
            " info "
            "JOIN " +
            TABLE_TASK +
            " task ON info.globalTaskId = task.globalTaskId "
            "JOIN " +
            TABLE_STRING_IDS +
            " s1 ON info.name = s1.id "
            "JOIN " +
            TABLE_STRING_IDS +
            " s2 ON info.opType = s2.id "
            "WHERE task.deviceId = ? " +
            timeCondSql +
            " AND s1.value IN ("
            "    SELECT str.value FROM " +
            TABLE_COMPUTE_TASK_INFO +
            " info "
            "    JOIN " +
            TABLE_STRING_IDS +
            " str ON info.name = str.id "
            "    WHERE str.value LIKE 'aclnn%' "
            "    GROUP BY str.value HAVING COUNT(str.value) >= ? "
            ") ORDER BY " +
            params.orderBy + " " + params.order;
    }

    static std::string GenerateOperatorDispatchQueryDbSql(const Protocol::KernelDetailsParams &params) {
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND ca.endNs >= ? AND ca.startNs <= ? ";
        }
        return "SELECT"
               "  ca.ROWID as id, "
               "  s.value as name, "
               "  ca.startNs - ? as startTime, "
               "  (ca.endNs - ca.startNs) as duration, "
               "  ca.globalTid as pid, "
               "  ca.type as tid, "
               "  ca.depth as depth "
               "FROM " +
            TABLE_CANN_API +
            " ca "
            "JOIN " +
            TABLE_ENUM_API_TYPE +
            " enum ON ca.type = enum.id "
            "JOIN " +
            TABLE_STRING_IDS +
            " s ON ca.name = s.id "
            "WHERE s.value LIKE '%aclopCompileAndExecute' " +
            timeCondSql + "ORDER BY " + params.orderBy + " " + params.order;
    }

    static std::string GenerateAICpuQueryDbSql(const std::vector<std::string> &replace,
        const Protocol::KernelDetailsParams &params,
        const std::map<std::string, Timeline::AICpuCheckDataType> &dataTypeMap) {
        std::vector<std::string> opTypeList{};
        for (const auto &item : dataTypeMap) { // 获取除other以外的算子类型列表
            if (item.first != "other") {
                opTypeList.emplace_back(item.first);
            }
        }
        std::vector<std::string> dataTypeCheck{};
        for (const auto &item : dataTypeMap) {
            std::string opType = item.first;
            if (item.first == "other") { // 对于other，使用Not IN排除opTypeList以外的算子类型
                opType = StringUtil::Join4SqlGroup(opTypeList);
            }
            dataTypeCheck.emplace_back(GenerateAICpuOpFilterSqlDB(opType, item.second));
        }
        std::string dataTypeCheckSql = StringUtil::join(dataTypeCheck, "OR");
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND t.endNs >= ? AND t.startNs <= ? ";
        }
        std::string sql = "SELECT info.ROWID as id, s2.value as name, s1.value as type, s0.value as unit, "
                          "t.startNs - ? as startTime, (t.endNs - t.startNs) as duration, 'Ascend Hardware' as pid, "
                          "t.streamId as tid, t.depth as depth, lower(s3.value) as input, lower(s4.value) as output "
                          "FROM COMPUTE_TASK_INFO info "
                          "JOIN STRING_IDS s0 ON info.taskType = s0.id "
                          "JOIN TASK t ON info.globalTaskId = t.globalTaskId "
                          "JOIN STRING_IDS s1 ON info.opType = s1.id "
                          "JOIN STRING_IDS s2 ON info.name = s2.id "
                          "JOIN STRING_IDS s3 ON info.inputDataTypes = s3.id "
                          "JOIN STRING_IDS s4 ON info.outputDataTypes = s4.id "
                          "WHERE t.deviceId = ? AND s0.value ='AI_CPU' " +
            timeCondSql +
            " AND ("
            "    lower(s1.value) IN (" +
            StringUtil::Join4SqlGroup(replace) +
            ") " // 特定类型的算子可以修改代码
            "    OR ("
            "    " +
            dataTypeCheckSql + // 检查数据类型是否符合要求
            "    ) OR "
            "    duration >= ?" // 执行时间超过20us
            ") ORDER BY " +
            params.orderBy + " " + params.order;
        return sql;
    }

    static std::string QueryAffinityOptimizerDbSql(
        const std::string &optimizers, const Protocol::KernelDetailsParams &params) {
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND py.endNs >= ? AND py.startNs <= ? ";
        }
        return "SELECT py.ROWID as id, py.startNs - ? as startTime, (py.endNs - py.startNs) as duration, "
               "str.value as originOptimizer, py.globalTid as pid, 'pytorch' as tid, py.depth as depth "
               "FROM " +
            TABLE_STRING_IDS + " str JOIN " + TABLE_API +
            " py ON py.name = str.id "
            "WHERE str.value IN (" +
            optimizers + ") " + timeCondSql + " ORDER BY " + params.orderBy + " " + params.order;
    }

    static std::string GeneratorCommunicationSummarySql4Db(
        const Protocol::SystemViewOverallReqParam &requestParams, const std::string &sqlForVersion) {
        std::string timeCondSql;
        if (requestParams.startTime != requestParams.endTime) {
            timeCondSql += " WHERE d1.end_time >= ? AND d1.start_time <= ? ";
        }
        std::string sql = sqlForVersion +
            "SELECT d1.name as name, d1.start_time as startTime, d1.duration as duration, d1.end_time as endTime, "
            "d1.groupName as groupName, d1.planeId as plane, d1.thread_name as threadName, d1.type as type, "
            "CASE "
            "    WHEN d1.name = 'Notify_Wait' THEN "
            "        CASE "
            "           WHEN d2.name = 'RDMASend' AND d3.name = 'RDMASend' OR "
            "               (d3.name = 'Notify_Wait' AND d4.name = 'RDMASend' AND d5.name = 'RDMASend') THEN '0' "
            "           ELSE '1' "
            "        END "
            "    ELSE '0' "
            "END as flag "
            "FROM data d1 "
            "LEFT JOIN data d2 ON d2.groupName = d1.groupName AND d2.planeId = d1.planeId AND d2.row_num = d1.row_num "
            "- 1 "
            "LEFT JOIN data d3 ON d3.groupName = d1.groupName AND d3.planeId = d1.planeId AND d3.row_num = d1.row_num "
            "- 2 "
            "LEFT JOIN data d4 ON d4.groupName = d1.groupName AND d4.planeId = d1.planeId AND d4.row_num = d1.row_num "
            "- 3 "
            "LEFT JOIN data d5 ON d5.groupName = d1.groupName AND d5.planeId = d1.planeId AND d5.row_num = d1.row_num "
            "- 4 " +
            timeCondSql + "ORDER BY d1.groupName, d1.planeId, d1.start_time";
        return sql;
    }

    static std::string AppendDbTimeRangeConditionSql(const uint64_t &startTime, const uint64_t &endTime) {
        if (startTime == endTime) { // default request, not time range analysis
            return "";
        }
        return " AND endNs >= ? AND startNs <= ? ";
    }

  private:
    static std::string GenerateAICpuOpFilterSqlDB(
        const std::string &opType, const Timeline::AICpuCheckDataType &dataType) {
        std::string sql = " ( ";
        if (std::find(opType.begin(), opType.end(), ',') == opType.end()) { // 输入为单个算子类型
            sql += "lower(s1.value) = '" + opType + "' AND ";
        } else { // 输入为算子类型组
            sql += "lower(s1.value) NOT IN ( " + opType + " ) AND ";
        }
        sql += "lower(s3.value) NOT IN ( " + StringUtil::Join4SqlGroup(dataType.input) +
            " ) AND "
            "lower(s4.value) NOT IN ( " +
            StringUtil::Join4SqlGroup(dataType.output) + " )) ";
        return sql;
    }
    // LCOV_EXCL_BR_STOP
};
}
#endif // PROFILER_SERVER_TRACEDATABASESQLCONST_H
