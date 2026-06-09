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

#ifndef PROFILER_SERVER_TEXTSQLCONSTANT_H
#define PROFILER_SERVER_TEXTSQLCONSTANT_H
#include <string>
#include <utility>
#include "StringUtil.h"
#include "ServerLog.h"
#include "TimelineProtocolRequest.h"
#include "TableDefs.h"
// LCOV_EXCL_BR_START
// clang-format off
namespace Dic::Module::Timeline {
const int CACHE_SIZE = 1000;
const std::string SLICE_TABLE = "slice";
const std::string THREAD_TABLE = "thread";
const std::string PROCESS_TABLE = "process";
const std::string FLOW_TABLE = "flow";
const std::string COUNTER_TABLE = "counter";
const std::string TRACKID_TIME_INDEX = "track_id_timestamp_end_time_index";
const std::string TRACKID_CAT_INDEX = "track_id_cat_index";
const std::string FLOW_INDEX = "flow_id_time_index";
const std::string KERNEL_DETAIL = "kernel_detail";

const std::string UPDATE_PROCESS_NAME_SQL = "INSERT INTO  process  (pid, process_name) VALUES (?, ?) ON CONFLICT (pid) "
    "DO UPDATE SET process_name = excluded.process_name;";
const std::string SIMULATION_INSERT_PROCESS_NAME_SQL =
    "INSERT INTO  process  (pid, process_name) VALUES (?, ?) ON CONFLICT (pid) "
    "DO UPDATE SET process_name = CASE WHEN process_name IS NULL THEN excluded.process_name ELSE "
    "process_name END;";
const std::string UPDATE_PROCESS_LABLE_SQL =
    "INSERT INTO process (pid, label) VALUES (?, ?) ON CONFLICT (pid) DO UPDATE SET label = excluded.label;";
const std::string UPDATE_PROCESS_SORTINDEX_SQL = "INSERT INTO process (pid, process_sort_index) VALUES (?, ?) ON "
    "CONFLICT (pid) DO UPDATE SET process_sort_index = "
    "excluded.process_sort_index;";
const std::string UPDATE_THREAD_NAME_SQL = "INSERT INTO thread (track_id, tid, pid, thread_name) VALUES (?, ?, ?, "
    "?) ON CONFLICT (track_id) DO UPDATE SET tid "
    "= excluded.tid, pid = excluded.pid, thread_name = excluded.thread_name;";
const std::string UPDATE_THREAD_SORTINDEX_SQL = "INSERT INTO thread (track_id, thread_sort_index) VALUES (?, ?) ON "
    "CONFLICT (track_id) DO UPDATE SET thread_sort_index = "
    "excluded.thread_sort_index;";
const std::string SIMULATION_INSERT_THREAD_NAME_AND_SORT_INDEX_SQL = "INSERT INTO thread"
    " (track_id, tid, pid, thread_name, thread_sort_index) VALUES (?, ?, ?, ?, ?)"
    " ON CONFLICT (track_id) DO UPDATE SET tid = CASE WHEN tid IS NULL OR tid = '' THEN excluded.tid ELSE tid END,"
    " pid = CASE WHEN pid IS NULL OR pid = '' THEN excluded.pid ELSE pid END,"
    " thread_name = CASE WHEN thread_name IS NULL THEN excluded.thread_name ELSE thread_name END,"
    " thread_sort_index = CASE WHEN thread_sort_index IS NULL AND excluded.thread_sort_index != 0 THEN excluded.thread_sort_index "
    "ELSE thread_sort_index END;";
const std::string CREATE_TABLE_SQL = "CREATE TABLE " + SLICE_TABLE +
    " (id INTEGER PRIMARY KEY AUTOINCREMENT, timestamp INTEGER, duration INTEGER,"
    " name TEXT, depth INTEGER, track_id INTEGER, cat TEXT, args TEXT, cname TEXT, end_time INTEGER, flag_id TEXT,"
    " group_id TEXT);" +
    "CREATE TABLE " + THREAD_TABLE + " (track_id INTEGER PRIMARY KEY, tid TEXT, pid TEXT, thread_name TEXT," +
    " thread_sort_index INTEGER);" + "CREATE TABLE " + PROCESS_TABLE +
    " (pid TEXT PRIMARY KEY, process_name TEXT, label TEXT," + " process_sort_index INTEGER, parentPid TEXT DEFAULT '0');" + "CREATE TABLE " +
    FLOW_TABLE + " (id INTEGER PRIMARY KEY AUTOINCREMENT, flow_id TEXT, name TEXT, cat TEXT," +
    " track_id INTEGER, timestamp INTEGER, type TEXT);" + "CREATE TABLE " + COUNTER_TABLE +
    " (id INTEGER PRIMARY KEY AUTOINCREMENT, name TEXT, pid TEXT," + "timestamp INTEGER, cat TEXT, args TEXT);";

const std::string CREATE_INDEX_SQL = "CREATE INDEX IF NOT EXISTS " + TRACKID_TIME_INDEX + " ON " + SLICE_TABLE +
    " (timestamp, end_time, track_id);" + "CREATE INDEX IF NOT EXISTS " + TRACKID_CAT_INDEX + " ON " + SLICE_TABLE +
    " (track_id, cat);" + "CREATE INDEX IF NOT EXISTS " + FLOW_INDEX + " ON " + FLOW_TABLE + " (cat, type);";
const std::string QUERY_FLOW_BY_FLOWID_SQL = "SELECT name, cat, flow_id as flowId, timestamp, type, track_id as trackId"
    " FROM " + FLOW_TABLE + " WHERE flow_id = ?";
const std::string QUERY_ALL_THREAD_SQL = "SELECT track_id as trackId, tid, pid FROM " + THREAD_TABLE + " ;";
const std::string QUERY_SLICE_BY_ID_SQL = "SELECT track_id, flag_id FROM " + SLICE_TABLE + " WHERE id = ?";
const std::string QUERY_SLICE_BY_FLAG_ID_SQL = "SELECT id"
    " FROM " + SLICE_TABLE + " WHERE flag_id = ? AND track_id = ?;";
const std::string QUERY_EXETREME_TIME_SQL = "SELECT  min(minTimestamp) AS totalMinTimestamp, max(maxTimestamp) AS "
    "totalMaxTimestamp FROM ("
    "    SELECT min(timestamp) as minTimestamp, max(end_time) as maxTimestamp FROM " + SLICE_TABLE +
    "    UNION SELECT min(timestamp) as minTimestamp, max(timestamp) as maxTimestamp FROM " + COUNTER_TABLE +
    ")";
const std::string QUERY_UNIT_COUNTER_SQL = "SELECT timestamp - ? as startTime, args FROM " + COUNTER_TABLE + " "
    "WHERE pid = ? AND name = ? AND startTime >= ? AND startTime <= ? ORDER BY timestamp ASC";
const std::string QUERY_QUERY_TYPE_SQL =
    "SELECT DISTINCT accelerator_core FROM " + KERNEL_DETAIL + " ORDER BY accelerator_core";

const std::string QUERY_COMMUNICATION_GROUP_MAP_TEXT_SQL =
    "SELECT pid as groupName, tid as planeId, thread_name as threadName FROM " + THREAD_TABLE + " "
    "WHERE track_id in ( "
    "    SELECT track_id FROM " + THREAD_TABLE + " thread WHERE pid in ( "
    "        SELECT pid FROM " + PROCESS_TABLE + " process "
    "WHERE (process.pid & 0x1f) = ? AND process_name in ('HCCL', 'COMMUNICATION', 'Communication') "
    "    ) "
    ") ORDER BY thread_sort_index ASC";

const std::string QUERY_COMMUNICATION_GROUP_ID_TEXT_SQL =
    "SELECT track_id as groupId, thread_name as groupName "
    "FROM " + THREAD_TABLE + " WHERE pid in (SELECT pid FROM " + PROCESS_TABLE +
    " WHERE (pid & 0x1f) = ? AND process_name in ('HCCL', 'COMMUNICATION', 'Communication'))";

const std::string QUERY_BYTE_ALIGNMENT_ANALYZER_DATA_SQL = "SELECT name, args FROM " + SLICE_TABLE +
    " WHERE SUBSTR(name, 1, 4) = 'hcom' OR SUBSTR(name, 1, 6) = 'Memcpy' OR SUBSTR(name, 1, 6) = 'Reduce'";

class TextSqlConstant {
public:
    static std::string GetInsertSliceSql()
    {
        std::string sql = "INSERT INTO slice "
            " (timestamp, duration, name, track_id, cat, args, cname, end_time, flag_id, group_id) VALUES"
            " (?,?,?,?,?,?,?,?,?,?)";
        for (int i = 0; i < CACHE_SIZE - 1; ++i) {
            sql.append(",(?,?,?,?,?,?,?,?,?,?)");
        }
        return sql;
    }
    static std::string GetInsertFlowSql()
    {
        std::string sql = "INSERT INTO flow (flow_id, name, track_id, timestamp, cat, type)  VALUES (?,?,?,?,?,?)";
        for (int i = 0; i < CACHE_SIZE - 1; ++i) {
            sql.append(",(?,?,?,?,?,?)");
        }
        return sql;
    }
    static std::string GetInsertCounterSql()
    {
        std::string sql = "INSERT INTO  counter  (name, pid, timestamp, cat, args) VALUES (?,?,?,?,?)";
        for (int i = 0; i < CACHE_SIZE - 1; ++i) {
            sql.append(",(?,?,?,?,?)");
        }
        return sql;
    }

