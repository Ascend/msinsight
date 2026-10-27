/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#include "FreeAttributionRepository.h"

#include <algorithm>
#include <deque>
#include <functional>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

#include "DataBaseManager.h"
#include "RankOverlapStore.h"
#include "SqlitePreparedStatement.h"
#include "TraceTime.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::Timeline {
namespace {
constexpr size_t SQL_BATCH_SIZE = 400;
constexpr size_t MAX_CACHE_ENTRIES = 3;

using DatabasePtr = std::shared_ptr<VirtualTraceDatabase>;
using StatementPtr = std::unique_ptr<SqlitePreparedStatement>;
using ResultPtr = std::unique_ptr<SqliteResultSet>;
using EvidenceIndex = std::map<std::pair<uint64_t, std::string>, std::vector<FreeAttributionEvidence>>;

struct Source {
    std::string fileId;
    std::string deviceId;
    DatabasePtr database;
    std::weak_ptr<void> poolToken;
    bool text = false;
};

struct Event {
    FreeAttributionTarget target;
    uint64_t startNs = 0;
    uint64_t endNs = 0;
    uint64_t trackId = 0;
    std::string kind;
    std::string connection;
};

struct TrackSlices {
    std::vector<Event> slices;
    std::vector<uint64_t> maxEnds;
};

struct FlowPoint {
    uint64_t id = 0;
    uint64_t trackId = 0;
    uint64_t timestamp = 0;
    std::string group;
    std::string type;
    std::vector<size_t> predecessors;
    bool orderIncomplete = false;
};

struct FlowChainState {
    std::vector<size_t> anchors;
    bool previousStart = false;
    bool ended = false;
    bool incomplete = false;
};

struct CacheEntry {
    std::vector<std::weak_ptr<void>> pools;
    std::vector<FreeAttributionRecord> records;
};
std::mutex cacheMutex;
std::map<std::string, CacheEntry> cache;

void AppendKey(std::string &key, const std::string &part) { key += std::to_string(part.size()) + ":" + part; }

std::string EventKey(const Event &event) {
    std::string result;
    for (const auto &part : {event.target.dbPath, event.target.metaType, event.target.id}) {
        AppendKey(result, part);
    }
    return result;
}

bool Read(const DatabasePtr &database, const std::string &sql, const std::function<void(StatementPtr &)> &bind,
    const std::function<void(ResultPtr &)> &consume, std::string &error) {
    auto statement = database->CreatPreparedStatement(sql);
    if (statement == nullptr) {
        error = "Failed to prepare Free attribution query.";
        return false;
    }
    bind(statement);
    auto result = statement->ExecuteQuery();
    if (result == nullptr) {
        error = "Failed to execute Free attribution query.";
        return false;
    }
    while (result->Next()) {
        consume(result);
    }
    if (result->GetErrorCode() != SQLITE_DONE) {
        error = "Failed to read Free attribution query results.";
        return false;
    }
    return true;
}

std::string Placeholders(size_t size) {
    std::string result;
    for (size_t index = 0; index < size; ++index) {
        result += index == 0 ? "?" : ",?";
    }
    return result;
}

FreeAttributionTarget MakeTarget(
    ResultPtr &row, const Source &source, const std::string &rankId, const std::string &metaType, uint64_t minTime) {
    FreeAttributionTarget target;
    target.id = row->GetString("id");
    target.name = row->GetString("name");
    target.cardId = rankId;
    target.dbPath = source.fileId;
    target.pid = row->GetString("pid");
    target.tid = row->GetString("tid");
    target.depth = row->GetUint64("depth");
    target.metaType = metaType;
    const auto start = row->GetUint64("startNs");
    const auto end = row->GetUint64("endNs");
    target.timestamp = start >= minTime ? start - minTime : 0;
    target.duration = end >= start ? end - start : 0;
    return target;
}

bool ResolveSources(
    const FreeAttributionParams &params, std::vector<Source> &sources, Source &overlap, std::string &error) {
    auto &manager = DataBaseManager::Instance();
    auto fileIds = manager.GetSourceFileIdsByRankId(params.rankId);
    if (fileIds.empty()) {
        const auto fileId = manager.GetFileIdByRankId(params.rankId);
        if (!fileId.empty()) {
            fileIds.push_back(fileId);
        }
    }
    // dbPath alone is never authority to read another card's database.
    if (fileIds.empty()) {
        error = "No source database is registered for the selected card.";
        return false;
    }
    for (const auto &fileId : fileIds) {
        Source source;
        source.fileId = fileId;
        source.poolToken = manager.GetTraceDatabasePoolToken(fileId);
        source.database = manager.GetTraceDatabaseByFileId(fileId);
        if (source.database == nullptr) {
            error = "A source database of the selected card is unavailable.";
            return false;
        }
        source.deviceId = manager.FindDeviceIdByFileIdAndRankId(fileId, params.rankId);
        source.text = source.database->CheckTableExist("slice");
        if (source.deviceId.empty()) {
            source.deviceId = manager.GetDeviceIdFromRankId(params.rankId, false);
        }
        if (source.deviceId.empty()) {
            error = "The device identity of the selected card is unavailable.";
            return false;
        }
        sources.emplace_back(std::move(source));
    }
    overlap = sources.front();
    const auto derived = RankOverlapStore::Instance().ResolveSource(params.rankId);
    if (!derived.empty()) {
        overlap.fileId = derived;
        overlap.poolToken = manager.GetTraceDatabasePoolToken(derived);
        overlap.database = manager.GetTraceDatabaseByFileId(derived);
        overlap.deviceId = manager.FindDeviceIdByFileIdAndRankId(derived, params.rankId);
        overlap.text = false;
        if (overlap.database == nullptr || overlap.deviceId.empty()) {
            error = "The card's merged Overlap Analysis is unavailable.";
            return false;
        }
    }
    return true;
}

bool LoadOverlap(const Source &source, const std::string &rankId, uint64_t minTime, std::vector<Event> &freeEvents,
    std::map<uint64_t, std::set<std::string>> &behaviors, std::string &error) {
    if ((!source.text && !source.database->CheckTableExist("OVERLAP_ANALYSIS")) ||
        (source.text &&
            (!source.database->CheckTableExist("thread") || !source.database->CheckTableExist("process")))) {
        return true;
    }
    const std::string sql = source.text
        ? "SELECT s.id AS id, s.name AS name, s.timestamp AS startNs, s.end_time AS endNs, "
          "t.pid AS pid, t.tid AS tid, s.depth AS depth, t.thread_name AS kind "
          "FROM slice s JOIN thread t ON s.track_id=t.track_id JOIN process p ON t.pid=p.pid "
          "WHERE p.process_name='Overlap Analysis' AND (p.pid & 31)=CAST(? AS INTEGER) "
          "AND t.thread_name IN ('Free','Computing','Communication') ORDER BY s.timestamp,s.id"
        : "SELECT ROWID AS id, CASE type WHEN 3 THEN 'Free' WHEN 0 THEN 'Computing' "
          "WHEN 1 THEN 'Communication' END AS name, startNs, endNs, 'OVERLAP_ANALYSIS' AS pid, "
          "type AS tid, 0 AS depth, CASE type WHEN 3 THEN 'Free' WHEN 0 THEN 'Computing' "
          "WHEN 1 THEN 'Communication' END AS kind FROM OVERLAP_ANALYSIS "
          "WHERE deviceId=? AND type IN (0,1,3) ORDER BY startNs,ROWID";
    return Read(
        source.database, sql, [&source](auto &statement) { statement->BindParams(source.deviceId); },
        [&](auto &row) {
            Event event;
            event.startNs = row->GetUint64("startNs");
            event.endNs = row->GetUint64("endNs");
            if (event.endNs <= event.startNs) {
                return;
            }
            event.kind = row->GetString("kind");
            event.target = MakeTarget(row, source, rankId, source.text ? "TEXT" : "OVERLAP_ANALYSIS", minTime);
            if (event.kind == "Free") {
                freeEvents.emplace_back(std::move(event));
            } else {
                behaviors[event.startNs].insert(event.kind);
            }
        },
        error);
}

std::set<uint64_t> NeededStarts(
    const std::vector<Event> &freeEvents, const std::map<uint64_t, std::set<std::string>> &behaviors) {
    std::set<uint64_t> starts;
    for (const auto &event : freeEvents) {
        const auto next = behaviors.lower_bound(event.endNs);
        if (next != behaviors.end() && next->first == event.endNs) {
            starts.insert(next->first);
        }
    }
    return starts;
}

bool LoadDbCandidates(const Source &source, const std::string &rankId, uint64_t minTime,
    const std::set<uint64_t> &starts, std::vector<Event> &events, std::string &error) {
    if (!source.database->CheckTableExist("STRING_IDS")) {
        return true;
    }
    const bool compute =
        source.database->CheckTableExist("TASK") && source.database->CheckTableExist("COMPUTE_TASK_INFO");
    const bool communication = source.database->CheckTableExist("COMMUNICATION_OP");
    const std::vector<uint64_t> times(starts.begin(), starts.end());
    for (const auto &kind : {std::string("Computing"), std::string("Communication")}) {
        if ((kind == "Computing" && !compute) || (kind == "Communication" && !communication)) {
            continue;
        }
        std::string sql;
        if (kind == "Computing") {
            const auto depth = source.database->CheckColumnExist("TASK", "depth") ? "task.depth" : "0";
            sql = "SELECT DISTINCT task.ROWID AS id, names.value AS name, task.startNs, task.endNs, "
                  "'Ascend Hardware' AS pid, task.streamId AS tid, " +
                std::string(depth) +
                " AS depth, task.connectionId AS connectionId FROM TASK task "
                "JOIN COMPUTE_TASK_INFO info ON task.globalTaskId=info.globalTaskId "
                "LEFT JOIN STRING_IDS names ON info.name=names.id WHERE task.deviceId=? AND task.startNs IN (";
        } else {
            std::string deviceCondition;
            if (source.database->CheckColumnExist("COMMUNICATION_OP", "deviceId")) {
                deviceCondition = "op.deviceId=?";
            } else if (source.database->CheckTableExist("TASK")) {
                deviceCondition = "EXISTS (SELECT 1 FROM TASK task WHERE task.connectionId=op.connectionId "
                                  "AND task.deviceId=?)";
            } else {
                continue;
            }
            sql = "SELECT op.ROWID AS id, names.value AS name, op.startNs, op.endNs, 'HCCL' AS pid, "
                  "op.groupName||'group' AS tid, 0 AS depth, op.connectionId AS connectionId "
                  "FROM COMMUNICATION_OP op LEFT JOIN STRING_IDS names ON op.opName=names.id WHERE " +
                deviceCondition + " AND op.startNs IN (";
        }
        for (size_t offset = 0; offset < times.size(); offset += SQL_BATCH_SIZE) {
            const size_t count = std::min(SQL_BATCH_SIZE, times.size() - offset);
            if (!Read(
                    source.database, sql + Placeholders(count) + ")",
                    [&](auto &statement) {
                        statement->BindParams(source.deviceId);
                        for (size_t index = 0; index < count; ++index) {
                            statement->BindParams(times[offset + index]);
                        }
                    },
                    [&](auto &row) {
                        Event event;
                        event.startNs = row->GetUint64("startNs");
                        event.endNs = row->GetUint64("endNs");
                        event.kind = kind;
                        event.connection = row->GetString("connectionId");
                        event.target =
                            MakeTarget(row, source, rankId, kind == "Computing" ? "Ascend Hardware" : "HCCL", minTime);
                        events.emplace_back(std::move(event));
                    },
                    error)) {
                return false;
            }
        }
    }
    return true;
}

bool LoadDbEvidence(const Source &source, const std::string &rankId, uint64_t minTime, const std::set<uint64_t> &starts,
    EvidenceIndex &index, std::string &error) {
    std::vector<Event> events;
    if (!LoadDbCandidates(source, rankId, minTime, starts, events, error)) {
        return false;
    }
    std::set<std::string> connectionSet;
    for (const auto &event : events) {
        if (!event.connection.empty() && event.connection != "-1" && event.connection != "4294967295") {
            connectionSet.insert(event.connection);
        }
    }
    const std::vector<std::string> connections(connectionSet.begin(), connectionSet.end());
    std::map<std::string, std::vector<FreeAttributionTarget>> apis;
    std::set<std::string> knownConnections;
    std::set<std::string> incompleteConnections;
    if (source.database->CheckTableExist("CONNECTION_IDS")) {
        for (size_t offset = 0; offset < connections.size(); offset += SQL_BATCH_SIZE) {
            const size_t count = std::min(SQL_BATCH_SIZE, connections.size() - offset);
            const auto bind = [&](auto &statement) {
                for (size_t item = 0; item < count; ++item) {
                    statement->BindParams(connections[offset + item]);
                }
            };
            if (!Read(
                    source.database,
                    "SELECT connectionId FROM CONNECTION_IDS WHERE connectionId IN (" + Placeholders(count) + ")", bind,
                    [&](auto &row) { knownConnections.insert(row->GetString("connectionId")); }, error)) {
                return false;
            }
            if (!source.database->CheckTableExist("PYTORCH_API") || !source.database->CheckTableExist("STRING_IDS")) {
                continue;
            }
            const auto depth = source.database->CheckColumnExist("PYTORCH_API", "depth") ? "api.depth" : "0";
            const std::string sql = "SELECT api.ROWID AS id, names.value AS name, api.startNs, api.endNs, "
                                    "api.globalTid AS pid, 'pytorch' AS tid, " +
                std::string(depth) +
                " AS depth, ids.connectionId AS connectionId FROM PYTORCH_API api "
                "JOIN STRING_IDS names ON api.name=names.id "
                "JOIN CONNECTION_IDS ids ON api.connectionId=ids.id "
                "WHERE (api.type IS NULL OR api.type!=50003) AND ids.connectionId IN (" +
                Placeholders(count) + ") ORDER BY api.startNs,api.ROWID";
            if (!Read(
                    source.database, sql, bind,
                    [&](auto &row) {
                        auto target = MakeTarget(row, source, rankId, "PYTORCH_API", minTime);
                        apis[row->GetString("connectionId")].push_back(std::move(target));
                    },
                    error)) {
                return false;
            }
            const std::string missingSql = "SELECT DISTINCT ids.connectionId FROM CONNECTION_IDS ids "
                                           "WHERE ids.connectionId IN (" +
                Placeholders(count) +
                ") AND NOT EXISTS ("
                "SELECT 1 FROM PYTORCH_API api JOIN STRING_IDS names ON api.name=names.id "
                "WHERE api.connectionId=ids.id AND (api.type IS NULL OR api.type!=50003))";
            if (!Read(
                    source.database, missingSql, bind,
                    [&](auto &row) { incompleteConnections.insert(row->GetString("connectionId")); }, error)) {
                return false;
            }
        }
    }
    for (const auto &event : events) {
        FreeAttributionEvidence evidence;
        evidence.targetFound = true;
        evidence.flowFound = knownConnections.count(event.connection) != 0;
        evidence.complete = incompleteConnections.count(event.connection) == 0;
        const auto candidates = apis.find(event.connection);
        if (candidates != apis.end()) {
            // Explicit connection mappings remain valid across Host/Device
            // timestamp inversions. Retain every mapped API if a key is reused.
            evidence.apiTargets = candidates->second;
        }
        index[{event.startNs, event.kind}].emplace_back(std::move(evidence));
    }
    return true;
}

class TextEvidenceLoader {
  public:
    TextEvidenceLoader(const Source &source, std::string rankId, uint64_t minTime)
        : source(source), rankId(std::move(rankId)), minTime(minTime) {}

