/* Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2: http://license.coscl.org.cn/MulanPSL2
 */
#include "MemSnapshotFrameTreeService.h"
#include "MemSnapshotSliceService.h"
#include "JsonUtil.h"
#include "FileUtil.h"
#include "PythonUtil.h"
#include "istreamwrapper.h"
#include <fstream>
#include <random>
#include <mutex>
#include <stdexcept>
#include <atomic>
#include <functional>

namespace Dic::Module::MemSnapshot {
namespace {
constexpr size_t MAX_TREE_NODES = 200000;
constexpr size_t MAX_RESULT_BYTES = 256 * 1024 * 1024;
constexpr size_t MAX_FRAME_BYTES = 1024 * 1024;
std::mutex cacheMutex;
std::mutex queryMutex;
std::atomic<uint64_t> queryVersion{0};
std::optional<uint32_t> latestRequestId;
fs::path activeCancelPath;

// 仅发送通知；Python 自行中断 SQLite、计算及输出，不强杀子进程。
// 调用方持有 cacheMutex，与临时目录销毁互斥。
void SignalCancellation() {
    if (!activeCancelPath.empty()) {
        std::ofstream marker(activeCancelPath, std::ios::binary);
        if (!marker) {
            throw std::runtime_error("Cannot notify Python frame tree cancellation");
        }
    }
}
std::string cachedKey;
std::shared_ptr<const SnapshotFrameTree> cachedTree;

std::string Fingerprint(const std::string &path, bool required) {
    std::uintmax_t size = 0;
    uint64_t time = 0;
    if (!MemSnapshotSliceService::GetFileFingerprint(path, size, time)) {
        if (required) {
            throw std::runtime_error("Frame tree dataset member is missing");
        }
        return "absent;";
    }
    // 只读打开 WAL 数据库也可能创建空旁路文件；空文件和不存在都没有事务内容。
    if (!required && size == 0) {
        return "absent;";
    }
    return std::to_string(size) + ":" + std::to_string(time) + ";";
}

struct Sources {
    std::string dataKey;
    std::vector<std::string> allPaths;
    std::string manifestFingerprint;
    std::string Generation() const {
        if (manifestFingerprint != Fingerprint(MemSnapshotSliceService::GetManifestPath(dataKey), true)) {
            throw std::runtime_error("Frame tree manifest changed; restart loading");
        }
        std::string key = dataKey + "|" + manifestFingerprint;
        for (const auto &path : allPaths) {
            key += "|" + path + ":" + Fingerprint(path, true) + Fingerprint(path + "-wal", false) +
                Fingerprint(path + "-journal", false);
        }
        return key;
    }
};

Sources Resolve(const std::string &dataKey, const std::string &deviceId, int64_t eventId) {
    if (deviceId.empty() || deviceId.size() > 20 || deviceId.find_first_not_of("0123456789") != std::string::npos ||
        (deviceId.size() > 1 && deviceId.front() == '0') || eventId < 0 || eventId > 9007199254740991LL) {
        throw std::runtime_error("Invalid frame tree device or event ID");
    }
    const auto manifestFingerprint = Fingerprint(MemSnapshotSliceService::GetManifestPath(dataKey), true);
    const auto manifest = MemSnapshotSliceService::LoadManifest(dataKey);
    if (!manifest.has_value() || !manifest->IsComplete()) {
        throw std::runtime_error("Frame tree requires a completely parsed snapshot dataset");
    }
    const auto device = manifest->devices.find(deviceId);
    if (device == manifest->devices.end() || eventId >= device->second.eventCount) {
        throw std::runtime_error("Frame tree device or event does not exist");
    }
    Sources result{dataKey, {}, manifestFingerprint};
    for (const auto &entry : manifest->devices) {
        for (const auto &slice : entry.second.slices) {
            const auto path = MemSnapshotSliceService::ResolveSliceDbPath(dataKey, slice);
            if (!slice.ready || path.empty()) {
                throw std::runtime_error("Frame tree dataset member is not ready or unsafe");
            }
            result.allPaths.push_back(path);
        }
    }
    return result;
}

class QueryOutput {
  public:
    QueryOutput() {
        // 临时结果隔离于解析产物，目录由本次调用独占。
        std::random_device random;
        const auto temp = fs::temp_directory_path();
        for (int attempt = 0; attempt < 10; ++attempt) {
            const auto candidate = temp /
                ("insight-frame-tree-" + std::to_string(random()) + "-" + std::to_string(random()) + "-" +
                    std::to_string(random()));
            if (fs::create_directory(candidate)) {
                directory = candidate;
#ifndef _WIN32
                std::error_code error;
                fs::permissions(directory, fs::perms::owner_all, error);
                if (error) {
                    fs::remove(directory, error);
                    throw std::runtime_error("Cannot secure frame tree temporary directory");
                }
#endif
                return;
            }
        }
        throw std::runtime_error("Cannot create frame tree temporary directory");
    }
    ~QueryOutput() {
        std::lock_guard<std::mutex> lock(cacheMutex);
        if (activeCancelPath == CancelPath()) {
            activeCancelPath.clear();
        }
        std::error_code error;
        fs::remove_all(directory, error);
    }
    fs::path Path() const { return directory / "result.json"; }
    fs::path CancelPath() const { return directory / "cancel"; }
    QueryOutput(const QueryOutput &) = delete;
    QueryOutput &operator=(const QueryOutput &) = delete;

