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

#include "../../TestSuit.h"
#include "MemScopeEntities.h"
#include "MemScopeProtocolRequest.h"
#include "MemScopeProtocolResponse.h"
#include "MemScopeProtocolEvent.h"
#include "MemScopeProtocol.h"
#include "MemScopeModule.h"

class InspectableMemScopeModule : public Dic::Module::MemScopeModule {
  public:
    size_t HandlerCount() const { return requestHandlerMap.size(); }
    bool HasHandler(const std::string &command) const { return requestHandlerMap.count(command) != 0; }
};

class MemScopeProtocolTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
};

TEST_F(MemScopeProtocolTest, RegistersBothScopeAndSnapshotRoutes) {
    Dic::Protocol::MemScopeProtocolUtil protocol;
    protocol.Register();
    std::string error;
    Dic::Protocol::MemScopeMemoryAllocationsResponse response;
    EXPECT_TRUE(protocol.ToJson(response, error).has_value());
    protocol.UnRegister();

    InspectableMemScopeModule module;
    module.RegisterRequestHandlers();
    EXPECT_EQ(module.HandlerCount(), 12U);
    EXPECT_TRUE(module.HasHandler(Dic::Protocol::REQ_RES_MEM_SCOPE_MEMORY_BLOCKS));
    EXPECT_TRUE(module.HasHandler(Dic::Protocol::REQ_RES_MEM_SNAPSHOT_BLOCKS));
    module.RegisterRequestHandlers();
    EXPECT_EQ(module.HandlerCount(), 12U);
}

TEST_F(MemScopeProtocolTest, RequestsRejectMissingBaseAndRequiredParams) {
    using namespace Dic::Protocol;
    using Decoder = std::unique_ptr<Request> (*)(const json_t &, std::string &);
    const std::vector<Decoder> decoders = {MemScopeMemoryBlockRequest::FromJson,
        MemScopeMemoryAllocationRequest::FromJson, MemScopeMemoryDetailRequest::FromJson,
        MemScopePythonTraceRequest::FromJson, MemScopeEventRequest::FromJson};
    for (const auto decode : decoders) {
        std::string error;
        auto invalid = JsonUtil::TryParse("{}", error);
        ASSERT_TRUE(invalid.has_value());
        EXPECT_EQ(decode(*invalid, error), nullptr);
        EXPECT_FALSE(error.empty());
        error.clear();
        auto missing = JsonUtil::TryParse(
            R"({"id":1,"type":"request","moduleName":"leaks","command":"Memory/leaks/blocks","params":{}})", error);
        ASSERT_TRUE(missing.has_value());
        EXPECT_EQ(decode(*missing, error), nullptr);
        EXPECT_FALSE(error.empty());
    }
}

TEST_F(MemScopeProtocolTest, BlockAndAllocationValidationRejectsEachInvalidDimension) {
    std::string error;
    Dic::Protocol::MemScopeMemoryBlockParams block;
    block.deviceId = "0";
    block.eventType = "PTA";
    block.currentPage = 1;
    block.pageSize = 10;
    EXPECT_TRUE(block.CommonCheck(error));
    block.minSize = 2;
    EXPECT_FALSE(block.CommonCheck(error));
    block.minSize = 0;
    block.maxSize = UINT64_MAX;
    EXPECT_FALSE(block.CommonCheck(error));
    block.maxSize = 10;
    block.startTimestamp = 2;
    block.endTimestamp = 1;
    EXPECT_FALSE(block.CommonCheck(error));
    block.endTimestamp = UINT64_MAX;
    EXPECT_FALSE(block.CommonCheck(error));
    block.endTimestamp = 2;
    block.deviceId.clear();
    EXPECT_FALSE(block.CommonCheck(error));
    block.deviceId = "0";
    block.lazyUsedThreshold.perT = 101;
    EXPECT_FALSE(block.CommonCheck(error));
    block.lazyUsedThreshold.perT = 0;
    block.delayedFreeThreshold.perT = 101;
    EXPECT_FALSE(block.CommonCheck(error));
    block.delayedFreeThreshold.perT = 0;
    block.longIdleThreshold.perT = 101;
    EXPECT_FALSE(block.CommonCheck(error));
    block.longIdleThreshold.perT = 0;
    block.eventType.clear();
    EXPECT_FALSE(block.CommonCheck(error));

    Dic::Protocol::MemScopeMemoryAllocationParams allocation;
    allocation.deviceId = "0";
    allocation.eventType = "PTA";
    EXPECT_TRUE(allocation.CommonCheck(error));
    allocation.startTimestamp = 2;
    allocation.endTimestamp = 1;
    EXPECT_FALSE(allocation.CommonCheck(error));
    allocation.endTimestamp = UINT64_MAX;
    EXPECT_FALSE(allocation.CommonCheck(error));
    allocation.endTimestamp = 2;
    allocation.deviceId.clear();
    EXPECT_FALSE(allocation.CommonCheck(error));
    allocation.deviceId = "0";
    allocation.eventType.clear();
    EXPECT_FALSE(allocation.CommonCheck(error));
}

