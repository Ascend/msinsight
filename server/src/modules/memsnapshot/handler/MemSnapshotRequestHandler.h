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

#ifndef PROFILER_SERVER_MEM_SNAPSHOT_REQUEST_HANDLER_H
#define PROFILER_SERVER_MEM_SNAPSHOT_REQUEST_HANDLER_H

#include "DataBaseManager.h"
#include "pch.h"
#include "ModuleRequestHandler.h"
#include "ProtocolDefs.h"
#include "NumberUtil.h"
#include "MemSnapshotProtocolRequest.h"
#include "MemSnapshotProtocolResponse.h"
#include "MemSnapshotDatabase.h"
#include "MemSnapshotSliceService.h"

namespace Dic::Module::MemSnapshot {
struct MemSnapshotResolvedRequest {
    std::shared_ptr<FullDb::MemSnapshotDatabase> database;
    std::optional<MemSnapshotSliceInfo> slice;
};

class MemSnapshotRequestHandler : public ModuleRequestHandler {
  public:
    MemSnapshotRequestHandler() {
        moduleName = MODULE_MEM_SCOPE; // Snapshot与MemScope为同一模块下的两种类型，为了方便处理，这里统一为MemScope
        async = false;
    }

    ~MemSnapshotRequestHandler() override = default;

  protected:
    inline static const std::string REQUEST_ERROR_UNKNOWN = "An unknown exception occurred while querying data. "
                                                            "Please check whether your data contains anomalies or "
                                                            "review the logs for more information.";

    static inline std::string GetMemSnapshotDataKey(const Protocol::Request &request) {
        return request.fileId.empty() ? request.projectName : request.fileId;
    }

    static inline uint64_t ToNonNegativeEventId(int64_t eventId) {
        return eventId < 0 ? 0 : static_cast<uint64_t>(eventId);
    }

    static inline MemSnapshotResolvedRequest ResolveMemSnapshotRequest(
        const Protocol::Request &request, const std::string &deviceId, int sliceIndex) {
        MemSnapshotResolvedRequest resolved;
        const auto dataKey = GetMemSnapshotDataKey(request);
        const auto manifest = MemSnapshotSliceService::LoadManifest(dataKey);
        if (!manifest.has_value()) {
            resolved.database = Timeline::DataBaseManager::Instance().GetMemSnapshotDatabase(dataKey);
            return resolved;
        }
        resolved.slice = MemSnapshotSliceService::ResolveSlice(manifest.value(), deviceId, sliceIndex);
        if (!resolved.slice.has_value()) {
            return resolved;
        }
        const auto dbPath = MemSnapshotSliceService::ResolveSliceDbPath(dataKey, resolved.slice.value());
        if (dbPath.empty()) {
            resolved.slice.reset();
            return resolved;
        }
        const auto dbKey = MemSnapshotSliceService::BuildDatabaseKey(dataKey, deviceId, resolved.slice->index);
        resolved.database = Timeline::DataBaseManager::Instance().GetMemSnapshotDatabase(dbKey);
        if (resolved.database != nullptr && !resolved.database->IsOpen() &&
            !resolved.database->OpenDbReadOnly(dbPath)) {
            resolved.database = nullptr;
        }
        return resolved;
    }

    static inline bool HasOpenMemSnapshotDatabase(const MemSnapshotResolvedRequest &resolved) {
        return resolved.database != nullptr && resolved.database->IsOpen();
    }

    // Manifest 尚未写出或目标分窗未就绪时，查询早于解析，不应报成连库失败。
    static inline bool IsMemSnapshotQueryPending(const MemSnapshotResolvedRequest &resolved) {
        return !HasOpenMemSnapshotDatabase(resolved) && !resolved.slice.has_value();
    }

    static inline std::shared_ptr<FullDb::MemSnapshotDatabase> GetMemSnapshotDatabaseByRequest(
        const Protocol::Request &request, const std::string &deviceId, int sliceIndex) {
        return ResolveMemSnapshotRequest(request, deviceId, sliceIndex).database;
    }

    static inline std::optional<MemSnapshotSliceInfo> GetMemSnapshotSliceByRequest(
        const Protocol::Request &request, const std::string &deviceId, int sliceIndex) {
        const auto manifest = MemSnapshotSliceService::LoadManifest(GetMemSnapshotDataKey(request));
        return manifest.has_value() ? MemSnapshotSliceService::ResolveSlice(manifest.value(), deviceId, sliceIndex)
                                    : std::nullopt;
    }

    static inline std::string GetMemSnapshotStateCacheKey(
        const Protocol::Request &request, const std::string &deviceId, int sliceIndex) {
        const auto dataKey = GetMemSnapshotDataKey(request);
        const auto slice = GetMemSnapshotSliceByRequest(request, deviceId, sliceIndex);
        return slice.has_value() ? dataKey + "#slice=" + std::to_string(slice->index) : dataKey;
    }

    static inline std::optional<TraceEntry> QueryTraceEntryByIdAcrossSlices(const Protocol::Request &request,
        const std::string &deviceId, int currentSliceIndex, int64_t eventId,
        const std::shared_ptr<FullDb::MemSnapshotDatabase> &currentDatabase) {
        if (currentDatabase != nullptr && currentDatabase->IsOpen()) {
            if (auto entry = currentDatabase->QueryTraceEntryById(eventId, deviceId); entry.has_value()) {
                return entry;
            }
        }
        if (eventId < 0) {
            return std::nullopt;
        }
        const auto manifest = MemSnapshotSliceService::LoadManifest(GetMemSnapshotDataKey(request));
        if (!manifest.has_value()) {
            return std::nullopt;
        }
        const auto slice = MemSnapshotSliceService::ResolveSliceByEventId(manifest.value(), deviceId, eventId);
        if (!slice.has_value() || slice->index == currentSliceIndex) {
            return std::nullopt;
        }
        const auto database = GetMemSnapshotDatabaseByRequest(request, deviceId, slice->index);
        if (database == nullptr || !database->IsOpen()) {
            return std::nullopt;
        }
        return database->QueryTraceEntryById(eventId, deviceId);
    }
};
} // namespace Dic::Module::MemSnapshot
#endif // PROFILER_SERVER_MEM_SNAPSHOT_REQUEST_HANDLER_H
