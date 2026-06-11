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

#ifndef PROFILER_SERVER_DBSQLDEFS_H
#define PROFILER_SERVER_DBSQLDEFS_H
#include <string>
#include "ConstantDefs.h"

namespace Dic {
// init full db
const static std::map<std::string, std::string> FULL_DB_TABLE_MAP = {
    {TABLE_TASK,
        "create TEMPORARY table if not exists TASK( startNs INTEGER, endNs INTEGER, deviceId INTEGER, "
        " connectionId INTEGER, globalTaskId INTEGER, globalPid INTEGER, taskType INTEGER,contextId INTEGER, "
        " streamId INTEGER, taskId INTEGER, modelId INTEGER, depth integer);"},
    {TABLE_CANN_API,
        "create TEMPORARY table if not exists CANN_API(startNs INTEGER, endNs INTEGER, type INTEGER, "
        " globalTid INTEGER, connectionId INTEGER primary key, name INTEGER, depth integer);  "},
    {TABLE_COMMUNICATION_OP,
        "create TEMPORARY table if not exists COMMUNICATION_OP( opName INTEGER, startNs INTEGER, "
        " endNs INTEGER, connectionId INTEGER, groupName INTEGER, opId INTEGER primary key, "
        " relay INTEGER, retry INTEGER, dataType INTEGER, algType INTEGER, count NUMERIC, waitNs INTEGER);  "},
    {TABLE_CCU,
        "create TEMPORARY table if not exists CCU(deviceId INTEGER, globalTaskId INTEGER, name INTEGER, "
        "startNs INTEGER, endNs INTEGER, args INTEGER);"},
    {TABLE_COMMUNICATION_TASK_INFO,
        "create TEMPORARY table if not exists COMMUNICATION_TASK_INFO( name INTEGER, "
        " globalTaskId INTEGER, taskType INTEGER, planeId INTEGER, groupName INTEGER, notifyId INTEGER,"
        " rdmaType INTEGER, srcRank INTEGER, dstRank INTEGER, transportType INTEGER, size INTEGER, "
        " dataType INTEGER, linkType INTEGER, opId INTEGER);  "},
    {TABLE_COMPUTE_TASK_INFO,
        "create TEMPORARY table if not exists COMPUTE_TASK_INFO( name INTEGER, "
        " globalTaskId INTEGER primary key, blockNum INTEGER, mixBlockDim INTEGER, taskType INTEGER,"
        " opType INTEGER, inputFormats INTEGER, inputDataTypes INTEGER, inputShapes INTEGER, "
        " outputFormats INTEGER, outputDataTypes INTEGER, outputShapes INTEGER, attrInfo INTEGER, "
        " waitNs INTEGER, gridDim INTEGER, blockDim INTEGER);  "},
    {TABLE_MEMCPY_INFO,
        "create TEMPORARY table if not exists MEMCPY_INFO(globalTaskId NUMERIC, size NUMERIC,"
        " memcpyOperation NUMERIC);"},
    {TABLE_CONNECTION_IDS, "create TEMPORARY table if not exists CONNECTION_IDS( id INTEGER, connectionId INTEGER);  "},
    {TABLE_ENUM_API_TYPE, "create TEMPORARY table if not exists ENUM_API_TYPE( id INTEGER primary key, name TEXT);  "},
    {TABLE_API,
        "create TEMPORARY table if not exists PYTORCH_API(startNs TEXT, endNs TEXT,globalTid INTEGER, "
        " type INTEGER, connectionId INTEGER, name INTEGER, sequenceNumber INTEGER, fwdThreadId INTEGER,inputDtypes "
        "INTEGER, "
        " inputShapes INTEGER, callchainId INTEGER, depth integer);  "},
    {TABLE_MSTX_EVENTS,
        "create TEMPORARY table if not exists MSTX_EVENTS(startNs INTEGER,endNs INTEGER, "
        " eventType INTEGER,rangeId INTEGER, category INTEGER, message INTEGER, globalTid INTEGER, "
        " endGlobalTid INTEGER, domainId INTEGER, connectionId INTEGER, depth integer); "},
    {TABLE_OSRT_API,
        "create TEMPORARY table if not exists OSRT_API(name INTEGER, globalTid NUMERIC,"
        " startNs INTEGER, endNs INTEGER);"},
    {TABLE_PYTORCH_CALLCHAINS,
        "create TEMPORARY table if not exists PYTORCH_CALLCHAINS(id INTEGER, stack INTEGER"
        " , stackDepth INTEGER );"},
    {TABLE_COMMUNICATION_SCHEDULE_TASK,
        "create TEMPORARY table if not exists COMMUNICATION_SCHEDULE_TASK_INFO("
        "name INTEGER, globalTaskId INTEGER primary key, taskType INTEGER, "
        "opType INTEGER);"},
    {TABLE_NPU_INFO, "create TEMPORARY table if not exists NPU_INFO(id INTEGER primary key, name TEXT);"},
    {TABLE_STRING_IDS, "create TEMPORARY table if not exists STRING_IDS(id INTEGER primary key, value TEXT);"}};

// sql of same operate detail
inline std::string GetAscendSameNameDetailSql(const std::vector<std::string> &tidList) {
    std::string sql;
    // 已经在DbTraceDataBase::QueryThreadSameOperatorsDetails中检查过tid sql注入风险
    // Device侧的非MSTX事件和MSTX事件分开显示，其中MSTX事件会分domainId展示，且摆放在非MSTX事件的上方
    // 非MSTX事件的threadId是其Stream编号，MSTX事件的threadId是{Stream编号}_{domain编号}
    // 非MSTX事件查询时必须使用connectionId NOT IN显式排除MSTX事件，否则会将MSTX事件同时查询
    // 因为TASK表没有字段表征该事件是否为MSTX事件，所以需要和MSTX_EVENTS表连接，和MSTX_EVENTS表中具有相同connectionId的事件才是Device侧的MSTX事件
    // 因为DbTraceDataBase在执行OpenDb()方法时当MSTX_EVENTS表不存在时，会创建临时表MSTX_EVENTS，所以可以默认MSTX_EVENTS表在操作数据库时存在
    const std::string tidListStr = StringUtil::Join4SqlGroup(tidList);
    sql = " select main.startNs - p.minTime as timestamp, main.endNs - main.startNs as duration, "
          "   main.depth, main.ROWID as id, streamId as tid, 'Ascend Hardware' as pid from TASK main "
          "   left join COMPUTE_TASK_INFO c on c.globalTaskId = main.globalTaskId left join " +
        TABLE_COMMUNICATION_SCHEDULE_TASK +
        " s on main.globalTaskId = s.globalTaskId "
        "     join nameIds n on coalesce(c.name, s.name, main.taskType) = id join params p"
        " where deviceId = p.rankId and main.connectionId NOT IN (select connectionId from " +
        TABLE_MSTX_EVENTS + ") and streamId in (" + tidListStr +
        " ) and timestamp + duration >= p.startTime AND timestamp <= p.endTime";

    sql += " union select main.startNs - p.minTime as timestamp, main.endNs - main.startNs as duration, "
           "main.depth, main.ROWID as id, streamId || '_' || m.domainId as tid, 'Ascend Hardware' as pid from TASK "
           "as main inner join " +
        TABLE_MSTX_EVENTS +
        " as m on main.connectionId = m.connectionId "
        "inner join nameIds n on m.message = id "
        "inner join params p where deviceId = p.rankId "
        " and streamId || '_' || m.domainId in (" +
        tidListStr +
        ")"
        " and timestamp + duration >= p.startTime and timestamp <=p.endTime";

    return sql;
}
inline std::string GetHcclSameNameDetailSql(const std::string &tidListStr, const bool uniqueDevice) {
    return " select startNs - p.minTime as timestamp, endNs - startNs as duration, 0 as depth, "
           "   main.ROWID as id, groupName || '_' || planeId as tid, 'HCCL' as pid from TASK main "
           "   join COMMUNICATION_TASK_INFO c on c.globalTaskId = main.globalTaskId join nameIds on c.taskType = id "
           "   join params p where deviceId = p.rankId and groupName || '_' || planeId in (" +
        tidListStr + "   ) and timestamp+duration >= p.startTime AND timestamp <= p.endTime " +
        (uniqueDevice
                ? " UNION select op.startNs - minTime as timestamp, op.endNs - op.startNs as duration, 0 as depth, "
                  "   op.opId as id , op.groupName || 'group' as tid, 'HCCL' as pid from COMMUNICATION_OP op "
                  "   join nameIds on op.opName = id join params p where op.groupName || 'group' in (" +
                    tidListStr +
                    "   ) and timestamp + duration >= p.startTime AND timestamp <= p.endTime group by op.opId "
                : " UNION select op.startNs - p.minTime as timestamp, op.endNs - op.startNs as duration, 0 as depth, "
                  "   op.opId as id , op.groupName || 'group' as tid, 'HCCL' as pid from COMMUNICATION_OP op "
                  "   join COMMUNICATION_TASK_INFO c ON op.opId = c.opId "
                  "   join TASK main on c.globalTaskId = main.globalTaskId "
                  "   join nameIds on op.opName = id join params p "
                  "   where main.deviceId = p.rankId and op.groupName||'group' in (" +
                    tidListStr +
                    "   ) and timestamp+duration >= p.startTime AND timestamp <= p.endTime group by op.opId ");
}
inline std::string GetCANNSameNameDetailSql(const std::string &pidListStr, const std::string &tidListStr) {
    return " select startNs - p.minTime as timestamp, endNs - startNs as duration, depth, "
           "   main.ROWID as id, type as tid, globalTid as pid"
           " from CANN_API main join nameIds n on name = n.id join params p where globalTid in (" +
        pidListStr + " ) and type in (" + tidListStr +
        " ) and timestamp + duration >= p.startTime AND timestamp <= p.endTime ";
}
inline std::string GetMstxSameNameDetailSql(const std::string &pidListStr, const std::string &tidListStr) {
    return " select startNs - p.minTime as timestamp, endNs - startNs as duration, depth, main.ROWID as id, "
           "   domainId as tid, globalTid as pid from MSTX_EVENTS main join nameIds n on message = n.id join params p"
           "   where globalTid in (" +
        pidListStr + ") and domainId in (" + tidListStr +
        " ) "
        "   and timestamp + duration >= p.startTime AND timestamp <= p.endTime ";
}
inline std::string GetPythonSameNameDetailSql(const std::string &pidListStr) {
    return " select startNs - p.minTime as timestamp, endNs - startNs as duration, depth, main.ROWID as id,"
           "   'pytorch' as tid, globalTid as pid from PYTORCH_API main join nameIds n on name = n.id join params p\n"
           "   where globalTid in (" +
        pidListStr + ") and timestamp + duration >= p.startTime AND timestamp <= p.endTime ";
}
inline std::string GetOverlapAnalysisSameNameDetailSql(const int type) {
    return " select startNs - p.minTime as timestamp, endNs - startNs as duration, 0 as depth, "
           " main.ROWID as id , type as tid, 'OVERLAP_ANALYSIS' as pid"
           " from OVERLAP_ANALYSIS main join params p where deviceId = p.rankId and type = " +
        std::to_string(type) + " and timestamp + duration >= p.startTime AND timestamp <= p.endTime ";
}
inline std::string GetCcuSameNameDetailSql(const std::string &tidListStr) {
    return " select startNs - p.minTime as timestamp, endNs - startNs as duration, 0 as depth, "
           " main.ROWID as id, deviceId as tid, 'CCU' as pid from CCU main join nameIds n on name = n.id "
           " join params p where deviceId = p.rankId and deviceId in (" +
        tidListStr + " ) and timestamp + duration >= p.startTime AND timestamp <= p.endTime ";
}
inline std::string GetOsrtSameNameDetailSql(const std::string &pidListStr) {
    return "SELECT startNs - p.minTime AS timestamp, endNs - startNs AS duration, 0 AS depth,"
           " main.ROWID AS id, 'OSRT_API' AS tid, globalTid AS pid"
           " FROM OSRT_API main JOIN nameIds n ON name = n.id JOIN params p WHERE globalTid IN (" +
        pidListStr + " ) AND timestamp + duration >= p.startTime AND timestamp <= p.endTime ";
}
// sql of singleUnitFlow
const static std::string PYTORCH_UNIT_FLOW_SQL =
    " select api.ROWID as id, 'pytorch' as tid, depth, startNs - constValue.minTime as startTime, "
    "     endNs - startNs as duration, globalTid as pid, 'PYTORCH_API' as metaType, name, "
    "     '' as deviceId from PYTORCH_API api join constValue "
    "     join CONNECTION_IDS ids on api.connectionId = ids.id and ids.connectionId = constValue.connectionId ";
const static std::string CANN_UNIT_FLOW_SQL =
    " select api.ROWID as id, type as tid, depth, startNs - constValue.minTime as startTime, "
    "     endNs - startNs as duration, globalTid as pid, 'CANN_API' as metaType, name, "
    "     '' as deviceId from CANN_API api join constValue "
    "     where api.connectionId = constValue.connectionId ";
const static std::string MSTX_UNIT_FLOW_SQL =
    " select api.ROWID as id, domainId as tid, depth, startNs - constValue.minTime as startTime, "
    "     endNs - startNs as duration, globalTid as pid, 'MSTX_EVENTS' as metaType, message as name, "
    "     '' as deviceId from MSTX_EVENTS api join constValue "
    "     where api.connectionId = constValue.connectionId and api.connectionId != 4294967295";
const static std::string TASK_UNIT_FLOW_SQL =
    " select task.ROWID as id, task.streamId as tid, task.depth as depth,"
    " task.startNs - constValue.minTime as startTime, "
    " task.endNs - task.startNs as duration, 'Ascend Hardware' as pid, 'Ascend Hardware' as metaType, "
    " task.taskType as name, task.deviceId as deviceId, m.domainId as domainId from TASK task join constValue "
    " left join MSTX_EVENTS m on task.connectionId = m.connectionId "
    " where task.connectionId = constValue.connectionId "
    " and task.connectionId != " +
    WRONG_DATA + " ";
const static std::string COM_OP_UNIT_FLOW_SQL =
    " select op.ROWID as id,groupName||'group' as tid,0 as depth,op.startNs-constValue.minTime as startTime,"
    "     op.endNs - op.startNs as duration, 'HCCL' as pid, 'HCCL' as metaType, opName as name, "
    " task.deviceId from COMMUNICATION_OP op join constValue join TASK task on task.connectionId = op.connectionId "
    "     where op.connectionId = constValue.connectionId group by opId ";
const static std::string COM_OP_UNIT_FLOW_SQL_UNIQUE_DEVICE =
    " select op.ROWID as id,groupName||'group' as tid,0 as depth,op.startNs-constValue.minTime as startTime,"
    "     op.endNs - op.startNs as duration, 'HCCL' as pid, 'HCCL' as metaType, opName as name, "
    "     ? as deviceId from COMMUNICATION_OP op join constValue "
    "     where op.connectionId = constValue.connectionId group by opId ";

// full_db_update_wait_time
const static std::string FULL_DB_UPDATE_TIME =
    "SELECT deviceId, startNs, endNs,'compute' AS type, CTI.ROWID AS id FROM TASK main JOIN "
    " COMPUTE_TASK_INFO CTI ON main.globalTaskId = CTI.globalTaskId UNION SELECT TASK.deviceId, "
    "opInfo.startNs, opInfo.endNs, 'communication' AS type, opInfo.ROWID AS id FROM COMMUNICATION_OP "
    "opInfo JOIN TASK ON TASK.connectionId = opInfo.connectionId ORDER BY startNs;";

// QueryThreadsByPid
const static std::string ASCEND_THREADS_EXCLUDING_MSTX_BY_PID =
    "select main.ROWID as id, main.startNs,main.endNs - main.startNs as duration,main.endNs, "
    " coalesce(c.name, s.name, main.taskType) as name, main.depth "
    " from " +
    TABLE_TASK + " main left join " + TABLE_COMPUTE_TASK_INFO +
    " c on c.globalTaskId = main.globalTaskId "
    " left join " +
    TABLE_COMMUNICATION_SCHEDULE_TASK +
    " s on main.globalTaskId = s.globalTaskId"
    " where deviceId = ? and streamId = ? and main.endNs >= ? AND main.startNs <= ?"
    " and connectionId not in (select connectionId from " +
    TABLE_MSTX_EVENTS +
    ")"
    " ORDER BY main.depth ASC, main.startNs ASC;";
const static std::string ASCEND_THREADS_MSTX_BY_PID =
    "SELECT main.ROWID as id,main.startNs, main.endNs - main.startNs as duration, main.endNs, m.message AS name, "
    "main.depth FROM " +
    TABLE_TASK +
    " AS main "
    "INNER JOIN " +
    TABLE_MSTX_EVENTS +
    " AS m ON main.connectionId = m.connectionId "
    "WHERE main.deviceId = ? AND main.streamId = ? AND m.domainId = ? AND main.endNs >= ? AND main.startNs <= ? "
    "ORDER BY main.depth ASC, main.startNs ASC";

const static std::string HCCL_THREADS_BY_PID =
    "with sub as ("
    "select main.ROWID as id, startNs, endNs-startNs as duration, endNs, info.taskType as name from " +
    TABLE_TASK + " main join " + TABLE_COMMUNICATION_TASK_INFO +
    " info on info.globalTaskId = main.globalTaskId\n"
    " where deviceId = ? and groupName || '_' || planeId = ?\n"
    " UNION select opInfo.opId as id, startNs,endNs-startNs as duration,endNs,opInfo.opName"
    " from COMMUNICATION_OP opInfo"
    " where groupName||'group' = ?) select * from sub where sub.endNs >= ? "
    " and sub.startNs <= ?;";

const static std::string CANN_API_THREADS_BY_PID =
    "select main.ROWID as id, startNs, endNs - startNs as duration, endNs, name, depth from CANN_API"
    " main where type = ? and globalTid = ? and endNs >= ? AND startNs <= ?"
    " ORDER BY depth ASC, startNs ASC;";

const static std::string API_THREADS_BY_PID =
    "select main.ROWID as id, startNs, endNs - startNs as duration, endNs, name, depth from PYTORCH_API "
    " main where globalTid = ? and endNs >= ? AND startNs <= ?"
    " ORDER BY depth ASC, startNs ASC;";

const static std::string API_THREADS_BY_PID_AND_NO_PYTHON_FUNCTION =
    "select main.ROWID as id, startNs, endNs - startNs as duration, endNs, name, depth from PYTORCH_API "
    " main where globalTid = ? and endNs >= ? AND startNs <= ? and type != 50003 " // 50003 is the python function
    " ORDER BY depth ASC, startNs ASC;";

const static std::string API_THREADS_BY_PID_AND_PYTHON_FUNCTION =
    "select main.ROWID as id, startNs, endNs - startNs as duration, endNs, name, depth from PYTORCH_API "
    " main where globalTid = ? and endNs >= ? AND startNs <= ? and type = 50003 " // 50003 is the python function
    " ORDER BY depth ASC, startNs ASC;";

const static std::string OVERLAP_ANALYSIS_THREAD_BY_PID =
    "select ROWID as id, startNs, endNs - startNs as duration, endNs, 'OVERLAP_ANALYSIS'||type as name, "
    " 0 as depth from " +
    TABLE_OVERLAP_ANALYSIS +
    " where deviceId = ? and type = ? "
    " and endNs >= ? AND startNs <= ? ORDER BY startNs;";

const static std::string MS_TX_THREAD_BY_PID =
    "select ROWID as id, startNs, endNs - startNs as duration,endNs,message as name,depth from " + TABLE_MSTX_EVENTS +
    " where globalTid = ? and domainId = ? and endNs >= ? AND startNs <= ? ORDER BY startNs";

const static std::string OSRT_API_THREADS_BY_PID =
    "SELECT ROWID as id, startNs, endNs - startNs AS duration, endNs, name, 0 AS depth FROM " + TABLE_OSRT_API +
    " WHERE globalTid = ? AND endNs >= ? AND startNs <= ? ORDER BY startNs ASC;";

const static std::string CCU_THREADS_BY_PID =
    "SELECT ROWID as id, startNs as startNs, endNs - startNs AS duration, endNs as endNs, "
    "name, 0 AS depth FROM " +
    TABLE_CCU + " WHERE deviceId = ? AND endNs >= ? AND startNs <= ? ORDER BY startNs ASC;";

// QueryEventsViewData4Db
const static std::string QUERY_EVENTS_VIEW_FOR_DEVICE_HCCL_DEVICE_ID_NOT_UNIQUE =
    "with tmp as (select * from TASK main join COMMUNICATION_TASK_INFO "
    "info on info.globalTaskId = main.globalTaskId where main.deviceId = ?), "
    "sub as (select COMMUNICATION_OP.ROWID, startNs, endNs, endNs-startNs as duration, si.value as name,"
    "groupName from COMMUNICATION_OP LEFT JOIN STRING_IDS AS si ON si.id = opName "
    "where opId in (select opId from tmp group by opId)) "
    "select ROWID as id, name, startNs as start, duration, 0 as depth, 'HCCL' as processId, "
    "groupName||'group' as threadId, "
    "'Group '||((DENSE_RANK() OVER (ORDER BY groupName)) - 1)||' Communication' AS threadName "
    "from sub ";
const static std::string QUERY_EVENTS_VIEW_FOR_DEVICE_HCCL_DEVICE_ID_UNIQUE =
    "with sub as (select COMMUNICATION_OP.ROWID, startNs, endNs, endNs-startNs as duration, "
    "si.value as name, groupName from COMMUNICATION_OP "
    "LEFT JOIN STRING_IDS AS si ON si.id = opName) "
    "select ROWID as id, name, startNs as start, duration, 0 as depth, 'HCCL' as processId, "
    "groupName||'group' as threadId, "
    "'Group '||((DENSE_RANK() OVER (ORDER BY groupName)) - 1)||' Communication' AS threadName "
    "from sub ";
const static std::string QUERY_EVENTS_VIEW_FOR_GROUP_DEVICE_ID_NOT_UNIQUE =
    "with tmp as (select * from TASK main join COMMUNICATION_TASK_INFO "
    "info on info.globalTaskId = main.globalTaskId where main.deviceId = ?), "
    "sub as (select COMMUNICATION_OP.ROWID, startNs, endNs, endNs-startNs as duration, si.value as name,"
    "groupName from COMMUNICATION_OP LEFT JOIN STRING_IDS AS si ON si.id = opName "
    "where opId in (select opId from tmp group by opId)) "
    "select ROWID as id, name, startNs as start, duration, 0 as depth, 'HCCL' as processId, "
    "groupName||'group' as threadId, ? AS threadName from sub "
    "WHERE groupName||'group' = ? ";
const static std::string QUERY_EVENTS_VIEW_FOR_GROUP_DEVICE_ID_UNIQUE =
    "with sub as (select COMMUNICATION_OP.ROWID, startNs, endNs, endNs-startNs as duration, "
    "si.value as name, groupName "
    "from COMMUNICATION_OP LEFT JOIN STRING_IDS AS si ON si.id = opName) "
    "select ROWID as id, name, startNs as start, duration, 0 as depth, 'HCCL' as processId, "
    "groupName||'group' as threadId, ? AS threadName from sub "
    "WHERE groupName||'group' = ? ";
const static std::string QUERY_EVENTS_VIEW_FOR_OVERLAP =
    "select OVERLAP_ANALYSIS.ROWID as id, type AS name, startNs as start, "
    "endNs - startNs as duration, type AS threadName, "
    "0 as depth, 'OVERLAP_ANALYSIS' as processId, type as threadId "
    "from OVERLAP_ANALYSIS where deviceId = ? ";
const static std::string QUERY_EVENTS_VIEW_FOR_OVERLAP_SUB =
    "select OVERLAP_ANALYSIS.ROWID as id, type AS name, startNs as start, "
    "endNs - startNs as duration, type AS threadName, "
    "0 as depth, 'OVERLAP_ANALYSIS' as processId, type as threadId "
    "from OVERLAP_ANALYSIS where deviceId = ? AND type = ? ";

class DbSqlDefs {
  public:
    static std::string GetConnectionCatSql() {
        std::string sql = "CREATE TABLE IF NOT EXISTS connectionCats as "
                          " with operateConnIds as (select op.connectionId from COMMUNICATION_OP op "
                          "   UNION all select connectionId from TASK task "
                          " join COMPUTE_TASK_INFO CTI on task.globalTaskId = CTI.globalTaskId) ";
        sql.append(" select api.connectionId, 'HostToDevice' as cat  from CANN_API api " // cann侧
                   " join operateConnIds on api.connectionId = operateConnIds.connectionId ");
        sql.append(" union select api.connectionId, 'async_npu' as cat from CONNECTION_IDS api " // pytorch侧
                   " join operateConnIds on api.connectionId = operateConnIds.connectionId union "
                   " select conn.connectionId, case when ids.value like 'Enqueue%' then 'async_task_queue'"
                   " else 'fwdbwd' end as cat from CONNECTION_IDS conn join PYTORCH_API PA "
                   " on conn.id = PA.connectionId join STRING_IDS ids on ids.id = PA.name "
                   " group by conn.connectionId having count(1) > 1 ");
        sql.append(
            " union select api.connectionId, 'MsTx' as cat  from MSTX_EVENTS api " // 打点侧
            "      join Task task on api.connectionId = task.connectionId where api.connectionId != 4294967295 ");
        return sql;
    }

