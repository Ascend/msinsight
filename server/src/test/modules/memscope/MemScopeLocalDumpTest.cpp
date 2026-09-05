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

#include <algorithm>
#include <filesystem>
#include <gtest/gtest.h>
#include "DataBaseManager.h"
#include "MemScopeAllocationDataProcessor.h"
#include "MemScopeParser.h"
#include "MemScopeProtocolRequest.h"
#include "MemScopeProtocolResponse.h"
#include "JsonUtil.h"
#include "TestSuit.h"

using namespace Dic::Module::Timeline;
using namespace Dic::Module::MemScope;
using namespace Dic::Module::FullDb;
using namespace Dic::Protocol;

namespace {
// test_data/memscope/attr_usage/: 1/10 prefix of a memscope dump whose Attr has total/process_used/device_used.
std::string FixtureDumpPath(const std::string &fileName) {
    return TestSuit::GetTestDataFile("memscope", "attr_usage", fileName);
}

std::string CopyDumpForParse(const std::string &srcPath, const std::string &name) {
    const auto workDir = std::filesystem::temp_directory_path() / "memscope_attr_usage_test";
    std::filesystem::create_directories(workDir);
    const auto dest = workDir / name;
    std::filesystem::copy_file(srcPath, dest, std::filesystem::copy_options::overwrite_existing);
    return dest.string();
}

void ParseDump(const std::string &dbPath) {
    DataBaseManager::Instance().Clear();
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    ASSERT_TRUE(memoryDatabase->OpenDb(dbPath, false));
    ASSERT_TRUE(memoryDatabase->DropMemoryAllocationAndBlockTable());
    ASSERT_TRUE(MemScopeParser::ParseMemoryMemScopeDumpEventsAndPythonTraces("0"));
}

void QueryPtaAllocations(std::vector<MemoryAllocation> &allocations) {
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    MemScopeMemoryAllocationParams params;
    params.deviceId = "0";
    params.eventType = "PTA";
    params.optimized = false;
    memoryDatabase->QueryMemoryAllocations(params, allocations);
}

bool HasPositive(const std::vector<MemoryAllocation> &allocations, uint64_t MemoryAllocation::*field) {
    for (const auto &allocation : allocations) {
        if (allocation.*field > 0) {
            return true;
        }
    }
    return false;
}
} // namespace

class MemScopeLocalDumpTest : public ::testing::Test {
  protected:
    static void TearDownTestSuite() {
        auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
        if (memoryDatabase != nullptr) {
            memoryDatabase->CloseDb();
        }
        DataBaseManager::Instance().Clear();
    }
};

TEST_F(MemScopeLocalDumpTest, NewDumpPopulatesReservedProcessAndDeviceLines) {
    const auto src = FixtureDumpPath("memscope_dump_20260820113144.db");
    ASSERT_TRUE(std::filesystem::exists(src)) << src;
    const auto dbPath = CopyDumpForParse(src, "memscope_local_new.db");
    ParseDump(dbPath);

    std::vector<MemoryAllocation> allocations;
    QueryPtaAllocations(allocations);
    ASSERT_FALSE(allocations.empty());
    EXPECT_TRUE(HasPositive(allocations, &MemoryAllocation::reservedSize));
    EXPECT_TRUE(HasPositive(allocations, &MemoryAllocation::processUsed));
    EXPECT_TRUE(HasPositive(allocations, &MemoryAllocation::deviceUsed));

    const auto reservedLine = MemScopeAllocationDataProcessor::CompressReservedLine(allocations);
    const auto processUsedLine = MemScopeAllocationDataProcessor::CompressProcessUsedLine(allocations);
    const auto deviceUsedLine = MemScopeAllocationDataProcessor::CompressDeviceUsedLine(allocations);
    EXPECT_FALSE(reservedLine.empty());
    EXPECT_FALSE(processUsedLine.empty());
    EXPECT_FALSE(deviceUsedLine.empty());

    MemScopeMemoryAllocationsResponse response;
    response.reservedLine = reservedLine;
    response.processUsedLine = processUsedLine;
    response.deviceUsedLine = deviceUsedLine;
    const auto json = response.ToJson();
    ASSERT_TRUE(json.has_value());
    EXPECT_GT((*json)["body"]["reservedLine"].Size(), 0);
    EXPECT_GT((*json)["body"]["processUsedLine"].Size(), 0);
    EXPECT_GT((*json)["body"]["deviceUsedLine"].Size(), 0);
}

