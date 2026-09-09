/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2025 Huawei Technologies Co.,Ltd.
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
#include "SafeFile.h"
#include "EventUtil.h"
#include "ServerLog.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Server;
namespace {
thread_local int64_t g_timestampOffsetNs = 0;

bool IsSuccessfulZeroTimestamp(const EventUtil::json_t &json) {
    if (!json.HasMember("ts") || json["ts"].IsNull()) {
        return false;
    }
    const auto &tsVal = json["ts"];
    if (tsVal.IsNumber()) {
        return tsVal.GetDouble() == 0;
    }
    if (tsVal.IsString()) {
        const char *ts = tsVal.GetString();
        return ts != nullptr && ts[0] == '0';
    }
    return false;
}

int64_t AddTimestampOffset(int64_t ts, int64_t offset) {
    if (offset > 0 && ts > INT64_MAX - offset) {
        ServerLog::Warn("Skip timestamp offset due to overflow. ts:", ts, ", offset:", offset);
        return ts;
    }
    if (offset < 0 && ts < INT64_MIN - offset) {
        ServerLog::Warn("Skip timestamp offset due to overflow. ts:", ts, ", offset:", offset);
        return ts;
    }
    return ts + offset;
}

int64_t ConvertTsToNs(const EventUtil::json_t &json) {
    int64_t ts = NumberUtil::ConvertUsStrToNanoseconds(JsonUtil::GetDumpString(json, "ts"));
    if (g_timestampOffsetNs == 0) {
        return ts;
    }
    if (ts == 0 && !IsSuccessfulZeroTimestamp(json)) {
        return 0;
    }
    return AddTimestampOffset(ts, g_timestampOffsetNs);
}
} // namespace
EventUtil::EventUtil() { Register(); }

EventUtil::~EventUtil() { UnRegister(); }

void EventUtil::Register() {
    jsonToEventFactory.emplace("M", ToMetaDataEvent);
    jsonToEventFactory.emplace("X", ToSliceEvent);
    jsonToEventFactory.emplace("I", ToSliceEvent);
    jsonToEventFactory.emplace("SX", ToSimulationSliceEvent);
    jsonToEventFactory.emplace("SB", ToSimulationBeginSliceEvent);
    jsonToEventFactory.emplace("SE", ToSimulationEndSliceEvent);
    jsonToEventFactory.emplace("Ss", ToFlowEvent);
    jsonToEventFactory.emplace("St", ToFlowEvent);
    jsonToEventFactory.emplace("SM", ToMetaDataEvent);
    jsonToEventFactory.emplace("s", ToFlowEvent);
    jsonToEventFactory.emplace("f", ToFlowEvent);
    jsonToEventFactory.emplace("t", ToFlowEvent);
    jsonToEventFactory.emplace("C", ToCounterEvent);
    jsonToEventFactory.emplace("SC", ToCounterEvent);
}

void EventUtil::UnRegister() { jsonToEventFactory.clear(); }

std::string EventUtil::Type(const json_t &json) {
    if (json.HasMember("ph") && json["ph"].IsString()) {
        return json["ph"].GetString();
    }
    return "";
}

Trace::Event *EventUtil::FromJson(const json_t &json, const std::string &type) {
    if (type.empty()) {
        return nullptr;
    }
    std::optional<EventUtil::JsonToEventFunc> func = GetJsonToEventFunc(type);
    if (!func.has_value()) {
        return nullptr;
    }
    return func.value()(json);
}

std::optional<EventUtil::JsonToEventFunc> EventUtil::GetJsonToEventFunc(const std::string &type) {
    if (jsonToEventFactory.count(type) == 0) {
        return std::nullopt;
    }
    return jsonToEventFactory[type];
}