  private:
    fs::path directory;
};

const rapidjson::Value &Member(const rapidjson::Value &object, const char *name) {
    if (!object.IsObject() || !object.HasMember(name)) {
        throw std::runtime_error("Incomplete Python frame tree result");
    }
    return object[name];
}

std::string Text(const rapidjson::Value &value) {
    if (!value.IsString() || value.GetStringLength() > MAX_FRAME_BYTES) {
        throw std::runtime_error("Invalid Python frame tree text");
    }
    return {value.GetString(), value.GetStringLength()};
}

uint64_t Unsigned(const rapidjson::Value &value) {
    if (!value.IsUint64()) {
        throw std::runtime_error("Invalid Python frame tree metric");
    }
    return value.GetUint64();
}

SnapshotFrameMetrics Metrics(const rapidjson::Value &value) {
    if (!value.IsArray() || value.Size() != 3) {
        throw std::runtime_error("Invalid Python frame tree metrics");
    }
    return {Unsigned(value[0]), Unsigned(value[1]), Unsigned(value[2])};
}

std::shared_ptr<SnapshotFrameTree> ReadResult(const fs::path &path, const std::function<bool()> &cancelled) {
    if (!fs::is_regular_file(path) || fs::file_size(path) > MAX_RESULT_BYTES) {
        throw std::runtime_error("Missing or oversized Python frame tree result");
    }
    std::ifstream input(path, std::ios::binary);
    rapidjson::IStreamWrapper stream(input);
    rapidjson::Document json;
    json.ParseStream<rapidjson::kParseIterativeFlag | rapidjson::kParseValidateEncodingFlag>(stream);
    if (!input.is_open() || input.bad() || json.HasParseError() || !json.IsObject() ||
        !Member(json, "version").IsInt() || json["version"].GetInt() != 1) {
        throw std::runtime_error("Invalid Python frame tree result");
    }
    if (json.HasMember("error")) {
        throw std::runtime_error(Text(json["error"]));
    }
    const auto &nodes = Member(json, "nodes");
    if (!nodes.IsArray() || nodes.Empty() || nodes.Size() > MAX_TREE_NODES) {
        throw std::runtime_error("Invalid Python frame tree nodes");
    }
    auto tree = std::make_shared<SnapshotFrameTree>();
    tree->nodes.reserve(nodes.Size());
    for (const auto &value : nodes.GetArray()) {
        if (cancelled()) {
            throw std::runtime_error("Frame tree query cancelled by a newer request or reset");
        }
        SnapshotFrameNode node;
        node.nodeId = Text(Member(value, "nodeId"));
        const auto &parent = Member(value, "parentId");
        if (!parent.IsNull()) {
            node.parentId = Text(parent);
        }
        node.depth = Unsigned(Member(value, "depth"));
        node.kind = Text(Member(value, "kind"));
        node.childCount = Unsigned(Member(value, "childCount"));
        node.total = Metrics(Member(value, "total"));
        node.self = Metrics(Member(value, "self"));
        const auto &frame = Member(value, "frame");
        if (!frame.IsNull()) {
            const auto &line = Member(frame, "line");
            if (!line.IsInt64()) {
                throw std::runtime_error("Invalid Python frame tree line");
            }
            node.frame = SnapshotFrame{Text(Member(frame, "filename")), line.GetInt64(), Text(Member(frame, "name"))};
        }
        tree->nodes.push_back(std::move(node));
    }
    const auto &coverage = Member(json, "coverage");
    if (!coverage.IsArray() || coverage.Size() != 3) {
        throw std::runtime_error("Invalid Python frame tree coverage");
    }
    tree->dynamicBlocks = Unsigned(coverage[0]);
    tree->resolvedBlocks = Unsigned(coverage[1]);
    tree->orderedFrameBlocks = Unsigned(coverage[2]);
    return tree;
}
}

std::shared_ptr<const SnapshotFrameTree> MemSnapshotFrameTreeService::Query(const std::string &dataKey,
    const std::string &deviceId, int64_t eventId, bool includeStatic, std::optional<uint32_t> requestId) {
    uint64_t version;
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        if (requestId && latestRequestId != requestId) {
            throw std::runtime_error("Frame tree query cancelled by a newer request");
        }
        version = requestId ? queryVersion.load() : ++queryVersion;
        if (!requestId) {
            SignalCancellation();
        }
    }
    const auto cancelled = [version]() { return queryVersion.load() != version; };
    const auto checkCancelled = [&cancelled]() {
        if (cancelled()) {
            throw std::runtime_error("Frame tree query cancelled by a newer request or reset");
        }
    };
    // 等待旧 Python 协作退出后才开始新计算，避免并行占用大量内存。
    std::lock_guard<std::mutex> queryLock(queryMutex);
    checkCancelled();
    auto sources = Resolve(dataKey, deviceId, eventId);
    const auto generation = sources.Generation();
    const auto key = generation + "|python-frame-tree-v1|" + deviceId + "|" + std::to_string(eventId) +
        (includeStatic ? "|static" : "|dynamic");
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        checkCancelled();
        if (cachedTree && cachedKey == key) {
            return cachedTree;
        }
    }
    QueryOutput output;
    const auto resultPath = output.Path();
    // 一次计算完整树，后续分页只读缓存。替换工具仓时只需适配 Python 查询入口。
    checkCancelled();
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        checkCancelled();
        activeCancelPath = output.CancelPath();
    }
    const auto exitCode = PythonUtil::ExecuteScript(FileUtil::SplicePath("mem_snap_dump", "tools", "frame_tree.py"),
        {"--manifest", MemSnapshotSliceService::GetManifestPath(dataKey), "--device", deviceId, "--event",
            std::to_string(eventId), "--include-static", includeStatic ? "1" : "0", "--cancel-file",
            output.CancelPath().u8string(), "--output", resultPath.u8string()},
        std::nullopt);
    checkCancelled();
    auto result = ReadResult(resultPath, cancelled);
    if (exitCode != 0) {
        throw std::runtime_error("Python frame tree query failed");
    }
    if (generation != sources.Generation()) {
        throw std::runtime_error("Frame tree dataset changed; restart loading");
    }
    {
        std::lock_guard<std::mutex> lock(cacheMutex);
        checkCancelled();
        cachedKey = key;
        cachedTree = result;
    }
    return result;
}

void MemSnapshotFrameTreeService::NotifyRequest(uint32_t requestId) {
    std::lock_guard<std::mutex> lock(cacheMutex);
    latestRequestId = requestId;
    ++queryVersion;
    SignalCancellation();
}

void MemSnapshotFrameTreeService::ClearCache() {
    std::lock_guard<std::mutex> lock(cacheMutex);
    ++queryVersion;
    SignalCancellation();
    latestRequestId.reset();
    cachedTree.reset();
    cachedKey.clear();
}
}
