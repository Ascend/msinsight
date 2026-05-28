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

class MemScopeProtocolTest : public ::testing::Test {
  public:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() {}
};

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