TEST_F(MemScopeLocalDumpTest, LegacyHostPinnedDumpIsRewrittenToHostWithPinnedAttr) {
    const auto src = TestSuit::GetTestDataFile("memscope", "host_pinned", "memscope_dump_20260527062213.db");
    ASSERT_TRUE(std::filesystem::exists(src)) << src;
    const auto dbPath = CopyDumpForParse(src, "memscope_host_pinned_rewrite.db");
    ParseDump(dbPath);

    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);

    std::vector<MemScopeEvent> events;
    memoryDatabase->QueryEntireEventsTable(events);
    ASSERT_FALSE(events.empty());
    bool sawHost = false;
    for (const auto &event : events) {
        EXPECT_NE(event.eventType, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED);
        if (event.event != MEM_SCOPE_DUMP_EVENT::MALLOC && event.event != MEM_SCOPE_DUMP_EVENT::FREE) {
            continue;
        }
        EXPECT_EQ(event.eventType, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST);
        sawHost = true;
        std::string error;
        auto attrsJson = JsonUtil::TryParse(event.attr, error);
        ASSERT_TRUE(attrsJson.has_value());
        EXPECT_EQ(JsonUtil::GetString(attrsJson.value(), BLOCK_EVENT_ATTR_PINNED_FIELD), "true");
        EXPECT_FALSE(attrsJson->HasMember(BLOCK_EVENT_ATTR_TOTAL_FIELD.c_str()));
    }
    EXPECT_TRUE(sawHost);

    std::unordered_map<std::string, std::vector<std::string>> deviceEventTypes;
    memoryDatabase->QueryMallocOrFreeEventTypeWithDeviceId(deviceEventTypes);
    ASSERT_TRUE(deviceEventTypes.find("cpu") != deviceEventTypes.end());
    const auto &eventTypes = deviceEventTypes["cpu"];
    EXPECT_NE(
        std::find(eventTypes.begin(), eventTypes.end(), MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST), eventTypes.end());
    EXPECT_EQ(std::find(eventTypes.begin(), eventTypes.end(), MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED),
        eventTypes.end());

    MemScopeMemoryAllocationParams params;
    params.deviceId = "cpu";
    params.eventType = MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST;
    params.relativeTime = true;
    std::vector<MemoryAllocation> allocations;
    memoryDatabase->QueryMemoryAllocations(params, allocations);
    EXPECT_FALSE(allocations.empty());
}

TEST_F(MemScopeLocalDumpTest, HostDumpRewriteFailureKeepsNotFinishAndDoesNotSplitTypes) {
    const auto src = TestSuit::GetTestDataFile("memscope", "host_pinned", "memscope_dump_20260527062213.db");
    ASSERT_TRUE(std::filesystem::exists(src)) << src;
    const auto dbPath = CopyDumpForParse(src, "memscope_host_pinned_rewrite_fail.db");

    DataBaseManager::Instance().Clear();
    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);
    ASSERT_TRUE(memoryDatabase->OpenDb(dbPath, false));
    ASSERT_TRUE(memoryDatabase->DropMemoryAllocationAndBlockTable());
    ASSERT_TRUE(memoryDatabase->CreateMemoryAllocationAndBlockTable());
    ASSERT_TRUE(memoryDatabase->InitStmt());
    ASSERT_TRUE(memoryDatabase->UpdateParseStatus(NOT_FINISH_STATUS));

    auto context = MemScopeParser::BuildParseContext(memoryDatabase);
    ASSERT_TRUE(context.has_value());
    ASSERT_TRUE(
        memoryDatabase->ExecSql("CREATE TRIGGER memscope_dump_block_update BEFORE UPDATE ON memscope_dump BEGIN "
                                "SELECT RAISE(ABORT, 'forced dump rewrite failure'); END;"));

    EXPECT_FALSE(MemScopeParser::ParseEventsToBlockAndAllocations(*context));
    EXPECT_FALSE(memoryDatabase->HasFinishedParseLastTime());

    std::vector<MemScopeEvent> events;
    memoryDatabase->QueryEntireEventsTable(events);
    ASSERT_FALSE(events.empty());
    bool sawHostPinned = false;
    for (const auto &event : events) {
        if (event.event != MEM_SCOPE_DUMP_EVENT::MALLOC && event.event != MEM_SCOPE_DUMP_EVENT::FREE) {
            continue;
        }
        EXPECT_EQ(event.eventType, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED);
        sawHostPinned = true;
    }
    EXPECT_TRUE(sawHostPinned);

    std::unordered_map<std::string, std::vector<std::string>> deviceEventTypes;
    memoryDatabase->QueryMallocOrFreeEventTypeWithDeviceId(deviceEventTypes);
    ASSERT_TRUE(deviceEventTypes.find("cpu") != deviceEventTypes.end());
    const auto &eventTypes = deviceEventTypes["cpu"];
    EXPECT_NE(std::find(eventTypes.begin(), eventTypes.end(), MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED),
        eventTypes.end());
    EXPECT_EQ(
        std::find(eventTypes.begin(), eventTypes.end(), MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST), eventTypes.end());

    MemScopeMemoryAllocationParams params;
    params.deviceId = "cpu";
    params.eventType = MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST;
    params.relativeTime = true;
    std::vector<MemoryAllocation> allocations;
    memoryDatabase->QueryMemoryAllocations(params, allocations);
    EXPECT_TRUE(allocations.empty());
}