    static std::string GetSummarySliceSql(uint64_t size)
    {
        std::string sql = "SELECT timestamp , end_time FROM " + SLICE_TABLE + " WHERE track_id in ( ? ";
        for (uint64_t i = 0; i < size; ++i) {
            sql += ", ? ";
        }
        sql += ") ORDER BY timestamp";
        return sql;
    }
    static std::string GetSearchSliceNameCountSql(bool isMatchExact, bool isMatchCase)
    {
        std::string nameMatch = GetSearchNameSqlSuffix(isMatchExact, isMatchCase);
        return "SELECT count(*) as count FROM " + SLICE_TABLE + " WHERE " + nameMatch;
    }
    static std::string GetSearchSliceNameSql(bool isMatchExact, bool isMatchCase)
    {
        std::string nameMatch = GetSearchNameSqlSuffix(isMatchExact, isMatchCase);
        return "SELECT id, pid, tid, timestamp - ? as startTime, duration, track_id AS trackId"
            " FROM " + SLICE_TABLE + " JOIN " + THREAD_TABLE + " USING (track_id) WHERE " + nameMatch +
            " ORDER BY timestamp ASC, track_id ASC, id ASC LIMIT 1 OFFSET ?";
    }
    static std::string GetComputeStatisticsSQL(const std::string &stepId)
    {
        std::string stepCondition;
        if (!stepId.empty() && stepId != "ALL") {
            stepCondition.append(" and step_id =? ");
        }
        return "SELECT sum(duration) as duration,accelerator_core as acceleratorCore FROM kernel_detail"
            " WHERE accelerator_core in ('AI_CPU','AI_CORE', 'AI_VECTOR_CORE', 'MIX_AIC', 'MIX_AIV', 'FFTS_PLUS') " +
            stepCondition + " GROUP BY accelerator_core";
    }
    static std::string GetCommunicationStatisticsSql(const std::string &stepId)
    {
        std::string timestampCondition;
        if (!stepId.empty()) {
            timestampCondition = " and timestamp >= ? and timestamp <= ? ";
        }
        return "select duration / 1000, t.thread_name as overlapType from (select sum(duration) as duration,"
            " track_id from " + SLICE_TABLE + " where track_id in (select track_id from thread where thread_name "
            " in ('Communication(Not Overlapped)', 'Communication')) " +
            timestampCondition + " group by track_id) s left join thread t on s.track_id=t.track_id";
    }
    static std::string GetQueryLayerDataSql(std::vector<std::string> layers,  const std::string &timeCondSql)
    {
        const auto layerStr = StringUtil::Join4SqlGroup(std::move(layers));
        if (layerStr.find("python") != std::string::npos || layerStr.find("cann") != std::string::npos) {
            return "SELECT sum(case when name != 'Communication' then duration else 0 end) "
                "AS totalTime, count(distinct name) FROM slice "
                "WHERE lower(name) LIKE lower(?) and slice.track_id IN "
                "( SELECT track_id FROM process JOIN thread t ON process.pid = t.pid "
                " WHERE lower(process_name) in ("+ layerStr +") ) " + timeCondSql;
        }
        return "SELECT sum(case when name != 'Communication' then duration else 0 end) "
                "AS totalTime, count(distinct name) FROM slice "
                "WHERE lower(name) LIKE lower(?) and slice.track_id IN "
                "( SELECT track_id FROM process JOIN thread t ON process.pid = t.pid "
                " WHERE (process.pid & 0x1f) = ? AND lower(process_name) in ("+ layerStr +") ) " + timeCondSql;
    }
    static std::string GetQueryPythonViewDataSql(const std::string &order, const std::string &orderByField,
        std::vector<std::string> layers, const std::string &timeCondSql)
    {
        std::string orderBy;
        if (order == "descend") {
            orderBy = " ORDER BY " + orderByField + " DESC";
        } else {
            orderBy = " ORDER BY " + orderByField + " ASC";
        }
        const auto layerStr = StringUtil::Join4SqlGroup(std::move(layers));
        if (layerStr.find("python") != std::string::npos || layerStr.find("cann") != std::string::npos) {
            return "SELECT name, ROUND(cast(sum(duration) as double) * 100 / ?, 2) as "
                "time, sum(duration) / 1000.0 as totalTime, count(1) as numberCalls, "
                "ROUND(avg(duration) / 1000.0, 4) as avg, "
                "min(duration) / 1000.0 as min, max(duration) / 1000.0 as max "
                "FROM slice WHERE lower(name) LIKE lower(?) AND slice.track_id IN ( SELECT track_id "
                "FROM process JOIN thread t ON process.pid = t.pid "
                "WHERE lower(process_name) in (" + layerStr + ")) " + timeCondSql +
                "GROUP BY name " + orderBy + " limit ? offset ?";
        }
        return "SELECT name, ROUND(cast(sum(duration) as double) * 100 / ?, 2) as "
            "time, sum(duration) / 1000.0 as totalTime, count(1) as numberCalls, "
            "ROUND(avg(duration) / 1000.0, 4) as avg, "
            "min(duration) / 1000.0 as min, max(duration) / 1000.0 as max "
            "FROM slice WHERE lower(name) LIKE lower(?) AND slice.track_id IN ( SELECT track_id "
            "FROM process JOIN thread t ON process.pid = t.pid "
            "WHERE (process.pid & 0x1f) = ? AND lower(process_name) in (" + layerStr + ")) " + timeCondSql +
            "GROUP BY name " + orderBy + " limit ? offset ?";
    }

