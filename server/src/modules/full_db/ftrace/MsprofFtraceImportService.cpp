/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#include "MsprofFtraceImportService.h"
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include "sqlite3.h"
#include "MsprofFtraceReader.h"
#include "MsprofFtraceBuilder.h"
#include "MsprofFtraceWriter.h"
#include "MsprofFtraceCacheSchema.h"
#include "DataBaseManager.h"
#include "BaselineManager.h"
#include "SystemMemoryDatabaseDef.h"
#include "OperatorDepthPersistenceService.h"
#include "ParseUnitManager.h"
#include "ParserStatusManager.h"
#include "ProjectParserFactory.h"
#include "TrackInfoManager.h"
#include "TraceTime.h"
#include "ThreadPool.h"
#include "ConstantDefs.h"
#include "ServerLog.h"
#include "FileUtil.h"
#include "StringUtil.h"
#include "SystemUtil.h"

namespace Dic::Module::FullDb {
using namespace Timeline;
namespace {
// Change this whenever the adapter's persisted semantics change.
constexpr const char *ADAPTER_VERSION = "msprof-ftrace-v8";
constexpr size_t MAX_RETAINED_SOURCES = 4;
using Sqlite = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;

std::filesystem::path CacheDirectory() {
#ifdef _WIN32
    const auto process = GetCurrentProcessId();
#else
    const auto process = getpid();
#endif
    // Separate server processes must not retire one another's active SQLite generations.
    return std::filesystem::temp_directory_path() / "mindstudio-msprof-ftrace" / std::to_string(process);
}

// Two independent streaming hashes. Values include SQLite storage type and byte length;
// unrelated FullDb helper/status writes do not invalidate the derived cache.
class ContentHash {
  public:
    void Bytes(const void *data, size_t size) {
        auto bytes = static_cast<const unsigned char *>(data);
        for (size_t i = 0; i < size; ++i) {
            first = (first ^ bytes[i]) * 1099511628211ULL;
            second = (second ^ bytes[i]) * 14029467366897019727ULL;
        }
    }
    void Text(const std::string &value) {
        uint64_t size = value.size();
        Bytes(&size, sizeof(size));
        Bytes(value.data(), value.size());
    }
    std::string Value() const {
        std::ostringstream out;
        out << std::hex << std::setfill('0') << std::setw(16) << first << std::setw(16) << second;
        return out.str();
    }

