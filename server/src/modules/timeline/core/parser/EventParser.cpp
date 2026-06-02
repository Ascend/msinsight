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
#include "TrackInfoManager.h"
#include "ParserStatusManager.h"
#include "SimulationSliceCacheManager.h"
#include "FileReader.h"
#include "JsonParseMemPool.h"
#include "EventParser.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;
using json_t = rapidjson::Value;
EventParser::EventParser(
    const std::string &filePath, const std::string &fileId, std::shared_ptr<TextTraceDatabase> textDatabase)
    : filePath(filePath), fileId(fileId), database(std::move(textDatabase)) {
    ServerLog::Info("Init event parser. fileId:", fileId);
    InitEventHandle();
    fileReader = std::make_unique<FileReader>();
}

void EventParser::InitEventHandle() {
    eventHandleMap.emplace("M", std::bind(&EventParser::MetaDataHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("X", std::bind(&EventParser::CompleteEventsHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("I", std::bind(&EventParser::CompleteEventsHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("C", std::bind(&EventParser::CounterEventsHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("s", std::bind(&EventParser::FlowEventsHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("t", std::bind(&EventParser::FlowEventsHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("f", std::bind(&EventParser::FlowEventsHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("SX", std::bind(&EventParser::SimulationEventHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("SB", std::bind(&EventParser::SimulationBeginEventHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("SE", std::bind(&EventParser::SimulationEndEventHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("Ss", std::bind(&EventParser::SimulationFlowEventsHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("St", std::bind(&EventParser::SimulationFlowEventsHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("SM", std::bind(&EventParser::MetaDataHandle, this, std::placeholders::_1));
    eventHandleMap.emplace("SC", std::bind(&EventParser::CounterEventsHandle, this, std::placeholders::_1));
}

bool EventParser::Parse(int64_t startPosition, int64_t endPosition) {
    database->InitStmt();
    std::string buffer = fileReader->ReadJsonArray(filePath, startPosition, endPosition);
    if (buffer.empty()) {
        error = "Failed to read file when parse. file path is: " + filePath;
        ServerLog::Error("Event parser failed to read buffer. fileId:", fileId);
        return false;
    }
    // 通过静态线程级的内存池,预分配5M的大小,获得线程相关的线程性能提升，更大的内存池略微提高性能，但是会带来较大的内存占用
    // 这里包含一个隐藏逻辑，当前json切片为50M，当前线程在解析过较大切片后，不用再重新申请内存池，后续优化下这块内存
    auto allocator = Dic::Module::JsonParseMemPool::Instance().GetMemBuff(std::this_thread::get_id());
    document_t doc(allocator.get());
    doc.Parse<kParseNumbersAsStringsFlag>(buffer.data());
    if (doc.HasParseError()) {
        error = "File is not valid json. " + error;
        ServerLog::Error("Event Parser. fileId:", fileId, ". ", error);
        return false;
    }
    if (!doc.IsArray()) {
        error = "json is not an array.";
        ServerLog::Error("Event Parser. json is not an array. fileId:", fileId);
        return false;
    }

    for (auto &event : doc.GetArray()) {
        if (ParserStatusManager::Instance().GetParserStatus(fileId) != ParserStatus::RUNNING) {
            return false;
        }
        EventHandle(event);
    }
    ProcessLastFlagSlice();
    database->CommitData();
    ServerLog::Info("Event Parser. fileId:", fileId, " Parse ", startPosition, " to ", endPosition,
        ". Count:", parseCount, ", ignore Count:", ignoreCount);
    return true;
}

void EventParser::ProcessLastFlagSlice() {
    if (!waitFlagSliceMap.empty() || !setFlagSliceMap.empty()) {
        std::vector<Slice> sliceVec =
            SimulationSliceCacheManager::Instance().GetCompeteSlice(setFlagSliceMap, waitFlagSliceMap, fileId);
        for (const auto &item : sliceVec) {
            database->InsertSlice(item);
        }
    }
}

void EventParser::SetSimulationStatus(const bool &isSimulation) { m_isSimulation = isSimulation; }

void EventParser::EventHandle(const json_t &json) {
    std::string type = EventUtil::Type(json);
    if (m_isSimulation) {
        type = "S" + type;
    }
    if (type.empty()) {
        return;
    }
    if (eventHandleMap.count(type) > 0) {
        parseCount++;
        eventHandleMap.at(type)(EventUtil::Instance().FromJson(json, type));
    } else {
        ignoreCount++;
    }
}

void EventParser::MetaDataHandle(Trace::Event *eventPtr) {
    if (eventPtr == nullptr) {
        return;
    }
    auto &event = dynamic_cast<Trace::MetaData &>(*eventPtr);
    if (event.name == "process_name") {
        database->UpdateProcessName(event);
    } else if (event.name == "thread_name") {
        event.trackId = GetTrackId(event.pid, event.tid);
        database->UpdateThreadName(event);
    } else if (event.name == "process_labels") {
        database->UpdateProcessLabel(event);
    } else if (event.name == "process_sort_index") {
        database->UpdateProcessSortIndex(event);
    } else if (event.name == "thread_sort_index") {
        event.trackId = GetTrackId(event.pid, event.tid);
        database->UpdateThreadSortIndex(event);
    } else {
        ServerLog::Error("Event Parser. Failed to get meta data type. name: %", event.name);
    }
}

void EventParser::CompleteEventsHandle(Trace::Event *eventPtr) {
    if (eventPtr == nullptr) {
        return;
    }
    auto &event = dynamic_cast<Trace::Slice &>(*eventPtr);
    event.trackId = GetTrackId(event.pid, event.tid);
    event.end = event.ts + event.dur;
    ProcessEvent processEvent;
    processEvent.pid = event.pid;
    processEvent.processName = event.pid;
    ThreadEvent threadEvent;
    threadEvent.trackId = event.trackId;
    threadEvent.tid = event.tid;
    threadEvent.pid = event.pid;
    threadEvent.threadName = event.tid;
    database->AddSimulationProcessCache(std::move(processEvent));
    database->AddSimulationThreadCache(std::move(threadEvent));
    database->InsertSlice(event);
}

void EventParser::SimulationBeginEventHandle(Trace::Event *eventPtr) {
    if (eventPtr == nullptr) {
        return;
    }
    auto &event = dynamic_cast<Trace::Slice &>(*eventPtr);
    if (event.name == "SET_FLAG" || event.name == "set_event") {
        if (setFlagSliceMap.find(event.flagId) == setFlagSliceMap.end()) {
            setFlagSliceMap[event.flagId] = event;
            return;
        } else {
            event.dur = setFlagSliceMap[event.flagId].ts > event.ts ? setFlagSliceMap[event.flagId].ts - event.ts : 0;
            SimulationEventHandle(eventPtr);
            setFlagSliceMap.erase(event.flagId);
            return;
        }
    }

    if (event.name == "WAIT_FLAG" || event.name == "wait_event") {
        if (waitFlagSliceMap.find(event.flagId) == waitFlagSliceMap.end()) {
            waitFlagSliceMap[event.flagId] = event;
            return;
        } else {
            event.dur = waitFlagSliceMap[event.flagId].ts > event.ts ? waitFlagSliceMap[event.flagId].ts - event.ts : 0;
            SimulationEventHandle(eventPtr);
            waitFlagSliceMap.erase(event.flagId);
            return;
        }
    }
}

void EventParser::SimulationEndEventHandle(Trace::Event *eventPtr) {
    if (eventPtr == nullptr) {
        return;
    }
    auto &event = dynamic_cast<Trace::Slice &>(*eventPtr);

    if (event.name == "SET_FLAG" || event.name == "set_event") {
        if (setFlagSliceMap.find(event.flagId) == setFlagSliceMap.end()) {
            setFlagSliceMap[event.flagId] = event;
            return;
        } else {
            Trace::Slice beginSlice = setFlagSliceMap[event.flagId];
            beginSlice.dur = event.ts > beginSlice.ts ? event.ts - beginSlice.ts : 0;
            SimulationEventHandle(&beginSlice);
            setFlagSliceMap.erase(event.flagId);
            return;
        }
    }

    if (event.name == "WAIT_FLAG" || event.name == "wait_event") {
        if (waitFlagSliceMap.find(event.flagId) == waitFlagSliceMap.end()) {
            waitFlagSliceMap[event.flagId] = event;
            return;
        } else {
            Trace::Slice beginSlice = waitFlagSliceMap[event.flagId];
            beginSlice.dur = event.ts > beginSlice.ts ? event.ts - beginSlice.ts : 0;
            SimulationEventHandle(&beginSlice);
            waitFlagSliceMap.erase(event.flagId);
            return;
        }
    }
}

void EventParser::SimulationEventHandle(Trace::Event *eventPtr) {
    if (eventPtr == nullptr) {
        return;
    }
    auto &event = dynamic_cast<Trace::Slice &>(*eventPtr);
    if (event.processName.empty() || event.threadName.empty()) {
        ServerLog::Error("processName or threadName is empty");
        return;
    }
    event.pid = event.processName;
    event.tid = event.threadName;
    event.trackId = GetTrackId(event.pid, event.tid);
    event.end = event.ts + event.dur;
    // thread泳道排序逻辑：
    // 同pid的泳道，先按照thread_sort_index升序，同thread_sort_index按照thread_name升序
    // thread_sort_index的来源：
    // 采集侧通过Metadata Events类thread_sort_index事件指定，优先级最高
    // 采集侧无Metadata Events时，代码通过SetThreadSortIndex()指定排序顺序，但优先级低于Metadata Events，所以相关SQL里得满足当前表中thread_sort_index为空才会插入
    ThreadEvent threadEvent;
    threadEvent.trackId = event.trackId;
    threadEvent.tid = event.tid;
    threadEvent.pid = event.pid;
    threadEvent.threadName = event.threadName;
    threadEvent.SetThreadSortIndex();
    // process泳道排序逻辑：
    // 先按照process_sort_index升序，同process_sort_index按照process_name升序
    // process_sort_index的来源：
    // 采集侧通过Metadata Events类process_sort_index事件指定，优先级最高
    // 采集侧无Metadata Events时，代码未指定排序顺序，即使后续指定，优先级也低于Metadata Events，所以相关SQL里得满足当前表中process_sort_index为空才会插入
    ProcessEvent processEvent;
    processEvent.pid = event.pid;
    processEvent.processName = event.processName;
    database->InsertSlice(event);
    database->AddSimulationProcessCache(std::move(processEvent));
    database->AddSimulationThreadCache(std::move(threadEvent));
}

void EventParser::FlowEventsHandle(Trace::Event *eventPtr) {
    if (eventPtr == nullptr) {
        return;
    }
    auto &event = dynamic_cast<Trace::Flow &>(*eventPtr);
    event.trackId = GetTrackId(event.pid, event.tid);
    ThreadEvent threadEvent;
    threadEvent.trackId = event.trackId;
    threadEvent.tid = event.tid;
    threadEvent.pid = event.pid;
    threadEvent.threadName = event.tid;
    ProcessEvent processEvent;
    processEvent.pid = event.pid;
    processEvent.processName = event.pid;
    database->AddSimulationProcessCache(std::move(processEvent));
    database->AddSimulationThreadCache(std::move(threadEvent));
    database->InsertFlow(event);
}

void EventParser::SimulationFlowEventsHandle(Trace::Event *eventPtr) {
    if (eventPtr == nullptr) {
        return;
    }
    auto &event = dynamic_cast<Trace::Flow &>(*eventPtr);
    std::string processName = event.pid;
    std::string threadName = event.tid;
    event.trackId = GetTrackId(event.pid, event.tid);
    ThreadEvent threadEvent;
    threadEvent.trackId = event.trackId;
    threadEvent.tid = event.tid;
    threadEvent.pid = event.pid;
    threadEvent.threadName = threadName;
    threadEvent.SetThreadSortIndex();
    ProcessEvent processEvent;
    processEvent.pid = event.pid;
    processEvent.processName = processName;
    database->AddSimulationProcessCache(std::move(processEvent));
    database->AddSimulationThreadCache(std::move(threadEvent));
    database->InsertFlow(event);
}

void EventParser::CounterEventsHandle(Trace::Event *eventPtr) {
    if (eventPtr == nullptr) {
        return;
    }
    auto &event = dynamic_cast<Trace::Counter &>(*eventPtr);
    if (event.ts == 0) {
        return;
    }
    event.trackId = GetTrackId(event.pid, event.tid);
    ThreadEvent threadEvent;
    threadEvent.trackId = event.trackId;
    threadEvent.tid = event.tid;
    threadEvent.pid = event.pid;
    threadEvent.threadName = event.tid;
    ProcessEvent processEvent;
    processEvent.pid = event.pid;
    processEvent.processName = event.pid;
    database->AddSimulationProcessCache(std::move(processEvent));
    database->AddSimulationThreadCache(std::move(threadEvent));
    database->InsertCounter(event);
}

uint64_t EventParser::GetTrackId(const std::string &pid, const std::string &tid) {
    std::string str = std::to_string(pid.size()) + ":" + pid + "|" + std::to_string(tid.size()) + ":" + tid;
    if (trackIdMap.count(str) > 0) {
        return trackIdMap.at(str);
    }
    uint64_t id = TrackInfoManager::Instance().GetTrackId(fileId, pid, tid);
    trackIdMap.emplace(str, id);
    return id;
}

std::string EventParser::GetError() const { return error; }
} // end of namespace Timeline
} // end of namespace Module
} // end of namespace Dic