    static std::string GetQueryPythonViewTraceDataSql(const std::string &order, const std::string &orderByField,
        std::vector<std::string> layers, const std::string &timeCondSql)
    {
        std::string orderBy;
        if (order == "descend") {
            orderBy = " ORDER BY " + orderByField + " DESC";
        } else {
            orderBy = " ORDER BY " + orderByField + " ASC";
        }
        const auto layerStr = StringUtil::Join4SqlGroup(std::move(layers));
        if (layerStr.find("python") != std::string::npos || layerStr.find("cann") != std::string::npos) {
            return "SELECT name, timestamp - ? as startTime, duration / 1000.0 as duration, "
                "count(*) over() as total "
                "FROM slice WHERE lower(name) LIKE lower(?) AND slice.track_id IN ( SELECT track_id "
                "FROM process JOIN thread t ON process.pid = t.pid "
                "WHERE lower(process_name) in (" + layerStr + ")) " + timeCondSql +
                orderBy + " limit ? offset ?";
        }
        return "SELECT name, timestamp - ? as startTime, duration / 1000.0 as duration, "
            "count(*) over() as total "
            "FROM slice WHERE lower(name) LIKE lower(?) AND slice.track_id IN ( SELECT track_id "
            "FROM process JOIN thread t ON process.pid = t.pid "
            "WHERE (process.pid & 0x1f) = ? AND lower(process_name) in (" + layerStr + ")) " + timeCondSql +
            orderBy + " limit ? offset ?";
    }

