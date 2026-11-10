/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#include "MsprofFtraceWriter.h"
#include <limits>
#include <utility>
#include "TextTraceDatabase.h"
#include "MsprofFtraceCacheSchema.h"

namespace Dic::Module::FullDb {
namespace {
constexpr size_t WRITE_BATCH_SIZE = 256;
}

MsprofFtraceWriter::MsprofFtraceWriter(std::shared_ptr<Timeline::TextTraceDatabase> db) : database(std::move(db)) {}

bool MsprofFtraceWriter::Begin(std::string &error) {
    if (begun) {
        return !finished;
    }
    if (database == nullptr || !database->IsOpen()) {
        error = "The ftrace TEXT cache is not open";
        return false;
    }
    if ((!database->CheckTableExist("slice") && !database->CreateTable()) || !database->InitStmt() ||
        !database->ExecSql(MSPROF_FTRACE_CACHE_SCHEMA)) {
        error = "Failed to initialize ftrace TEXT cache tables";
        return false;
    }
    begun = true;
    return true;
}

bool MsprofFtraceWriter::EnsureProcess(const std::string &process, std::string &error) {
    if (processIds.count(process) != 0) {
        return true;
    }
    Timeline::Trace::ProcessEvent metadata;
    metadata.pid = process;
    metadata.processName = process;
    if (!database->AddSimulationProcessCache(std::move(metadata))) {
        error = "Failed to stage a ftrace process track";
        return false;
    }
    processIds.insert(process);
    return true;
}

uint64_t MsprofFtraceWriter::EnsureTrack(const MsprofFtraceSlice &slice, std::string &error) {
    const auto key = std::make_pair(slice.process, slice.threadKey);
    const auto existing = trackIds.find(key);
    if (existing != trackIds.end()) {
        return existing->second;
    }
    if (!EnsureProcess(slice.process, error)) {
        return 0;
    }
    const uint64_t trackId = ++nextTrackId;
    Timeline::Trace::ThreadEvent metadata;
    metadata.trackId = trackId;
    metadata.pid = slice.process;
    metadata.tid = slice.threadKey;
    metadata.threadName = slice.threadName;
    if (!database->AddSimulationThreadCache(std::move(metadata))) {
        error = "Failed to stage a ftrace thread track";
        return 0;
    }
    trackIds.emplace(key, trackId);
    return trackId;
}

bool MsprofFtraceWriter::AddSlice(const MsprofFtraceSlice &slice, std::string &error) {
    if (!begun || finished) {
        error = "The ftrace TEXT writer is not accepting events";
        return false;
    }
    constexpr uint64_t MAX_TIME = static_cast<uint64_t>(std::numeric_limits<int64_t>::max());
    if (slice.timestampNs > MAX_TIME || slice.durationNs > MAX_TIME - slice.timestampNs) {
        error = "A ftrace slice exceeds the SQLite timestamp range";
        return false;
    }
    const uint64_t trackId = EnsureTrack(slice, error);
    if (trackId == 0) {
        return false;
    }
    Timeline::Trace::Slice output;
    output.trackId = trackId;
    output.ts = static_cast<int64_t>(slice.timestampNs);
    output.dur = static_cast<int64_t>(slice.durationNs);
    output.end = output.ts + output.dur;
    output.name = slice.name;
    output.pid = slice.process;
    output.tid = slice.threadKey;
    output.processName = slice.process;
    output.threadName = slice.threadName;
    output.cat = "ftrace";
    output.args = slice.args;
    slices.emplace_back(std::move(output));
    return slices.size() < WRITE_BATCH_SIZE || Flush(error);
}

bool MsprofFtraceWriter::Flush(std::string &error) {
    if (!slices.empty()) {
        if (!database->InsertSliceList(slices)) {
            error = "Failed to write ftrace slices";
            return false;
        }
        slices.clear();
    }
    return true;
}

bool MsprofFtraceWriter::Finish(std::string &error) {
    if (finished) {
        return true;
    }
    if (!begun || !Flush(error)) {
        if (error.empty()) {
            error = "The ftrace TEXT writer has not started";
        }
        return false;
    }
    // Cover both depth scans so they do not fetch the large args-bearing slice rows.
    if (!database->InsertSimulationProcessList() || !database->InsertSimulationThreadList() ||
        !database->CreateIndex() ||
        !database->ExecSql("CREATE INDEX IF NOT EXISTS msprof_ftrace_slice_track_time "
                           "ON slice(track_id,timestamp,id,end_time,cat,group_id);") ||
        !database->ExecSql("CREATE INDEX IF NOT EXISTS msprof_ftrace_slice_name_track_time "
                           "ON slice(name,track_id,timestamp);")) {
        error = "Failed to finalize ftrace track metadata and indexes";
        return false;
    }
    finished = true;
    return true;
}

}
