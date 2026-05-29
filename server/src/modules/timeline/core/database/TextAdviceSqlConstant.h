/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2025-2025. All rights reserved.
 */

#ifndef PROFILER_SERVER_TEXTADVICESQLCONSTANT_H
#define PROFILER_SERVER_TEXTADVICESQLCONSTANT_H

#include <string>
#include <utility>
#include "algorithm"
#include "StringUtil.h"
#include "ServerLog.h"
#include "TimelineProtocolRequest.h"
#include "TableDefs.h"
#include "TraceDatabaseDef.h"

namespace Dic::Module::Timeline {

class TextAdviceSqlConstant {
  public:
    static std::string GenerateFusibleOpFilterTextSql(
        const Protocol::KernelDetailsParams &params, const std::vector<Timeline::FuseableOpRule> &rule) {
        // 1. 找到所有规则中最长的 opList 长度
        size_t maxLen = 0;
        for (const auto &r : rule) {
            maxLen = std::max(maxLen, r.opList.size());
        }
        // 2. 时间条件（用于 CTE 内 WHERE）
        std::string timeCondSql;
        if (params.startTime != params.endTime) {
            timeCondSql += " AND (kd.start_time + kd.duration * 1000) >= ? AND kd.start_time <= ? ";
        }
        // 3. 构造 CTE + LEAD() 列
        std::string sql = "WITH data AS ( "
                          "SELECT kd.deviceId, kd.name AS name, kd.op_type, kd.accelerator_core, "
                          "kd.start_time - ? AS startTime, "
                          "s.duration AS duration, t.pid AS pid, t.tid AS tid, "
                          "s.id AS id, s.track_id AS track_id, "
                          "ROW_NUMBER() OVER (ORDER BY s.track_id ASC, s.timestamp ASC) AS row_num, "
                          "op_type AS op";

        // 添加 LEAD(op_type, k)
        for (size_t k = 1; k < maxLen; ++k) {
            sql += ", LEAD(op_type, " + std::to_string(k) +
                ") OVER (ORDER BY s.track_id ASC, s.timestamp ASC) AS next" + std::to_string(k);
        }

        sql += " FROM " + KERNEL_DETAIL +
            " kd "
            "JOIN " +
            SLICE_TABLE +
            " s ON kd.name = s.name AND kd.start_time = s.timestamp "
            "JOIN " +
            THREAD_TABLE +
            " t ON s.track_id = t.track_id "
            "WHERE kd.deviceId = ? "
            "AND kd.accelerator_core NOT IN ('HCCL', 'COMMUNICATION') " +
            timeCondSql + ") ";

        sql = GenerateTextFusedOpRuleSql(sql, rule);

        // 6. 排序 + 分页
        sql += " ORDER BY " + params.orderBy + " " + params.order + " ) LIMIT ? OFFSET ? ";
        return sql;
    }

  private:
    static std::string GenerateTextFusedOpRuleSql(std::string sql, const std::vector<Timeline::FuseableOpRule> &rule) {
        // ----------- 工具 lambda：生成规则条件 -----------
        auto buildRuleCond = [](const Timeline::FuseableOpRule &rule) {
            std::string cond = "( op = '" + rule.opList[0] + "' ";
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

#endif // PROFILER_SERVER_TEXTADVICESQLCONSTANT_H