  private:
    uint64_t first = 14695981039346656037ULL;
    uint64_t second = 7809847782465536322ULL;
};

bool Open(const std::string &path, Sqlite &db, bool write, std::string &error) {
    sqlite3 *handle = nullptr;
#ifdef _WIN32
    const std::string utfPath = FileUtil::ConvertToLongPath(path);
#else
    const std::string &utfPath = path;
#endif
    if (sqlite3_open_v2(utfPath.c_str(), &handle, write ? SQLITE_OPEN_READWRITE : SQLITE_OPEN_READONLY, nullptr) !=
        SQLITE_OK) {
        error = handle ? sqlite3_errmsg(handle) : "Cannot open ftrace database";
        if (handle) {
            sqlite3_close(handle);
        }
        return false;
    }
    db.reset(handle);
    sqlite3_busy_timeout(handle, 5000);
    return true;
}

bool Prepare(sqlite3 *db, const std::string &sql, Statement &stmt, std::string &error) {
    sqlite3_stmt *handle = nullptr;
    if (sqlite3_prepare_v2(db, sql.c_str(), -1, &handle, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db);
        return false;
    }
    stmt.reset(handle);
    return true;
}

bool Exists(sqlite3 *db, const char *table) {
    Statement stmt(nullptr, sqlite3_finalize);
    std::string error;
    if (!Prepare(db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? COLLATE NOCASE", stmt, error)) {
        return false;
    }
    sqlite3_bind_text(stmt.get(), 1, table, -1, SQLITE_STATIC);
    return sqlite3_step(stmt.get()) == SQLITE_ROW;
}

bool Fingerprint(const std::string &path, std::string &fingerprint, std::string &error,
    const MsprofFtraceReader::Cancelled &cancelled) {
    Sqlite db(nullptr, sqlite3_close);
    if (!Open(path, db, false, error)) {
        return false;
    }
    sqlite3_progress_handler(
        db.get(), 4096,
        [](void *p) -> int { return (*static_cast<const MsprofFtraceReader::Cancelled *>(p))() ? 1 : 0; },
        const_cast<MsprofFtraceReader::Cancelled *>(&cancelled));
    if (sqlite3_exec(db.get(), "BEGIN", nullptr, nullptr, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db.get());
        return false;
    }
    ContentHash hash;
    hash.Text(ADAPTER_VERSION);
    std::ostringstream databaseVersion;
    databaseVersion << DATABASE_VERSION;
    hash.Text(databaseVersion.str());
    const std::string selected = "WITH selected_types AS (SELECT * FROM GENERIC_EVENT_TYPES WHERE typeId IN (" +
        MsprofFtraceSupportedTypesSql() +
        ")),"
        "selected_sources AS (SELECT * FROM GENERIC_EVENT_SOURCES WHERE sourceId IN "
        "(SELECT sourceId FROM selected_types)),"
        "selected_fields AS (SELECT * FROM GENERIC_EVENT_TYPE_FIELDS WHERE typeId IN "
        "(SELECT typeId FROM selected_types)),"
        "selected_events AS (SELECT * FROM GENERIC_EVENTS WHERE typeId IN (SELECT typeId FROM selected_types)),"
        "selected_data AS (SELECT * FROM GENERIC_EVENT_DATA WHERE genericEventId IN "
        "(SELECT genericEventId FROM selected_events)) ";
    const std::vector<std::string> queries = {
        "SELECT name,value FROM META_DATA WHERE name IN ('SCHEMA_VERSION','ftrace_events_complete') ORDER BY name",
        selected + "SELECT * FROM selected_sources ORDER BY sourceId",
        selected + "SELECT * FROM selected_types ORDER BY typeId",
        selected + "SELECT * FROM selected_fields ORDER BY typeId,fieldIdx",
        selected + "SELECT * FROM selected_events ORDER BY genericEventId",
        selected + "SELECT * FROM selected_data ORDER BY genericEventId,fieldIdx",
        selected +
            "SELECT id,value FROM STRING_IDS WHERE id IN (SELECT nameId FROM selected_sources "
            "UNION SELECT nameId FROM selected_types UNION SELECT fieldNameId FROM selected_fields "
            "UNION SELECT strId FROM selected_data WHERE strId IS NOT NULL) ORDER BY id"};
    for (const auto &sql : queries) {
        if (cancelled()) {
            error = "Ftrace import cancelled";
            return false;
        }
        hash.Text(sql);
        Statement stmt(nullptr, sqlite3_finalize);
        if (!Prepare(db.get(), sql, stmt, error)) {
            return false;
        }
        int step;
        while ((step = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            if (cancelled()) {
                error = "Ftrace import cancelled";
                return false;
            }
            for (int column = 0; column < sqlite3_column_count(stmt.get()); ++column) {
                int type = sqlite3_column_type(stmt.get(), column);
                hash.Bytes(&type, sizeof(type));
                if (type == SQLITE_INTEGER) {
                    auto value = sqlite3_column_int64(stmt.get(), column);
                    hash.Bytes(&value, sizeof(value));
                } else if (type == SQLITE_FLOAT) {
                    auto value = sqlite3_column_double(stmt.get(), column);
                    hash.Bytes(&value, sizeof(value));
                } else if (type != SQLITE_NULL) {
                    int length = sqlite3_column_bytes(stmt.get(), column);
                    hash.Bytes(&length, sizeof(length));
                    hash.Bytes(sqlite3_column_blob(stmt.get(), column), length);
                }
            }
        }
        if (step != SQLITE_DONE) {
            error = sqlite3_errmsg(db.get());
            return false;
        }
    }
    // Main DB task identities affect scheduling details. Power curves stay with the main profiling view.
    for (const auto *table : {"CANN_API", "PYTORCH_API"}) {
        hash.Text(table);
        if (!Exists(db.get(), table)) {
            continue;
        }
        Statement stmt(nullptr, sqlite3_finalize);
        const std::string sql = "SELECT DISTINCT globalTid FROM " + std::string(table) + " ORDER BY globalTid";
        if (!Prepare(db.get(), sql, stmt, error)) {
            return false;
        }
        int step;
        while ((step = sqlite3_step(stmt.get())) == SQLITE_ROW) {
            auto value = sqlite3_column_int64(stmt.get(), 0);
            hash.Bytes(&value, sizeof(value));
        }
        if (step != SQLITE_DONE) {
            error = sqlite3_errmsg(db.get());
            return false;
        }
    }
    fingerprint = hash.Value();
    return true;
}

std::map<uint32_t, uint32_t> KnownThreads(const std::string &path) {
    Sqlite db(nullptr, sqlite3_close);
    std::string error;
    std::map<uint32_t, uint32_t> result;
    if (!Open(path, db, false, error)) {
        return result;
    }
    for (const auto *table : {"CANN_API", "PYTORCH_API"}) {
        if (!Exists(db.get(), table)) {
            continue;
        }
        Statement stmt(nullptr, sqlite3_finalize);
        if (!Prepare(db.get(), "SELECT DISTINCT globalTid FROM " + std::string(table), stmt, error)) {
            continue;
        }
        while (sqlite3_step(stmt.get()) == SQLITE_ROW) {
            if (sqlite3_column_type(stmt.get(), 0) != SQLITE_INTEGER) {
                continue;
            }
            auto id = static_cast<uint64_t>(sqlite3_column_int64(stmt.get(), 0));
            if ((id >> 32) != 0) {
                result[static_cast<uint32_t>(id)] = static_cast<uint32_t>(id >> 32);
            }
        }
    }
    return result;
}

void RemovePrivateCache(const std::string &path) {
    // Only caller-created generation filenames reach this function; never remove a source DB.
    std::error_code ec;
    for (const auto &suffix : {"", "-wal", "-shm"}) {
        std::filesystem::remove(std::filesystem::u8path(path + suffix), ec);
    }
}

bool CacheMetadata(const std::string &path, const std::string &fingerprint, std::vector<std::string> &capabilities,
    bool write, std::string &error) {
    Sqlite db(nullptr, sqlite3_close);
    if (!Open(path, db, write, error)) {
        return false;
    }
    if (write && sqlite3_exec(db.get(), MSPROF_FTRACE_CACHE_SCHEMA, nullptr, nullptr, nullptr) != SQLITE_OK) {
        error = sqlite3_errmsg(db.get());
        return false;
    }
    Statement stmt(nullptr, sqlite3_finalize);
    if (!Prepare(db.get(),
            write ? "INSERT INTO msprof_ftrace_cache VALUES(?,?,?)"
                  : "SELECT capabilities FROM msprof_ftrace_cache WHERE fingerprint=? AND version=?",
            stmt, error)) {
        return false;
    }
    sqlite3_bind_text(stmt.get(), 1, fingerprint.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt.get(), 2, ADAPTER_VERSION, -1, SQLITE_STATIC);
    if (write) {
        std::string value;
        for (const auto &capability : capabilities) {
            value += capability + '\n';
        }
        sqlite3_bind_text(stmt.get(), 3, value.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt.get()) == SQLITE_DONE) {
            return true;
        }
    } else if (sqlite3_step(stmt.get()) == SQLITE_ROW) {
        const auto *value = sqlite3_column_text(stmt.get(), 0);
        std::istringstream in(value ? reinterpret_cast<const char *>(value) : "");
        capabilities.clear();
        std::string capability;
        while (std::getline(in, capability)) {
            if (!capability.empty()) {
                capabilities.push_back(capability);
            }
        }
        return true;
    }
    error = "Invalid or incomplete ftrace derived cache";
    return false;
}

void RegisterTracks(const MsprofFtraceResource &resource) {
    auto &tracks = TrackInfoManager::Instance();
    tracks.RemoveFile(resource.fileId, resource.rankId);
    tracks.UpdateHost(resource.fileId, resource.host);
    tracks.UpdateDeviceToRankIdMap(resource.fileId, resource.rankId);
    tracks.SetRankListByFileId(resource.fileId, {resource.cluster, resource.host, resource.rankId, "Ftrace", "Ftrace"});
    tracks.SetClusterByFileId(resource.fileId, resource.cluster);
    auto db = std::dynamic_pointer_cast<TextTraceDatabase>(
        DataBaseManager::Instance().GetTraceDatabaseByFileId(resource.fileId));
    if (db) {
        tracks.UpdateTrackIdMap(resource.fileId, db->QueryAllThreadMap());
    }
}
}

struct MsprofFtraceImportService::Worker {
    ThreadPool pool{1};
};

MsprofFtraceImportService &MsprofFtraceImportService::Instance() {
    // Dependencies must outlive the worker, including when first used by a focused import test.
    (void)DataBaseManager::Instance();
    (void)ParserStatusManager::Instance();
    (void)TrackInfoManager::Instance();
    (void)TraceTime::Instance();
    (void)Global::BaselineManager::Instance();
    static MsprofFtraceImportService service;
    return service;
}
MsprofFtraceImportService::MsprofFtraceImportService() = default;
MsprofFtraceImportService::~MsprofFtraceImportService() {
    // Singleton teardown must not call other, possibly already destroyed, singleton managers.
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &item : resources_) {
            item.second->cancelled = true;
        }
    }
    if (worker_) {
        worker_->pool.ShutDown();
    }
}
std::string MsprofFtraceImportService::ResourceId(const std::string &sourceFileId) {
    return sourceFileId + "::msprof_ftrace";
}
std::shared_ptr<MsprofFtraceImportService::Entry> MsprofFtraceImportService::FindEntry(const std::string &id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = resources_.find(id);
    if (found != resources_.end()) {
        return found->second;
    }
    for (const auto &item : resources_) {
        if (item.second->resource.rankId == id) {
            return item.second;
        }
    }
    return nullptr;
}
std::optional<MsprofFtraceResource> MsprofFtraceImportService::Discover(
    const std::string &source, const std::string &host, const std::string &cluster, const std::string &projectName) {
    const auto inspection = MsprofFtraceReader::Inspect(source);
    if (!inspection.HasFtrace()) {
        Remove(ResourceId(source));
        return std::nullopt;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    auto &entry = resources_[ResourceId(source)];
    if (!entry) {
        entry = std::make_shared<Entry>();
    }
    auto &resource = entry->resource;
    resource.sourceFileId = source;
    resource.fileId = ResourceId(source);
    resource.rankId = resource.fileId;
    resource.host = host;
    resource.cluster = cluster;
    resource.projectName = projectName;
    resource.inspection = inspection;
    return resource;
}
std::optional<MsprofFtraceResource> MsprofFtraceImportService::GetResource(const std::string &id) const {
    auto entry = FindEntry(id);
    if (!entry) {
        return std::nullopt;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    return entry->resource;
}
bool MsprofFtraceImportService::IsResource(const std::string &id) const { return FindEntry(id) != nullptr; }
bool MsprofFtraceImportService::HasSource(const std::string &id) const { return IsResource(ResourceId(id)); }
std::vector<std::string> MsprofFtraceImportService::GetCapabilities(const std::string &id) const {
    auto resource = GetResource(id);
    return resource ? resource->capabilities : std::vector<std::string>{};
}

bool MsprofFtraceImportService::Import(const std::string &source, std::string &error, bool notify) {
    auto entry = FindEntry(ResourceId(source));
    if (!entry) {
        if (!Discover(source)) {
            error = "No msprof ftrace marker";
            return false;
        }
        entry = FindEntry(ResourceId(source));
    }
    return Build(entry, error, notify);
}

bool MsprofFtraceImportService::Build(const std::shared_ptr<Entry> &entry, std::string &error, bool notify) {
    std::lock_guard<std::mutex> building(entry->buildMutex);
    MsprofFtraceResource resource;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        resource = entry->resource;
    }
    const std::string previousCachePath = resource.cachePath;
    auto &manager = DataBaseManager::Instance();
    const auto sourceToken = manager.GetTraceDatabasePoolToken(resource.sourceFileId);
    const bool managedSource = !sourceToken.expired();
    MsprofFtraceReader::Cancelled cancelled = [&]() {
        return entry->cancelled.load() || (managedSource && sourceToken.expired());
    };
    const auto inspection = MsprofFtraceReader::Inspect(resource.sourceFileId);
    std::string fingerprint;
    std::string privateId;
    std::string buildingPath;
    bool success = false;
    bool reusePublished = false;
    try {
        if (!inspection.IsReady()) {
            error = inspection.error;
        } else if (Fingerprint(resource.sourceFileId, fingerprint, error, cancelled)) {
            ContentHash sourceHash;
            sourceHash.Text(
                std::filesystem::weakly_canonical(std::filesystem::u8path(resource.sourceFileId)).u8string());
            auto directory = CacheDirectory();
            std::filesystem::create_directories(directory);
            std::string cachePath = (directory / (sourceHash.Value() + "-" + fingerprint + ".db")).u8string();
            {
                std::lock_guard<std::mutex> lock(mutex_);
                entry->cachePins.insert(cachePath);
            }
            std::vector<std::string> capabilities;
            std::string cacheError;
            bool cached = std::filesystem::exists(std::filesystem::u8path(cachePath)) &&
                CacheMetadata(cachePath, fingerprint, capabilities, false, cacheError);
            if (!cached) {
                std::string retained;
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    const auto found = retainedCaches_.find(resource.sourceFileId);
                    if (found != retainedCaches_.end()) {
                        retained = found->second.path;
                        entry->cachePins.insert(retained);
                    }
                }
                // A protected older generation may have forced the completed cache to use a suffix.
                cached = !retained.empty() && std::filesystem::exists(std::filesystem::u8path(retained)) &&
                    CacheMetadata(retained, fingerprint, capabilities, false, cacheError);
                if (cached) {
                    cachePath = retained;
                }
            }
            if (!cached) {
                const auto generation = std::to_string(++generation_) + "-" +
                    std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
                if (std::filesystem::exists(std::filesystem::u8path(cachePath))) {
                    RemoveUnusedCache(cachePath);
                    if (std::filesystem::exists(std::filesystem::u8path(cachePath))) {
                        cachePath = (directory / (sourceHash.Value() + "-" + fingerprint + "-" + generation + ".db"))
                                        .u8string();
                    }
                }
                privateId = resource.fileId + "::building:" + generation;
                buildingPath = (directory / (sourceHash.Value() + "-building-" + generation + ".db")).u8string();
                std::recursive_mutex dbMutex;
                auto db = std::make_shared<TextTraceDatabase>(dbMutex);
                if (!db->OpenDb(buildingPath, false)) {
                    error = "Cannot create ftrace derived database";
                } else {
                    auto known = KnownThreads(resource.sourceFileId);
                    bool built = false;
                    {
                        MsprofFtraceWriter writer(db);
                        MsprofFtraceBuilder builder(writer, known);
                        built = writer.Begin(error) &&
                            MsprofFtraceReader::Read(
                                resource.sourceFileId,
                                [&](const MsprofFtraceEvent &event) { return builder.Consume(event, error); }, error,
                                cancelled) &&
                            builder.Finish(error) && writer.Finish(error);
                        if (built && !cancelled()) {
                            built = OperatorDepthPersistenceService::CalculateAndPersistTextDepth(*db, privateId);
                            if (!built) {
                                error = "Cannot persist ftrace operator depths";
                            }
                        }
                        if (built) {
                            const auto &summary = builder.GetSummary();
                            ServerLog::Info("Msprof ftrace build events: read=", summary.eventCount,
                                ", trusted=", summary.trustedEventCount, ", fieldFailed=", summary.failedEventCount,
                                ", slices=", summary.sliceCount);
                            if (summary.hasScheduling) {
                                capabilities.push_back("scheduling");
                            }
                            if (summary.hasWakeup) {
                                capabilities.push_back("wakeup");
                            }
                            if (summary.hasSoftirq) {
                                capabilities.push_back("softirq");
                            }
                            if (summary.hasHardirq) {
                                capabilities.push_back("hardirq");
                            }
                            db->SetDataBaseVersion();
                        }
                    } // Release the writer's shared DB reference before finalizing SQLite statements.
                    db->CloseDb();
                    db.reset();
                    if (built && !cancelled()) {
                        built = manager.PublishDerivedTextTraceConnectionPool(privateId, privateId, buildingPath) &&
                            ParseUnitManager::Instance().ExecuteUnitListChecked({privateId, false}, FTRACE_STATUS_LIST);
                        if (built) {
                            auto cache = manager.GetTraceDatabaseByFileId(privateId);
                            for (const auto &unit : DB_STATUS_LIST) {
                                if (!cache || !cache->UpdateValueIntoStatusInfoTable(unit, FINISH_STATUS)) {
                                    built = false;
                                    break;
                                }
                            }
                        }
                        if (!built) {
                            error = "Cannot build ftrace statistics";
                        }
                        manager.ReleaseDatabaseByFileId(privateId);
                    }
                    std::string after;
                    if (built && !cancelled() && Fingerprint(resource.sourceFileId, after, error, cancelled)) {
                        if (after != fingerprint) {
                            error = "Ftrace source changed during parsing; retry the import";
                        } else if (CacheMetadata(buildingPath, fingerprint, capabilities, true, error)) {
                            // The private pool is closed before rename. Never overwrite an active generation.
                            if (!std::filesystem::exists(std::filesystem::u8path(cachePath))) {
                                std::filesystem::rename(
                                    std::filesystem::u8path(buildingPath), std::filesystem::u8path(cachePath));
                            } else {
                                std::vector<std::string> existingCaps;
                                if (!CacheMetadata(cachePath, fingerprint, existingCaps, false, error)) {
                                    throw std::runtime_error(
                                        "Existing cache is incomplete; remove its owned cache file before retrying");
                                }
                                RemovePrivateCache(buildingPath);
                            }
                            cached = true;
                        }
                    }
                }
            }
            if (cached && !cancelled()) {
                reusePublished = resource.ready && resource.cachePath == cachePath &&
                    entry->fingerprint == fingerprint && !manager.GetTraceDatabasePoolToken(resource.fileId).expired();
                if (!entry->cancelled &&
                    (reusePublished ||
                        manager.PublishDerivedTextTraceConnectionPool(resource.fileId, resource.rankId, cachePath))) {
                    std::lock_guard<std::mutex> lock(mutex_);
                    resource.cachePath = cachePath;
                    resource.capabilities = capabilities;
                    resource.inspection = inspection;
                    resource.ready = true;
                    resource.error.clear();
                    if (!entry->cancelled) {
                        entry->resource = resource;
                        entry->fingerprint = fingerprint;
                        success = true;
                    }
                }
            }
        }
    } catch (const std::exception &exception) {
        error = exception.what();
    }
    if (!success) {
        if (cancelled()) {
            error = "Ftrace import cancelled";
        }
        if (error.empty()) {
            error = "Failed to import msprof ftrace";
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            entry->resource.error = error;
        }
        if (!privateId.empty()) {
            manager.ReleaseDatabaseByFileId(privateId);
        }
        if (!buildingPath.empty()) {
            RemovePrivateCache(buildingPath);
        }
    } else {
        error.clear();
        // The active resource now pins this generation. Retire older idle results from the same source.
        const auto retained = TakeRetainedCache(resource.sourceFileId);
        if (retained != resource.cachePath) {
            RemoveUnusedCache(retained);
        }
        if (!reusePublished) {
            TraceTime::Instance().RemoveCard(resource.rankId);
            RegisterTracks(resource);
        }
        if (previousCachePath != resource.cachePath) {
            RemoveUnusedCache(previousCachePath);
        }
    }
    std::set<std::string> candidates;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        candidates.swap(entry->cachePins);
    }
    for (const auto &path : candidates) {
        // A cancelled refresh can race with Reset clearing the registry before it retains this generation.
        if (!resource.ready || path != resource.cachePath) {
            RemoveUnusedCache(path);
        }
    }
    if (notify && !cancelled()) {
        if (!success && resource.ready) {
            Server::ServerLog::Warn("Msprof ftrace refresh failed; retaining the published generation. source:",
                resource.sourceFileId, ", error:", error);
        }
        PublishCompletion(resource, success || resource.ready, error);
        if (!success && resource.ready) {
            ProjectParserBase::ParseEndCallBack(resource.rankId, resource.fileId, false,
                "Ftrace refresh failed; the previous result is still available. " + error);
        }
    }
    return success;
}

