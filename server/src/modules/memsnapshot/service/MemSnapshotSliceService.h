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

#ifndef PROFILER_SERVER_MEM_SNAPSHOT_SLICE_SERVICE_H
#define PROFILER_SERVER_MEM_SNAPSHOT_SLICE_SERVICE_H

#include "pch.h"

namespace Dic::Module::MemSnapshot {
constexpr int MEM_SNAPSHOT_SLICE_MANIFEST_SCHEMA_VERSION = 1;
constexpr std::string_view MEM_SNAPSHOT_ARTIFACT_DIR_SUFFIX = ".msinsight";
constexpr std::string_view MEM_SNAPSHOT_MANIFEST_FILE_NAME = "manifest.json";

struct MemSnapshotSliceInfo {
    int index{-1};
    int64_t startEventId{0};
    int64_t endEventId{-1};
    std::string file;
    bool ready{false};
};

struct MemSnapshotDeviceSliceInfo {
    int64_t eventCount{0};
    int sliceCount{0};
    std::vector<int> readySlices;
    std::vector<MemSnapshotSliceInfo> slices;
};

struct MemSnapshotSliceManifest {
    int schemaVersion{0};
    std::string status;
    std::string sourceFile;
    std::string cacheHash;
    int64_t eventsPerSlice{0};
    std::unordered_map<std::string, MemSnapshotDeviceSliceInfo> devices;

    [[nodiscard]] bool IsValid() const;
    [[nodiscard]] bool IsComplete() const;
};

class MemSnapshotSliceService {
  public:
    static std::string GetArtifactDirectory(const std::string &snapshotPath);
    static std::string GetManifestPath(const std::string &snapshotPath);
    static std::string GetLogPath(const std::string &snapshotPath);
    static std::optional<MemSnapshotSliceManifest> LoadManifest(const std::string &snapshotPath);
    static std::optional<MemSnapshotSliceInfo> ResolveSlice(
        const MemSnapshotSliceManifest &manifest, const std::string &deviceId, int requestedSliceIndex);
    static std::optional<MemSnapshotSliceInfo> ResolveSliceByEventId(
        const MemSnapshotSliceManifest &manifest, const std::string &deviceId, int64_t eventId);
    static std::string ResolveSliceDbPath(const std::string &snapshotPath, const MemSnapshotSliceInfo &sliceInfo);
    static std::string BuildDatabaseKey(const std::string &snapshotPath, const std::string &deviceId, int sliceIndex);
};
} // namespace Dic::Module::MemSnapshot

#endif // PROFILER_SERVER_MEM_SNAPSHOT_SLICE_SERVICE_H
