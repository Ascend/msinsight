/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#ifndef PROFILER_SERVER_FTRACE_STATISTICS_SLICE_READER_H
#define PROFILER_SERVER_FTRACE_STATISTICS_SLICE_READER_H

#include <string>
#include <vector>
#include "RenderEngine.h"
#include "TextTraceDatabase.h"

namespace Dic::Module::FullDb::FtraceStatistics {

// New private caches stream one row at a time. Legacy/fake DataEngine callers keep their original path.
template <typename Consumer>
bool ForEachSlice(Timeline::TextTraceDatabase &database, const std::string &fileId, const std::string &process,
    const std::vector<std::string> &names, Consumer consumer, std::string &error) {
    if (!database.CheckTableExist("msprof_ftrace_cache")) {
        const auto slices =
            RenderEngine::Instance()->QuerySliceDetailByNameList(fileId, Timeline::DataType::TEXT, process, names);
        for (const auto &slice : slices) {
            if (!consumer(slice)) {
                return false;
            }
        }
        return true;
    }
    if (names.empty()) {
        error = "A ftrace statistics event-name whitelist is required.";
        return false;
    }
    std::string sql = "SELECT s.track_id,s.name,s.duration,s.args FROM slice s "
                      "JOIN thread t ON t.track_id=s.track_id JOIN process p ON p.pid=t.pid "
                      "WHERE p.process_name=? AND s.name IN (?";
    for (size_t index = 1; index < names.size(); ++index) {
        sql += ",?";
    }
    sql += ")";
    auto statement = database.CreatPreparedStatement(sql);
    if (!statement) {
        error = "Cannot prepare the streaming ftrace statistics query.";
        return false;
    }
    statement->BindParams(process);
    for (const auto &name : names) {
        statement->BindParams(name);
    }
    auto rows = statement->ExecuteQuery();
    if (!rows) {
        error = "Cannot bind the streaming ftrace statistics query: " + statement->GetErrorMessage();
        return false;
    }
    while (rows->Next()) {
        const auto trackId = sqlite3_column_int64(statement->stmt, 0);
        const auto duration = sqlite3_column_int64(statement->stmt, 2);
        if (sqlite3_column_type(statement->stmt, 0) != SQLITE_INTEGER || trackId < 0 ||
            sqlite3_column_type(statement->stmt, 2) != SQLITE_INTEGER || duration < 0) {
            error = "Invalid integer track/duration in the ftrace statistics cache.";
            return false;
        }
        CompeteSliceDomain slice;
        slice.trackId = static_cast<uint64_t>(trackId);
        slice.name = rows->GetString(1);
        slice.duration = static_cast<uint64_t>(duration);
        slice.args = rows->GetString(3);
        if (!consumer(slice)) {
            return false;
        }
    }
    if (rows->GetErrorCode() != SQLITE_DONE) {
        error = "Cannot stream ftrace statistics slices: " + rows->GetErrorMessage();
        return false;
    }
    return true;
}
}
#endif
