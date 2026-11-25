/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * msinsight is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 */
#include "PmuCounterRepository.h"
#include <limits>

namespace Dic::Module::Timeline {
namespace {
std::optional<int64_t> ReadInteger(sqlite3_stmt *statement, int column) {
    if (sqlite3_column_type(statement, column) != SQLITE_INTEGER) {
        return std::nullopt;
    }
    return sqlite3_column_int64(statement, column);
}

bool ColumnsExist(Database &database, const std::string &table, const std::vector<std::string> &columns) {
    for (const auto &column : columns) {
        if (!database.CheckColumnExist(table, column)) {
            return false;
        }
    }
    return true;
}
}

PmuCounterCapabilities PmuCounterRepository::DetectCapabilities() {
    PmuCounterCapabilities capabilities;
    database.RunExclusive([&]() {
        const bool events = database.CheckTableExist("HOST_PMU_EVENTS");
        const bool counters = database.CheckTableExist("HOST_PMU_COUNTER_INFO");
        const bool samples = database.CheckTableExist("HOST_PMU_COUNTER_DATA");
        if (!events && !counters && !samples) {
            capabilities.reason = "pmu_not_collected";
            return true;
        }
        if (!events || !counters || !samples ||
            !ColumnsExist(database, "HOST_PMU_EVENTS", {"eventId", "name", "source"}) ||
            !ColumnsExist(database, "HOST_PMU_COUNTER_INFO", {"counterId", "eventId", "pid", "cpu", "intervalNs"}) ||
            !ColumnsExist(database, "HOST_PMU_COUNTER_DATA", {"counterId", "timestamp", "valid"})) {
            capabilities.reason = "incomplete_pmu_schema";
            return true;
        }
        capabilities.available = true;
        capabilities.hasUnit = database.CheckColumnExist("HOST_PMU_EVENTS", "unit");
        capabilities.hasRawValues =
            ColumnsExist(database, "HOST_PMU_COUNTER_DATA", {"rawValue", "timeEnabled", "timeRunning"});
        return true;
    });
    return capabilities;
}

bool PmuCounterRepository::ReadCounters(std::vector<PmuCounterInfo> &counters,
    const PmuCounterCapabilities &capabilities, std::optional<int64_t> counterId) {
    counters.clear();
    if (!capabilities.available) {
        return true;
    }
    return database.RunExclusive([&]() {
        const std::string unit = capabilities.hasUnit ? "e.unit" : "'unknown'";
        const std::string sql = "SELECT c.counterId,c.eventId,c.pid,c.cpu,c.intervalNs,e.name,e.source," + unit +
            ",EXISTS(SELECT 1 FROM HOST_PMU_COUNTER_DATA s WHERE s.counterId=c.counterId)"
            " FROM HOST_PMU_COUNTER_INFO c JOIN HOST_PMU_EVENTS e ON e.eventId=c.eventId" +
            (counterId.has_value() ? std::string(" WHERE c.counterId=?") : std::string()) +
            " ORDER BY c.cpu,e.name,e.source,c.eventId,c.counterId";
        auto statement = database.CreatPreparedStatement(sql);
        if (statement == nullptr) {
            return false;
        }
        auto result = counterId.has_value() ? statement->ExecuteQuery(*counterId) : statement->ExecuteQuery();
        if (result == nullptr) {
            return false;
        }
        while (result->Next()) {
            const auto id = ReadInteger(statement->stmt, 0);
            const auto eventId = ReadInteger(statement->stmt, 1);
            const auto pid = ReadInteger(statement->stmt, 2);
            const auto cpu = ReadInteger(statement->stmt, 3);
            const auto interval = ReadInteger(statement->stmt, 4);
            if (!id || *id < 0 || !eventId || *eventId < 0 || !pid || *pid != -1 || !cpu || *cpu < 0) {
                continue;
            }
            PmuCounterInfo counter;
            counter.counterId = *id;
            counter.eventId = *eventId;
            counter.cpu = *cpu;
            counter.intervalNs = interval && *interval > 0 ? static_cast<uint64_t>(*interval) : 0;
            counter.name = result->GetString(5);
            const auto source = ReadInteger(statement->stmt, 6);
            counter.source = source && *source >= 0 && *source <= 4 ? static_cast<int>(*source) : 0;
            const auto readUnit = result->GetString(7);
            counter.unit = readUnit == "count" || readUnit == "ns" ? readUnit : "unknown";
            counter.hasSamples = result->GetInt32(8) != 0;
            counters.emplace_back(std::move(counter));
        }
        return result->GetErrorCode() == SQLITE_DONE;
    });
}

bool PmuCounterRepository::ListCounters(
    std::vector<PmuCounterInfo> &counters, const PmuCounterCapabilities &capabilities) {
    return ReadCounters(counters, capabilities, std::nullopt);
}

bool PmuCounterRepository::GetCounter(
    int64_t counterId, std::optional<PmuCounterInfo> &counter, const PmuCounterCapabilities &capabilities) {
    counter.reset();
    std::vector<PmuCounterInfo> counters;
    if (!ReadCounters(counters, capabilities, counterId)) {
        return false;
    }
    if (counters.size() == 1) {
        counter = std::move(counters.front());
    }
    return true;
}

bool PmuCounterRepository::ReadWindowWithNeighbors(int64_t counterId, uint64_t absoluteStart, uint64_t absoluteEnd,
    const PmuCounterCapabilities &capabilities, const std::function<void(const PmuCounterSample &)> &consume) {
    if (!capabilities.available || counterId < 0 || absoluteStart > absoluteEnd ||
        absoluteEnd > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
        return false;
    }
    if (!capabilities.hasRawValues) {
        return true;
    }
    return database.RunExclusive([&]() {
        auto boundaryStatement = database.CreatPreparedStatement(
            "SELECT (SELECT MAX(timestamp) FROM HOST_PMU_COUNTER_DATA WHERE counterId=? AND timestamp<?),"
            "(SELECT MIN(timestamp) FROM HOST_PMU_COUNTER_DATA WHERE counterId=? AND timestamp>?)");
        if (boundaryStatement == nullptr) {
            return false;
        }
        auto bounds = boundaryStatement->ExecuteQuery(counterId, absoluteStart, counterId, absoluteEnd);
        if (bounds == nullptr || !bounds->Next()) {
            return false;
        }
        const auto before = ReadInteger(boundaryStatement->stmt, 0);
        const auto after = ReadInteger(boundaryStatement->stmt, 1);
        const int64_t left = before.value_or(static_cast<int64_t>(absoluteStart));
        const int64_t right = after.value_or(static_cast<int64_t>(absoluteEnd));
        auto statement = database.CreatPreparedStatement(
            "SELECT timestamp,valid,rawValue,timeEnabled,timeRunning"
            " FROM HOST_PMU_COUNTER_DATA WHERE counterId=? AND timestamp>=? AND timestamp<=? ORDER BY timestamp");
        if (statement == nullptr) {
            return false;
        }
        auto result = statement->ExecuteQuery(counterId, left, right);
        if (result == nullptr) {
            return false;
        }
        while (result->Next()) {
            PmuCounterSample sample;
            const auto timestamp = ReadInteger(statement->stmt, 0);
            sample.timestamp = timestamp.value_or(sqlite3_column_int64(statement->stmt, 0));
            const auto valid = ReadInteger(statement->stmt, 1);
            sample.valid = timestamp && *timestamp >= 0 && valid && *valid == 1 ? 1 : 0;
            sample.rawValue = ReadInteger(statement->stmt, 2);
            sample.timeEnabled = ReadInteger(statement->stmt, 3);
            sample.timeRunning = ReadInteger(statement->stmt, 4);
            consume(sample);
        }
        return result->GetErrorCode() == SQLITE_DONE;
    });
}
}