Trace::Event *EventUtil::ToSliceEvent(const json_t &json) {
    thread_local static std::shared_ptr<Slice> event = std::make_shared<Slice>();
    event->type = Type(json);
    event->ts = ConvertTsToNs(json);
    event->dur = NumberUtil::ConvertUsStrToNanoseconds(JsonUtil::GetDumpString(json, "dur"));
    event->name = JsonUtil::GetString(json, "name");
    event->tid = JsonUtil::GetDumpString(json, "tid");
    event->pid = JsonUtil::GetDumpString(json, "pid");
    event->cat = JsonUtil::GetOptionalString(json, "cat");
    event->args = JsonUtil::GetOptionalString(json, "args");
    event->groupId = JsonUtil::GetString(json, "group_id");
    return event.get();
}

Trace::Event *EventUtil::ToSimulationSliceEvent(const json_t &json) {
    thread_local static std::shared_ptr<Slice> event = std::make_shared<Slice>();
    event->type = "SX";
    long double start = JsonUtil::GetLongDouble(json, "ts");
    double tempDur = JsonUtil::GetDouble(json, "dur");
    event->ts = NumberUtil::TimestampUsToNs(start);
    long double end = start > std::numeric_limits<long double>::max() - tempDur ? 0 : start + tempDur;
    event->end = NumberUtil::TimestampUsToNs(end);
    event->dur = event->end > event->ts ? event->end - event->ts : 0;
    event->name = JsonUtil::GetString(json, "name");
    event->threadName = JsonUtil::GetString(json, "tid");
    event->processName = JsonUtil::GetString(json, "pid");
    event->cname = JsonUtil::GetString(json, "cname");
    event->args = JsonUtil::GetOptionalString(json, "args");
    event->groupId = JsonUtil::GetString(json, "group_id");
    return event.get();
}

Trace::Event *EventUtil::ToSimulationBeginSliceEvent(const json_t &json) {
    thread_local static std::shared_ptr<Slice> event = std::make_shared<Slice>();
    event->type = "SB";
    event->ts = NumberUtil::TimestampUsToNs(JsonUtil::GetLongDouble(json, "ts"));
    event->name = JsonUtil::GetString(json, "name");
    event->threadName = JsonUtil::GetString(json, "tid");
    event->processName = JsonUtil::GetString(json, "pid");
    event->cname = JsonUtil::GetString(json, "cname");
    event->args = JsonUtil::GetOptionalString(json, "args");
    event->flagId = JsonUtil::GetDumpString(json, "id");
    event->groupId = JsonUtil::GetString(json, "group_id");
    return event.get();
}

Trace::Event *EventUtil::ToSimulationEndSliceEvent(const json_t &json) {
    thread_local static std::shared_ptr<Slice> event = std::make_shared<Slice>();
    event->type = "SE";
    event->ts = NumberUtil::TimestampUsToNs(JsonUtil::GetLongDouble(json, "ts"));
    event->name = JsonUtil::GetString(json, "name");
    event->flagId = JsonUtil::GetDumpString(json, "id");
    event->groupId = JsonUtil::GetString(json, "group_id");
    return event.get();
}

Trace::Event *EventUtil::ToMetaDataEvent(const json_t &json) {
    thread_local static std::shared_ptr<MetaData> event = std::make_shared<MetaData>();
    event->type = Type(json);
    event->name = JsonUtil::GetString(json, "name");
    event->tid = JsonUtil::GetDumpString(json, "tid");
    event->pid = JsonUtil::GetDumpString(json, "pid");
    if (json.HasMember("args")) {
        event->args.name = JsonUtil::GetString(json["args"], "name");
        event->args.labels = JsonUtil::GetString(json["args"], "labels");
        event->args.sortIndex = JsonUtil::GetInteger(json["args"], "sort_index");
    }
    return event.get();
}

Trace::Event *EventUtil::ToFlowEvent(const json_t &json) {
    thread_local static std::shared_ptr<Flow> event = std::make_shared<Flow>();
    event->type = Type(json);
    event->ts = ConvertTsToNs(json);
    event->tid = JsonUtil::GetDumpString(json, "tid");
    event->pid = JsonUtil::GetDumpString(json, "pid");
    event->flowId = JsonUtil::GetDumpString(json, "id");
    event->name = JsonUtil::GetString(json, "name");
    event->cat = JsonUtil::GetOptionalString(json, "cat");
    return event.get();
}