    bool Load(const std::set<uint64_t> &starts, EvidenceIndex &index, std::string &error) {
        std::vector<Event> candidates;
        if (!LoadCandidates(starts, candidates, error) || !LoadFlows(error)) {
            return false;
        }
        for (const auto &candidate : candidates) {
            FreeAttributionEvidence evidence;
            if (!Trace(candidate, evidence, error)) {
                return false;
            }
            index[{candidate.startNs, candidate.kind}].push_back(std::move(evidence));
        }
        return true;
    }

  private:
    static bool IsPythonProcess(const std::string &process) {
        return process == "Python" || process == "PyTorch" || process == "pytorch" || process == "python3";
    }

    Event ReadEvent(ResultPtr &row) const {
        Event event;
        event.startNs = row->GetUint64("startNs");
        event.endNs = row->GetUint64("endNs");
        event.trackId = row->GetUint64("trackId");
        const auto process = row->GetString("processName");
        const auto cat = row->GetString("cat");
        const bool python = IsPythonProcess(process) && cat != "python_function";
        event.kind = cat == "python_function" ? "PythonStack" : (python ? "Python" : process);
        event.target = MakeTarget(row, source, rankId, "TEXT", minTime);
        return event;
    }

    std::string SliceSelect() const {
        return "SELECT s.id AS id,s.name AS name,s.timestamp AS startNs,s.end_time AS endNs,"
               "s.track_id AS trackId,s.depth AS depth,s.cat AS cat,t.pid AS pid,t.tid AS tid,"
               "p.process_name AS processName FROM slice s JOIN thread t ON s.track_id=t.track_id "
               "JOIN process p ON t.pid=p.pid ";
    }