    static std::string GetAICoreViewDataSql()
    {
        return "SELECT timestamp, args, process.pid as pid, thread.tid as tid "
            "FROM counter JOIN process ON counter.pid = process.pid JOIN thread ON process.pid = thread.pid "
            "WHERE (process.pid & 0x1f) = ? AND counter.name LIKE '%AI Core Freq%' ORDER BY timestamp ASC ";
    }

    static std::string GetOverlapAnalysisTextSqlByType(const SystemViewOverallReqParam &params)
    {
        std::string timeCondSql;
        if (params.startTime != params.endTime) { // time range analysis
            timeCondSql += " AND end_time >= ? AND timestamp <= ? ";
        }
        return "SELECT name, timestamp - ? as startNs, end_time - ? as endNs, duration FROM " + SLICE_TABLE + " "
            "WHERE track_id in (SELECT track_id FROM " + THREAD_TABLE + " t "
            "JOIN " + PROCESS_TABLE + " p ON t.pid = p.pid "
            "WHERE (p.pid & 0x1f) = ? AND t.thread_name = ?) " + timeCondSql + " ORDER BY timestamp ASC";
    }

    static std::string GetCommunicationOpTextSqlByGroupId(const SystemViewOverallReqParam &params)
    {
        std::string timeCondSql;
        if (params.startTime != params.endTime) { // time range analysis
            timeCondSql += " AND s.end_time >= ? AND s.timestamp <= ? ";
        }
        std::string nameCondSql;
        if (!params.name.empty()) {
            nameCondSql += " AND s.name LIKE ? ";
        }
        return "SELECT id, name, timestamp - ? as startNs, duration, end_time - ? as endNs FROM " + SLICE_TABLE + " s"
            " JOIN " + THREAD_TABLE + " t ON s.track_id = t.track_id"
            " JOIN " + PROCESS_TABLE + " p ON p.pid = t.pid"
            " WHERE (p.pid & 0x1f) = ? AND s.track_id = ? " + nameCondSql + timeCondSql + " ORDER by s.timestamp ASC";
    }

