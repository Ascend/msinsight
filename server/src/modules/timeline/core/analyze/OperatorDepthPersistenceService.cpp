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

#include "OperatorDepthPersistenceService.h"

#include <exception>
#include <string>
#include <vector>

#include "ConstantDefs.h"
#include "Database.h"
#include "DbTraceDataBase.h"
#include "OperatorDepthCalculator.h"
#include "ServerLog.h"
#include "TextTraceDatabase.h"

namespace Dic::Module::Timeline {
using Dic::Server::ServerLog;

namespace {
const std::string TEXT_SLICE_TABLE = "slice";

bool PersistPartitionedDepths(
    Database &database, const std::string &tableName, const std::string &querySql, size_t laneColumnCount) {
    auto queryStmt = database.CreatPreparedStatement(querySql);
    auto updateStmt = database.CreatPreparedStatement("UPDATE " + tableName + " SET depth = ? WHERE ROWID = ?");
    if (queryStmt == nullptr || updateStmt == nullptr) {
        ServerLog::Error("Failed to prepare Full DB operator depth statements. table: ", tableName);
        return false;
    }
    auto result = queryStmt->ExecuteQuery();
    if (result == nullptr) {
        ServerLog::Error("Failed to query Full DB operator depth input. table: ", tableName,
            ", error: ", queryStmt->GetErrorMessage());
        return false;
    }

    std::vector<std::string> currentLane;
    bool hasCurrentLane = false;
    std::vector<SliceDomain> slices;
    auto persistLane = [&]() {
        OperatorDepthCalculator::AssignDepths(slices);
        for (const auto &slice : slices) {
            updateStmt->Reset();
            if (!updateStmt->Execute(slice.depth, slice.id)) {
                ServerLog::Error("Failed to update Full DB operator depth. table: ", tableName,
                    ", error: ", updateStmt->GetErrorMessage());
                return false;
            }
        }
        slices.clear();
        return true;
    };

    while (result->Next()) {
        std::vector<std::string> lane;
        lane.reserve(laneColumnCount);
        for (size_t i = 0; i < laneColumnCount; ++i) {
            lane.emplace_back(result->GetString(static_cast<int>(i)));
        }
        if (hasCurrentLane && lane != currentLane && !persistLane()) {
            return false;
        }
        currentLane = std::move(lane);
        hasCurrentLane = true;
        SliceDomain slice;
        slice.id = result->GetUint64(static_cast<int>(laneColumnCount));
        slice.timestamp = result->GetUint64(static_cast<int>(laneColumnCount + 1));
        slice.endTime = result->GetUint64(static_cast<int>(laneColumnCount + 2));
        slices.emplace_back(std::move(slice));
    }
    if (result->GetErrorCode() != SQLITE_DONE) {
        ServerLog::Error("Failed while reading Full DB operator depth input. table: ", tableName,
            ", error: ", result->GetErrorMessage());
        return false;
    }
    return !hasCurrentLane || persistLane();
}

bool ValidateTaskMstxDomainOwnership(FullDb::DbTraceDataBase &database) {
    if (!database.CheckTableExist(TABLE_TASK) || !database.CheckTableExist(TABLE_MSTX_EVENTS)) {
        return true;
    }
    const std::string querySql = "SELECT main.ROWID FROM " + TABLE_TASK + " AS main INNER JOIN " + TABLE_MSTX_EVENTS +
        " AS mstx ON main.connectionId = mstx.connectionId "
        "GROUP BY main.ROWID HAVING COUNT(DISTINCT mstx.domainId) > 1 LIMIT 1";
    auto stmt = database.CreatPreparedStatement(querySql);
    if (stmt == nullptr) {
        ServerLog::Error("Failed to prepare TASK MSTX domain ownership validation.");
        return false;
    }
    auto result = stmt->ExecuteQuery();
    if (result == nullptr) {
        ServerLog::Error("Failed to validate TASK MSTX domain ownership: ", stmt->GetErrorMessage());
        return false;
    }
    if (result->Next()) {
        ServerLog::Error(
            "A TASK row is linked to multiple MSTX domains. rowId: ", result->GetUint64(static_cast<int>(0)));
        return false;
    }
    return result->GetErrorCode() == SQLITE_DONE;
}

bool PersistTaskDepths(FullDb::DbTraceDataBase &database) {
    if (!database.CheckTableExist(TABLE_TASK)) {
        return true;
    }
    std::string ordinaryQuery = "SELECT deviceId, streamId, ROWID, startNs, endNs FROM " + TABLE_TASK;
    if (database.CheckTableExist(TABLE_MSTX_EVENTS)) {
        ordinaryQuery += " WHERE connectionId NOT IN (SELECT connectionId FROM " + TABLE_MSTX_EVENTS + ")";
    }
    ordinaryQuery += " ORDER BY deviceId, streamId, startNs, ROWID";
    if (!PersistPartitionedDepths(database, TABLE_TASK, ordinaryQuery, 2)) {
        return false;
    }
    if (!database.CheckTableExist(TABLE_MSTX_EVENTS)) {
        return true;
    }
    if (!ValidateTaskMstxDomainOwnership(database)) {
        return false;
    }
    const std::string mstxQuery = "SELECT DISTINCT main.deviceId, main.streamId, mstx.domainId, main.ROWID, "
                                  "main.startNs, main.endNs FROM " +
        TABLE_TASK + " AS main INNER JOIN " + TABLE_MSTX_EVENTS +
        " AS mstx ON main.connectionId = mstx.connectionId "
        "ORDER BY main.deviceId, main.streamId, mstx.domainId, main.startNs, main.ROWID";
    return PersistPartitionedDepths(database, TABLE_TASK, mstxQuery, 3);
}

bool PersistDbDepths(FullDb::DbTraceDataBase &database) {
    if (database.CheckTableExist(TABLE_CANN_API)) {
        const std::string querySql = "SELECT globalTid, type, ROWID, startNs, endNs FROM " + TABLE_CANN_API +
            " ORDER BY globalTid, type, startNs, ROWID";
        if (!PersistPartitionedDepths(database, TABLE_CANN_API, querySql, 2)) {
            return false;
        }
    }
    if (database.CheckTableExist(TABLE_API)) {
        const std::string ordinaryQuery = "SELECT globalTid, ROWID, startNs, endNs FROM " + TABLE_API +
            " WHERE type IS NULL OR type != 50003 ORDER BY globalTid, startNs, ROWID";
        const std::string pythonStackQuery = "SELECT globalTid, ROWID, startNs, endNs FROM " + TABLE_API +
            " WHERE type = 50003 ORDER BY globalTid, startNs, ROWID";
        if (!PersistPartitionedDepths(database, TABLE_API, ordinaryQuery, 1) ||
            !PersistPartitionedDepths(database, TABLE_API, pythonStackQuery, 1)) {
            return false;
        }
    }
    if (database.CheckTableExist(TABLE_MSTX_EVENTS)) {
        const std::string querySql = "SELECT globalTid, domainId, ROWID, startNs, endNs FROM " + TABLE_MSTX_EVENTS +
            " ORDER BY globalTid, domainId, startNs, ROWID";
        if (!PersistPartitionedDepths(database, TABLE_MSTX_EVENTS, querySql, 2)) {
            return false;
        }
    }
    if (database.CheckTableExist(TABLE_DPU_TASK)) {
        const std::string querySql = "SELECT globalTid, dpuDeviceId, streamId, ROWID, startNs, endNs FROM " +
            TABLE_DPU_TASK +
            " WHERE globalTid IS NOT NULL AND dpuDeviceId IS NOT NULL AND streamId IS NOT NULL"
            " ORDER BY globalTid, dpuDeviceId, streamId, startNs, ROWID";
        if (!PersistPartitionedDepths(database, TABLE_DPU_TASK, querySql, 3)) {
            return false;
        }
    }
    return PersistTaskDepths(database);
}

bool PersistTextLaneDepths(TextTraceDatabase &database, bool isPythonStack) {
    const std::string catFilter =
        isPythonStack ? "cat = 'python_function'" : "(cat IS NULL OR cat != 'python_function')";
    const std::string querySql = "SELECT track_id, id, timestamp, end_time, COALESCE(group_id, '') "
                                 "FROM slice WHERE " +
        catFilter + " ORDER BY track_id, timestamp, id";
    auto queryStmt = database.CreatPreparedStatement(querySql);
    auto updateStmt = database.CreatPreparedStatement("UPDATE slice SET depth = ? WHERE id = ?");
    if (queryStmt == nullptr || updateStmt == nullptr) {
        ServerLog::Error("Failed to prepare Text operator depth statements.");
        return false;
    }
    auto result = queryStmt->ExecuteQuery();
    if (result == nullptr) {
        ServerLog::Error("Failed to query Text operator depth input: ", queryStmt->GetErrorMessage());
        return false;
    }

    uint64_t currentTrackId = 0;
    bool hasCurrentTrack = false;
    std::vector<SliceDomain> slices;
    auto persistTrack = [&]() {
        OperatorDepthCalculator::AssignDepths(slices);
        for (const auto &slice : slices) {
            updateStmt->Reset();
            if (!updateStmt->Execute(slice.depth, slice.id)) {
                ServerLog::Error("Failed to update Text operator depth: ", updateStmt->GetErrorMessage());
                return false;
            }
        }
        slices.clear();
        return true;
    };

    while (result->Next()) {
        const uint64_t trackId = result->GetUint64(static_cast<int>(0));
        if (hasCurrentTrack && trackId != currentTrackId && !persistTrack()) {
            return false;
        }
        currentTrackId = trackId;
        hasCurrentTrack = true;
        SliceDomain slice;
        slice.id = result->GetUint64(1);
        slice.timestamp = result->GetUint64(2);
        slice.endTime = result->GetUint64(3);
        slice.groupId = result->GetString(4);
        slices.emplace_back(std::move(slice));
    }
    if (result->GetErrorCode() != SQLITE_DONE) {
        ServerLog::Error("Failed while reading Text operator depth input: ", result->GetErrorMessage());
        return false;
    }
    return !hasCurrentTrack || persistTrack();
}

bool PersistTextDepths(TextTraceDatabase &database) {
    if (!database.CheckTableExist(TEXT_SLICE_TABLE)) {
        return true;
    }
    return PersistTextLaneDepths(database, false) && PersistTextLaneDepths(database, true);
}
}

bool OperatorDepthPersistenceService::RunWithStatusAndTransaction(Database &database, const PersistenceWork &work) {
    if (database.CheckValueFromStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS)) {
        return true;
    }
    if (!database.UpdateValueIntoStatusInfoTable(OPERATOR_DEPTH, NOT_FINISH_STATUS)) {
        ServerLog::Error("Failed to mark operator depth as not finished.");
        return false;
    }
    if (!database.StartTransaction()) {
        ServerLog::Error("Failed to start operator depth transaction.");
        return false;
    }

