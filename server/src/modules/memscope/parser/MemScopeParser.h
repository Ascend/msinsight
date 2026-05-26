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

#ifndef PROFILER_SERVER_MEMSCOPEPARSER_H
#define PROFILER_SERVER_MEMSCOPEPARSER_H
#include "pch.h"
#include "MemScopeDefs.h"
#include "MemScopeEntities.h"
#include "MemScopeDatabase.h"
#include "ThreadPool.h"

namespace Dic::Module {
using namespace Dic::Module::MemScope;
struct ParseEventContext {
    std::vector<MemScopeEvent> events;
    std::unordered_map<std::string, std::map<std::string, const MemScopeEvent *>> deviceMallocMap;
    std::unordered_map<std::string, uint64_t> deviceTotalSize;
    std::unordered_map<uint64_t, EventGroup> eventGroupMap;
    std::shared_ptr<FullDb::MemScopeDatabase> db;
    std::set<std::string> deviceIds;

    bool CheckDeviceIdValid(const std::string &deviceId);
};
class MemScopeParser {
  public:
    MemScopeParser(const MemScopeParser &) = delete;
    MemScopeParser &operator=(const MemScopeParser &) = delete;
    static MemScopeParser &Instance();
    void Reset() const;
    void AsyncParseMemScopeDbFile(const std::string &dbPath) const;
    static void ParseMemScopeDbTask(const std::string &dbPath);

    static std::optional<ParseEventContext> BuildParseContext(std::shared_ptr<FullDb::MemScopeDatabase> &db);
    static bool ParseMemoryMemScopeDumpEventsAndPythonTraces(const std::string &fileId);

    static void ParseEventsToBlockAndAllocations(ParseEventContext &context);

    // 传入slices必须为已按照startTimestamp升序排序的数组
    static bool ParseThreadPythonTrace(MemScopePythonTrace &trace, ParseEventContext &context);

    static bool SingleDeviceEventParse(const MemScopeEvent &event, ParseEventContext &context);
    static void SetMemoryBlockExtendByEventGroup(
        MemoryBlock &block, const uint64_t groupId, ParseEventContext &context);

    static void ParseRemainMallocEvents(ParseEventContext &context);

    static void ParserEnd(const std::string &dbPath, bool result);
    static void ParseCallBack(const std::string &dbPath, bool result, const std::string &msg);

  private:
    MemScopeParser();
    ~MemScopeParser();
    std::unique_ptr<ThreadPool> _threadPool;
};
}
#endif //PROFILER_SERVER_MEMSCOPEPARSER_H