Trace::Event *EventUtil::ToCounterEvent(const json_t &json) {
    thread_local static std::shared_ptr<Counter> event = std::make_shared<Counter>();
    event->type = Type(json);
    if (json.HasMember("id")) {
        event->name = JsonUtil::GetString(json, "name") + "[" + std::to_string(JsonUtil::GetInteger(json, "id")) + "]";
    } else {
        event->name = JsonUtil::GetString(json, "name");
    }
    event->pid = JsonUtil::GetDumpString(json, "pid");
    event->tid = JsonUtil::GetDumpString(json, "name");
    event->ts = ConvertTsToNs(json);
    event->cat = JsonUtil::GetOptionalString(json, "cat");
    event->args = JsonUtil::GetDumpString(json, "args");
    return event.get();
}

Trace::Event *EventUtil::TryToCpuTensorAllocatedCounter(const json_t &json) {
    constexpr const char *kMemoryEventName = "[memory]";
    constexpr const char *kLaneName = "CPU Tensor Allocated";
    constexpr int64_t kCpuDeviceType = 0;
    if (JsonUtil::GetString(json, "name") != kMemoryEventName) {
        return nullptr;
    }
    if (!json.HasMember("args") || !json["args"].IsObject()) {
        return nullptr;
    }
    const auto &args = json["args"];
    if (!args.HasMember("Total Allocated")) {
        return nullptr;
    }
    if (args.HasMember("Device Type") && JsonUtil::GetInteger(args, "Device Type") != kCpuDeviceType) {
        return nullptr;
    }
    thread_local static std::shared_ptr<Counter> event = std::make_shared<Counter>();
    event->type = Type(json);
    event->name = kLaneName;
    event->tid = kLaneName;
    event->pid = JsonUtil::GetDumpString(json, "pid");
    event->ts = ConvertTsToNs(json);
    event->cat = JsonUtil::GetOptionalString(json, "cat");
    event->args = "{\"Allocated (B)\":" + std::to_string(JsonUtil::GetInteger(args, "Total Allocated")) + "}";
    return event.get();
}

void EventUtil::SetTimestampOffsetNs(int64_t ns) { g_timestampOffsetNs = ns; }

int64_t EventUtil::ParseBaseTimeNanoseconds(std::string_view header) {
    constexpr std::string_view kKey = "\"baseTimeNanoseconds\"";
    const size_t keyPos = header.find(kKey);
    if (keyPos == std::string_view::npos) {
        return 0;
    }
    size_t pos = keyPos + kKey.size();
    while (pos < header.size() &&
        (header[pos] == ' ' || header[pos] == '\t' || header[pos] == ':' || header[pos] == '"' || header[pos] == '\r' ||
            header[pos] == '\n')) {
        ++pos;
    }
    if (pos >= header.size() || header[pos] < '0' || header[pos] > '9') {
        return 0;
    }
    int64_t value = 0;
    while (pos < header.size() && header[pos] >= '0' && header[pos] <= '9') {
        const int digit = header[pos] - '0';
        if (value > (INT64_MAX - digit) / 10) {
            return 0;
        }
        value = value * 10 + digit;
        ++pos;
    }
    return value;
}

int64_t EventUtil::ReadBaseTimeNanosecondsFromFile(const std::string &filePath) {
    std::ifstream file = OpenReadFileSafely(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return 0;
    }
    constexpr size_t kChunkSize = 4096;
    constexpr size_t kMaxHeaderBytes = 256 * 1024;
    std::string header;
    header.reserve(kChunkSize);
    while (file && header.size() < kMaxHeaderBytes) {
        char buffer[kChunkSize];
        file.read(buffer, sizeof(buffer));
        const auto readCount = file.gcount();
        if (readCount <= 0) {
            break;
        }
        header.append(buffer, static_cast<size_t>(readCount));
        if (header.find("\"baseTimeNanoseconds\"") != std::string::npos ||
            header.find("\"traceEvents\"") != std::string::npos) {
            break;
        }
    }
    return ParseBaseTimeNanoseconds(header);
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