void MsprofFtraceImportService::Schedule(const std::string &source) {
    auto entry = FindEntry(ResourceId(source));
    if (!entry && Discover(source)) {
        entry = FindEntry(ResourceId(source));
    }
    if (!entry || entry->scheduled.exchange(true)) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string rankId = entry->resource.rankId;
    auto registered = resources_.find(rankId);
    if (registered == resources_.end() || registered->second != entry) {
        entry->scheduled = false;
        return;
    }
    entry->cancelled = false;
    if (!worker_) {
        worker_ = std::make_shared<Worker>();
    }
    auto &status = ParserStatusManager::Instance();
    status.SetPendingStatus(rankId, {ProjectTypeEnum::DB, {}});
    status.ClearParserStatus(rankId);
    status.SetParserStatus(rankId, ParserStatus::INIT);
    worker_->pool.AddTask(
        [this, entry, rankId]() {
            if (ParserStatusManager::Instance().WaitStartParse([entry] { return entry->cancelled.load(); })) {
                ParserStatusManager::Instance().SetRunningStatus(rankId);
                std::string error;
                Build(entry, error, true);
            }
            entry->scheduled = false;
            ParserStatusManager::Instance().SetFinishStatus(rankId);
            if (!entry->cancelled) {
                ProjectParserBase::SendAllParseSuccessIfReady();
            }
        },
        "msprof-ftrace");
}