    static std::string GetKernelDetailSql(const Protocol::KernelDetailsParams &requestParams)
    {
        std::string orderBy = " ORDER BY " + requestParams.orderBy + (requestParams.order == "descend" ? " DESC" : " ASC");
        std::string coreTypes;
        if (!requestParams.coreType.empty()) {
            coreTypes = " AND accelerator_core = ? ";
        }
        std::string sql = "SELECT id, task_id as taskId, name, op_type AS type, accelerator_core AS acceleratorCore, "
            "duration, start_time AS startTime, wait_time AS waitTime, block_num AS blockNum, "
            "input_shapes AS inputShapes, input_data_types AS inputDataTypes, input_formats AS inputFormats, "
            "output_shapes AS outputShapes, output_data_types AS outputDataTypes, "
            "output_formats AS outputFormats FROM kernel_detail WHERE deviceId = ? ";
        if (requestParams.startTime != requestParams.endTime) {
            sql += " AND (start_time + duration*1000) >= ? AND start_time <= ? ";
        }
        for (const auto &filter : requestParams.filters) {
            if (!StringUtil::CheckSqlValid(filter.first)) {
                Server::ServerLog::Error("There is an SQL injection attack on this parameter. param: filter");
                sql.clear();
                return sql;
            }
            sql += " AND lower(" + filter.first + ") LIKE lower(?) ";
        }
        sql += coreTypes + orderBy + " limit ? offset ?";
        return sql;
    }
    static std::string GetThreadSameOperatorsDetailsSql(const std::string &order, const std::string &orderByField,
                                                        const std::vector<uint64_t> &trackIdList)
    {
        std::string orderBy = " ORDER BY " + orderByField + (order == "descend" ? " DESC" : " ASC");
        std::string trackIdPlaceholders = StringUtil::join(trackIdList, ", ");
        std::string sql = "SELECT timestamp, duration, id, coalesce(depth, 0) as depth, track_id FROM " + SLICE_TABLE +
            " WHERE name = ? AND track_id in (" + trackIdPlaceholders + ") AND timestamp <= ? AND timestamp "
            " + duration >= ? " + orderBy;
        return sql;
    }

    static std::string GetSearchNameSqlSuffix(bool isMatchExact, bool isMatchCase)
    {
        std::string nameMatch;
        if (isMatchExact && isMatchCase) {
            nameMatch = "name like ?";
        } else if (isMatchExact) {
            nameMatch = "lower(name) like lower(?)";
        } else if (isMatchCase) {
            nameMatch = "name like '%'||?||'%'";
        } else {
            nameMatch = "lower(name) like lower('%'||?||'%')";
        }
        return nameMatch;
    }

    static std::string GetSearchSliceDetailSql(bool isMatchExact, bool isMatchCase, const std::string& order,
        const std::string& orderByField)
    {
        std::string orderBy = " ORDER BY " + orderByField + (order == "descend" ? " DESC" : " ASC");
        std::string nameMatch = GetSearchNameSqlSuffix(isMatchExact, isMatchCase);
        std::string sql = "SELECT s.name as name, s.timestamp as timestamp, s.duration as duration,"
            " s.id as id, t.tid as tid, t.pid as pid FROM " + SLICE_TABLE + " s JOIN " + THREAD_TABLE +
            " t on s.track_id = t.track_id WHERE " + nameMatch + orderBy + " limit ? offset ?";
        return sql;
    }

