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
#include "DataBaseManager.h"
#include "MemScopeProtocolEvent.h"
#include "MemScopeService.h"
#include "MemScopeParser.h"

namespace Dic::Module {
bool ParseEventContext::CheckDeviceIdValid(const std::string &deviceId) {
    return deviceIds.find(deviceId) != deviceIds.end();
}

MemScopeParser &MemScopeParser::Instance() {
    static MemScopeParser instance;
    return instance;
}

void MemScopeParser::Reset() const {
    FullDb::MemScopeDatabase::Reset();
    _threadPool->Reset();
}

void MemScopeParser::AsyncParseMemScopeDbFile(const std::string &dbPath) const {
    _threadPool->AddTask(ParseMemScopeDbTask, TraceIdManager::GetTraceId(), dbPath);
}

void MemScopeParser::ParseMemScopeDbTask(const std::string &dbPath) {
    auto database = Timeline::DataBaseManager::Instance().GetMemScopeDatabase(dbPath);
    if (database == nullptr || !database->OpenDb(dbPath, false)) {
        const std::string err = "Failed to get memscope database";
        Server::ServerLog::Error(err);
        ParserEnd(dbPath, false);
        ParseCallBack(dbPath, false, err);
        return;
    }
    if (!database->CheckTableExist(TABLE_LEAKS_DUMP) && !database->CheckTableExist(TABLE_MEM_SCOPE_DUMP)) {
        const std::string err = "The 'leaks_dump' table or 'memscope_dump' table should exist in the memscope "
                                "database at a minimum.";
        Server::ServerLog::Error(err);
        ParserEnd(dbPath, false);
        ParseCallBack(dbPath, false, err);
        return;
    }
    if (ParseMemoryMemScopeDumpEventsAndPythonTraces(dbPath)) {
        ParserEnd(dbPath, true);
        ParseCallBack(dbPath, true, "");
    } else {
        Server::ServerLog::Error("Failed to connect or open memscope memory database.");
        ParserEnd(dbPath, false);
        ParseCallBack(dbPath, false,
            "An exception occurred while parsing the DB data: "
            "Please check the logs for details.");
    }
    Timeline::ParserStatusManager::Instance().SetParserStatus(dbPath, Timeline::ParserStatus::FINISH_ALL);
}

std::optional<ParseEventContext> MemScopeParser::BuildParseContext(std::shared_ptr<FullDb::MemScopeDatabase> &db) {
    if (db == nullptr) {
        Server::ServerLog::Error("Cannot get memscope db connections from database manager");
        return std::nullopt;
    }
    ParseEventContext context;
    db->QueryEntireEventsTable(context.events);
    if (context.events.empty()) {
        Server::ServerLog::Warn("No memory events could be found in memscope_db.");
        return std::nullopt;
    }
    db->QueryDeviceIds(context.deviceIds);
    context.db = db;
    return context;
}

bool MemScopeParser::ParseMemoryMemScopeDumpEventsAndPythonTraces(const std::string &fileId) {
    auto database = Timeline::DataBaseManager::Instance().GetMemScopeDatabase("");
    if (database == nullptr) {
        Server::ServerLog::Error("Cannot get memscope db connections from database manager");
        return false;
    }
    if (database->CheckAllTableExist() && database->HasFinishedParseLastTime()) {
        Timeline::ParserStatusManager::Instance().SetFinishStatus(MEMORY_PREFIX + fileId);
        return true;
    }

    if (!database->CreateMemoryAllocationAndBlockTable() || !database->InitStmt() ||
        !database->UpdateParseStatus(NOT_FINISH_STATUS)) {
        Server::ServerLog::Error("Cannot create memory allocation and memory block table.");
        return false;
    }
    auto context = BuildParseContext(database);
    if (!context.has_value()) {
        Server::ServerLog::Error("Parse failed: build parse context failed.");
        return false;
    }
    // 解析memscope_dump中的内存事件，生成memory_block及memory_allocation
    ParseEventsToBlockAndAllocations(*context);
    // 解析pythonTrace
    std::vector<uint64_t> threadIds;
    database->QueryThreadIds(threadIds);
    for (auto threadId : threadIds) {
        if (threadId == 0) {
            Server::ServerLog::Warn("Parsing python trace skip invalid threadId: 0.");
            continue;
        }
        MemScopeThreadPythonTraceParams params;
        params.threadId = threadId;
        params.relativeTime = true;
        MemScopePythonTrace trace;
        database->QueryPythonTrace(params, trace);
        if (!ParseThreadPythonTrace(trace, *context)) {
            Server::ServerLog::Warn("Parsing python trace failed, threadId: ", threadId);
        }
    }
    database->FlushPythonTraceCache();
    return true;
}

uint64_t SafeCalculateAllocationSize(uint64_t currentSize, int64_t eventSize) {
    uint64_t tmpSize;
    if (eventSize >= 0) {
        tmpSize = static_cast<uint64_t>(eventSize);
        if (currentSize > UINT64_MAX - tmpSize) {
            Server::ServerLog::Warn("Allocation total size is too large.");
            return UINT64_MAX;
        }
        return currentSize + tmpSize;
    }
    tmpSize = static_cast<uint64_t>(-eventSize);
    if (currentSize > tmpSize) {
        return currentSize - tmpSize;
    }
    return 0;
}

void MemScopeParser::ParseEventsToBlockAndAllocations(ParseEventContext &context) {
    if (context.db == nullptr) {
        Server::ServerLog::Error("Cannot parse event to blocks and allocations: invalid db connections");
        return;
    }
    for (auto &event : context.events) {
        if (!context.CheckDeviceIdValid(event.deviceId)) {
            Server::ServerLog::Error("Invalid device id: %.", event.deviceId);
            continue;
        }
        auto eventAttrs = BuildEventAttrsFromJson<MemoryEventBaseAttrs>(event.attr);
        if (eventAttrs.has_value() && eventAttrs->groupId > 0) {
            context.eventGroupMap[eventAttrs->groupId].groupId = static_cast<int64_t>(eventAttrs->groupId);
            context.eventGroupMap[eventAttrs->groupId].AddEvent(event);
        }
        if (!SingleDeviceEventParse(event, context)) {
            continue;
        }
        if (event.event == MEM_SCOPE_DUMP_EVENT::FREE) {
            eventAttrs->size = -std::abs(eventAttrs->size);
        }
        context.deviceTotalSize[event.deviceId + event.eventType] =
            SafeCalculateAllocationSize(context.deviceTotalSize[event.deviceId + event.eventType], eventAttrs->size);
        // 构造allocation折线图元素
        MemoryAllocation allocation(event.timestamp, context.deviceTotalSize[event.deviceId + event.eventType],
            event.deviceId, event.eventType, false);
        context.db->InsertMemoryAllocation(allocation);
    }
    ParseRemainMallocEvents(context);
    context.db->FlushMemoryBlocksCache();
    context.db->FlushMemoryAllocationsCache();
    context.db->UpdateParseStatus(FINISH_STATUS);
}

bool MemScopeParser::ParseThreadPythonTrace(MemScopePythonTrace &trace, ParseEventContext &context) {
    if (context.db == nullptr) {
        Server::ServerLog::Warn("Failed to parse thread python trace: cannot get db connection.");
        return false;
    }
    std::stack<PythonTraceSlice *> callStack;
    for (auto &slice : trace.slices) {
        // 弹出栈中已经结束的func
        while (!callStack.empty() && callStack.top()->endTimestamp < slice.startTimestamp) {
            callStack.pop();
        }
        // 当前调用栈深度 = 栈的大小
        size_t callStackDepth = callStack.size();
        if (callStackDepth > INT_MAX) {
            Server::ServerLog::Error("Build python call stack failed: the stack depth exceeds the max of int");
            return false;
        }
        slice.depth = static_cast<int>(callStackDepth);
        context.db->UpdatePythonTraceSlice(slice);
        // 将当前函数入栈
        callStack.push(&slice);
    }

    return true;
}

bool MemScopeParser::SingleDeviceEventParse(const MemScopeEvent &event, ParseEventContext &context) {
    if (event.event != MEM_SCOPE_DUMP_EVENT::MALLOC && event.event != MEM_SCOPE_DUMP_EVENT::FREE) {
        return false;
    }
    auto &allocMap = context.deviceMallocMap[event.deviceId];
    auto attrs = BuildEventAttrsFromJson<MallocFreeEventAttrs>(event.attr);
    if (!attrs.has_value() || attrs->size == 0) {
        Server::ServerLog::Warn("An invalid memory allocation/free event[" + event.ptr +
            "] was detected: cannot get the 'size' "
            "attribute from the 'attr' field.");
        return false;
    }
    // 如果是分配事件
    if (event.event == MEM_SCOPE_DUMP_EVENT::MALLOC) {
        // 已存在则忽略, 可能为重复申请
        if (allocMap.find(event.ptr + event.eventType) != allocMap.end()) {
            Server::ServerLog::Warn(StringUtil::FormatString("Invalid memory allocation event[{}]: the address "
                                                             "was already allocated and not released.",
                event.ptr));
            return false;
        }
        // 加入申请表
        allocMap[event.ptr + event.eventType] = &event;
    }
    // 如果是释放事件
    if (event.event == MEM_SCOPE_DUMP_EVENT::FREE) {
        // 内存分配表中未找到匹配的分配事件，则忽略
        if (allocMap.find(event.ptr + event.eventType) == allocMap.end()) {
            Server::ServerLog::Warn(StringUtil::FormatString("Invalid memory free event[{}]: "
                                                             "no corresponding allocation.",
                event.ptr));
            return false;
        }
        auto &allocEvent = allocMap[event.ptr + event.eventType];
        auto allocAttrs = BuildEventAttrsFromJson<MallocFreeEventAttrs>(allocEvent->attr);
        // 构造block
        MemoryBlock block(event.ptr, event.deviceId, std::abs(allocAttrs->size), allocEvent->timestamp, event.timestamp,
            allocAttrs->owner, event.eventType, allocEvent->attr, event.processId, event.threadId);
        // 构造block扩展属性
        SetMemoryBlockExtendByEventGroup(block, allocAttrs->groupId, context);
        context.db->InsertMemoryBlock(block);
        // 从申请表中去除内存申请事件
        allocMap.erase(event.ptr + event.eventType);
    }
    return true;
}

void MemScopeParser::SetMemoryBlockExtendByEventGroup(
    MemoryBlock &block, const uint64_t groupId, ParseEventContext &context) {
    MemoryBlockAttrs blockAttrs;
    blockAttrs.groupId = groupId;
    block.attrJsonString = blockAttrs.ToJsonString();
    uint64_t minTimestamp = context.db->GetGlobalMinTimestamp();
    EventGroup eventGroup = context.eventGroupMap[groupId];
    if (groupId == 0 || eventGroup.accessEvents.empty()) {
        block.firstAccessTimestamp = static_cast<int64_t>(minTimestamp - 1);
        block.lastAccessTimestamp = static_cast<int64_t>(minTimestamp - 1);
        block.maxAccessInterval = 0;
        return;
    }
    uint64_t maxInterval = 0;
    block.firstAccessTimestamp = static_cast<int64_t>(eventGroup.accessEvents.front().timestamp);
    block.lastAccessTimestamp = static_cast<int64_t>(eventGroup.accessEvents.back().timestamp);
    uint64_t preAccessTs = static_cast<uint64_t>(block.firstAccessTimestamp);
    for (auto &accessEvent : eventGroup.accessEvents) {
        uint64_t interval = accessEvent.timestamp > preAccessTs ? accessEvent.timestamp - preAccessTs : 0;
        maxInterval = std::max(interval, maxInterval);
    }
    block.maxAccessInterval = maxInterval;
}

void MemScopeParser::ParseRemainMallocEvents(ParseEventContext &context) {
    for (auto &devicePair : context.deviceMallocMap) {
        std::string deviceId = devicePair.first;
        std::map<std::string, const MemScopeEvent *> allocMap = devicePair.second;
        const std::uint64_t maxTimestamp = context.db->GetGlobalMaxTimestamp();
        for (auto &allocPair : allocMap) {
            auto &event = allocPair.second;
            auto eventAttrs = BuildEventAttrsFromJson<MallocFreeEventAttrs>(event->attr);
            if (!eventAttrs.has_value() || eventAttrs->size <= 0) {
                Server::ServerLog::Warn("An invalid memory allocation event was detected: cannot get the valid 'size' "
                                        "attribute from the 'attr' field");
                continue;
            }
            // 构造block
            MemoryBlock block(event->ptr, event->deviceId, eventAttrs->size, event->timestamp, maxTimestamp,
                eventAttrs->owner, event->eventType, event->attr, event->processId, event->threadId);
            // 构造block扩展属性
            auto blockAttrs = MemoryBlockAttrs::FromJson(event->attr);
            SetMemoryBlockExtendByEventGroup(block, blockAttrs->groupId, context);
            block.attrJsonString = blockAttrs.has_value() ? blockAttrs->ToJsonString() : event->attr;
            context.db->InsertMemoryBlock(block);
        }
    }
}

void MemScopeParser::ParserEnd(const std::string &dbPath, const bool result) {
    if (!result) {
        Server::ServerLog::Error("[MemScope]memscope database parser failed, filepath: ", dbPath);
        return;
    }
    Server::ServerLog::Info("[MemScope]memscope Dumps Parser ends, filepath: ", dbPath);
}

void MemScopeParser::ParseCallBack(const std::string &dbPath, bool result, const std::string &msg) {
    auto event = std::make_unique<Protocol::MemScopeParseSuccessEvent>();
    event->moduleName = Protocol::MODULE_MEM_SCOPE;
    if (dbPath.empty()) {
        event->result = true;
        SendEvent(std::move(event));
    } else {
        event->result = result;
        Protocol::MemScopeParseSuccessEventBody body;
        if (event->result) {
            const auto memoryDatabase = Timeline::DataBaseManager::Instance().GetMemScopeDatabase("");
            if (memoryDatabase == nullptr) {
                Server::ServerLog::Error("Cannot get memscope db connections from database manager");
                event->errMsg = "Failed parse memscope dump data.";
                event->result = false;
                SendEvent(std::move(event));
                return;
            }
            memoryDatabase->QueryMallocOrFreeEventTypeWithDeviceId(body.deviceIds);
            memoryDatabase->QueryThreadIds(body.threadIds);
            memoryDatabase->SetDataBaseVersion();
        } else {
            event->errMsg = msg;
        }
        body.fileId = dbPath;
        body.module = Protocol::MODULE_MEM_SCOPE;
        event->body = body;
        SendEvent(std::move(event));
    }
}

MemScopeParser::MemScopeParser() {
    // MemScopedb为单文件单线程解析
    _threadPool = std::make_unique<ThreadPool>(1);
}

MemScopeParser::~MemScopeParser() {
    if (_threadPool != nullptr) {
        _threadPool->ShutDown();
    }
}
}