    bool LoadCandidates(const std::set<uint64_t> &starts, std::vector<Event> &events, std::string &error) {
        const std::vector<uint64_t> times(starts.begin(), starts.end());
        for (size_t offset = 0; offset < times.size(); offset += SQL_BATCH_SIZE) {
            const auto count = std::min(SQL_BATCH_SIZE, times.size() - offset);
            const auto sql = SliceSelect() + "WHERE (p.pid & 31)=CAST(? AS INTEGER) AND s.timestamp IN (" +
                Placeholders(count) +
                ") AND (p.process_name='Ascend Hardware' OR "
                "(p.process_name IN ('HCCL','Communication','COMMUNICATION') AND t.thread_name LIKE 'Group%'))";
            if (!Read(
                    source.database, sql,
                    [&](auto &statement) {
                        statement->BindParams(source.deviceId);
                        for (size_t item = 0; item < count; ++item) {
                            statement->BindParams(times[offset + item]);
                        }
                    },
                    [&](auto &row) {
                        auto event = ReadEvent(row);
                        event.kind = event.kind == "Ascend Hardware" ? "Computing" : "Communication";
                        events.push_back(std::move(event));
                    },
                    error)) {
                return false;
            }
        }
        return true;
    }

    bool LoadFlows(std::string &error) {
        if (!source.database->CheckTableExist("flow")) {
            return true;
        }
        std::map<std::string, std::vector<size_t>> groups;
        if (!Read(
                source.database,
                "SELECT f.id,f.flow_id,f.cat,f.track_id,f.timestamp,f.type,p.process_name AS processName "
                "FROM flow f LEFT JOIN thread t ON f.track_id=t.track_id LEFT JOIN process p ON t.pid=p.pid "
                "WHERE f.cat='async_npu' ORDER BY f.timestamp,f.id",
                [](auto &) {},
                [&](auto &row) {
                    FlowPoint point;
                    point.id = row->GetUint64("id");
                    point.trackId = row->GetUint64("track_id");
                    point.timestamp = row->GetUint64("timestamp");
                    point.type = row->GetString("type");
                    AppendKey(point.group, row->GetString("cat"));
                    AppendKey(point.group, row->GetString("flow_id"));
                    const size_t position = points.size();
                    groups[point.group].push_back(position);
                    if (point.type == "s") {
                        const auto process = row->GetString("processName");
                        if (!process.empty() && !IsPythonProcess(process)) {
                            nonPythonStarts.insert(position);
                        }
                    }
                    if (point.type == "f") {
                        incoming[point.trackId].push_back(position);
                    }
                    points.push_back(std::move(point));
                },
                error)) {
            return false;
        }
        for (const auto &[key, group] : groups) {
            BuildGroupPredecessors(group);
        }
        return true;
    }