void MsprofFtraceImportService::PublishCompletion(
    const MsprofFtraceResource &resource, bool success, const std::string &error) {
    ProjectParserBase::ParseEndCallBack(resource.rankId, resource.fileId, success, error);
    if (!success) {
        return;
    }
    for (const auto &unit : DB_STATUS_LIST) {
        ProjectParserBase::SendUnitFinishNotify(resource.fileId, true, unit);
    }
    for (const auto &unit : FTRACE_STATUS_LIST) {
        ProjectParserBase::SendUnitFinishNotify(resource.fileId, true, unit);
    }
}

bool MsprofFtraceImportService::CheckBaselineReady(const std::string &id, std::string &error) const {
    const auto entry = FindEntry(id);
    if (!entry) {
        error = "Unknown Ftrace resource";
        return false;
    }
    std::unique_lock<std::mutex> building(entry->buildMutex, std::try_to_lock);
    if (!building.owns_lock()) {
        error = "Ftrace must finish parsing before selecting a baseline";
        return false;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!entry->resource.ready || entry->cancelled) {
        error = "Ftrace must finish parsing before selecting a baseline";
        return false;
    }
    error.clear();
    return true;
}

bool MsprofFtraceImportService::CreateBaseline(const std::string &id, Global::BaselineInfo &info, bool reuseSource) {
    auto entry = FindEntry(id);
    if (!entry) {
        info.errorMessage = "Unknown Ftrace resource";
        return false;
    }
    std::unique_lock<std::mutex> building(entry->buildMutex, std::try_to_lock);
    if (!building.owns_lock()) {
        info.errorMessage = "Ftrace must finish parsing before selecting a baseline";
        return false;
    }
    MsprofFtraceResource resource;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        resource = entry->resource;
    }
    if (!resource.ready || entry->cancelled) {
        info.errorMessage = "Ftrace must finish parsing before selecting a baseline";
        return false;
    }
    if (!reuseSource) {
        const std::string aliasId = resource.fileId + "::baseline:" + std::to_string(++generation_);
        if (!DataBaseManager::Instance().AliasDerivedTextTraceConnectionPool(aliasId, aliasId, resource.fileId)) {
            info.errorMessage = "Cannot open Ftrace baseline";
            return false;
        }
        auto alias = std::make_shared<Entry>();
        resource.fileId = aliasId;
        resource.rankId = aliasId;
        alias->resource = resource;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            resources_[aliasId] = alias;
        }
        RegisterTracks(resource);
    }
    info.host = resource.host;
    info.rankId = resource.rankId;
    info.fileId = resource.fileId;
    info.cardId = resource.rankId;
    info.cardName = reuseSource ? "Ftrace" : "Baseline_Ftrace";
    info.isFtrace = true;
    info.isCluster = false;
    info.parsedFilePath = id;
    info.clusterBaseLine = resource.cluster;
    Global::BaselineManager::Instance().SetBaselineInfo(info);
    auto &status = ParserStatusManager::Instance();
    if (status.GetParserStatus(resource.rankId) != ParserStatus::FINISH_ALL) {
        status.SetParserStatus(resource.rankId, ParserStatus::FINISH);
    }
    return true;
}