TEST_F(MemScopeProtocolTest, DetailTraceAndEventValidationBoundaries) {
    std::string error;
    Dic::Protocol::MemScopeMemoryDetailParams detail;
    EXPECT_FALSE(detail.CommonCheck(error));
    detail.deviceId = "0";
    detail.eventType = "PTA";
    detail.timestamp = UINT64_MAX;
    EXPECT_FALSE(detail.CommonCheck(error));
    detail.timestamp = 1;
    EXPECT_TRUE(detail.CommonCheck(error));

    Dic::Protocol::MemScopeThreadPythonTraceParams trace;
    trace.deviceId = "0";
    trace.startTimestamp = 2;
    trace.endTimestamp = 1;
    EXPECT_FALSE(trace.CommonCheck(error));
    trace.endTimestamp = UINT64_MAX;
    EXPECT_FALSE(trace.CommonCheck(error));
    trace.endTimestamp = 2;
    trace.threadId = INT64_MAX;
    EXPECT_FALSE(trace.CommonCheck(error));
    trace.threadId = 1;
    EXPECT_TRUE(trace.CommonCheck(error));

    Dic::Protocol::MemScopeEventParams events;
    events.deviceId = "0";
    events.currentPage = 1;
    events.pageSize = 10;
    EXPECT_TRUE(events.CommonCheck(error));
    events.startTimestamp = 2;
    events.endTimestamp = 1;
    EXPECT_FALSE(events.CommonCheck(error));
    events.endTimestamp = UINT64_MAX;
    EXPECT_FALSE(events.CommonCheck(error));
}

TEST_F(MemScopeProtocolTest, MemScopeParseSuccessEventDoesNotContainSnapshotState) {
    Dic::Protocol::MemScopeParseSuccessEvent event;
    event.body.module = "leaks";
    event.body.fileHash = "0123456789abcdef";
    event.body.snapshotParsingComplete = false;
    const auto json = event.ToJson();
    ASSERT_TRUE(json.has_value());
    ASSERT_TRUE(json->HasMember("body"));
    ASSERT_TRUE((*json)["body"].HasMember("fileHash"));
    EXPECT_STREQ((*json)["body"]["fileHash"].GetString(), "0123456789abcdef");
    EXPECT_FALSE((*json)["body"].HasMember("snapshotParsingComplete"));
    EXPECT_FALSE((*json)["body"].HasMember("snapshotSlices"));
}

TEST_F(MemScopeProtocolTest, ParseSuccessEventContainsSnapshotSlices) {
    Dic::Protocol::MemScopeParseSuccessEvent event;
    event.body.module = "memsnapshot";
    event.body.snapshotParsingComplete = false;
    Dic::Protocol::MemSnapshotDeviceSliceEventInfo device;
    device.eventCount = 200;
    device.sliceCount = 2;
    device.readySlices = {1};
    device.slices = {{0, 0, 99, false}, {1, 100, 199, true}};
    event.body.snapshotSlices.emplace("0", device);

    const auto json = event.ToJson();
    ASSERT_TRUE(json.has_value());
    ASSERT_TRUE((*json)["body"].HasMember("snapshotParsingComplete"));
    EXPECT_FALSE((*json)["body"]["snapshotParsingComplete"].GetBool());
    const auto &snapshotSlices = (*json)["body"]["snapshotSlices"];
    ASSERT_TRUE(snapshotSlices.HasMember("0"));
    EXPECT_EQ(snapshotSlices["0"]["sliceCount"].GetInt(), 2);
    EXPECT_EQ(snapshotSlices["0"]["readySlices"][0].GetInt(), 1);
    EXPECT_FALSE(snapshotSlices["0"]["slices"][0]["ready"].GetBool());
    EXPECT_TRUE(snapshotSlices["0"]["slices"][1]["ready"].GetBool());
}

