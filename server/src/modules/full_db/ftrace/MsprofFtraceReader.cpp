/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#include "MsprofFtraceReader.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <utility>
#include <vector>
#include "FileUtil.h"
#include "StringUtil.h"
#include "sqlite3.h"

namespace Dic::Module::FullDb {
namespace {
using Connection = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;
using Statement = std::unique_ptr<sqlite3_stmt, decltype(&sqlite3_finalize)>;
using TypeMap = std::map<uint64_t, std::shared_ptr<MsprofFtraceType>>;

const std::map<std::string, std::vector<std::string>> REQUIRED_COLUMNS = {{"STRING_IDS", {"id", "value"}},
    {"GENERIC_EVENT_SOURCES", {"sourceId", "nameId", "timeSourceId", "sourceGroupId", "data"}},
    {"GENERIC_EVENT_TYPES", {"typeId", "sourceId", "nameId", "ftraceFormat", "data"}},
    {"GENERIC_EVENT_TYPE_FIELDS",
        {"typeId", "fieldIdx", "fieldNameId", "offset", "size", "type", "ftracePrefix", "ftraceSuffix"}},
    {"GENERIC_EVENTS", {"genericEventId", "rawTimestamp", "timestamp", "typeId", "globalTid", "cpuId", "flags"}},
    {"GENERIC_EVENT_DATA", {"genericEventId", "fieldIdx", "intVal", "uintVal", "floatVal", "doubleVal", "strId"}}};

const std::map<std::string, std::vector<std::string>> PRIMARY_KEYS = {{"STRING_IDS", {"id"}},
    {"GENERIC_EVENT_SOURCES", {"sourceid"}}, {"GENERIC_EVENT_TYPES", {"typeid"}},
    {"GENERIC_EVENTS", {"genericeventid"}}};

bool Prepare(sqlite3 *db, const std::string &sql, Statement &statement, std::string &error) {
    sqlite3_stmt *raw = nullptr;
    const int result = sqlite3_prepare_v2(db, sql.c_str(), -1, &raw, nullptr);
    statement.reset(raw);
    if (result != SQLITE_OK) {
        error = std::string("Cannot read msprof ftrace database: ") + sqlite3_errmsg(db);
        return false;
    }
    return true;
}

std::string Text(sqlite3_stmt *statement, int column) {
    const auto *value = sqlite3_column_text(statement, column);
    if (value == nullptr) {
        return {};
    }
    return {reinterpret_cast<const char *>(value), static_cast<size_t>(sqlite3_column_bytes(statement, column))};
}

bool OpenReadOnly(const std::string &path, Connection &connection, std::string &error) {
#ifdef _WIN32
    const std::string utfPath = FileUtil::ConvertToLongPath(path);
#else
    const std::string &utfPath = path;
#endif
    sqlite3 *raw = nullptr;
    const int result = sqlite3_open_v2(utfPath.c_str(), &raw, SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
    connection.reset(raw);
    if (result != SQLITE_OK) {
        error = std::string("Cannot open msprof ftrace database read-only: ") +
            (raw == nullptr ? "SQLite open failed" : sqlite3_errmsg(raw));
        return false;
    }
    sqlite3_busy_timeout(raw, 5000);
    if (sqlite3_exec(raw, "BEGIN", nullptr, nullptr, nullptr) != SQLITE_OK) {
        error = std::string("Cannot establish msprof ftrace read snapshot: ") + sqlite3_errmsg(raw);
        return false;
    }
    return true;
}

bool TableExists(sqlite3 *db, const std::string &table) {
    Statement statement(nullptr, sqlite3_finalize);
    std::string error;
    if (!Prepare(
            db, "SELECT 1 FROM sqlite_master WHERE type='table' AND name=? COLLATE NOCASE LIMIT 1", statement, error)) {
        return false;
    }
    sqlite3_bind_text(statement.get(), 1, table.c_str(), -1, SQLITE_TRANSIENT);
    return sqlite3_step(statement.get()) == SQLITE_ROW;
}

bool HasMarker(sqlite3 *db) {
    if (!TableExists(db, "META_DATA")) {
        return false;
    }
    Statement statement(nullptr, sqlite3_finalize);
    std::string error;
    return Prepare(db, "SELECT 1 FROM META_DATA WHERE name='ftrace' LIMIT 1", statement, error) &&
        sqlite3_step(statement.get()) == SQLITE_ROW;
}

bool Metadata(sqlite3 *db, const std::string &name, std::string &value, std::string &error) {
    Statement statement(nullptr, sqlite3_finalize);
    if (!Prepare(db, "SELECT value FROM META_DATA WHERE name=?", statement, error)) {
        return false;
    }
    sqlite3_bind_text(statement.get(), 1, name.c_str(), -1, SQLITE_TRANSIENT);
    int step = sqlite3_step(statement.get());
    if (step == SQLITE_DONE) {
        value.clear();
        return true;
    }
    if (step != SQLITE_ROW) {
        error = sqlite3_errmsg(db);
        return false;
    }
    value = Text(statement.get(), 0);
    while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
        if (Text(statement.get(), 0) != value) {
            error = "Conflicting msprof metadata values for " + name;
            return false;
        }
    }
    if (step != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    return true;
}

bool ValidateColumns(sqlite3 *db, std::string &error) {
    for (const auto &table : REQUIRED_COLUMNS) {
        if (!TableExists(db, table.first)) {
            error = "Missing msprof ftrace table " + table.first;
            return false;
        }
        Statement statement(nullptr, sqlite3_finalize);
        if (!Prepare(db, "PRAGMA table_info(" + table.first + ")", statement, error)) {
            return false;
        }
        std::set<std::string> columns;
        std::map<int, std::string> primaryKey;
        int step;
        while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
            const auto column = StringUtil::ToLower(Text(statement.get(), 1));
            columns.insert(column);
            if (sqlite3_column_int(statement.get(), 5) > 0) {
                primaryKey.emplace(sqlite3_column_int(statement.get(), 5), column);
            }
        }
        if (step != SQLITE_DONE) {
            error = sqlite3_errmsg(db);
            return false;
        }
        for (const auto &column : table.second) {
            if (columns.count(StringUtil::ToLower(column)) == 0) {
                error = "Missing msprof ftrace column " + table.first + "." + column;
                return false;
            }
        }
        std::vector<std::string> keys;
        for (const auto &key : primaryKey) {
            keys.push_back(key.second);
        }
        // The upstream writer may implement composite keys with UNIQUE indexes, or omit
        // those indexes if their creation fails. Their actual rows are checked while reading.
        const auto identity = PRIMARY_KEYS.find(table.first);
        if (identity != PRIMARY_KEYS.end() && keys != identity->second) {
            error = "Invalid msprof ftrace primary key for " + table.first;
            return false;
        }
    }
    return true;
}

bool UnsignedInteger(sqlite3_stmt *statement, int column, uint64_t &value) {
    if (sqlite3_column_type(statement, column) != SQLITE_INTEGER || sqlite3_column_int64(statement, column) < 0) {
        return false;
    }
    value = static_cast<uint64_t>(sqlite3_column_int64(statement, column));
    return true;
}

MsprofFtraceInspection InspectConnection(sqlite3 *db) {
    MsprofFtraceInspection inspection;
    if (!HasMarker(db)) {
        return inspection;
    }
    inspection.status = MsprofFtraceStatus::INVALID;
    if (!Metadata(db, "SCHEMA_VERSION", inspection.schemaVersion, inspection.error)) {
        return inspection;
    }
    // The finalized zero-filled protocol uses exactly 1.2.0. Its feature marker,
    // complete flag, required columns and per-row sentinel validation remain mandatory.
    if (inspection.schemaVersion != "1.2.0") {
        inspection.status = MsprofFtraceStatus::UNSUPPORTED;
        inspection.error = "Unsupported msprof ftrace schema version (expected 1.2.0): " + inspection.schemaVersion;
        return inspection;
    }
    std::string complete;
    if (!Metadata(db, "ftrace_events_complete", complete, inspection.error)) {
        return inspection;
    }
    if (complete != "1") {
        inspection.status = MsprofFtraceStatus::INCOMPLETE;
        inspection.error = "msprof ftrace events have not been completely committed";
        return inspection;
    }
    if (!ValidateColumns(db, inspection.error)) {
        return inspection;
    }
    inspection.status = MsprofFtraceStatus::READY;
    return inspection;
}

bool ReadTypes(sqlite3 *db, TypeMap &types, std::string &error) {
    Statement statement(nullptr, sqlite3_finalize);
    const std::string sql = "SELECT t.typeId,t.sourceId,n.value,sn.value,t.ftraceFormat FROM GENERIC_EVENT_TYPES t "
                            "JOIN GENERIC_EVENT_SOURCES s ON s.sourceId=t.sourceId AND s.sourceGroupId=1 "
                            "LEFT JOIN STRING_IDS n ON n.id=t.nameId LEFT JOIN STRING_IDS sn ON sn.id=s.nameId "
                            "WHERE t.typeId IN (" +
        MsprofFtraceSupportedTypesSql() + ") ORDER BY t.typeId";
    if (!Prepare(db, sql, statement, error)) {
        return false;
    }
    int step;
    while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
        auto type = std::make_shared<MsprofFtraceType>();
        if (!UnsignedInteger(statement.get(), 0, type->typeId) ||
            !UnsignedInteger(statement.get(), 1, type->sourceId) ||
            sqlite3_column_type(statement.get(), 2) != SQLITE_TEXT || Text(statement.get(), 2).empty() ||
            sqlite3_column_type(statement.get(), 3) != SQLITE_TEXT || Text(statement.get(), 3).empty()) {
            error = "Invalid msprof ftrace type or source string reference";
            return false;
        }
        type->name = Text(statement.get(), 2);
        if (type->name.find(':') == std::string::npos) {
            type->name = Text(statement.get(), 3) + ":" + type->name;
        }
        type->format = Text(statement.get(), 4);
        if (!types.emplace(type->typeId, std::move(type)).second) {
            error = "Duplicate msprof ftrace type id";
            return false;
        }
    }
    if (step != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    return true;
}

bool ReadFields(sqlite3 *db, TypeMap &types, std::string &error) {
    Statement statement(nullptr, sqlite3_finalize);
    const std::string sql =
        "SELECT f.typeId,f.fieldIdx,n.value,f.offset,f.size,f.type FROM GENERIC_EVENT_TYPE_FIELDS f "
        "JOIN GENERIC_EVENT_TYPES t ON t.typeId=f.typeId "
        "JOIN GENERIC_EVENT_SOURCES s ON s.sourceId=t.sourceId AND s.sourceGroupId=1 "
        "LEFT JOIN STRING_IDS n ON n.id=f.fieldNameId WHERE f.typeId IN (" +
        MsprofFtraceSupportedTypesSql() + ") ORDER BY f.typeId,f.fieldIdx";
    if (!Prepare(db, sql, statement, error)) {
        return false;
    }
    std::map<uint64_t, std::set<std::string>> names;
    int step;
    while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
        uint64_t typeId = 0;
        uint64_t offset = 0;
        uint64_t size = 0;
        uint64_t scalarType = 0;
        MsprofFtraceField field;
        if (!UnsignedInteger(statement.get(), 0, typeId) || !UnsignedInteger(statement.get(), 1, field.index) ||
            !UnsignedInteger(statement.get(), 3, offset) || !UnsignedInteger(statement.get(), 4, size) ||
            !UnsignedInteger(statement.get(), 5, scalarType) || offset > UINT32_MAX || size > UINT32_MAX ||
            scalarType > 3 || sqlite3_column_type(statement.get(), 2) != SQLITE_TEXT ||
            Text(statement.get(), 2).empty() || types.count(typeId) == 0) {
            error = "Invalid msprof ftrace field definition";
            return false;
        }
        field.name = Text(statement.get(), 2);
        field.offset = static_cast<uint32_t>(offset);
        field.size = static_cast<uint32_t>(size);
        field.type = static_cast<uint32_t>(scalarType);
        if (!names[typeId].insert(field.name).second ||
            !types.at(typeId)->fields.emplace(field.index, std::move(field)).second) {
            error = "Duplicate msprof ftrace field definition";
            return false;
        }
    }
    if (step != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    return true;
}

bool ReadHeader(sqlite3_stmt *statement, const TypeMap &types, MsprofFtraceEvent &event, std::string &error) {
    uint64_t flags = 0;
    if (!UnsignedInteger(statement, 0, event.eventId) || !UnsignedInteger(statement, 1, event.rawTimestamp) ||
        !UnsignedInteger(statement, 2, event.timestampNs) || !UnsignedInteger(statement, 3, event.typeId) ||
        !UnsignedInteger(statement, 6, flags) || flags > UINT32_MAX || types.count(event.typeId) == 0) {
        error = "Invalid msprof ftrace event header";
        return false;
    }
    event.flags = static_cast<uint32_t>(flags);
    if ((event.flags & MSPROF_FTRACE_FLAG_FIELD_FAILED) == 0 && sqlite3_column_type(statement, 4) != SQLITE_NULL) {
        uint64_t globalTid = 0;
        if (!UnsignedInteger(statement, 4, globalTid)) {
            error = "Invalid msprof ftrace globalTid";
            return false;
        }
        event.globalTid = globalTid;
    }
    if (sqlite3_column_type(statement, 5) != SQLITE_NULL) {
        uint64_t cpu = 0;
        if (!UnsignedInteger(statement, 5, cpu) || cpu > UINT32_MAX) {
            error = "Invalid msprof ftrace cpuId";
            return false;
        }
        event.cpu = static_cast<uint32_t>(cpu);
    }
    event.type = types.at(event.typeId);
    event.sourceId = event.type->sourceId;
    event.name = event.type->name;
    return true;
}

bool DecodeValue(
    sqlite3_stmt *statement, const MsprofFtraceField &field, MsprofFtraceValue &value, std::string &error) {
    uint64_t stringId = 0;
    if (!UnsignedInteger(statement, 12, stringId)) {
        error = "Invalid msprof ftrace string sentinel for field " + field.name;
        return false;
    }
    if (stringId != 0) {
        if (field.type != 0 || sqlite3_column_type(statement, 13) != SQLITE_TEXT) {
            error = "Invalid msprof ftrace STRING_IDS reference for field " + field.name;
            return false;
        }
        value = Text(statement, 13);
        return true;
    }
    // The chosen column alone carries the value. Every unused column may contain
    // zero, so neither NULL counts nor physical field width can identify strings.
    const int populatedColumn = 8 + static_cast<int>(field.type);
    const int sqlType = sqlite3_column_type(statement, populatedColumn);
    if ((field.type <= 1 && sqlType != SQLITE_INTEGER) ||
        (field.type >= 2 && sqlType != SQLITE_INTEGER && sqlType != SQLITE_FLOAT)) {
        error = "Mismatched msprof ftrace value type for field " + field.name;
        return false;
    }
    if (field.type == 0) {
        value = static_cast<int64_t>(sqlite3_column_int64(statement, populatedColumn));
    } else if (field.type == 1) {
        const sqlite3_int64 number = sqlite3_column_int64(statement, populatedColumn);
        if (number < 0 && field.size != sizeof(uint64_t)) {
            error = "Invalid unsigned msprof ftrace value for field " + field.name;
            return false;
        }
        // SQLite INTEGER is signed; an explicitly unsigned 8-byte field retains its raw 64-bit pattern.
        value = static_cast<uint64_t>(number);
    } else {
        const double number = sqlite3_column_double(statement, populatedColumn);
        if (!std::isfinite(number)) {
            error = "Non-finite msprof ftrace value for field " + field.name;
            return false;
        }
        value = number;
    }
    return true;
}

bool ReadValue(sqlite3_stmt *statement, MsprofFtraceEvent &event, std::set<uint64_t> &indexes, std::string &error) {
    if (sqlite3_column_type(statement, 7) == SQLITE_NULL) {
        return true;
    }
    uint64_t index = 0;
    if (!UnsignedInteger(statement, 7, index) || event.type->fields.count(index) == 0) {
        error = "Unknown msprof ftrace field index for event " + std::to_string(event.eventId);
        return false;
    }
    // A failed event still validates its key structure, but its placeholder values
    // and derived thread identity are not trustworthy. Emit only its header as a barrier.
    if ((event.flags & MSPROF_FTRACE_FLAG_FIELD_FAILED) != 0) {
        if (!indexes.insert(index).second) {
            error = "Duplicate msprof ftrace field value for event " + std::to_string(event.eventId);
            return false;
        }
        return true;
    }
    const auto &field = event.type->fields.at(index);
    MsprofFtraceValue value;
    if (!DecodeValue(statement, field, value, error)) {
        error += " (event " + std::to_string(event.eventId) + ")";
        return false;
    }
    if (!event.fields.emplace(field.name, std::move(value)).second) {
        error = "Duplicate msprof ftrace field value for event " + std::to_string(event.eventId);
        return false;
    }
    return true;
}

struct ProgressContext {
    const MsprofFtraceReader::Cancelled *cancelled = nullptr;
    bool interrupted = false;
};

int CheckCancelled(void *data) {
    auto *context = static_cast<ProgressContext *>(data);
    try {
        context->interrupted = context->cancelled != nullptr && *context->cancelled && (*context->cancelled)();
    } catch (...) {
        context->interrupted = true;
    }
    return context->interrupted ? 1 : 0;
}

class ProgressGuard {
  public:
    ProgressGuard(sqlite3 *db, const MsprofFtraceReader::Cancelled &cancelled) : db_(db), context{&cancelled, false} {
        sqlite3_progress_handler(db_, 10000, CheckCancelled, &context);
    }
    ~ProgressGuard() { sqlite3_progress_handler(db_, 0, nullptr, nullptr); }
    ProgressGuard(const ProgressGuard &) = delete;
    ProgressGuard &operator=(const ProgressGuard &) = delete;