void MsprofFtraceImportService::NotifyReady(const std::string &id) {
    const auto resource = GetResource(id);
    if (resource && resource->ready) {
        PublishCompletion(*resource, true, "");
    }
}

bool MsprofFtraceImportService::Remove(const std::string &id, bool retainCache) {
    auto entry = FindEntry(id);
    if (!entry) {
        if (retainCache) {
            return false;
        }
        // An inactive project's logical leaf can still own a completed disk cache.
        std::string source = id;
        const auto suffix = ResourceId("");
        if (source.size() > suffix.size() &&
            source.compare(source.size() - suffix.size(), suffix.size(), suffix) == 0) {
            source.resize(source.size() - suffix.size());
        }
        const auto retained = TakeRetainedCache(source);
        if (retained.empty()) {
            return false;
        }
        RemoveUnusedCache(retained);
        return true;
    }
    entry->cancelled = true;
    std::string fileId;
    std::string rankId;
    std::string cachePath;
    MsprofFtraceResource resource;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        fileId = entry->resource.fileId;
        rankId = entry->resource.rankId;
        cachePath = entry->resource.cachePath;
        resource = entry->resource;
        resources_.erase(fileId);
    }
    ParserStatusManager::Instance().SetTerminateStatus(rankId);
    ParserStatusManager::Instance().SetPendingStatus(rankId, {ProjectTypeEnum::DB, {}});
    // Wait outside the registry and manager locks so an in-flight build can finish cancellation.
    std::lock_guard<std::mutex> building(entry->buildMutex);
    DataBaseManager::Instance().ReleaseDatabaseByFileId(fileId);
    TrackInfoManager::Instance().RemoveFile(fileId, rankId);
    TraceTime::Instance().RemoveCard(rankId);
    if (retainCache && resource.ready) {
        RetainCache(resource);
    } else if (!retainCache && fileId == ResourceId(resource.sourceFileId)) {
        // Explicitly removing a source also discards an older idle generation. Aliases remain independent.
        RemoveUnusedCache(TakeRetainedCache(resource.sourceFileId));
    }
    RemoveUnusedCache(cachePath);
    return true;
}
void MsprofFtraceImportService::RetainCache(const MsprofFtraceResource &resource) {
    if (!resource.ready || resource.cachePath.empty()) {
        return;
    }
    std::vector<std::string> retired;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &cached = retainedCaches_[resource.sourceFileId];
        if (!cached.path.empty() && cached.path != resource.cachePath) {
            retired.push_back(cached.path);
        }
        cached = {resource.cachePath, ++cacheSequence_};
        while (retainedCaches_.size() > MAX_RETAINED_SOURCES) {
            auto oldest = retainedCaches_.begin();
            for (auto it = retainedCaches_.begin(); it != retainedCaches_.end(); ++it) {
                if (it->second.lastUsed < oldest->second.lastUsed) {
                    oldest = it;
                }
            }
            retired.push_back(oldest->second.path);
            retainedCaches_.erase(oldest);
        }
    }
    for (const auto &path : retired) {
        RemoveUnusedCache(path);
    }
}
std::string MsprofFtraceImportService::TakeRetainedCache(const std::string &sourceFileId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto found = retainedCaches_.find(sourceFileId);
    if (found == retainedCaches_.end()) {
        return {};
    }
    const auto path = found->second.path;
    retainedCaches_.erase(found);
    return path;
}
void MsprofFtraceImportService::RemoveUnusedCache(const std::string &path) {
    if (path.empty()) {
        return;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &item : resources_) {
        if (item.second->resource.cachePath == path || item.second->cachePins.count(path) != 0) {
            return;
        }
    }
    for (const auto &item : retainedCaches_) {
        if (item.second.path == path) {
            return;
        }
    }
    // Pool release/replacement has already waited for in-flight leases. Registry entries pin
    // baseline generations. Validate ownership even though paths originate from our builder.
    const auto directory = CacheDirectory();
    const auto candidate = std::filesystem::u8path(path).lexically_normal();
    if (candidate.parent_path() == directory.lexically_normal() && candidate.extension() == ".db") {
        RemovePrivateCache(path);
    }
}
void MsprofFtraceImportService::OnDatabaseReleased(const std::string &id) {
    // Remove aliases independently; releasing a physical source removes its child, not pinned baselines.
    if (IsResource(id)) {
        Remove(id);
    } else if (HasSource(id)) {
        Remove(ResourceId(id));
    }
}
void MsprofFtraceImportService::Reset(bool retainCaches) {
    std::vector<std::shared_ptr<Entry>> entries;
    std::vector<std::string> retired;
    std::shared_ptr<Worker> worker;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &item : resources_) {
            item.second->cancelled = true;
            entries.push_back(item.second);
        }
        resources_.clear();
        if (!retainCaches) {
            for (const auto &item : retainedCaches_) {
                retired.push_back(item.second.path);
            }
            retainedCaches_.clear();
        }
        worker = std::move(worker_);
    }
    for (const auto &entry : entries) {
        ParserStatusManager::Instance().SetTerminateStatus(entry->resource.rankId);
        ParserStatusManager::Instance().SetPendingStatus(entry->resource.rankId, {ProjectTypeEnum::DB, {}});
    }
    if (worker) {
        worker->pool.Reset();
        worker->pool.ShutDown();
    }
    for (auto &entry : entries) {
        std::lock_guard<std::mutex> building(entry->buildMutex);
        DataBaseManager::Instance().ReleaseDatabaseByFileId(entry->resource.fileId);
        TrackInfoManager::Instance().RemoveFile(entry->resource.fileId, entry->resource.rankId);
        TraceTime::Instance().RemoveCard(entry->resource.rankId);
    }
    for (const auto &entry : entries) {
        if (retainCaches && entry->resource.fileId == ResourceId(entry->resource.sourceFileId)) {
            RetainCache(entry->resource);
        }
    }
    for (const auto &entry : entries) {
        RemoveUnusedCache(entry->resource.cachePath);
    }
    for (const auto &path : retired) {
        RemoveUnusedCache(path);
    }
}
}