    void SetPredecessors(const std::vector<size_t> &endpoints, const std::vector<size_t> &anchors, bool incomplete) {
        for (const auto position : endpoints) {
            points[position].predecessors = anchors;
            points[position].orderIncomplete = incomplete;
        }
    }

    static void SetStarts(const std::vector<size_t> &starts, FlowChainState &state, bool append) {
        if (!append) {
            state.anchors.clear();
            state.incomplete = false;
        }
        state.anchors.insert(state.anchors.end(), starts.begin(), starts.end());
        state.previousStart = true;
        state.ended = false;
    }

    void LinkStepsAndEnds(const std::vector<size_t> &steps, const std::vector<size_t> &ends, FlowChainState &state) {
        if (!steps.empty()) {
            // Several simultaneous steps have no recoverable order. Keep all
            // possible final relays and propagate uncertainty to later endpoints.
            state.incomplete = state.incomplete || state.ended || steps.size() > 1;
            SetPredecessors(steps, state.anchors, state.incomplete);
            state.anchors = steps;
            state.previousStart = false;
        }
        if (!ends.empty()) {
            SetPredecessors(ends, state.anchors, state.incomplete);
            state.ended = true;
            state.previousStart = false;
        }
    }

    void LinkTimestampBucket(const std::vector<size_t> &starts, const std::vector<size_t> &steps,
        const std::vector<size_t> &ends, FlowChainState &state) {
        if (starts.empty()) {
            LinkStepsAndEnds(steps, ends, state);
            return;
        }
        if (state.anchors.empty()) {
            SetStarts(starts, state, false);
            LinkStepsAndEnds(steps, ends, state);
            return;
        }
        if (ends.empty() && (steps.empty() || state.previousStart)) {
            // Consecutive starts remain parallel roots until the first relay
            // or final, including a start tied with that first relay.
            SetStarts(starts, state, state.previousStart);
            LinkStepsAndEnds(steps, ends, state);
            return;
        }
        if (steps.empty()) {
            // End the older instance before opening a reused ID at the same
            // timestamp. Sorting every start first would merge both instances.
            auto predecessors = state.anchors;
            const bool uncertainBoundary = state.ended || ends.size() > 1;
            if (uncertainBoundary) {
                // Multiple final points can span both instances. A prior final
                // also permits an older fan-out or a new zero-latency instance.
                predecessors.insert(predecessors.end(), starts.begin(), starts.end());
            }
            SetPredecessors(ends, predecessors, state.incomplete || uncertainBoundary);
            SetStarts(starts, state, false);
            return;
        }
        // A new start and a relay coexist with an older active instance. Keep
        // both generations as evidence instead of inventing a unique ordering.
        auto predecessors = state.anchors;
        predecessors.insert(predecessors.end(), starts.begin(), starts.end());
        SetPredecessors(steps, predecessors, true);
        predecessors.insert(predecessors.end(), steps.begin(), steps.end());
        SetPredecessors(ends, predecessors, true);
        SetStarts(starts, state, false);
        state.anchors.insert(state.anchors.end(), steps.begin(), steps.end());
        state.previousStart = false;
        state.incomplete = true;
        state.ended = !ends.empty();
    }