    static std::string GetSearchSliceDetailWithFilterSql(bool isMatchExact, bool isMatchCase, const std::string& order,
        const std::string& orderByField)
    {
        std::string orderBy = " ORDER BY " + orderByField + (order == "descend" ? " DESC" : " ASC");
        std::string nameMatch = GetSearchNameSqlSuffix(isMatchExact, isMatchCase);
        // 二级筛选：nameFilter 模糊匹配
        std::string nameFilterMatch = "lower(s.name) LIKE lower('%'||?||'%')";
        std::string sql = "SELECT s.name as name, s.timestamp as timestamp, s.duration as duration,"
            " s.id as id, t.tid as tid, t.pid as pid FROM " + SLICE_TABLE + " s JOIN " + THREAD_TABLE +
            " t on s.track_id = t.track_id WHERE " + nameMatch + " AND " + nameFilterMatch + orderBy + " limit ? offset ?";
        return sql;
    }

    static std::string GetSearchSliceCountWithFilterSql(bool isMatchExact, bool isMatchCase)
    {
        std::string nameMatch;
        if (isMatchExact && isMatchCase) {
            nameMatch = "s.name like ?";
        } else if (isMatchExact) {
            nameMatch = "lower(s.name) like lower(?)";
        } else if (isMatchCase) {
            nameMatch = "s.name like '%'||?||'%'";
        } else {
            nameMatch = "lower(s.name) like lower('%'||?||'%')";
        }
        std::string nameFilterMatch = "lower(s.name) LIKE lower('%'||?||'%')";
        std::string sql = "SELECT count(1) as count FROM " + SLICE_TABLE + " s WHERE " + nameMatch + " AND " + nameFilterMatch;
        return sql;
    }

    // 款选场景下带二级筛选的SQL
    static std::string GetSearchSliceDetailWithLockRangeAndFilterSql(bool isMatchExact, bool isMatchCase,
        const std::string& order, const std::string& orderByField)
    {
        std::string orderBy = " ORDER BY " + orderByField + (order == "descend" ? " DESC" : " ASC");
        std::string nameMatch;
        if (isMatchExact && isMatchCase) {
            nameMatch = "s.name like ?";
        } else if (isMatchExact) {
            nameMatch = "lower(s.name) like lower(?)";
        } else if (isMatchCase) {
            nameMatch = "s.name like '%'||?||'%'";
        } else {
            nameMatch = "lower(s.name) like lower('%'||?||'%')";
        }
        // 二级筛选：nameFilter 模糊匹配
        std::string nameFilterMatch = "lower(s.name) LIKE lower('%'||?||'%')";
        std::string sql = "SELECT s.name as name, s.timestamp as timestamp, s.duration as duration,"
            " s.id as id, t.tid as tid, t.pid as pid FROM " + SLICE_TABLE + " s JOIN " + THREAD_TABLE +
            " t on s.track_id = t.track_id WHERE " + nameMatch + " AND " + nameFilterMatch +
            " AND s.track_id = ? AND s.timestamp >= ? AND s.end_time <= ?" + orderBy + " limit ? offset ?";
        return sql;
    }

    // 款选场景下带二级筛选的计数SQL
    static std::string GetSearchSliceCountWithLockRangeAndFilterSql(bool isMatchExact, bool isMatchCase)
    {
        std::string nameMatch;
        if (isMatchExact && isMatchCase) {
            nameMatch = "s.name like ?";
        } else if (isMatchExact) {
            nameMatch = "lower(s.name) like lower(?)";
        } else if (isMatchCase) {
            nameMatch = "s.name like '%'||?||'%'";
        } else {
            nameMatch = "lower(s.name) like lower('%'||?||'%')";
        }
        std::string nameFilterMatch = "lower(s.name) LIKE lower('%'||?||'%')";
        std::string sql = "SELECT count(1) as count FROM " + SLICE_TABLE + " s WHERE " + nameMatch + " AND " + nameFilterMatch +
            " AND s.track_id = ? AND s.timestamp >= ? AND s.end_time <= ?";
        return sql;
    }

    static std::string GenerateAffinityApiTextSql(const Protocol::KernelDetailsParams &params)
    {
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND s.end_time >= ? AND s.timestamp <= ? ";
        }
        return "SELECT s.track_id as track, s.id as id, s.name as name, s.timestamp - ? as startTime, "
            "s.end_time - ? as endTime, t.pid as pid, t.tid as tid "
            "FROM " + SLICE_TABLE + " s JOIN " + THREAD_TABLE + " t on s.track_id = t.track_id "
            "WHERE s.cat = 'cpu_op' AND (s.name LIKE 'aten::%' OR s.name LIKE 'npu::%') " + timeCondSql +
            "ORDER BY s.track_id ASC, s.timestamp ASC";
    }

