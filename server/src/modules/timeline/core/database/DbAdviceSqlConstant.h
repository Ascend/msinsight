/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef PROFILER_SERVER_DBADVICESQLCONSTANT_H
#define PROFILER_SERVER_DBADVICESQLCONSTANT_H

#include <string>
#include "algorithm"
#include "TimelineProtocolRequest.h"
#include "TableDefs.h"
#include "TraceDatabaseDef.h"

namespace Dic::Module::Timeline {

class DbAdviceSqlConstant {
  public:
    static std::string GenerateFusibleOpFilterDbSql(
        const Protocol::KernelDetailsParams &params, const std::vector<Timeline::FuseableOpRule> &rule) {
        // 1. 找到所有规则中最长的 opList 长度
        size_t maxLen = 0;
        for (const auto &r : rule) {
            maxLen = std::max(maxLen, r.opList.size());
        }
        // 2. 时间条件（用于 CTE 内 WHERE）
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND task.endNs >= ? AND task.startNs <= ? ";
        }
        // 3. 构造 CTE + LEAD() 列
        std::string sql =
            "WITH data AS ( "
            "SELECT info.ROWID as id, task.deviceId as deviceId, s1.value as name, s2.value as op_type, task.taskType, "
            "task.startNs - ? as startTime, (task.endNs - task.startNs) as duration, 'Ascend Hardware' as pid, "
            "task.streamId as tid, task.depth as depth, "
            "ROW_NUMBER() OVER (ORDER BY task.globalPid ASC, task.startNs ASC) AS row_num ";

        // 添加 LEAD(op_type, k)
        for (size_t k = 1; k < maxLen; ++k) {
            sql += ", LEAD(s2.value, " + std::to_string(k) +
                ") OVER (ORDER BY task.globalPid ASC, task.startNs ASC) AS next" + std::to_string(k);
        }

        sql += " FROM " + TABLE_COMPUTE_TASK_INFO +
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
            timeCondSql + " ) ";

        sql = GenerateDbFusedOpRuleSql(sql, rule);

        // 6. 排序 + 分页
        sql += " ORDER BY " + params.orderBy + " " + params.order + " ) LIMIT ? OFFSET ? ";
        return sql;
    }

  private:
    static std::string GenerateDbFusedOpRuleSql(std::string sql, const std::vector<Timeline::FuseableOpRule> &rule) {
        // ----------- 工具 lambda：生成规则条件 -----------
        auto buildRuleCond = [](const Timeline::FuseableOpRule &rule) {
            std::string cond = "( op_type = '" + rule.opList[0] + "' ";
            for (size_t i = 1; i < rule.opList.size(); ++i) {
                cond += " AND next" + std::to_string(i) + " = '" + rule.opList[i] + "' ";
            }
            cond += ")";
            return cond;
        };

        // 4. 外层 SELECT（增加 CASE 字段）
        sql += "SELECT * FROM ( SELECT data.*, COUNT(*) OVER () AS total_count, CASE ";

        // originOpList
        for (const auto &ruleItem : rule) {
            sql += "WHEN " + buildRuleCond(ruleItem) + " THEN '" + StringUtil::join(ruleItem.opList, ", ") + "' ";
        }
        sql += "END AS originOpList, CASE ";

        // fusedOp
        for (const auto &ruleItem : rule) {
            sql += "WHEN " + buildRuleCond(ruleItem) + " THEN '" + ruleItem.fusedOp + "' ";
        }
        sql += "END AS fusedOp FROM data WHERE ";

        // 5. WHERE（规则 OR）
        bool firstRule = true;
        for (const auto &ruleItem : rule) {
            if (!firstRule) {
                sql += " OR ";
            }
            firstRule = false;
            sql += buildRuleCond(ruleItem);
        }

        return sql;
    }
};
}

#endif // PROFILER_SERVER_DBADVICESQLCONSTANT_H