    void BuildGroupPredecessors(const std::vector<size_t> &group) {
        // A single explicit pair establishes identity independently of the
        // relative Host/Device clocks or the order in which points were stored.
        if (group.size() == 2) {
            const auto first = group[0];
            const auto second = group[1];
            if (points[first].type == "s" && points[second].type == "f") {
                SetPredecessors({second}, {first}, false);
                return;
            }
            if (points[first].type == "f" && points[second].type == "s") {
                SetPredecessors({first}, {second}, false);
                return;
            }
        }
        FlowChainState state;
        for (size_t begin = 0; begin < group.size();) {
            size_t end = begin;
            std::vector<size_t> starts;
            std::vector<size_t> steps;
            std::vector<size_t> ends;
            while (end < group.size() && points[group[end]].timestamp == points[group[begin]].timestamp) {
                const auto position = group[end++];
                if (points[position].type == "s") {
                    starts.push_back(position);
                } else if (points[position].type == "t") {
                    steps.push_back(position);
                } else if (points[position].type == "f") {
                    ends.push_back(position);
                }
            }
            LinkTimestampBucket(starts, steps, ends, state);
            begin = end;
        }
    }

    bool MapPoint(size_t position, std::vector<Event> &events, std::string &error) {
        const auto &point = points[position];
        auto found = tracks.find(point.trackId);
        if (found == tracks.end()) {
            TrackSlices track;
            if (!Read(
                    source.database,
                    SliceSelect() +
                        "WHERE s.track_id=? "
                        "AND (s.cat IS NULL OR s.cat!='python_function') ORDER BY s.timestamp,s.id",
                    [&](auto &statement) { statement->BindParams(point.trackId); },
                    [&](auto &row) { track.slices.push_back(ReadEvent(row)); }, error)) {
                return false;
            }
            if (!track.slices.empty()) {
                track.maxEnds.resize(track.slices.size() * 4);
                BuildIntervalIndex(track, 1, 0, track.slices.size());
            }
            found = tracks.emplace(point.trackId, std::move(track)).first;
        }
        const auto &track = found->second;
        const auto &slices = track.slices;
        const auto cached = mappedPoints.find(position);
        if (cached != mappedPoints.end()) {
            for (const auto index : cached->second) {
                events.push_back(slices[index]);
            }
            return true;
        }
        std::vector<size_t> containing;
        if (!slices.empty()) {
            const auto upper = std::upper_bound(slices.begin(), slices.end(), point.timestamp,
                [](uint64_t timestamp, const auto &event) { return timestamp < event.startNs; });
            QueryIntervals(
                track, 1, 0, slices.size(), static_cast<size_t>(upper - slices.begin()), point.timestamp, containing);
        }
        std::vector<size_t> selected;
        bool exact = false;
        uint64_t bestDepth = 0;
        uint64_t bestStart = 0;
        for (const auto index : containing) {
            const auto &slice = slices[index];
            const bool startsHere = slice.startNs == point.timestamp;
            if (!events.empty() &&
                ((exact && !startsHere) ||
                    (exact == startsHere &&
                        (slice.target.depth < bestDepth ||
                            (slice.target.depth == bestDepth && slice.startNs < bestStart))))) {
                continue;
            }
            if (events.empty() || startsHere != exact || slice.target.depth > bestDepth || slice.startNs > bestStart) {
                events.clear();
                exact = startsHere;
                bestDepth = slice.target.depth;
                bestStart = slice.startNs;
                selected.clear();
            }
            events.push_back(slice);
            selected.push_back(index);
        }
        mappedPoints.emplace(position, std::move(selected));
        return true;
    }