    static std::string GenerateAclnnQueryTextSql(const Protocol::KernelDetailsParams &params)
    {
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND s.end_time >= ? AND s.timestamp <= ? ";
        }
        return "SELECT s.id as id, s.name as name, s.timestamp - ? as startTime, s.duration as duration, "
            "t.pid as pid, t.tid as tid, t.track_id as track_id "
            "FROM " + SLICE_TABLE + " s JOIN " + THREAD_TABLE + " t on s.track_id = t.track_id "
            "JOIN " + PROCESS_TABLE + " p ON p.pid = t.pid "
            "WHERE (p.pid & 0x1f) = ? AND t.thread_name LIKE 'Stream%' " + timeCondSql + " AND s.name IN ( "
            "    SELECT name FROM " + SLICE_TABLE + "    WHERE name LIKE 'aclnn%' "
            "    GROUP BY name HAVING COUNT(name) >= ? "
            ") ORDER BY " + params.orderBy + " " + params.order;
    }

    static std::string GenerateOperatorDispatchQueryTextSql(const Protocol::KernelDetailsParams &params)
    {
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND s.end_time >= ? AND s.timestamp <= ? ";
        }
        return "SELECT s.id AS id, s.name AS name, s.timestamp - ? AS startTime, s.duration AS duration, "
            "  t.pid AS pid, t.tid AS tid, t.track_id AS track_id "
            "FROM " + SLICE_TABLE + " s "
            "  JOIN " + THREAD_TABLE + " t ON s.track_id = t.track_id "
            "WHERE "
            "  t.thread_name LIKE 'Thread%' "
            "  AND s.name LIKE '%aclopCompileAndExecute' " + timeCondSql +
            "ORDER BY " + params.orderBy + " " + params.order;
    }

    static std::string GenerateAICpuQueryTextSql(const std::vector<std::string> &replace,
        const Protocol::KernelDetailsParams &params,
        const std::map<std::string, Timeline::AICpuCheckDataType> &dataTypeMap)
    {
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
            dataTypeCheck.emplace_back(GenerateAICpuOpFilterSql(opType, item.second));
        }
        std::string dataTypeCheckSql = StringUtil::join(dataTypeCheck, "OR");
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND (kd.start_time + kd.duration * 1000) >= ? AND kd.start_time <= ? ";
        }
        std::string sql =
            "SELECT kd.name as name, kd.op_type as type, kd.start_time - ? as startTime, (kd.duration * 1000) as duration, "
            "t.pid as pid, t.tid as tid, t.track_id as track_id, s.id as id, "
            "lower(kd.input_data_types) as input,  lower(kd.output_data_types) as output "
            "FROM " + KERNEL_DETAIL + " kd "
            "JOIN " + SLICE_TABLE +" s ON kd.name = s.name AND kd.start_time = s.timestamp "
            "JOIN " + THREAD_TABLE + " t ON s.track_id = t.track_id "
            "WHERE kd.deviceId = ? AND kd.accelerator_core='AI_CPU' " + timeCondSql + " AND ("
            "    lower(kd.op_type) IN (" +
            StringUtil::Join4SqlGroup(replace) +
            ") " // 特定类型的算子可以修改代码
            "    OR ("
            "    " +
            dataTypeCheckSql + // 检查数据类型是否符合要求
            "    ) OR "
            "    kd.duration >= ?" // 执行时间超过20us
            ") ORDER BY " +
            params.orderBy + " " + params.order;
        return sql;
    }

    static std::string QueryAffinityOptimizerTextSql(const std::string &optimizers, const Protocol::KernelDetailsParams &params)
    {
        std::string timeCondSql;
        if (params.startTime != params.endTime) { // time range analysis
            timeCondSql += " AND s.end_time >= ? AND s.timestamp <= ? ";
        }
        return "Select (s.timestamp - ?) as startTime, s.duration as duration, s.name as name, "
            "t.pid as pid, t.tid as tid, s.id as id, t.track_id as track_id "
            "From " + SLICE_TABLE + " s Join " + THREAD_TABLE + " t ON s.track_id = t.track_id "
            "WHERE s.name IN ( " + optimizers + ") " + timeCondSql + " order by " + params.orderBy + " " + params.order;
    }

    static std::string GeneratorCommunicationSummarySql4Text(const SystemViewOverallReqParam &params)
    {
        std::string timeCondSql;
        if (params.startTime != params.endTime) { // time range analysis
            timeCondSql += " WHERE d1.end_time >= ? AND d1.start_time <= ? ";
        }
        std::string sql =
            "WITH data as ("
            "    SELECT s.name, s.timestamp - ? as start_time, s.duration, s.end_time - ? as end_time, t.pid, t.tid, s.track_id, "
            "    t.thread_name, CASE WHEN t.thread_name like 'Group%' THEN 1 ELSE 0 END as type, "
            "    row_number() OVER (ORDER by s.track_id ASC, s.timestamp ASC) as row_num FROM " + SLICE_TABLE + " s "
            "    JOIN " + THREAD_TABLE + " t ON s.track_id = t.track_id WHERE s.track_id in ( "
            "        SELECT track_id FROM " + THREAD_TABLE + " WHERE pid in ( "
            "            SELECT pid FROM " + PROCESS_TABLE +
            " WHERE (pid & 0x1f) = ? AND process_name in ('HCCL', 'COMMUNICATION', 'Communication') "
                    "        ) "
            "    ) "
            ") "
            "SELECT d1.name as name, d1.start_time as startTime, d1.duration as duration, d1.end_time as endTime, "
            "d1.pid as groupName, d1.tid as plane, d1.thread_name as threadName, d1.type as type, "
            "CASE "
            "    WHEN d1.name = 'Notify_Wait' THEN "
            "        CASE "
            "            WHEN d2.name = 'RDMASend' AND d3.name = 'RDMASend' OR "
            "                (d3.name = 'Notify_Wait' AND d4.name = 'RDMASend' AND d5.name = 'RDMASend') THEN '0' "
            "            ELSE '1' "
            "        END "
            "    ELSE '0' "
            "END as flag "
            "FROM data d1 "
            "LEFT JOIN data d2 ON d2.row_num = d1.row_num - 1 AND d2.track_id = d1.track_id "
            "LEFT JOIN data d3 ON d3.row_num = d1.row_num - 2 AND d3.track_id = d1.track_id "
            "LEFT JOIN data d4 ON d4.row_num = d1.row_num - 3 AND d4.track_id = d1.track_id "
            "LEFT JOIN data d5 ON d5.row_num = d1.row_num - 4 AND d5.track_id = d1.track_id "
            + timeCondSql + "ORDER BY d1.pid, d1.tid, d1.start_time";
        return sql;
    }

    static std::string& AppendOverlapFilterSql(const EventsViewParams& params, std::string& sql)
    {
        std::vector<std::string> types;
        for (const auto& filter : params.filters) {
            if (StringUtil::ContainsIgnoreCase("Computing", filter.second)) {
                types.emplace_back("0");
            }
            if (StringUtil::ContainsIgnoreCase("Communication", filter.second)) {
                types.emplace_back("1");
            }
            if (StringUtil::ContainsIgnoreCase("Communication(Not Overlapped)", filter.second)) {
                types.emplace_back("2");
            }
            if (StringUtil::ContainsIgnoreCase("Free", filter.second)) {
                types.emplace_back("3");
            }
        }
        if (!types.empty()) {
            std::string filterSql = StringUtil::join(types, ",");
            sql += " AND name IN (" + filterSql + ") ";
        }
        if (types.empty() && !params.filters.empty()) {
            sql += " AND name IN () ";
        }
        return sql;
    }

    static std::string AppendTextTimeRangeConditionSql(const uint64_t &startTime, const uint64_t &endTime)
    {
        if (startTime == endTime) { // default request, not time range analysis
            return "";
        }
        return " AND end_time >= ? AND timestamp <= ? ";
    }

private:
    static std::string GenerateAICpuOpFilterSql(const std::string &opType, const Timeline::AICpuCheckDataType &dataType)
    {
        std::string sql = " ( ";
        if (std::find(opType.begin(), opType.end(), ',') == opType.end()) { // 输入为单个算子类型
            sql += "lower(kd.op_type) = '" + opType + "' AND ";
        } else { // 输入为算子类型组
            sql += "lower(kd.op_type) NOT IN ( " + opType + " ) AND ";
        }
        sql += "lower(kd.input_data_types) NOT IN ( " + StringUtil::Join4SqlGroup(dataType.input) + " ) AND "
            "lower(kd.output_data_types) NOT IN ( " + StringUtil::Join4SqlGroup(dataType.output) + " )) ";
        return sql;
    }
};
}
// clang-format on
// LCOV_EXCL_BR_STOP

#endif // PROFILER_SERVER_TEXTSQLCONSTANT_H