TEST_F(MemScopeProtocolTest, BuildEventTableRequestFromJson) {
    std::string jsonStr =
        "{"
        "  \"id\": 37, "
        "  \"moduleName\": \"leaks\", "
        "  \"type\": \"request\", "
        "  \"command\": \"Memory/leaks/events\", "
        "  \"fileId\": \"\", "
        "  \"projectName\": \"/home/fuzz-test/test-data/930/leaks/callstack/leaks_dump_20250807173133.db\", "
        "  \"params\": { "
        "    \"deviceId\": \"1\", "
        "    \"relativeTime\": true, "
        "    \"startTimestamp\": 14104097470, "
        "    \"endTimestamp\": 81806122630, "
        "    \"currentPage\": 1, "
        "    \"pageSize\": 10, "
        "    \"desc\": false, "
        "    \"orderBy\": \"ptr\", "
        "    \"filters\": { "
        "      \"event\": \"MALLOC\" "
        "    }, "
        "    \"rangeFilters\": { "
        "      \"_timestamp\": [ "
        "        14104097470, "
        "        24104097470 "
        "      ] "
        "    } "
        "  } "
        "}";
    std::string errMsg;
    auto json = JsonUtil::TryParse(jsonStr, errMsg);
    EXPECT_TRUE(errMsg.empty());
    EXPECT_FALSE(json->HasParseError());
    EXPECT_TRUE(json.has_value());
    auto requestPtr = MemScopeEventRequest::FromJson(json.value(), errMsg);
    EXPECT_TRUE(errMsg.empty());
    auto &request = dynamic_cast<MemScopeEventRequest &>(*requestPtr);
    EXPECT_TRUE(request.params.CommonCheck(errMsg));
    EXPECT_EQ(request.params.currentPage, 1);
    EXPECT_EQ(request.params.pageSize, 10);
    EXPECT_EQ(request.params.orderBy, "ptr");
    EXPECT_EQ(request.params.filters.size(), 1);
    EXPECT_EQ(request.params.rangeFilters.size(), 1);
}

TEST_F(MemScopeProtocolTest, BuildBlockTableRequestFromJson) {
    std::string jsonStr =
        "{ "
        "  \"id\": 21, "
        "  \"moduleName\": \"leaks\", "
        "  \"type\": \"request\", "
        "  \"command\": \"Memory/leaks/blocks\", "
        "  \"fileId\": \"\", "
        "  \"projectName\": \"/home/fuzz-test/test-data/930/leaks/callstack/leaks_dump_20250807173133.db\", "
        "  \"params\": { "
        "    \"deviceId\": \"1\", "
        "    \"relativeTime\": true, "
        "    \"eventType\": \"PTA\", "
        "    \"isTable\": true, "
        "    \"startTimestamp\": 14104097470, "
        "    \"endTimestamp\": 81806122630, "
        "    \"currentPage\": 1, "
        "    \"pageSize\": 10, "
        "    \"desc\": false, "
        "    \"orderBy\": \"_startTimestamp\", "
        "    \"filters\": { "
        "      \"owner\": \"ops\" "
        "    } "
        "  } "
        "}";
    std::string errMsg;
    auto json = JsonUtil::TryParse(jsonStr, errMsg);
    EXPECT_TRUE(errMsg.empty());
    EXPECT_FALSE(json->HasParseError());
    EXPECT_TRUE(json.has_value());
    auto requestPtr = MemScopeMemoryBlockRequest::FromJson(json.value(), errMsg);
    EXPECT_TRUE(errMsg.empty());
    auto &request = dynamic_cast<MemScopeMemoryBlockRequest &>(*requestPtr);
    EXPECT_TRUE(request.params.CommonCheck(errMsg));
    EXPECT_EQ(request.params.currentPage, 1);
    EXPECT_EQ(request.params.pageSize, 10);
    EXPECT_EQ(request.params.orderBy, "_startTimestamp");
    EXPECT_EQ(request.params.filters.size(), 1);
    EXPECT_EQ(request.params.rangeFilters.size(), 0);
}