  private:
    sqlite3 *db_;

  public:
    ProgressContext context;
};

bool Emit(const MsprofFtraceEvent &event, const MsprofFtraceReader::Consumer &consumer, std::string &error) {
    if ((event.flags & MSPROF_FTRACE_FLAG_FIELD_FAILED) == 0 && event.fields.size() != event.type->fields.size()) {
        error = "Missing msprof ftrace field values for event " + std::to_string(event.eventId);
        return false;
    }
    if (!consumer(event)) {
        error = "msprof ftrace read cancelled by consumer";
        return false;
    }
    return true;
}

std::string QuoteIdentifier(const std::string &name) {
    std::string result = "\"";
    for (const auto character : name) {
        if (character == '"') {
            result += '"';
        }
        result += character;
    }
    return result + '"';
}

bool FindTimestampIndex(sqlite3 *db, std::string &name, std::string &error) {
    Statement indexes(nullptr, sqlite3_finalize);
    if (!Prepare(db, "PRAGMA index_list(GENERIC_EVENTS)", indexes, error)) {
        return false;
    }
    int step;
    while ((step = sqlite3_step(indexes.get())) == SQLITE_ROW) {
        if (sqlite3_column_int(indexes.get(), 4) != 0) {
            continue; // A partial index cannot cover every supported event.
        }
        const auto candidate = Text(indexes.get(), 1);
        Statement details(nullptr, sqlite3_finalize);
        if (!Prepare(db, "PRAGMA index_info(" + QuoteIdentifier(candidate) + ")", details, error)) {
            return false;
        }
        std::vector<std::string> columns;
        int detailStep;
        while ((detailStep = sqlite3_step(details.get())) == SQLITE_ROW) {
            columns.push_back(StringUtil::ToLower(Text(details.get(), 2)));
        }
        if (detailStep != SQLITE_DONE) {
            error = sqlite3_errmsg(db);
            return false;
        }
        if (columns == std::vector<std::string>{"timestamp"} ||
            columns == std::vector<std::string>{"timestamp", "genericeventid"}) {
            name = candidate;
            return true;
        }
    }
    if (step != SQLITE_DONE) {
        error = sqlite3_errmsg(db);
        return false;
    }
    return true;
}

bool ReadStream(sqlite3 *db, const TypeMap &types, const MsprofFtraceReader::Consumer &consumer,
    const MsprofFtraceReader::Cancelled &cancelled, std::string &error) {
    std::string timestampIndex;
    if (!FindTimestampIndex(db, timestampIndex, error)) {
        return false;
    }
    const std::string eventTable = "GENERIC_EVENTS e" +
        (timestampIndex.empty() ? std::string{} : " INDEXED BY " + QuoteIdentifier(timestampIndex));
    // Keep events as the outer, timestamp-ordered scan. The supported-type subquery
    // already validates the source join; repeating it here can make SQLite sort every EAV row.
    const std::string sql = "SELECT e.genericEventId,e.rawTimestamp,e.timestamp,e.typeId,e.globalTid,e.cpuId,e.flags,"
                            "d.fieldIdx,d.intVal,d.uintVal,d.floatVal,d.doubleVal,d.strId,sv.value FROM " +
        eventTable +
        " "
        "LEFT JOIN GENERIC_EVENT_DATA d ON d.genericEventId=e.genericEventId "
        "LEFT JOIN STRING_IDS sv ON sv.id=d.strId AND d.strId>0 AND (e.flags & 2)=0 WHERE e.typeId IN (" +
        MsprofFtraceSupportedTypesSql() + ") ORDER BY e.timestamp,e.genericEventId,d.fieldIdx";
    Statement statement(nullptr, sqlite3_finalize);
    if (!Prepare(db, sql, statement, error)) {
        return false;
    }
    std::optional<MsprofFtraceEvent> current;
    std::set<uint64_t> indexes;
    int step;
    while ((step = sqlite3_step(statement.get())) == SQLITE_ROW) {
        if (cancelled && cancelled()) {
            error = "msprof ftrace read cancelled";
            return false;
        }
        const uint64_t eventId = static_cast<uint64_t>(sqlite3_column_int64(statement.get(), 0));
        if (!current || current->eventId != eventId) {
            if (current && !Emit(*current, consumer, error)) {
                return false;
            }
            current.emplace();
            indexes.clear();
            if (!ReadHeader(statement.get(), types, *current, error)) {
                return false;
            }
        }
        if (!ReadValue(statement.get(), *current, indexes, error)) {
            return false;
        }
    }
    if (step != SQLITE_DONE) {
        error = std::string("Cannot stream msprof ftrace values: ") + sqlite3_errmsg(db);
        return false;
    }
    return !current || Emit(*current, consumer, error);
}

}

MsprofFtraceInspection MsprofFtraceReader::Inspect(const std::string &path) {
    Connection connection(nullptr, sqlite3_close);
    std::string error;
    if (!OpenReadOnly(path, connection, error)) {
        MsprofFtraceInspection inspection;
        inspection.status = MsprofFtraceStatus::INVALID;
        inspection.error = error;
        return inspection;
    }
    auto inspection = InspectConnection(connection.get());
    if (!inspection.IsReady()) {
        return inspection;
    }
    TypeMap types;
    if (!ReadTypes(connection.get(), types, inspection.error) ||
        !ReadFields(connection.get(), types, inspection.error)) {
        inspection.status = MsprofFtraceStatus::INVALID;
        return inspection;
    }
    return inspection;
}

bool MsprofFtraceReader::Read(
    const std::string &path, const Consumer &consumer, std::string &error, const Cancelled &cancelled) {
    error.clear();
    if (!consumer) {
        error = "A msprof ftrace event consumer is required";
        return false;
    }
    if (cancelled && cancelled()) {
        error = "msprof ftrace read cancelled";
        return false;
    }
    Connection connection(nullptr, sqlite3_close);
    if (!OpenReadOnly(path, connection, error)) {
        return false;
    }
    ProgressGuard progress(connection.get(), cancelled);
    const auto inspection = InspectConnection(connection.get());
    if (!inspection.IsReady()) {
        error = inspection.error.empty() ? "Source has no msprof ftrace marker" : inspection.error;
        return false;
    }
    TypeMap types;
    const bool result = ReadTypes(connection.get(), types, error) && ReadFields(connection.get(), types, error) &&
        ReadStream(connection.get(), types, consumer, cancelled, error);
    if (progress.context.interrupted) {
        error = "msprof ftrace read cancelled";
    }
    return result;
}

}