    static std::string GetQueryCANNApiLocationSql() {
        std::string sql = "with constValue as (select ? as minTime), "
                          "     rankIds as (select deviceId, globalPid from TASK group by globalPid) ";
        sql.append(" select connectionCats.cat, connectionCats.connectionId, api.ROWID as id, api.type as tid,"
                   " depth, startNs - constValue.minTime as startTime, endNs - startNs as duration, globalTid as pid, "
                   " 'CANN_API' as metaType, name, deviceId from " +
            TABLE_CANN_API +
            " api join constValue "
            " join connectionCats on api.connectionId = connectionCats.connectionId "
            " join rankIds on api.globalTid >> 32 = rankIds.globalPid "
            "  where cat = 'HostToDevice'");
        sql.append(" order by startTime");
        return sql;
    }

    static std::string GetQueryPyApiLocationSql() {
        std::string sql = "with constValue as (select ? as minTime), "
                          "     rankIds as (select deviceId, globalPid from TASK group by globalPid) ";
        sql.append(" select connectionCats.cat, connectionCats.connectionId, api.ROWID as id, 'pytorch' as tid,depth,"
                   " startNs - constValue.minTime as startTime, endNs - startNs as duration, globalTid as pid,"
                   " 'PYTORCH_API' as metaType, name, deviceId from PYTORCH_API api join constValue "
                   " join CONNECTION_IDS ids on api.connectionId = ids.id "
                   " join connectionCats on ids.connectionId = connectionCats.connectionId "
                   " join rankIds on api.globalTid >> 32 = rankIds.globalPid "
                   " where cat = 'async_task_queue' or cat = 'fwdbwd' or cat = 'async_npu' ");
        sql.append(" order by startTime");
        return sql;
    }

    static std::string GetQueryMstxApiLocationSql() {
        std::string sql = "with constValue as (select ? as minTime), "
                          "     rankIds as (select deviceId, globalPid from TASK group by globalPid) ";
        sql.append(" select connectionCats.cat, connectionCats.connectionId, api.ROWID as id, domainId as tid, "
                   "  depth, startNs - constValue.minTime as startTime, endNs - startNs as duration, globalTid as pid, "
                   "       'MSTX_EVENTS' as metaType, message as name, deviceId from MSTX_EVENTS api join constValue "
                   "        join connectionCats on api.connectionId = connectionCats.connectionId "
                   " join rankIds on api.globalTid >> 32 = rankIds.globalPid "
                   " where cat = 'MsTx'");
        sql.append(" order by startTime");
        return sql;
    }
};
};

#endif // PROFILER_SERVER_DBSQLDEFS_H