    bool success = false;
    try {
        success = work();
    } catch (const std::exception &error) {
        ServerLog::Error("Failed to persist operator depth: ", error.what());
    } catch (...) {
        ServerLog::Error("Failed to persist operator depth due to an unknown error.");
    }

    if (!success) {
        if (!database.RollbackTransaction()) {
            ServerLog::Error("Failed to roll back operator depth transaction.");
        }
        return false;
    }
    if (!database.EndTransaction()) {
        ServerLog::Error("Failed to commit operator depth transaction.");
        if (!database.RollbackTransaction()) {
            ServerLog::Error("Failed to roll back operator depth transaction after commit failure.");
        }
        return false;
    }
    if (!database.UpdateValueIntoStatusInfoTable(OPERATOR_DEPTH, FINISH_STATUS)) {
        ServerLog::Error("Failed to mark operator depth as finished.");
        return false;
    }
    return true;
}

bool OperatorDepthPersistenceService::CalculateAndPersistTextDepth(
    TextTraceDatabase &database, const std::string &rankId) {
    if (!database.EnsureSliceGroupIdColumn()) {
        ServerLog::Error("Failed to prepare Text slice schema for operator depth. rankId: ", rankId);
        return false;
    }
    return RunWithStatusAndTransaction(database, [&database, &rankId]() {
        if (!PersistTextDepths(database)) {
            ServerLog::Error("Failed to persist Text operator depth. rankId: ", rankId);
            return false;
        }
        return true;
    });
}

bool OperatorDepthPersistenceService::CalculateAndPersistDbDepth(
    FullDb::DbTraceDataBase &database, const std::string &dbId) {
    return RunWithStatusAndTransaction(database, [&database, &dbId]() {
        if (!PersistDbDepths(database)) {
            ServerLog::Error("Failed to persist Full DB operator depth. dbId: ", dbId);
            return false;
        }
        return true;
    });
}
}
