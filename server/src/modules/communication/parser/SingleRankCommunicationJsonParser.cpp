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

#include "pch.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <limits>

#include "DbClusterDataBase.h"
#include "SafeFile.h"
#include "SingleRankCommunicationJsonParser.h"
#include "istreamwrapper.h"

namespace Dic {
namespace Module {
namespace Timeline {
namespace {
using Dic::Module::FullDb::DbClusterDataBase;
using Dic::Server::ServerLog;

constexpr const char *CACHE_SUFFIX = ".communication_detail.db";
constexpr const char *TEMP_SUFFIX = ".tmp";
constexpr const char *META_TABLE = "TimelineCommunicationDetailMeta";
constexpr int64_t CACHE_SCHEMA_VERSION = 1;

struct SourceFingerprint {
    std::string path;
    int64_t size = 0;
    int64_t modifiedTime = 0;

    bool operator==(const SourceFingerprint &other) const {
        return path == other.path && size == other.size && modifiedTime == other.modifiedTime;
    }
};

struct TimeRecord {
    std::optional<double> transitTime;
    std::optional<double> waitTime;
};

struct BandwidthRecord {
    std::optional<double> transitSize;
    std::optional<double> transitTime;
    std::optional<double> bandwidth;
};

std::optional<SourceFingerprint> GetSourceFingerprint(const std::string &path) {
    try {
        const std::string realPath = FileUtil::GetRealPath(path);
        if (realPath.empty()) {
            ServerLog::Warn("Failed to resolve communication.json path. path:", path);
            return std::nullopt;
        }
#ifdef _WIN32
        // Use the same Win32 metadata API family as FileUtil's security checks, and normalize the path before
        // adding the extended-length prefix. This avoids inconsistent path handling in MinGW std::filesystem.
        WIN32_FILE_ATTRIBUTE_DATA fileData{};
        const std::wstring filePath = FileUtil::ConvertToLongPathW(realPath);
        if (GetFileAttributesExW(filePath.c_str(), GetFileExInfoStandard, &fileData) == 0) {
            ServerLog::Warn(
                "Failed to get communication.json attributes. error code:", GetLastError(), ", path:", realPath);
            return std::nullopt;
        }
        if ((fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            ServerLog::Warn("The communication.json path is not a regular file. path:", realPath);
            return std::nullopt;
        }
        ULARGE_INTEGER fileSize{};
        fileSize.HighPart = fileData.nFileSizeHigh;
        fileSize.LowPart = fileData.nFileSizeLow;
        ULARGE_INTEGER modifiedTime{};
        modifiedTime.HighPart = fileData.ftLastWriteTime.dwHighDateTime;
        modifiedTime.LowPart = fileData.ftLastWriteTime.dwLowDateTime;
        if (fileSize.QuadPart > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) ||
            modifiedTime.QuadPart > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
            ServerLog::Warn("The communication.json metadata exceeds the supported range. path:", realPath);
            return std::nullopt;
        }
        return SourceFingerprint{
            realPath, static_cast<int64_t>(fileSize.QuadPart), static_cast<int64_t>(modifiedTime.QuadPart)};
#else
        const fs::path filePath(realPath);
        if (!fs::is_regular_file(filePath)) {
            ServerLog::Warn("The communication.json path is not a regular file. path:", realPath);
            return std::nullopt;
        }
        const auto size = fs::file_size(filePath);
        if (size > static_cast<uintmax_t>(std::numeric_limits<int64_t>::max())) {
            ServerLog::Warn("The communication.json size exceeds the supported range. path:", realPath);
            return std::nullopt;
        }
        const auto modifiedTime = fs::last_write_time(filePath).time_since_epoch().count();
        if (modifiedTime > std::numeric_limits<int64_t>::max() || modifiedTime < std::numeric_limits<int64_t>::min()) {
            ServerLog::Warn("The communication.json modified time exceeds the supported range. path:", realPath);
            return std::nullopt;
        }
        return SourceFingerprint{realPath, static_cast<int64_t>(size), static_cast<int64_t>(modifiedTime)};
#endif
    } catch (const fs::filesystem_error &error) {
        ServerLog::Warn("Failed to get communication.json fingerprint. error:", error.what());
        return std::nullopt;
    }
}

bool RemoveFileIfExists(const std::string &path) {
    if (!FileUtil::IsRegularFile(path)) {
        return true;
    }
#ifdef _WIN32
    return DeleteFileW(FileUtil::ConvertToLongPathW(path).c_str()) != 0;
#else
    return std::remove(path.c_str()) == 0;
#endif
}

bool RemoveDatabaseSidecars(const std::string &path) {
    return RemoveFileIfExists(path + "-wal") && RemoveFileIfExists(path + "-shm") &&
        RemoveFileIfExists(path + "-journal");
}

bool RemoveDatabaseFiles(const std::string &path) { return RemoveDatabaseSidecars(path) && RemoveFileIfExists(path); }

bool PublishCache(const std::string &tempPath, const std::string &cachePath) {
#ifdef _WIN32
    if (MoveFileExW(FileUtil::ConvertToLongPathW(tempPath).c_str(), FileUtil::ConvertToLongPathW(cachePath).c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
        return true;
    }
    ServerLog::Error("Failed to publish communication detail cache. error code:", GetLastError());
    return false;
#else
    if (std::rename(tempPath.c_str(), cachePath.c_str()) == 0) {
        return true;
    }
    ServerLog::Error("Failed to publish communication detail cache. error code:", errno);
    return false;
#endif
}

bool IsCacheCurrent(const std::string &cachePath, const SourceFingerprint &fingerprint) {
    if (!FileUtil::IsRegularFile(cachePath)) {
        return false;
    }
    std::recursive_mutex mutex;
    DbClusterDataBase database(mutex);
    if (!database.OpenDb(cachePath, false) || !database.CheckTableExist(META_TABLE) ||
        !database.CheckTableExist("CommAnalyzerTime") || !database.CheckTableExist("CommAnalyzerBandwidth")) {
        return false;
    }
    auto statement = database.CreatPreparedStatement("SELECT source_path, source_size, source_mtime, schema_version "
                                                     "FROM TimelineCommunicationDetailMeta LIMIT 2");
    if (statement == nullptr) {
        return false;
    }
    auto result = statement->ExecuteQuery();
    if (result == nullptr || !result->Next()) {
        return false;
    }
    const SourceFingerprint cached{
        result->GetString("source_path"), result->GetInt64("source_size"), result->GetInt64("source_mtime")};
    const int64_t schemaVersion = result->GetInt64("schema_version");
    if (result->Next() || result->GetErrorCode() != SQLITE_DONE) {
        return false;
    }
    return cached == fingerprint && schemaVersion == CACHE_SCHEMA_VERSION;
}

class SingleRankCommunicationJsonHandler
    : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, SingleRankCommunicationJsonHandler> {
  public:
    SingleRankCommunicationJsonHandler(
        SqlitePreparedStatement &timeStatement, SqlitePreparedStatement &bandwidthStatement)
        : timeStatement_(timeStatement), bandwidthStatement_(bandwidthStatement) {}

    bool Null() { return true; }
    bool Bool(bool) { return true; }
    bool Int(int value) { return SetNumber(static_cast<double>(value)); }
    bool Uint(unsigned value) { return SetNumber(static_cast<double>(value)); }
    bool Int64(int64_t value) { return SetNumber(static_cast<double>(value)); }
    bool Uint64(uint64_t value) { return SetNumber(static_cast<double>(value)); }
    bool Double(double value) { return SetNumber(value); }
    bool RawNumber(const char *value, rapidjson::SizeType length, bool) { return SetNumber(value, length); }
    bool String(const char *value, rapidjson::SizeType length, bool) { return SetNumber(value, length); }

    bool StartObject() {
        ++depth_;
        return true;
    }

    bool Key(const char *value, rapidjson::SizeType length, bool) {
        currentKey_.assign(value, length);
        switch (depth_) {
        case STEP_DEPTH:
            step_ = currentKey_;
            break;
        case STAGE_DEPTH:
            validStage_ = currentKey_ == "p2p" || currentKey_ == "collective";
            break;
        case OPERATOR_DEPTH:
            BeginOperator(currentKey_);
            break;
        case SECTION_DEPTH:
            section_ = currentKey_;
            if (section_ == "Communication Time Info") {
                timeRecord_ = {};
            }
            break;
        case TRANSPORT_DEPTH:
            if (section_ == "Communication Bandwidth Info") {
                transportType_ = currentKey_;
                bandwidthRecord_ = {};
            }
            break;
        default:
            break;
        }
        return true;
    }

    bool EndObject(rapidjson::SizeType) {
        bool success = true;
        if (depth_ == BANDWIDTH_VALUE_DEPTH && section_ == "Communication Bandwidth Info" && !skipOperator_) {
            success = InsertBandwidth();
        } else if (depth_ == TIME_VALUE_DEPTH && section_ == "Communication Time Info" && !skipOperator_) {
            success = InsertTime();
        }
        if (depth_ == SECTION_DEPTH) {
            section_.clear();
        } else if (depth_ == OPERATOR_DEPTH) {
            opName_.clear();
            group_.clear();
            skipOperator_ = false;
            validStage_ = false;
        }
        if (depth_ > 0) {
            --depth_;
        }
        return success;
    }

    bool StartArray() { return true; }
    bool EndArray(rapidjson::SizeType) { return true; }

  private:
    void BeginOperator(const std::string &rawOpName) {
        const size_t separator = rawOpName.find_last_of('@');
        if (separator == std::string::npos) {
            opName_ = rawOpName;
            group_.clear();
        } else {
            opName_ = rawOpName.substr(0, separator);
            group_ = rawOpName.substr(separator + 1);
        }
        skipOperator_ = !validStage_ || opName_ == "Total Op Info";
    }

    bool SetNumber(const char *value, rapidjson::SizeType length) {
        if (value == nullptr || !IsDetailNumber()) {
            return value != nullptr;
        }
        const std::string number(value, length);
        char *end = nullptr;
        errno = 0;
        const double parsed = std::strtod(number.c_str(), &end);
        if (errno != 0 || end != number.c_str() + number.size() || !std::isfinite(parsed)) {
            return true;
        }
        return SetNumber(parsed);
    }

    bool IsDetailNumber() const {
        return !skipOperator_ &&
            ((depth_ == TIME_VALUE_DEPTH && section_ == "Communication Time Info") ||
                (depth_ == BANDWIDTH_VALUE_DEPTH && section_ == "Communication Bandwidth Info"));
    }

    bool SetNumber(double value) {
        if (!IsDetailNumber() || !std::isfinite(value)) {
            return true;
        }
        if (section_ == "Communication Time Info") {
            if (currentKey_ == "Transit Time(ms)") {
                timeRecord_.transitTime = value;
            } else if (currentKey_ == "Wait Time(ms)") {
                timeRecord_.waitTime = value;
            }
            return true;
        }
        if (currentKey_ == "Transit Size(MB)") {
            bandwidthRecord_.transitSize = value;
        } else if (currentKey_ == "Transit Time(ms)") {
            bandwidthRecord_.transitTime = value;
        } else if (currentKey_ == "Bandwidth(GB/s)") {
            bandwidthRecord_.bandwidth = value;
        }
        return true;
    }

    bool InsertTime() {
        timeStatement_.Reset();
        if (!timeStatement_.Execute(step_, opName_, group_, timeRecord_.transitTime, timeRecord_.waitTime)) {
            ServerLog::Error(
                "Failed to insert communication time cache record. error:", timeStatement_.GetErrorMessage());
            return false;
        }
        return true;
    }

    bool InsertBandwidth() {
        bandwidthStatement_.Reset();
        if (!bandwidthStatement_.Execute(step_, opName_, group_, transportType_, bandwidthRecord_.transitSize,
                bandwidthRecord_.transitTime, bandwidthRecord_.bandwidth)) {
            ServerLog::Error(
                "Failed to insert communication bandwidth cache record. error:", bandwidthStatement_.GetErrorMessage());
            return false;
        }
        return true;
    }

    SqlitePreparedStatement &timeStatement_;
    SqlitePreparedStatement &bandwidthStatement_;
    // Raw single-rank layout: step -> p2p|collective -> opName@group -> section -> transport -> values.
    static constexpr uint32_t STEP_DEPTH = 1;
    static constexpr uint32_t STAGE_DEPTH = 2;
    static constexpr uint32_t OPERATOR_DEPTH = 3;
    static constexpr uint32_t SECTION_DEPTH = 4;
    static constexpr uint32_t TIME_VALUE_DEPTH = 5;
    static constexpr uint32_t TRANSPORT_DEPTH = 5;
    static constexpr uint32_t BANDWIDTH_VALUE_DEPTH = 6;
    uint32_t depth_ = 0;
    std::string currentKey_;
    std::string step_;
    std::string opName_;
    std::string group_;
    std::string section_;
    std::string transportType_;
    bool validStage_ = false;
    bool skipOperator_ = false;
    TimeRecord timeRecord_;
    BandwidthRecord bandwidthRecord_;
};

bool CreateCache(
    const std::string &tempPath, const std::string &communicationJsonPath, const SourceFingerprint &fingerprint) {
    if (!RemoveDatabaseFiles(tempPath)) {
        ServerLog::Error("Failed to remove stale temporary communication detail cache.");
        return false;
    }

    std::recursive_mutex mutex;
    DbClusterDataBase database(mutex);
    if (!database.OpenDb(tempPath, false) || !database.ExecSql("PRAGMA journal_mode=DELETE") ||
        !database.StartTransaction()) {
        database.CloseDb();
        RemoveDatabaseFiles(tempPath);
        return false;
    }

    bool success = database.ExecSql("CREATE TABLE CommAnalyzerTime(step TEXT, hccl_op_name TEXT, group_name TEXT, "
                                    "transit_time REAL, wait_time REAL)") &&
        database.ExecSql("CREATE TABLE CommAnalyzerBandwidth(step TEXT, hccl_op_name TEXT, group_name TEXT, "
                         "transport_type TEXT, transit_size REAL, transit_time REAL, bandwidth REAL)") &&
        database.ExecSql("CREATE TABLE TimelineCommunicationDetailMeta(source_path TEXT NOT NULL, "
                         "source_size INTEGER NOT NULL, source_mtime INTEGER NOT NULL, "
                         "schema_version INTEGER NOT NULL)");

    auto timeStatement = success
        ? database.CreatPreparedStatement("INSERT INTO CommAnalyzerTime(step, hccl_op_name, group_name, transit_time, "
                                          "wait_time) VALUES (?, ?, ?, ?, ?)")
        : nullptr;
    auto bandwidthStatement = success
        ? database.CreatPreparedStatement("INSERT INTO CommAnalyzerBandwidth(step, hccl_op_name, group_name, "
                                          "transport_type, transit_size, transit_time, bandwidth) "
                                          "VALUES (?, ?, ?, ?, ?, ?, ?)")
        : nullptr;
    success = success && timeStatement != nullptr && bandwidthStatement != nullptr;

    if (success) {
        std::ifstream input = OpenReadFileSafely(communicationJsonPath, std::ios::binary);
        if (!input.is_open()) {
            success = false;
        } else {
            rapidjson::IStreamWrapper stream(input);
            rapidjson::Reader reader;
            SingleRankCommunicationJsonHandler handler(*timeStatement, *bandwidthStatement);
            const rapidjson::ParseResult parseResult = reader.Parse(stream, handler);
            if (!parseResult) {
                ServerLog::Error("Failed to parse single-rank communication.json. error offset:", parseResult.Offset(),
                    "error code:", parseResult.Code());
                success = false;
            }
        }
    }

    timeStatement.reset();
    bandwidthStatement.reset();
    const auto fingerprintAfterParse = success ? GetSourceFingerprint(communicationJsonPath) : std::nullopt;
    success = success && fingerprintAfterParse.has_value() && fingerprintAfterParse.value() == fingerprint;

    if (success) {
        auto metaStatement = database.CreatPreparedStatement(
            "INSERT INTO TimelineCommunicationDetailMeta(source_path, source_size, source_mtime, schema_version) "
            "VALUES (?, ?, ?, ?)");
        success = metaStatement != nullptr &&
            metaStatement->Execute(fingerprint.path, fingerprint.size, fingerprint.modifiedTime, CACHE_SCHEMA_VERSION);
    }
    if (success) {
        success = database.ExecSql("CREATE INDEX idx_comm_detail_time_op ON CommAnalyzerTime(hccl_op_name)") &&
            database.ExecSql("CREATE INDEX idx_comm_detail_bandwidth_op ON "
                             "CommAnalyzerBandwidth(step, group_name, hccl_op_name, transport_type)");
    }

    if (success) {
        success = database.EndTransaction();
    } else {
        database.RollbackTransaction();
    }
    database.CloseDb();
    if (success) {
        success = RemoveDatabaseSidecars(tempPath);
    }
    if (!success) {
        RemoveDatabaseFiles(tempPath);
    }
    return success;
}

} // namespace

std::optional<std::string> PrepareSingleRankCommunicationJsonCache(
    const std::string &traceDbPath, const std::string &communicationJsonPath) {
    if (traceDbPath.empty() || communicationJsonPath.empty() || FileUtil::GetParentPath(traceDbPath).empty()) {
        return std::nullopt;
    }
    if (!FileUtil::CheckPathSecurity(communicationJsonPath, CHECK_FILE_READ)) {
        return std::nullopt;
    }
    const auto fingerprint = GetSourceFingerprint(communicationJsonPath);
    if (!fingerprint.has_value()) {
        return std::nullopt;
    }

    const std::string cachePath = traceDbPath + CACHE_SUFFIX;
    if (IsCacheCurrent(cachePath, fingerprint.value())) {
        return cachePath;
    }

    const std::string tempPath = cachePath + TEMP_SUFFIX;
    if (!CreateCache(tempPath, communicationJsonPath, fingerprint.value())) {
        return std::nullopt;
    }
    if (!RemoveDatabaseSidecars(cachePath)) {
        RemoveDatabaseFiles(tempPath);
        return std::nullopt;
    }
    if (!PublishCache(tempPath, cachePath)) {
        RemoveDatabaseFiles(tempPath);
        return std::nullopt;
    }
    return cachePath;
}

} // namespace Timeline
} // namespace Module
} // namespace Dic
