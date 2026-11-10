/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

#ifndef PROFILER_SERVER_FTRACE_STATISTICS_ARGS_H
#define PROFILER_SERVER_FTRACE_STATISTICS_ARGS_H

#include <charconv>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include "JsonUtil.h"
#include "SqlitePreparedStatement.h"

namespace Dic::Module::FullDb::FtraceStatistics {

using Args = std::unordered_map<std::string, std::string>;

inline Args ParseArgs(const std::string &json) {
    document_t document;
    if (document.Parse(json.c_str()).HasParseError() || !document.IsObject()) {
        return {};
    }
    Args result;
    for (auto it = document.MemberBegin(); it != document.MemberEnd(); ++it) {
        const auto &value = it->value;
        std::string text;
        if (value.IsString()) {
            text.assign(value.GetString(), value.GetStringLength());
        } else if (value.IsBool()) {
            text = value.GetBool() ? "true" : "false";
        } else if (value.IsInt64()) {
            text = std::to_string(value.GetInt64());
        } else if (value.IsUint64()) {
            text = std::to_string(value.GetUint64());
        } else {
            continue;
        }
        result.emplace(std::string(it->name.GetString(), it->name.GetStringLength()), std::move(text));
    }
    return result;
}

inline std::string Get(const Args &args, const std::string &name) {
    auto it = args.find(name);
    return it == args.end() ? std::string{} : it->second;
}

inline bool IsMsprof(const Args &args) { return Get(args, "source") == "msprof_ftrace"; }

inline bool ParseTid(const std::string &value, uint64_t &tid) {
    if (value.empty()) {
        return false;
    }
    const auto result = std::from_chars(value.data(), value.data() + value.size(), tid);
    return result.ec == std::errc{} && result.ptr == value.data() + value.size();
}

inline int32_t Cpu(const Args &args) {
    uint64_t cpu = 0;
    if (!ParseTid(Get(args, "cpu"), cpu) || cpu > static_cast<uint64_t>(std::numeric_limits<int32_t>::max())) {
        return -1;
    }
    return static_cast<int32_t>(cpu);
}

inline bool Task(const Args &args, std::string &comm, uint64_t &tid) {
    if (!ParseTid(Get(args, "tid"), tid)) {
        return false;
    }
    comm = Get(args, "comm");
    if (comm.empty()) {
        comm = "Task " + std::to_string(tid);
    }
    return true;
}

inline bool Preempted(const Args &args) {
    const std::string value = Get(args, "preempted");
    return value == "true" || value == "1";
}

template <typename Database>
bool EnsureTaskSummaryRow(Database &database, const std::string &comm, uint64_t tid, int32_t cpu) {
    auto statement =
        database.CreatPreparedStatement("INSERT OR IGNORE INTO trace_task_summary(comm,pid,cpu_id) VALUES(?,?,?)");
    return statement != nullptr && statement->Execute(comm, static_cast<int64_t>(tid), static_cast<int64_t>(cpu));
}

} // namespace Dic::Module::FullDb::FtraceStatistics

#endif // PROFILER_SERVER_FTRACE_STATISTICS_ARGS_H
