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

#include "DbTraceDataBase.h"
#include "TraceDatabaseHelper.h"

// clang-format off
namespace Dic::Module::FullDb {
using namespace Server;

std::string DbTraceDataBase::GetSearchSliceNameSql(bool isMatchExact, bool isMatchCase, const std::string& rankId,
    const std::string &path)
{
    const std::string order = "ascend";
    const std::string orderByField = "timestamp";
    std::string sql;
    std::string nameMatch = " select id from STRING_IDS where ";
    std::string orderKey = orderByField == "timestamp" ? "startTime" : orderByField;
    std::string orderBy = " ORDER BY " + orderKey + (order == "ascend" ? " ASC" : "DESC");
    nameMatch.append(isMatchCase ? " value like " : "lower(value) like lower(");
    nameMatch.append(isMatchExact ? "?" : "'%'||?||'%'");
    nameMatch.append(isMatchCase ? " " : ")");
    std::string associationTaskSql;
    if (!TraceDatabaseHelper::IsDeviceIdUnique(path)) {
        associationTaskSql = "join tasks on op.connectionId = tasks.connectionId";
    }
    const std::string hostSql =
        " SELECT name, globalTid as pid, metaType,  type as tid, startNs - minTime.value as startTime,endNs "
        "- startNs as duration, depth, api.id "
        " FROM (select globalTid, type, startNs, endNs, depth, cann.ROWID as id, name, 'CANN_API' as metaType from "
        + TABLE_CANN_API + " cann join ids on ids.id = cann.name "
        " Union all select globalTid, domainId as type, startNs, endNs, depth, mstx.ROWID as id, message as name, "
        " 'MSTX_EVENTS' as metaType from " + TABLE_MSTX_EVENTS + " mstx join ids on ids.id = mstx.message "
        " UNION all select globalTid, 'pytorch' as type, startNs, endNs, depth, python.ROWID as id, name, "
        " 'PYTORCH_API' as metaType from " + TABLE_API + " python join ids on ids.id = python.name" +
        " UNION ALL SELECT globalTid, 'OSRT_API' AS type, startNs, endNs, 0 AS depth, osrt.ROWID AS id, name,"
        " 'OSRT_API' as metaType FROM " + TABLE_OSRT_API + " osrt JOIN ids ON ids.id = osrt.name) api join minTime ";
    std::string comSql = "select opName as name,'HCCL' as pid, 'HCCL' as metaType, groupName||'group' as tid,"
                         " startNs - minTime.value as startTime, endNs - startNs as duration, 0 as depth, op.ROWID"
                         " as id from COMMUNICATION_OP op join minTime " +
                         associationTaskSql + " join ids on ids.id = opName group by opId";
    sql = "with ids as (" + nameMatch +
          "), minTime as (select ? as value), "
          " tasks as (select ROWID, globalTaskId, taskType, 'Ascend Hardware' as pid, streamId as tid, connectionId, "
          " startNs - minTime.value as startTime, endNs - startNs as duration,depth from TASK join minTime "
          " where deviceId = ? ORDER BY startTime), "
          " com as (select opId, tasks.ROWID as id, 'HCCL' as pid, groupName || '_' || planeId as tid, "
          " startTime, duration, 0 as depth, info.taskType as name from COMMUNICATION_TASK_INFO info "
          " join tasks on info.globalTaskId=tasks.globalTaskId ORDER BY startTime) "
          " select * from ( select coalesce(compute.name, schedule.name, main.taskType) as name, main.pid, main.pid "
          " as metaType, main.tid, main.startTime, main.duration, main.depth, main.ROWID as id from tasks main "
          " left join COMPUTE_TASK_INFO compute on compute.globalTaskId = main.globalTaskId "
          " left join COMMUNICATION_SCHEDULE_TASK_INFO schedule ON main.globalTaskId = schedule.globalTaskId "
          " join ids on ids.id = coalesce(compute.name, schedule.name, main.taskType) "
          " union ALL select name, pid, pid as meatType, tid, startTime, duration, depth, com.id from com "
          " join ids on ids.id = com.name  union ALL " +
          comSql + " union ALL " + hostSql + ") allNames " + orderBy + " LIMIT 1 OFFSET ?";
    return sql;
}

std::string DbTraceDataBase::GetSearchAllSlicesDetailsSql(const SearchSliceSqlParams &params)
{
    std::string orderKey = params.orderByField == "timestamp" ? "startTime" : params.orderByField;
    std::string orderBy = " ORDER BY " + orderKey + (params.order == "descend" ? " DESC" : " ASC");

    std::string nameMatch;
    if (params.isMatchExact && params.isMatchCase) {
        nameMatch = "select id, value from STRING_IDS where value like ?";
    } else if (params.isMatchExact) {
        nameMatch = "select id, value from STRING_IDS where lower(value) like lower(?)";
    } else if (params.isMatchCase) {
        nameMatch = "select id, value from STRING_IDS where value like '%'||?||'%'";
    } else {
        nameMatch = "select id, value from STRING_IDS where lower(value) like lower('%'||?||'%')";
    }

    std::string communicationOpSql = TraceDatabaseHelper::GetComOpSliceDetailsSql(params.rankId);
    std::string mstxEventsSql = TraceDatabaseHelper::GetMsTxEventsSliceDetailSql();

    std::string filterCte;
    std::string filterJoin;
    if (!params.nameFilter.empty()) {
        filterCte = ", filterIds as (select id from STRING_IDS where lower(value) like lower('%'||?||'%'))";
        filterJoin = " join filterIds on filterIds.id = allNames.name";
    }

    std::string sql = "with ids as (" + nameMatch + ")" + filterCte +
          ", minTime as (select ? as value),\n"
          " tasks as (select deviceId, TASK.ROWID, globalTaskId, taskType, 'Ascend Hardware' as pid, streamId as tid, "
          " startNs - minTime.value as startTime,endNs - startNs as duration,depth,connectionId from TASK join minTime "
          " where deviceId = ? ORDER BY startTime),\n"
          " com as (select deviceId, opId, tasks.ROWID as id, 'HCCL' as pid, groupName || '_' || planeId as tid,"
          " startTime, duration, 0 as depth, info.taskType as name"
          " from COMMUNICATION_TASK_INFO info join tasks on info.globalTaskId=tasks.globalTaskId "
          " ORDER BY startTime)\n"
          " select * from ( select deviceId, coalesce(compute.name, schedule.name, main.taskType) as name, main.pid,"
          " main.pid as metaType,"
          " main.tid, main.startTime, main.duration, main.depth, main.ROWID as id from tasks main\n"
          " left join COMPUTE_TASK_INFO compute on compute.globalTaskId = main.globalTaskId "
          " LEFT JOIN COMMUNICATION_SCHEDULE_TASK_INFO schedule ON main.globalTaskId = schedule.globalTaskId union ALL"
          " select deviceId,name, pid, pid as meatType, tid, startTime, duration, depth, id from com union ALL " +
          communicationOpSql +
          " UNION all select '' as deviceId, name, globalTid as pid, 'HOST' as metaType, type as tid, "
          "startNs - minTime.value AS startTime, endNs - startNs AS duration, depth, CANN_API.ROWID as id from "
          "CANN_API JOIN minTime UNION all " + mstxEventsSql +
          "UNION all select '' as deviceId, name, globalTid as pid,"
          "'HOST' as metaType, 'pytorch' as tid, "
          "startNs - minTime.value AS startTime, endNs - startNs AS duration, depth, PYTORCH_API.ROWID as id from "
          "PYTORCH_API JOIN minTime "
          "UNION ALL SELECT '' AS deviceId, name, globalTid AS pid, 'HOST' AS metaType, 'OSRT_API' AS tid, "
          "startNs - minTime.value AS startTime, endNs - startNs AS duration, 0 AS depth, osrt.ROWID AS id FROM " +
          TABLE_OSRT_API + " osrt JOIN minTime) allNames join ids on ids.id = allNames.name" + filterJoin + orderBy +
          " LIMIT ? OFFSET ?";
    return sql;
}

std::string DbTraceDataBase::GetSearchSliceNameCountSql(const SearchSliceSqlParams &params)
{
    std::string nameMatch;
    if (params.isMatchExact && params.isMatchCase) {
        nameMatch = "select id from STRING_IDS where value like ?";
    } else if (params.isMatchExact) {
        nameMatch = "select id from STRING_IDS where lower(value) like lower(?)";
    } else if (params.isMatchCase) {
        nameMatch = "select id from STRING_IDS where value like '%'||?||'%'";
    } else {
        nameMatch = "select id from STRING_IDS where lower(value) like lower('%'||?||'%')";
    }

    std::string hostSql = "select name from " + TABLE_CANN_API + " union all select message from  " +
                          TABLE_MSTX_EVENTS + " union all select name from  " + TABLE_API +
                          " UNION ALL SELECT name FROM " + TABLE_OSRT_API;

    std::string communicationOpSql;
    if (!TraceDatabaseHelper::IsDeviceIdUnique(params.rankId)) {
        communicationOpSql = "select opName as name from COMMUNICATION_OP op "
                             " join tasks on op.connectionId = tasks.connectionId group by opId";
    } else {
        communicationOpSql = "select opName as name from COMMUNICATION_OP op";
    }

    std::string filterCte;
    std::string filterJoin;
    if (!params.nameFilter.empty()) {
        filterCte = ", filterIds as (select id from STRING_IDS where lower(value) like lower('%'||?||'%'))";
        filterJoin = " join filterIds on filterIds.id = allNames.name";
    }

    std::string sql = "with ids as (" + nameMatch + ")" + filterCte +
          ", tasks as (select globalTaskId, taskType, connectionId from TASK where deviceId = ?), "
          " com as (select opId, info.globalTaskId,info.taskType as name from COMMUNICATION_TASK_INFO info "
          " join tasks on  info.globalTaskId = tasks.globalTaskId), "
          " compute as (select info.globalTaskId, name from COMPUTE_TASK_INFO info join tasks "
          " on  info.globalTaskId = tasks.globalTaskId), "
          " schedule as (select info.globalTaskId, name from COMMUNICATION_SCHEDULE_TASK_INFO info left join tasks "
          " on info.globalTaskId = tasks.globalTaskId)"
          "select count(1) as count from ( "
          "    select coalesce(compute.name, schedule.name, main.taskType) as name from tasks main "
          "         left join compute on compute.globalTaskId = main.globalTaskId "
          " left join schedule ON main.globalTaskId = schedule.globalTaskId"
          "    union ALL select name from com "
          "    union ALL " +
          communicationOpSql + " union ALL " + hostSql + ") allNames join ids on id = allNames.name" + filterJoin + ";";
    return sql;
}

std::string DbTraceDataBase::GetSearchCountWithLockSql(const SearchCountParams &params,
                                                       const std::vector<TrackQuery> &trackQuery)
{
    std::string nameMatch;
    if (params.isMatchExact && params.isMatchCase) {
        nameMatch = "select id from STRING_IDS where value like ?";
    } else if (params.isMatchExact) {
        nameMatch = "select id from STRING_IDS where lower(value) like lower(?)";
    } else if (params.isMatchCase) {
        nameMatch = "select id from STRING_IDS where value like '%'||?||'%'";
    } else {
        nameMatch = "select id from STRING_IDS where lower(value) like lower('%'||?||'%')";
    }

    std::string filterCte;
    if (!params.nameFilter.empty()) {
        filterCte = ", filterIds as (select id from STRING_IDS where lower(value) like lower('%'||?||'%'))";
    }

    std::string sql = "with ids as (" + nameMatch + ")" + filterCte + " ";
    std::vector<std::string> sqls;
    for (const auto &item: trackQuery) {
        std::string tempSql = GetSingleSearchCountLockRangeSql(params, item);
        if (!tempSql.empty()) {
            sqls.emplace_back(tempSql);
        }
    }
    sql = sql + StringUtil::join(sqls, " UNION ALL ");
    return sql;
}

std::string DbTraceDataBase::GetSingleSearchCountLockRangeSql(const SearchCountParams &params, const TrackQuery &item)
{
    PROCESS_TYPE type = STR_TO_ENUM<PROCESS_TYPE>(item.metaType).value();
    std::string filterJoin;
    if (!params.nameFilter.empty()) {
        filterJoin = " join filterIds on filterIds.id = ";
    }

    std::string tempSql;
    // 当filterJoin非空时，需要补上具体的字段名；为空时不需要追加
    std::string filterSuffix = filterJoin.empty() ? "" : filterJoin;
    if (type == PROCESS_TYPE::API) {
        filterSuffix += filterJoin.empty() ? "" : "api.name";
        tempSql = "SELECT count(1) as count FROM (SELECT name from " + TABLE_API +
                  " WHERE globalTid = ? AND startNs >= ? AND endNs <= ?) api join ids on id = api.name" +
                  filterSuffix + " ";
    } else if (type == PROCESS_TYPE::CANN_API) {
        filterSuffix += filterJoin.empty() ? "" : "cann.name";
        tempSql = "SELECT count(1) as count FROM (SELECT name from " + TABLE_CANN_API +
                  " WHERE globalTid = ? AND type = ? AND startNs >= ? AND endNs <= ?) cann join ids on id = cann.name" +
                  filterSuffix + " ";
    } else if (type == PROCESS_TYPE::MS_TX) {
        filterSuffix += filterJoin.empty() ? "" : "mstx.message";
        tempSql = "SELECT count(1) as count FROM (SELECT message from " + TABLE_MSTX_EVENTS +
                  " WHERE globalTid = ? AND startNs >= ? AND endNs <= ?) mstx join ids on id = mstx.message" +
                  filterSuffix + " ";
    } else if (type == PROCESS_TYPE::OSRT_API) {
        filterSuffix += filterJoin.empty() ? "" : "osrt.name";
        tempSql = "SELECT count(1) as count FROM (SELECT name from " + TABLE_OSRT_API +
                  " WHERE globalTid = ? AND startNs >= ? AND endNs <= ?) osrt join ids on id = osrt.name" +
                  filterSuffix + " ";
    } else if (type == PROCESS_TYPE::ASCEND_HARDWARE) {
        filterSuffix += filterJoin.empty() ? "" : "hadware.name";
        tempSql = "SELECT count(1) as count FROM (SELECT coalesce(c.name, m.message, s.name, main.taskType) as "
                  "name FROM " + TABLE_TASK +
                  " main "
                  " left join " + TABLE_COMPUTE_TASK_INFO +
                  " c on c.globalTaskId = main.globalTaskId "
                  " left join " + TABLE_MSTX_EVENTS +
                  " m on "
                  " (m.connectionId = main.connectionId and  m.connectionId != " +
                  WRONG_DATA + " ) left join " + TABLE_COMMUNICATION_SCHEDULE_TASK +
                  " s on main.globalTaskId = s.globalTaskId WHERE main.deviceId = ? AND main.streamId = ? AND "
                  "main.startNs >= ? AND main.endNs <= ?) hadware  join ids on id = hadware.name" +
                  filterSuffix + " ";
    } else if (type == PROCESS_TYPE::HCCL) {
        if (StringUtil::EndWith(item.threadId, "group")) {
            filterSuffix += filterJoin.empty() ? "" : "op.name";
            tempSql = "SELECT count(1) as count FROM (SELECT opName as name from " + TABLE_COMMUNICATION_OP +
                      " WHERE groupName = ? AND startNs >= ? AND endNs <= ?) op join ids on id = op.name" +
                      filterSuffix + " ";
        } else {
            filterSuffix += filterJoin.empty() ? "" : "info.name";
            tempSql = "SELECT count(1) as count FROM (SELECT ci.taskType as name from TASK main left join " +
                      TABLE_COMMUNICATION_TASK_INFO + " ci on ci.globalTaskId = main.globalTaskId " +
                      " WHERE main.deviceId = ? and ci.groupName = ? AND ci.planeId = ? AND main.startNs >= ? AND "
                      "main.endNs <= ?) info join ids on id = info.name" + filterSuffix + " ";
        }
    }
    return tempSql;
}
}
// clang-format on