TEST_F(MemScopeLocalDumpTest, NativeHostDumpKeepsHostTypeAndPinnedAttr) {
    const auto src = TestSuit::GetTestDataFile("memscope", "host", "memscope_dump_20260903104158.db");
    ASSERT_TRUE(std::filesystem::exists(src)) << src;
    const auto dbPath = CopyDumpForParse(src, "memscope_native_host.db");
    ParseDump(dbPath);

    auto memoryDatabase = DataBaseManager::Instance().GetMemScopeDatabase("0");
    ASSERT_TRUE(memoryDatabase != nullptr);

    std::vector<MemScopeEvent> events;
    memoryDatabase->QueryEntireEventsTable(events);
    ASSERT_FALSE(events.empty());
    bool sawHost = false;
    bool sawPinnedTrue = false;
    bool sawUnpinnedHost = false;
    for (const auto &event : events) {
        EXPECT_NE(event.eventType, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST_PINNED);
        if (event.event != MEM_SCOPE_DUMP_EVENT::MALLOC && event.event != MEM_SCOPE_DUMP_EVENT::FREE) {
            continue;
        }
        EXPECT_EQ(event.eventType, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST);
        sawHost = true;
        std::string error;
        auto attrsJson = JsonUtil::TryParse(event.attr, error);
        ASSERT_TRUE(attrsJson.has_value());
        const std::string pinned = JsonUtil::GetString(attrsJson.value(), BLOCK_EVENT_ATTR_PINNED_FIELD);
        if (pinned == "true") {
            sawPinnedTrue = true;
        } else {
            sawUnpinnedHost = true;
        }
        EXPECT_FALSE(attrsJson->HasMember(BLOCK_EVENT_ATTR_TOTAL_FIELD.c_str()));
    }
    EXPECT_TRUE(sawHost);
    EXPECT_TRUE(sawPinnedTrue);
    EXPECT_TRUE(sawUnpinnedHost);

    MemScopeMemoryAllocationParams params;
    params.deviceId = "cpu";
    params.eventType = MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HOST;
    params.relativeTime = true;
    std::vector<MemoryAllocation> allocations;
    memoryDatabase->QueryMemoryAllocations(params, allocations);
    EXPECT_FALSE(allocations.empty());
    EXPECT_TRUE(HasPositive(allocations, &MemoryAllocation::reservedSize));
    EXPECT_TRUE(HasPositive(allocations, &MemoryAllocation::processUsed));
    EXPECT_FALSE(HasPositive(allocations, &MemoryAllocation::deviceUsed));
    EXPECT_FALSE(MemScopeAllocationDataProcessor::CompressReservedLine(allocations).empty());
    EXPECT_FALSE(MemScopeAllocationDataProcessor::CompressProcessUsedLine(allocations).empty());
    EXPECT_TRUE(MemScopeAllocationDataProcessor::CompressDeviceUsedLine(allocations).empty());
}