TEST_F(MemScopeProtocolTest, BuildDetailRequestFromJson) {
    std::string jsonStr =
        "{ "
        "  \"id\": 39, "
        "  \"moduleName\": \"leaks\", "
        "  \"type\": \"request\", "
        "  \"command\": \"Memory/leaks/details\", "
        "  \"fileId\": \"\", "
        "  \"projectName\": \"/home/fuzz-test/test-data/930/leaks/callstack/leaks_dump_20250807173133.db\", "
        "  \"params\": { "
        "    \"deviceId\": \"1\", "
        "    \"timestamp\": 65215342604, "
        "    \"eventType\": \"PTA\", "
        "    \"relativeTime\": true "
        "  } "
        "}";
    std::string errMsg;
    auto json = JsonUtil::TryParse(jsonStr, errMsg);
    EXPECT_TRUE(errMsg.empty());
    EXPECT_FALSE(json->HasParseError());
    EXPECT_TRUE(json.has_value());
    auto requestPtr = MemScopeMemoryDetailRequest::FromJson(json.value(), errMsg);
    EXPECT_TRUE(errMsg.empty());
    auto &request = dynamic_cast<MemScopeMemoryDetailRequest &>(*requestPtr);
    EXPECT_TRUE(request.params.CommonCheck(errMsg));
    EXPECT_EQ(request.params.deviceId, "1");
    const uint64_t expectTimestamp = 65215342604;
    EXPECT_EQ(request.params.timestamp, expectTimestamp);
    EXPECT_EQ(request.params.eventType, "PTA");
    EXPECT_TRUE(request.params.relativeTime);
}

TEST_F(MemScopeProtocolTest, BuildAllocationRequestFromJson) {
    std::string jsonStr =
        "{ "
        "  \"id\": 47, "
        "  \"moduleName\": \"leaks\", "
        "  \"type\": \"request\", "
        "  \"command\": \"Memory/leaks/allocations\", "
        "  \"fileId\": \"\", "
        "  \"projectName\": \"/home/fuzz-test/test-data/930/leaks/callstack/leaks_dump_20250807173133.db\", "
        "  \"params\": { "
        "    \"deviceId\": \"1\", "
        "    \"relativeTime\": true, "
        "    \"eventType\": \"HAL\", "
        "    \"startTimestamp\": 45074528271, "
        "    \"endTimestamp\": 56927968979 "
        "  } "
        "}";
    std::string errMsg;
    auto json = JsonUtil::TryParse(jsonStr, errMsg);
    EXPECT_TRUE(errMsg.empty());
    EXPECT_FALSE(json->HasParseError());
    EXPECT_TRUE(json.has_value());
    auto requestPtr = MemScopeMemoryAllocationRequest::FromJson(json.value(), errMsg);
    EXPECT_TRUE(errMsg.empty());
    auto &request = dynamic_cast<MemScopeMemoryAllocationRequest &>(*requestPtr);
    EXPECT_TRUE(request.params.CommonCheck(errMsg));
    EXPECT_EQ(request.params.deviceId, "1");
    const uint64_t expectStartTimestamp = 45074528271;
    const uint64_t expectEndTimestamp = 56927968979;
    EXPECT_EQ(request.params.startTimestamp, expectStartTimestamp);
    EXPECT_EQ(request.params.endTimestamp, expectEndTimestamp);
    EXPECT_EQ(request.params.eventType, "HAL");
    EXPECT_TRUE(request.params.relativeTime);
}