    static uint64_t BuildIntervalIndex(TrackSlices &track, size_t node, size_t begin, size_t end) {
        if (end - begin == 1) {
            return track.maxEnds[node] = track.slices[begin].endNs;
        }
        const auto middle = begin + (end - begin) / 2;
        return track.maxEnds[node] = std::max(BuildIntervalIndex(track, node * 2, begin, middle),
                   BuildIntervalIndex(track, node * 2 + 1, middle, end));
    }

    static void QueryIntervals(const TrackSlices &track, size_t node, size_t begin, size_t end, size_t limit,
        uint64_t timestamp, std::vector<size_t> &result) {
        if (begin >= limit || track.maxEnds[node] < timestamp) {
            return;
        }
        if (end - begin == 1) {
            result.push_back(begin);
            return;
        }
        const auto middle = begin + (end - begin) / 2;
        QueryIntervals(track, node * 2, begin, middle, limit, timestamp, result);
        QueryIntervals(track, node * 2 + 1, middle, end, limit, timestamp, result);
    }

    bool ReadApiOrigins(size_t endpoint, FreeAttributionEvidence &evidence, bool &relevant, std::string &error) {
        std::deque<size_t> queue{endpoint};
        std::unordered_set<size_t> visited;
        while (!queue.empty()) {
            const auto position = queue.front();
            queue.pop_front();
            if (!visited.insert(position).second) {
                continue;
            }
            const auto &point = points[position];
            if (point.type == "s") {
                // Only an actual PyTorch origin defines API attribution. A
                // runtime origin or a Python relay is not a substitute for it.
                if (nonPythonStarts.count(position) != 0) {
                    continue;
                }
                relevant = true;
                evidence.flowFound = true;
                std::vector<Event> origins;
                if (!MapPoint(position, origins, error)) {
                    return false;
                }
                bool foundApi = false;
                for (const auto &origin : origins) {
                    if (origin.kind == "Python") {
                        evidence.apiTargets.push_back(origin.target);
                        foundApi = true;
                    }
                }
                evidence.complete = evidence.complete && foundApi;
                continue;
            }
            if (point.predecessors.empty()) {
                relevant = true;
                evidence.complete = false;
            }
            evidence.complete = evidence.complete && !point.orderIncomplete;
            queue.insert(queue.end(), point.predecessors.begin(), point.predecessors.end());
        }
        return true;
    }

