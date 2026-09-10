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

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <optional>
#include <system_error>
#include <unordered_map>

#include "FileUtil.h"
#include "JsonUtil.h"
#include "MemSnapshotSliceService.h"

namespace Dic::Module::MemSnapshot {
namespace {
bool ReadSliceInfo(const json_t &json, MemSnapshotSliceInfo &slice) {
    if (!json.IsObject() || !json.HasMember("index") || !json["index"].IsInt() || !json.HasMember("startEventId") ||
        !json["startEventId"].IsInt64() || !json.HasMember("endEventId") || !json["endEventId"].IsInt64() ||
        !json.HasMember("file") || !json["file"].IsString() || !json.HasMember("ready") || !json["ready"].IsBool()) {
        return false;
    }
    slice.index = json["index"].GetInt();
    slice.startEventId = json["startEventId"].GetInt64();
    slice.endEventId = json["endEventId"].GetInt64();
    slice.file = json["file"].GetString();
    slice.ready = json["ready"].GetBool();
    return slice.index >= 0 && slice.startEventId >= 0 && slice.endEventId >= slice.startEventId && !slice.file.empty();
}

bool ReadDeviceInfo(const json_t &json, MemSnapshotDeviceSliceInfo &deviceInfo) {
    if (!json.IsObject() || !json.HasMember("eventCount") || !json["eventCount"].IsInt64() ||
        !json.HasMember("sliceCount") || !json["sliceCount"].IsInt() || !json.HasMember("readySlices") ||
        !json["readySlices"].IsArray() || !json.HasMember("slices") || !json["slices"].IsArray()) {
        return false;
    }
    deviceInfo.eventCount = json["eventCount"].GetInt64();
    deviceInfo.sliceCount = json["sliceCount"].GetInt();
    for (const auto &item : json["readySlices"].GetArray()) {
        if (!item.IsInt()) {
            return false;
        }
        deviceInfo.readySlices.push_back(item.GetInt());
    }
    for (const auto &item : json["slices"].GetArray()) {
        MemSnapshotSliceInfo slice;
        if (!ReadSliceInfo(item, slice)) {
            return false;
        }
        deviceInfo.slices.emplace_back(std::move(slice));
    }
    if (deviceInfo.eventCount <= 0 || deviceInfo.sliceCount <= 0 ||
        static_cast<size_t>(deviceInfo.sliceCount) != deviceInfo.slices.size()) {
        return false;
    }
    std::vector<bool> declaredReadySlices(static_cast<size_t>(deviceInfo.sliceCount), false);
    for (const int sliceIndex : deviceInfo.readySlices) {
        if (sliceIndex < 0 || sliceIndex >= deviceInfo.sliceCount ||
            declaredReadySlices[static_cast<size_t>(sliceIndex)]) {
            return false;
        }
        declaredReadySlices[static_cast<size_t>(sliceIndex)] = true;
    }
    for (size_t index = 0; index < deviceInfo.slices.size(); ++index) {
        if (deviceInfo.slices[index].index != static_cast<int>(index) ||
            deviceInfo.slices[index].ready != declaredReadySlices[index]) {
            return false;
        }
    }
    return true;
}

bool IsDeviceLayoutValid(const MemSnapshotDeviceSliceInfo &deviceInfo, int64_t eventsPerSlice) {
    int64_t expectedStartEventId = 0;
    std::unordered_set<std::string> sliceFiles;
    for (const auto &slice : deviceInfo.slices) {
        const int64_t eventCount = slice.endEventId - slice.startEventId + 1;
        if (slice.startEventId != expectedStartEventId || eventCount <= 0 || eventCount > eventsPerSlice ||
            !sliceFiles.insert(slice.file).second) {
            return false;
        }
        expectedStartEventId = slice.endEventId + 1;
    }
    return expectedStartEventId == deviceInfo.eventCount;
}

struct ManifestCacheEntry {
    std::uintmax_t fileSize{0};
    fs::file_time_type writeTime{};
    bool exists{false};
    std::optional<MemSnapshotSliceManifest> manifest;
};

bool IsAbsoluteSliceFile(const std::string &file) {
    if (file.empty()) {
        return true;
    }
    if (file.front() == '/' || file.front() == '\\') {
        return true;
    }
    return file.size() >= 2 && file[1] == ':';
}

bool ContainsParentDirectory(const std::string &file) {
    size_t start = 0;
    while (start <= file.size()) {
        const size_t end = std::min(file.find_first_of("/\\", start), file.size());
        if (file.compare(start, end - start, "..") == 0) {
            return true;
        }
        if (end == file.size()) {
            break;
        }
        start = end + 1;
    }
    return false;
}

std::mutex g_manifestCacheMutex;
std::unordered_map<std::string, ManifestCacheEntry> g_manifestCache;

std::optional<MemSnapshotSliceManifest> ParseManifestFile(const std::string &manifestPath) {
    const auto json = JsonUtil::ReadJsonFromFile(manifestPath);
    if (!json.IsObject() || !json.HasMember("schemaVersion") || !json["schemaVersion"].IsInt() ||
        !json.HasMember("status") || !json["status"].IsString() || !json.HasMember("sourceFile") ||
        !json["sourceFile"].IsString() || !json.HasMember("cacheHash") || !json["cacheHash"].IsString() ||
        !json.HasMember("eventsPerSlice") || !json["eventsPerSlice"].IsInt64() || !json.HasMember("devices") ||
        !json["devices"].IsObject()) {
        return std::nullopt;
    }
    MemSnapshotSliceManifest manifest;
    manifest.schemaVersion = json["schemaVersion"].GetInt();
    manifest.status = json["status"].GetString();
    manifest.sourceFile = json["sourceFile"].GetString();
    manifest.cacheHash = json["cacheHash"].GetString();
    manifest.eventsPerSlice = json["eventsPerSlice"].GetInt64();
    const auto &devicesJson = json["devices"];
    for (auto device = devicesJson.MemberBegin(); device != devicesJson.MemberEnd(); ++device) {
        MemSnapshotDeviceSliceInfo info;
        if (!ReadDeviceInfo(device->value, info)) {
            return std::nullopt;
        }
        manifest.devices.emplace(device->name.GetString(), std::move(info));
    }
    if (std::any_of(manifest.devices.begin(), manifest.devices.end(),
            [&manifest](const auto &item) { return !IsDeviceLayoutValid(item.second, manifest.eventsPerSlice); })) {
        return std::nullopt;
    }
    return manifest.IsValid() ? std::make_optional(std::move(manifest)) : std::nullopt;
}
} // namespace

bool MemSnapshotSliceManifest::IsValid() const {
    return schemaVersion == MEM_SNAPSHOT_SLICE_MANIFEST_SCHEMA_VERSION && !sourceFile.empty() && eventsPerSlice > 0 &&
        !devices.empty() && (status == "building" || status == "complete");
}

bool MemSnapshotSliceManifest::IsComplete() const { return IsValid() && status == "complete"; }

std::string MemSnapshotSliceService::GetArtifactDirectory(const std::string &snapshotPath) {
    return snapshotPath + std::string(MEM_SNAPSHOT_ARTIFACT_DIR_SUFFIX);
}

std::string MemSnapshotSliceService::GetManifestPath(const std::string &snapshotPath) {
    return FileUtil::SplicePath(GetArtifactDirectory(snapshotPath), std::string(MEM_SNAPSHOT_MANIFEST_FILE_NAME));
}

std::string MemSnapshotSliceService::GetLogPath(const std::string &snapshotPath) {
    return FileUtil::SplicePath(GetArtifactDirectory(snapshotPath), "parse.log");
}

std::optional<MemSnapshotSliceManifest> MemSnapshotSliceService::LoadManifest(const std::string &snapshotPath) {
    const auto manifestPath = GetManifestPath(snapshotPath);
    std::error_code error;
    const bool exists = FileUtil::CheckFilePathExist(manifestPath);
    std::uintmax_t fileSize = 0;
    fs::file_time_type writeTime{};
    if (exists) {
        fileSize = fs::file_size(manifestPath, error);
        if (error) {
            fileSize = 0;
            error.clear();
        }
        writeTime = fs::last_write_time(manifestPath, error);
        if (error) {
            writeTime = {};
        }
    }
    {
        std::lock_guard<std::mutex> lock(g_manifestCacheMutex);
        const auto cached = g_manifestCache.find(snapshotPath);
        if (cached != g_manifestCache.end() && cached->second.exists == exists && cached->second.fileSize == fileSize &&
            cached->second.writeTime == writeTime) {
            return cached->second.manifest;
        }
    }
    std::optional<MemSnapshotSliceManifest> manifest;
    if (exists) {
        manifest = ParseManifestFile(manifestPath);
    }
    {
        std::lock_guard<std::mutex> lock(g_manifestCacheMutex);
        g_manifestCache[snapshotPath] = ManifestCacheEntry{fileSize, writeTime, exists, manifest};
    }
    return manifest;
}

std::optional<MemSnapshotSliceInfo> MemSnapshotSliceService::ResolveSlice(
    const MemSnapshotSliceManifest &manifest, const std::string &deviceId, int requestedSliceIndex) {
    const auto deviceIt = manifest.devices.find(deviceId);
    if (deviceIt == manifest.devices.end()) {
        return std::nullopt;
    }
    const auto &device = deviceIt->second;
    int sliceIndex = requestedSliceIndex;
    if (sliceIndex < 0) {
        if (device.readySlices.empty()) {
            return std::nullopt;
        }
        // 倒序解析时最先可用的是最右侧窗口，默认选择最新的就绪分片。
        sliceIndex = *std::max_element(device.readySlices.begin(), device.readySlices.end());
    }
    if (sliceIndex < 0 || sliceIndex >= device.sliceCount) {
        return std::nullopt;
    }
    const auto &slice = device.slices[static_cast<size_t>(sliceIndex)];
    return slice.ready ? std::make_optional(slice) : std::nullopt;
}

std::optional<MemSnapshotSliceInfo> MemSnapshotSliceService::ResolveSliceByEventId(
    const MemSnapshotSliceManifest &manifest, const std::string &deviceId, int64_t eventId) {
    if (eventId < 0) {
        return std::nullopt;
    }
    const auto deviceIt = manifest.devices.find(deviceId);
    if (deviceIt == manifest.devices.end()) {
        return std::nullopt;
    }
    for (const auto &slice : deviceIt->second.slices) {
        if (slice.ready && eventId >= slice.startEventId && eventId <= slice.endEventId) {
            return slice;
        }
    }
    return std::nullopt;
}

std::string MemSnapshotSliceService::ResolveSliceDbPath(
    const std::string &snapshotPath, const MemSnapshotSliceInfo &sliceInfo) {
    if (IsAbsoluteSliceFile(sliceInfo.file) || ContainsParentDirectory(sliceInfo.file)) {
        return "";
    }
    return FileUtil::SplicePath(GetArtifactDirectory(snapshotPath), sliceInfo.file);
}

std::string MemSnapshotSliceService::BuildDatabaseKey(
    const std::string &snapshotPath, const std::string &deviceId, int sliceIndex) {
    return snapshotPath + "#device=" + deviceId + "#slice=" + std::to_string(sliceIndex);
}
} // namespace Dic::Module::MemSnapshot
