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
#include <sstream>
#include "DbTraceDataBase.h"
namespace Dic::Module::FullDb {
using namespace Server;
std::string DbTraceDataBase::GetKernelDetailSql(
    const KernelDetailsParams &requestParams, const std::string &blockNumColumnName) {
    try {
        std::ostringstream sqlStream;
        sqlStream << "with nameIds as (select id, value as realName from STRING_IDS),\n"
                  << GetKernelDetailSqlWithHCCL(requestParams, blockNumColumnName) << ",\n" // 第一次绑定 filter.second
                  << GetKernelDetailSqlWithoutHCCL(requestParams, blockNumColumnName)
                  << ",\n" // 第二次绑定 filter.second
                  << "main_tmp as (select * from main_hccl UNION ALL select * from main_other), "
                  << "main as (SELECT ROWID as id, name, type, acceleratorCore, startTime,\n"
                  << "duration, waitTime, " + blockNumColumnName + ", inputShapes, inputDataTypes, inputFormats,\n"
                  << "outputShapes, outputDataTypes, outputFormats, taskId FROM main_tmp \n";
        if (requestParams.startTime != requestParams.endTime) {
            sqlStream << " WHERE (startTime + duration*1000) >= ? AND startTime <= ? ";
        }
        sqlStream << ") SELECT id, (SELECT COUNT(*) FROM main) as num, name, type, acceleratorCore, startTime, "
                  << "duration, waitTime, " + blockNumColumnName + ", inputShapes, inputDataTypes, inputFormats, "
                  << "outputShapes, outputDataTypes, outputFormats, taskId FROM main ";
        if (!StringUtil::CheckSqlValid(requestParams.orderBy)) {
            ServerLog::Error(
                "There is an SQL injection attack on this parameter. error param: %", requestParams.orderBy);
        } else if (!requestParams.orderBy.empty() && !requestParams.order.empty()) {
            sqlStream << " ORDER by "
                      << (requestParams.orderBy == "blockNum" ? blockNumColumnName : requestParams.orderBy) << " "
                      << (requestParams.order == "ascend" ? "ASC" : "DESC");
        }
        sqlStream << " LIMIT ? OFFSET ?";
        return sqlStream.str();
    } catch (DatabaseException &) {
        return "";
    } catch (const std::exception &e) {
        ServerLog::Error("An unexpected exception occurred: %", e.what());
        return "";
    }
}

std::string DbTraceDataBase::GetKernelDetailFilterSqlWithHCCL(const KernelDetailsParams &requestParams) {
    if (requestParams.filters.empty()) {
        return "";
    }
    std::string sql = " WHERE 1";
    for (const auto &[key, value] : requestParams.filters) {
        (void)value; // 去除编译告警
        if (!StringUtil::CheckSqlValid(key)) {
            ServerLog::Error("There is an SQL injection attack on this parameter. param: filter");
            throw DatabaseException("filter first value is invalid for sql.");
        }
        // hccl 查询出来的本身就是字面量，不需要映射
        sql += " AND lower(" + key + ") LIKE lower(?) "; // 绑定filter.second
    }
    return sql;
}

std::string DbTraceDataBase::GetKernelDetailFilterSqlWithoutHCCL(const KernelDetailsParams &requestParams) {
    if (requestParams.filters.empty()) {
        return "";
    }
    std::string sql = " WHERE 1";
    for (const auto &[key, value] : requestParams.filters) {
        (void)value; // 去除编译告警
        if (!StringUtil::CheckSqlValid(key)) {
            ServerLog::Error("There is an SQL injection attack on this parameter. param: filter");
            throw DatabaseException("filter first value is invalid for sql.");
        }
        // name 已经过 string_ids 表映射获取到真实的字符串字面量，taskId 本身就是数字，不需要映射
        if (key == "name" || key == "taskId") {
            sql += " AND lower(" + key + ") LIKE lower(?) "; // 绑定filter.second
        } else {
            sql += " AND " + key + " IN ( SELECT id FROM STRING_IDS WHERE lower(value) LIKE lower(?) )";
        }
    }
    return sql;
}

// throw DatabaseException
std::string DbTraceDataBase::GetKernelDetailSqlWithHCCL(
    const KernelDetailsParams &requestParams, const std::string &blockNumColumnName) {
    const std::string filterSql = GetKernelDetailFilterSqlWithHCCL(requestParams); // 绑定 filter.second
    // 前置已有 with nameIds as (select id, value as realName from STRING_IDS)
    return " main_hccl_tmp as ("
           "  select info.ROWID, nameIds.realName as name, substr(realName, 0, instr(realName, '__') + 1) as type, "
           "  'HCCL' as acceleratorCore, info.startNs as startTime, "
           "  round((info.endNs - info.startNs)/1000.0, 3) as duration, "
           "  0 as " +
        blockNumColumnName +
        ", round(waitNs/1000.0, 3) as waitTime, "
        "  'N/A' as inputShapes, 'N/A' as inputDataTypes, 'N/A' as inputFormats, "
        "  'N/A' as outputShapes, 'N/A' as outputDataTypes, 'N/A' as outputFormats, "
        "  TASK.taskId as taskId"
        "  from COMMUNICATION_OP info JOIN TASK ON info.connectionId = TASK.connectionId "
        "  join nameIds on opName = nameIds.id group by info.opName "
        "), "
        "main_hccl as ( select * from main_hccl_tmp " +
        filterSql + ") ";
}

// throw DatabaseException
std::string DbTraceDataBase::GetKernelDetailSqlWithoutHCCL(
    const KernelDetailsParams &requestParams, const std::string &blockNumColumnName) {
    const std::string filterSql = GetKernelDetailFilterSqlWithoutHCCL(requestParams); // 绑定 filter.second
    // 前置已有 with nameIds as (select id, value as realName from STRING_IDS)
    return " main_other_tmp as ("
           "  select TASK.ROWID, nameIds.realName as name, opType as type, info.taskType as acceleratorCore,"
           "  startNs as startTime, round((endNs - startNs)/1000.0, 3) as duration, "
           "  " +
        blockNumColumnName +
        ", round(waitNs/1000.0, 3) as waitTime, "
        "  inputShapes, inputDataTypes, inputFormats, outputShapes, outputDataTypes, outputFormats, "
        "  TASK.taskId as taskId "
        "  from COMPUTE_TASK_INFO info JOIN TASK ON info.globalTaskId = TASK.globalTaskId "
        "  join nameIds on name = nameIds.id where deviceId = ?"
        "), "
        " main_other as (select * from main_other_tmp " +
        filterSql + ") ";
}

}