TEST_F(MemScopeProtocolTest, BuildTraceRequestFromJson) {
    std::string jsonStr =
        "{ "
        "  \"id\": 45, "
        "  \"moduleName\": \"leaks\", "
        "  \"type\": \"request\", "
        "  \"command\": \"Memory/leaks/traces\", "
        "  \"fileId\": \"\", "
        "  \"projectName\": \"/home/fuzz-test/test-data/930/leaks/callstack/leaks_dump_20250807173133.db\", "
        "  \"params\": { "
        "    \"deviceId\": \"1\", "
        "    \"relativeTime\": true, "
        "    \"threadId\": 2637224, "
        "    \"startTimestamp\": 45074528271, "
        "    \"endTimestamp\": 56927968979 "
        "  } "
        "}";
    std::string errMsg;
    auto json = JsonUtil::TryParse(jsonStr, errMsg);
    EXPECT_TRUE(errMsg.empty());
    EXPECT_FALSE(json->HasParseError());
    EXPECT_TRUE(json.has_value());
    auto requestPtr = MemScopePythonTraceRequest::FromJson(json.value(), errMsg);
    EXPECT_TRUE(errMsg.empty());
    auto &request = dynamic_cast<MemScopePythonTraceRequest &>(*requestPtr);
    EXPECT_TRUE(request.params.CommonCheck(errMsg));
    EXPECT_EQ(request.params.deviceId, "1");
    const uint64_t expectStartTimestamp = 45074528271;
    const uint64_t expectEndTimestamp = 56927968979;
    const uint64_t expectThreadId = 2637224;
    EXPECT_EQ(request.params.startTimestamp, expectStartTimestamp);
    EXPECT_EQ(request.params.endTimestamp, expectEndTimestamp);
    EXPECT_EQ(request.params.threadId, expectThreadId);
    EXPECT_TRUE(request.params.relativeTime);
}

TEST_F(MemScopeProtocolTest, BuildBlockTableThresholdParamsFromJson) {
    std::string jsonStr = R"({"id": 11, "moduleName": "leaks", "type": "request", "command": "Memory/leaks/blocks",
                                "params": {"deviceId": "1", "relativeTime": true, "eventType": "PTA", "isTable": true,
                                "startTimestamp": 7707721000, "endTimestamp": 42623722980,
                                "lazyUsedThreshold":{"perT": 20, "valueT": 100000000},
                                "delayedFreeThreshold": {"perT": 0, "valueT": 999999}}})";
    std::string errMsg;
    auto json = JsonUtil::TryParse(jsonStr, errMsg);
    EXPECT_TRUE(errMsg.empty());
    EXPECT_FALSE(json->HasParseError());
    EXPECT_TRUE(json.has_value());
    auto requestPtr = MemScopeMemoryBlockRequest::FromJson(json.value(), errMsg);
    EXPECT_TRUE(errMsg.empty());
    auto &request = dynamic_cast<MemScopeMemoryBlockRequest &>(*requestPtr);
    EXPECT_TRUE(request.params.CommonCheck(errMsg));
    EXPECT_EQ(request.params.deviceId, "1");
    const uint64_t expectStartTimestamp = 7707721000;
    const uint64_t expectEndTimestamp = 42623722980;
    EXPECT_EQ(request.params.startTimestamp, expectStartTimestamp);
    EXPECT_EQ(request.params.endTimestamp, expectEndTimestamp);
    EXPECT_TRUE(request.isTable);
    EXPECT_EQ(request.params.lazyUsedThreshold.perT, 20);
    EXPECT_EQ(request.params.lazyUsedThreshold.valueT, 100000000);
    EXPECT_EQ(request.params.delayedFreeThreshold.perT, 0);
    EXPECT_EQ(request.params.delayedFreeThreshold.valueT, 999999);
    EXPECT_EQ(request.params.longIdleThreshold.perT, 0);
    EXPECT_EQ(request.params.longIdleThreshold.valueT, 0);
}

