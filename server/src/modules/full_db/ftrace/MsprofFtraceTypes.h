/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * This file is licensed under Mulan PSL v2.
 * http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_MSPROFFTRACETYPES_H
#define PROFILER_SERVER_MSPROFFTRACETYPES_H

#include <cstdint>
#include <array>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <variant>

namespace Dic::Module::FullDb {

inline constexpr uint32_t MSPROF_FTRACE_FLAG_FIELD_FAILED = uint32_t{1} << 1;

// The adapter builds scheduling and interrupt tracks only. Other tracepoints stay in the source DB.
inline constexpr std::array<const char *, 8> MSPROF_FTRACE_SCHEDULING_EVENTS = {"sched:sched_switch",
    "sched:sched_wakeup", "sched:sched_wakeup_new", "sched:sched_process_exit", "irq:softirq_entry", "irq:softirq_exit",
    "irq:irq_handler_entry", "irq:irq_handler_exit"};

inline std::string MsprofFtraceSupportedTypesSql() {
    std::string sql = "SELECT t.typeId FROM GENERIC_EVENT_TYPES t "
                      "JOIN GENERIC_EVENT_SOURCES s ON s.sourceId=t.sourceId AND s.sourceGroupId=1 "
                      "JOIN STRING_IDS n ON n.id=t.nameId LEFT JOIN STRING_IDS sn ON sn.id=s.nameId "
                      "WHERE (CASE WHEN instr(n.value,':')>0 THEN n.value ELSE sn.value||':'||n.value END) IN (";
    const char *separator = "";
    for (const auto *name : MSPROF_FTRACE_SCHEDULING_EVENTS) {
        sql += separator;
        sql += "'";
        sql += name;
        sql += "'";
        separator = ",";
    }
    return sql + ")";
}

using MsprofFtraceValue = std::variant<std::monostate, int64_t, uint64_t, double, std::string>;

struct MsprofFtraceField {
    uint64_t index = 0;
    std::string name;
    uint32_t offset = 0;
    uint32_t size = 0;
    uint32_t type = 0;
};

struct MsprofFtraceType {
    uint64_t typeId = 0;
    uint64_t sourceId = 0;
    std::string name;
    std::string format;
    std::map<uint64_t, MsprofFtraceField> fields;
};

struct MsprofFtraceEvent {
    uint64_t eventId = 0;
    uint64_t timestampNs = 0;
    uint64_t rawTimestamp = 0;
    uint64_t sourceId = 0;
    uint64_t typeId = 0;
    std::optional<uint32_t> cpu;
    std::optional<uint64_t> globalTid;
    uint32_t flags = 0;
    std::string name;
    std::map<std::string, MsprofFtraceValue> fields;
    std::shared_ptr<const MsprofFtraceType> type;
};

enum class MsprofFtraceStatus { ABSENT, READY, INCOMPLETE, UNSUPPORTED, INVALID };

struct MsprofFtraceInspection {
    MsprofFtraceStatus status = MsprofFtraceStatus::ABSENT;
    std::string error;
    std::string schemaVersion;
    uint64_t eventCount = 0; // Only tracepoints supported by the scheduling/interrupt adapter.
    uint64_t minTimestampNs = 0;
    uint64_t maxTimestampNs = 0;
    std::set<std::string> eventNames;

    bool HasFtrace() const { return status != MsprofFtraceStatus::ABSENT; }
    bool IsReady() const { return status == MsprofFtraceStatus::READY; }
};

}
#endif
