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
#include "EventUtil.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Server;
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
    event->ts = NumberUtil::ConvertUsStrToNanoseconds(JsonUtil::GetDumpString(json, "ts"));
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
    event->ts = NumberUtil::ConvertUsStrToNanoseconds(JsonUtil::GetDumpString(json, "ts"));
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
    event->ts = NumberUtil::ConvertUsStrToNanoseconds(JsonUtil::GetDumpString(json, "ts"));
    event->cat = JsonUtil::GetOptionalString(json, "cat");
    event->args = JsonUtil::GetDumpString(json, "args");
    return event.get();
}
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