TEST_F(MemScopeProtocolTest, AllocationResponseIncludesUsageLines) {
    Dic::Protocol::MemScopeMemoryAllocationsResponse response;
    response.minTimestamp = 1;
    response.maxTimestamp = 2;
    response.allocations.emplace_back(1, 100, "1", "PTA", false, 200, 300, 400);
    response.reservedLine.emplace_back(1, 200);
    response.processUsedLine.emplace_back(1, 300);
    response.deviceUsedLine.emplace_back(1, 400);

    const auto json = response.ToJson();
    ASSERT_TRUE(json.has_value());
    ASSERT_TRUE((*json)["body"]["allocations"].IsArray());
    ASSERT_TRUE((*json)["body"]["reservedLine"].IsArray());
    ASSERT_TRUE((*json)["body"]["processUsedLine"].IsArray());
    ASSERT_TRUE((*json)["body"]["deviceUsedLine"].IsArray());
    const auto &allocation = (*json)["body"]["allocations"][0];
    EXPECT_TRUE(allocation.HasMember("timestamp"));
    EXPECT_TRUE(allocation.HasMember("totalSize"));
    EXPECT_FALSE(allocation.HasMember("reservedSize"));
    EXPECT_EQ((*json)["body"]["reservedLine"][0]["timestamp"].GetUint64(), 1);
    EXPECT_EQ((*json)["body"]["reservedLine"][0]["reservedSize"].GetUint64(), 200);
    EXPECT_EQ((*json)["body"]["processUsedLine"][0]["processUsed"].GetUint64(), 300);
    EXPECT_EQ((*json)["body"]["deviceUsedLine"][0]["deviceUsed"].GetUint64(), 400);
}

TEST_F(MemScopeProtocolTest, BlocksResponseSerializesActualBlockAndHeaders) {
    Dic::Protocol::MemScopeMemoryBlocksResponse response;
    response.minTimestamp = 10;
    response.maxTimestamp = 20;
    response.blocks.emplace_back("0x100", "0", 64, 10, 20, "PTA", "BLOCK", "{}", 1, 2);
    const auto json = response.ToJson();
    ASSERT_TRUE(json.has_value());
    const auto &body = (*json)["body"];
    ASSERT_EQ(body["blocks"].Size(), 1U);
    EXPECT_STREQ(body["blocks"][0]["addr"].GetString(), "0x100");
    EXPECT_EQ(body["blocks"][0]["size"].GetUint64(), 64U);
    EXPECT_GT(body["headers"].Size(), 0U);
}

TEST_F(MemScopeProtocolTest, DetailsResponseSerializesNestedTreeAndEmptyState) {
    Dic::Protocol::MemScopeMemoryDetailsResponse response;
    auto empty = response.ToJson();
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE((*empty)["body"].IsObject());
    response.detail = std::make_unique<Dic::Module::MemScope::MemScopeMemoryDetailTreeNode>("PTA");
    response.detail->size = 64;
    response.detail->children.emplace_back(
        std::make_unique<Dic::Module::MemScope::MemScopeMemoryDetailTreeNode>("PTA@ops"));
    auto tree = response.ToJson();
    ASSERT_TRUE(tree.has_value());
    EXPECT_EQ((*tree)["body"]["size"].GetUint64(), 64U);
    ASSERT_EQ((*tree)["body"]["subNodes"].Size(), 1U);
}

TEST_F(MemScopeProtocolTest, PythonTraceAndEventResponsesHonorOptionalFields) {
    Dic::Protocol::MemScopePythonTracesResponse traces;
    traces.trace.threadId = 8;
    traces.trace.slices.emplace_back("forward", 10, 20, 1);
    const auto traceJson = traces.ToJson();
    ASSERT_TRUE(traceJson.has_value());
    EXPECT_STREQ((*traceJson)["body"]["traces"][0]["func"].GetString(), "forward");

    Dic::Protocol::MemScopeEventResponse events;
    events.events.emplace_back();
    events.events.back().callStackC = "c-stack";
    events.events.back().callStackPython = "py-stack";
    const auto withoutStacks = events.ToJson();
    ASSERT_TRUE(withoutStacks.has_value());
    EXPECT_FALSE((*withoutStacks)["body"]["events"][0].HasMember("callStackC"));
    events.withCallStackC = true;
    events.withCallStackPython = true;
    const auto withStacks = events.ToJson();
    ASSERT_TRUE(withStacks.has_value());
    EXPECT_STREQ((*withStacks)["body"]["events"][0]["callStackC"].GetString(), "c-stack");
    EXPECT_STREQ((*withStacks)["body"]["events"][0]["callStackPython"].GetString(), "py-stack");
}