    bool Trace(const Event &candidate, FreeAttributionEvidence &evidence, std::string &error) {
        evidence.targetFound = true;
        const auto track = incoming.find(candidate.trackId);
        if (track == incoming.end()) {
            evidence.complete = false;
            return true;
        }
        const auto begin = std::lower_bound(track->second.begin(), track->second.end(), candidate.startNs,
            [&](size_t position, uint64_t value) { return points[position].timestamp < value; });
        for (auto iterator = begin; iterator != track->second.end(); ++iterator) {
            const auto position = *iterator;
            if (points[position].timestamp > candidate.endNs) {
                break;
            }
            std::vector<Event> endpoints;
            if (!MapPoint(position, endpoints, error)) {
                return false;
            }
            const auto match = std::find_if(endpoints.begin(), endpoints.end(),
                [&](const auto &endpoint) { return EventKey(endpoint) == EventKey(candidate); });
            if (match == endpoints.end()) {
                continue;
            }
            FreeAttributionEvidence branch;
            bool relevant = false;
            if (!ReadApiOrigins(position, branch, relevant, error)) {
                return false;
            }
            if (!relevant) {
                continue;
            }
            evidence.flowFound = evidence.flowFound || branch.flowFound;
            evidence.complete = evidence.complete && branch.complete;
            evidence.apiTargets.insert(evidence.apiTargets.end(), branch.apiTargets.begin(), branch.apiTargets.end());
        }
        evidence.complete = evidence.complete && evidence.flowFound;
        return true;
    }

