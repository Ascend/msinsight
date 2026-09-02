#include "RankOverlapStore.h"

#include <functional>
#include <sqlite3.h>

#include "ConstantDefs.h"
#include "DataBaseManager.h"
#include "ServerLog.h"

namespace Dic::Module::Timeline {
RankOverlapStore &RankOverlapStore::Instance() {
    static RankOverlapStore instance;
    return instance;
}

std::string RankOverlapStore::CreateSourceUri(const std::string &rankId, uint64_t generation) {
    return "file:msinsight_overlap_" + std::to_string(std::hash<std::string>{}(rankId)) + "_" +
        std::to_string(generation) + "?mode=memory&cache=shared";
}

bool RankOverlapStore::WriteDatabase(
    Source &source, const std::string &deviceId, const std::vector<FullDb::OVERLAP_INFO> &rows) {
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX | SQLITE_OPEN_URI;
    if (sqlite3_open_v2(source.uri.c_str(), &source.keeper, flags, nullptr) != SQLITE_OK) {
        sqlite3_close(source.keeper);
        source.keeper = nullptr;
        return false;
    }
    const char *schema = "CREATE TABLE OVERLAP_ANALYSIS(id INTEGER PRIMARY KEY AUTOINCREMENT,deviceId INTEGER,startNs "
                         "INTEGER,endNs INTEGER,type INTEGER);"
                         "CREATE TABLE status_info(id INTEGER PRIMARY KEY AUTOINCREMENT,key TEXT,value TEXT);"
                         "INSERT INTO status_info(key,value) VALUES('OVERLAP_ANALYSIS','FINISH');"
                         "BEGIN TRANSACTION;";
    char *message = nullptr;
    bool success = sqlite3_exec(source.keeper, schema, nullptr, nullptr, &message) == SQLITE_OK;
    sqlite3_free(message);
    sqlite3_stmt *statement = nullptr;
    if (success) {
        success = sqlite3_prepare_v2(source.keeper,
                      "INSERT INTO OVERLAP_ANALYSIS(deviceId,startNs,endNs,type) VALUES(?,?,?,?)", -1, &statement,
                      nullptr) == SQLITE_OK;
    }
    for (const auto &row : rows) {
        if (!success) {
            break;
        }
        sqlite3_reset(statement);
        sqlite3_clear_bindings(statement);
        sqlite3_bind_text(statement, 1, deviceId.c_str(), static_cast<int>(deviceId.size()), SQLITE_TRANSIENT);
        sqlite3_bind_int64(statement, 2, row.startNs);
        sqlite3_bind_int64(statement, 3, row.endNs);
        sqlite3_bind_int64(statement, 4, row.type);
        success = sqlite3_step(statement) == SQLITE_DONE;
    }
    sqlite3_finalize(statement);
    sqlite3_exec(source.keeper, success ? "COMMIT;" : "ROLLBACK;", nullptr, nullptr, nullptr);
    if (!success) {
        sqlite3_close(source.keeper);
        source.keeper = nullptr;
    }
    return success;
}

void RankOverlapStore::RemoveSource(const std::string &rankId, Source &source) {
    if (source.uri.empty()) {
        return;
    }
    DataBaseManager::Instance().RemoveRankIdToDeviceId(source.uri, rankId);
    DataBaseManager::Instance().ReleaseDatabaseByFileId(source.uri);
    sqlite3_close(source.keeper);
    source.keeper = nullptr;
}

std::string RankOverlapStore::Publish(
    const std::string &rankId, const std::string &deviceId, const std::vector<FullDb::OVERLAP_INFO> &rows) {
    if (rankId.empty() || deviceId.empty()) {
        return "";
    }
    Source source;
    PublishFailurePointForTesting failurePoint;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        source.uri = CreateSourceUri(rankId, ++generation_);
        failurePoint = failurePointForTesting_;
        failurePointForTesting_ = PublishFailurePointForTesting::NONE;
    }
    if (source.uri.empty() || failurePoint == PublishFailurePointForTesting::WRITE_DATABASE ||
        !WriteDatabase(source, deviceId, rows)) {
        RemoveSource(rankId, source);
        return "";
    }
    auto &manager = DataBaseManager::Instance();
    manager.SetDataType(DataType::DB, source.uri);
    if (failurePoint == PublishFailurePointForTesting::CREATE_CONNECTION_POOL ||
        !manager.CreateDerivedTraceConnectionPool(source.uri, source.uri, true)) {
        RemoveSource(rankId, source);
        return "";
    }
    manager.UpdateRankIdToDeviceId(source.uri, rankId, deviceId);
    const std::string uri = source.uri;
    Source previous;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto current = sources_.find(rankId);
        if (current != sources_.end()) {
            previous = std::move(current->second);
        }
        sources_[rankId] = std::move(source);
    }
    RemoveSource(rankId, previous);
    return uri;
}

std::string RankOverlapStore::ResolveSource(const std::string &rankId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto source = sources_.find(rankId);
    return source == sources_.end() ? "" : source->second.uri;
}

void RankOverlapStore::Invalidate(const std::string &rankId) {
    Source source;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto current = sources_.find(rankId);
        if (current == sources_.end()) {
            return;
        }
        source = std::move(current->second);
        sources_.erase(current);
    }
    RemoveSource(rankId, source);
}

void RankOverlapStore::Clear() {
    std::vector<std::pair<std::string, Source>> sources;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto &[rankId, source] : sources_) {
            sources.emplace_back(rankId, std::move(source));
        }
        sources_.clear();
    }
    for (auto &[rankId, source] : sources) {
        RemoveSource(rankId, source);
    }
}

void RankOverlapStore::SetPublishFailurePointForTesting(PublishFailurePointForTesting failurePoint) {
    std::lock_guard<std::mutex> lock(mutex_);
    failurePointForTesting_ = failurePoint;
}
}