    const Source &source;
    std::string rankId;
    uint64_t minTime;
    std::vector<FlowPoint> points;
    std::unordered_set<size_t> nonPythonStarts;
    std::unordered_map<uint64_t, std::vector<size_t>> incoming;
    std::unordered_map<uint64_t, TrackSlices> tracks;
    std::unordered_map<size_t, std::vector<size_t>> mappedPoints;
};

void AppendFileVersion(std::string &key, const std::string &path) {
    for (const auto &file : {path, path + "-wal"}) {
        std::error_code error;
        const auto modified = std::filesystem::last_write_time(file, error);
        if (error) {
            AppendKey(key, "missing");
            continue;
        }
        // libc++ may use a 128-bit file clock representation, which std::to_string does not support.
        AppendKey(key, std::to_string(static_cast<long long>(modified.time_since_epoch().count())));
        const auto size = std::filesystem::file_size(file, error);
        AppendKey(key, error ? "unknown-size" : std::to_string(size));
    }
}

bool SnapshotKey(const std::vector<Source> &sources, const Source &overlap, const std::string &rankId, uint64_t minTime,
    std::string &key, std::string &error) {
    AppendKey(key, rankId);
    AppendKey(key, overlap.fileId);
    AppendKey(key, std::to_string(minTime));
    const auto overlapPool = overlap.poolToken.lock();
    if (!overlapPool) {
        error = "The Overlap Analysis source has been released.";
        return false;
    }
    AppendKey(key, std::to_string(reinterpret_cast<uintptr_t>(overlapPool.get())));
    for (const auto &source : sources) {
        AppendKey(key, source.fileId);
        AppendKey(key, source.deviceId);
        const auto pool = source.poolToken.lock();
        if (!pool) {
            error = "A source of the selected card has been released.";
            return false;
        }
        AppendKey(key, std::to_string(reinterpret_cast<uintptr_t>(pool.get())));
        AppendFileVersion(key, source.database->GetDbPath());
        for (const auto &sql : {std::string("PRAGMA data_version"), std::string("SELECT total_changes() AS version")}) {
            if (!Read(
                    source.database, sql, [](auto &) {},
                    [&](auto &row) {
                        const SqliteResultSet &value = *row;
                        AppendKey(key, value.GetString(0));
                    },
                    error)) {
                return false;
            }
        }
        const std::vector<std::string> tables = source.text
            ? std::vector<std::string>{"slice", "flow", "thread", "process"}
            : std::vector<std::string>{"TASK", "COMPUTE_TASK_INFO", "COMMUNICATION_OP", "PYTORCH_API", "CONNECTION_IDS",
                  "STRING_IDS", "OVERLAP_ANALYSIS"};
        for (const auto &table : tables) {
            if (!source.database->CheckTableExist(table)) {
                AppendKey(key, "missing:" + table);
                continue;
            }
            // ROWID maxima are indexed and avoid rescanning every event on a cache hit.
            if (!Read(
                    source.database, "SELECT COALESCE(MAX(ROWID),0) AS version FROM " + table, [](auto &) {},
                    [&](auto &row) { AppendKey(key, row->GetString("version")); }, error)) {
                return false;
            }
        }
    }
    return true;
}

bool BuildRecords(const std::vector<Source> &sources, const Source &overlap, const FreeAttributionParams &params,
    uint64_t minTime, std::vector<FreeAttributionRecord> &records, std::string &error) {
    std::vector<Event> freeEvents;
    std::map<uint64_t, std::set<std::string>> behaviors;
    if (!LoadOverlap(overlap, params.rankId, minTime, freeEvents, behaviors, error)) {
        return false;
    }
    const auto starts = NeededStarts(freeEvents, behaviors);
    EvidenceIndex evidence;
    for (const auto &source : sources) {
        if (starts.empty()) {
            break;
        }
        const bool success = source.text
            ? TextEvidenceLoader(source, params.rankId, minTime).Load(starts, evidence, error)
            : LoadDbEvidence(source, params.rankId, minTime, starts, evidence, error);
        if (!success) {
            return false;
        }
    }
    for (const auto &event : freeEvents) {
        const auto next = behaviors.lower_bound(event.endNs);
        if (next == behaviors.end()) {
            records.push_back(FreeAttributionService::AttributeFree(event.target, "", {}, "noNextBehavior"));
            continue;
        }
        std::string kinds;
        for (const auto &kind : next->second) {
            kinds += kinds.empty() ? kind : "/" + kind;
        }
        if (next->first != event.endNs) {
            records.push_back(FreeAttributionService::AttributeFree(event.target, kinds, {}, "nonContiguousBehavior"));
            continue;
        }
        std::vector<FreeAttributionEvidence> candidates;
        for (const auto &kind : next->second) {
            const auto found = evidence.find({next->first, kind});
            if (found == evidence.end()) {
                candidates.emplace_back();
            } else {
                candidates.insert(candidates.end(), found->second.begin(), found->second.end());
            }
        }
        records.push_back(FreeAttributionService::AttributeFree(event.target, kinds, candidates));
    }
    return true;
}
}

bool FreeAttributionRepository::Load(
    const FreeAttributionParams &params, std::vector<FreeAttributionRecord> &records, std::string &error) {
    records.clear();
    error.clear();
    std::vector<Source> sources;
    Source overlap;
    if (!ResolveSources(params, sources, overlap, error)) {
        return false;
    }
    const auto minTime = TraceTime::Instance().GetStartTime();
    std::string key;
    if (!SnapshotKey(sources, overlap, params.rankId, minTime, key, error)) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        const auto found = cache.find(key);
        if (found != cache.end()) {
            bool valid = found->second.pools.size() == sources.size() + 1;
            for (size_t index = 0; valid && index < sources.size(); ++index) {
                valid = !found->second.pools[index].expired() &&
                    !found->second.pools[index].owner_before(sources[index].poolToken) &&
                    !sources[index].poolToken.owner_before(found->second.pools[index]);
            }
            valid = valid && !found->second.pools.back().expired() &&
                !found->second.pools.back().owner_before(overlap.poolToken) &&
                !overlap.poolToken.owner_before(found->second.pools.back());
            if (valid) {
                records = found->second.records;
                return true;
            }
        }
    }
    if (!BuildRecords(sources, overlap, params, minTime, records, error)) {
        return false;
    }
    CacheEntry entry;
    entry.records = records;
    for (const auto &source : sources) {
        entry.pools.push_back(source.poolToken);
    }
    entry.pools.push_back(overlap.poolToken);
    std::lock_guard<std::mutex> lock(cacheMutex);
    if (cache.size() >= MAX_CACHE_ENTRIES) {
        cache.erase(cache.begin());
    }
    cache[key] = std::move(entry);
    return true;
}
}
