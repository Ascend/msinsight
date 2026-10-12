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

#include <gtest/gtest.h>
#include "JsonUtil.h"
#include "TimelineProtocol.h"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"

namespace {
using namespace Dic;
using namespace Dic::Protocol;

class TimelineProtocolCoverageTest : public ::testing::Test {
  protected:
    void SetUp() override { protocol.Register(); }

    document_t MakeRequest(const std::string &command, const std::string &params = "{}") {
        document_t json;
        const std::string request = R"({"id":42,"type":"request","moduleName":"timeline",)"
                                    R"("projectName":"protocol-ut","fileId":"trace.db","command":")" +
            command + R"(","params":)" + params + "}";
        json.Parse(request.c_str());
        EXPECT_FALSE(json.HasParseError());
        return json;
    }

    std::unique_ptr<Request> Parse(const std::string &command, const std::string &params = "{}") {
        error.clear();
        auto json = MakeRequest(command, params);
        return protocol.FromJson(json, error);
    }

    std::optional<document_t> Serialize(const Response &response) {
        error.clear();
        auto result = protocol.ToJson(response, error);
        EXPECT_TRUE(error.empty()) << error;
        return result;
    }

    static void ExpectJson(const json_t &actual, const char *expectedText) {
        document_t expected;
        expected.Parse(expectedText);
        ASSERT_FALSE(expected.HasParseError());
        EXPECT_TRUE(actual == expected) << "Actual: " << JsonUtil::JsonDump(actual) << "\nExpected: " << expectedText;
    }

    TimelineProtocol protocol;
    std::string error;
};

class TimelineProtocolMissingParamsCoverageTest : public TimelineProtocolCoverageTest,
                                                  public ::testing::WithParamInterface<std::string> {};

TEST_P(TimelineProtocolMissingParamsCoverageTest, RejectsIncompleteRequestEnvelope) {
    for (const auto *missing : {"id", "moduleName", "params"}) {
        SCOPED_TRACE(missing);
        auto json = MakeRequest(GetParam());
        json.RemoveMember(missing);
        error.clear();
        EXPECT_EQ(protocol.FromJson(json, error), nullptr);
        EXPECT_NE(error.find("Failed to set request base info"), std::string::npos);
    }
    auto json = MakeRequest(GetParam());
    json["params"].SetNull();
    error.clear();
    EXPECT_EQ(protocol.FromJson(json, error), nullptr);
    EXPECT_NE(error.find("Failed to set request base info"), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(TimelineCommands, TimelineProtocolMissingParamsCoverageTest,
    ::testing::Values(REQ_RES_KERNEL_E2E_TIME, REQ_RES_SYSTEM_VIEW_FTRACE_STAT, REQ_RES_MEMCPY_OVERALL,
        REQ_RES_KERNEL_OVERALL, REQ_RES_MEMCPY_DETAIL, REQ_RES_RANK_OFFSET, REQ_RES_SYSTEM_VIEW_OVERALL,
        REQ_RES_SYSTEM_VIEW_OVERALL_MORE_DETAILS));

TEST_F(TimelineProtocolCoverageTest, E2ERequestPreservesTimeRangePagingAndSort) {
    auto request = Parse(REQ_RES_KERNEL_E2E_TIME, R"({"rankId":"rank-2","startTime":100,"endTime":200,
        "current":3,"pageSize":20,"pathType":"normal","opName":"MatMul","sortField":"launchTime",
        "sortOrder":"asc"})");
    const auto *typed = dynamic_cast<KernelE2ETimeRequest *>(request.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->id, 42);
    EXPECT_EQ(typed->moduleName, "timeline");
    EXPECT_EQ(typed->projectName, "protocol-ut");
    EXPECT_EQ(typed->fileId, "trace.db");
    EXPECT_EQ(typed->params.rankId, "rank-2");
    EXPECT_EQ(typed->params.startTime, 100);
    EXPECT_EQ(typed->params.endTime, 200);
    EXPECT_EQ(typed->params.current, 3);
    EXPECT_EQ(typed->params.pageSize, 20);
    EXPECT_EQ(typed->params.pathType, "normal");
    EXPECT_EQ(typed->params.opName, "MatMul");
    EXPECT_EQ(typed->params.sortField, "launchTime");
    EXPECT_EQ(typed->params.sortOrder, "asc");
    EXPECT_TRUE(error.empty());
}

TEST_F(TimelineProtocolCoverageTest, E2ERequestKeepsDefaultsForAbsentOrWrongTypedOptionalFields) {
    for (const auto *params : {"{}", R"({"rankId":1,"startTime":"100","endTime":false,
        "current":"2","pageSize":null,"pathType":[],"opName":{},"sortField":3,"sortOrder":true})"}) {
        SCOPED_TRACE(params);
        auto request = Parse(REQ_RES_KERNEL_E2E_TIME, params);
        const auto *typed = dynamic_cast<KernelE2ETimeRequest *>(request.get());
        ASSERT_NE(typed, nullptr);
        EXPECT_TRUE(typed->params.rankId.empty());
        EXPECT_EQ(typed->params.startTime, 0);
        EXPECT_EQ(typed->params.endTime, 0);
        EXPECT_EQ(typed->params.current, 1);
        EXPECT_EQ(typed->params.pageSize, 100);
        EXPECT_EQ(typed->params.pathType, "all");
        EXPECT_TRUE(typed->params.opName.empty());
        EXPECT_EQ(typed->params.sortField, "endToEndTime");
        EXPECT_EQ(typed->params.sortOrder, "desc");
        EXPECT_TRUE(error.empty());
    }
}

TEST_F(TimelineProtocolCoverageTest, FtraceRequestPreservesLayerPagingSortAndFilters) {
    auto request = Parse(REQ_RES_SYSTEM_VIEW_FTRACE_STAT, R"({"layer":"CPU","rankId":"host-0",
        "current":2,"pageSize":15,"orderBy":"duration","order":"descend",
        "filterCondition":["{\"columnName\":\"name\",\"value\":\"worker\"}",
            "{\"columnName\":\"pid\",\"value\":\"41\"}"]})");
    const auto *typed = dynamic_cast<SystemViewFtraceStatRequest *>(request.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->params.layer, "CPU");
    EXPECT_EQ(typed->params.rankId, "host-0");
    EXPECT_EQ(typed->params.current, 2);
    EXPECT_EQ(typed->params.pageSize, 15);
    EXPECT_EQ(typed->params.orderBy, "duration");
    EXPECT_EQ(typed->params.order, "descend");
    const std::vector<std::pair<std::string, std::string>> expected{{"name", "worker"}, {"pid", "41"}};
    EXPECT_EQ(typed->params.filters, expected);
    EXPECT_TRUE(error.empty());
}

TEST_F(TimelineProtocolCoverageTest, KernelOverallRequestPreservesPagingTimeRangeSortAndFilters) {
    auto request = Parse(REQ_RES_KERNEL_OVERALL, R"({"rankId":"rank-0","current":4,"pageSize":25,
        "startTime":1000,"endTime":3000,"orderBy":"totalTime","order":"ascend",
        "filterCondition":["{\"columnName\":\"type\",\"value\":\"MatMul\"}",
            "{\"columnName\":\"acceleratorCore\",\"value\":\"AI_CORE\"}"]})");
    const auto *typed = dynamic_cast<KernelOverallRequest *>(request.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->params.rankId, "rank-0");
    EXPECT_EQ(typed->params.page.current, 4);
    EXPECT_EQ(typed->params.page.pageSize, 25);
    EXPECT_EQ(typed->params.startTime, 1000);
    EXPECT_EQ(typed->params.endTime, 3000);
    EXPECT_EQ(typed->params.orderBy, "totalTime");
    EXPECT_EQ(typed->params.order, "ascend");
    const std::vector<std::pair<std::string, std::string>> expected{{"type", "MatMul"}, {"acceleratorCore", "AI_CORE"}};
    EXPECT_EQ(typed->params.filters, expected);
    EXPECT_TRUE(error.empty());
}

class TimelineProtocolFilterCoverageTest : public TimelineProtocolCoverageTest,
                                           public ::testing::WithParamInterface<std::string> {
  protected:
    static const std::vector<std::pair<std::string, std::string>> *GetFilters(const Request *request) {
        if (const auto *ftrace = dynamic_cast<const SystemViewFtraceStatRequest *>(request)) {
            return &ftrace->params.filters;
        }
        if (const auto *kernel = dynamic_cast<const KernelOverallRequest *>(request)) {
            return &kernel->params.filters;
        }
        return nullptr;
    }
};

TEST_P(TimelineProtocolFilterCoverageTest, SkipsMalformedAndNonStringFiltersWithoutLosingValidFilters) {
    auto request = Parse(GetParam(), R"({"filterCondition":[7,null,false,{},"invalid json",
        "{\"columnName\":\"name\",\"value\":\"retained\"}"]})");
    const auto *filters = GetFilters(request.get());
    ASSERT_NE(filters, nullptr);
    const std::vector<std::pair<std::string, std::string>> expected{{"name", "retained"}};
    EXPECT_EQ(*filters, expected);
    EXPECT_NE(error.find("invalid filter json"), std::string::npos);
}

TEST_P(TimelineProtocolFilterCoverageTest, AbsentEmptyAndWrongTypedFilterCollectionsProduceNoFilters) {
    for (const auto *params :
        {"{}", R"({"filterCondition":[]})", R"({"filterCondition":{}})", R"({"filterCondition":null})"}) {
        SCOPED_TRACE(params);
        auto request = Parse(GetParam(), params);
        const auto *filters = GetFilters(request.get());
        ASSERT_NE(filters, nullptr);
        EXPECT_TRUE(filters->empty());
        EXPECT_TRUE(error.empty());
    }
}

INSTANTIATE_TEST_SUITE_P(FilterCommands, TimelineProtocolFilterCoverageTest,
    ::testing::Values(REQ_RES_SYSTEM_VIEW_FTRACE_STAT, REQ_RES_KERNEL_OVERALL));

TEST_F(TimelineProtocolCoverageTest, MemcpyOverallRequestPreservesPagingAndTimeRange) {
    auto request = Parse(
        REQ_RES_MEMCPY_OVERALL, R"({"rankId":"rank-3","current":2,"pageSize":50,"startTime":800,"endTime":1800})");
    const auto *typed = dynamic_cast<MemcpyOverallRequest *>(request.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->params.rankId, "rank-3");
    EXPECT_EQ(typed->params.page.current, 2);
    EXPECT_EQ(typed->params.page.pageSize, 50);
    EXPECT_EQ(typed->params.startTime, 800);
    EXPECT_EQ(typed->params.endTime, 1800);
    EXPECT_TRUE(error.empty());
}

TEST_F(TimelineProtocolCoverageTest, MoreDetailsAndMemcpyDetailRequestsPreserveCategoriesAndOrdering) {
    for (const auto &command : {REQ_RES_SYSTEM_VIEW_OVERALL_MORE_DETAILS, REQ_RES_MEMCPY_DETAIL}) {
        SCOPED_TRACE(command);
        auto request = Parse(command, R"({"rankId":"rank-1","categoryList":["Computing Time","MatMul"],
            "name":"kernel","orderBy":"duration","order":"descend","current":3,"pageSize":10,
            "startTime":100,"endTime":500})");
        const auto *typed = dynamic_cast<SystemViewOverallMoreDetailsRequest *>(request.get());
        ASSERT_NE(typed, nullptr);
        EXPECT_EQ(typed->command, command);
        EXPECT_EQ(typed->params.rankId, "rank-1");
        EXPECT_EQ(typed->params.categoryList, (std::vector<std::string>{"Computing Time", "MatMul"}));
        EXPECT_EQ(typed->params.name, "kernel");
        EXPECT_EQ(typed->params.order.orderBy, "duration");
        EXPECT_EQ(typed->params.order.orderType, "descend");
        EXPECT_EQ(typed->params.page.current, 3);
        EXPECT_EQ(typed->params.page.pageSize, 10);
        EXPECT_EQ(typed->params.startTime, 100);
        EXPECT_EQ(typed->params.endTime, 500);
    }
}

TEST_F(TimelineProtocolCoverageTest, MoreDetailsRequestHandlesEmptyAndWrongTypedCategoryCollections) {
    for (const auto *params : {"{}", R"({"categoryList":[]})", R"({"categoryList":"MatMul"})"}) {
        SCOPED_TRACE(params);
        auto request = Parse(REQ_RES_SYSTEM_VIEW_OVERALL_MORE_DETAILS, params);
        const auto *typed = dynamic_cast<SystemViewOverallMoreDetailsRequest *>(request.get());
        ASSERT_NE(typed, nullptr);
        EXPECT_TRUE(typed->params.categoryList.empty());
    }
}

TEST_F(TimelineProtocolCoverageTest, RankOffsetRequestPreservesLocatorAndLargeStringTimestamp) {
    auto request = Parse(REQ_RES_RANK_OFFSET, R"({"sliceName":"MatMul","rankId":"rank-4","fileId":"rank.db",
        "pid":"10","metaType":"Ascend Hardware","alignType":"RIGHT","id":"slice-9",
        "startTime":"9007199254740993","duration":120})");
    const auto *typed = dynamic_cast<RankOffsetRequest *>(request.get());
    ASSERT_NE(typed, nullptr);
    EXPECT_EQ(typed->params.sliceName, "MatMul");
    EXPECT_EQ(typed->params.rankId, "rank-4");
    EXPECT_EQ(typed->params.fileId, "rank.db");
    EXPECT_EQ(typed->params.pid, "10");
    EXPECT_EQ(typed->params.metaType, "Ascend Hardware");
    EXPECT_EQ(typed->params.alignType, "RIGHT");
    EXPECT_EQ(typed->params.id, "slice-9");
    EXPECT_EQ(typed->params.startTime, 9007199254740993ULL);
    EXPECT_EQ(typed->params.duration, 120);
    EXPECT_TRUE(error.empty());
}

TEST_F(TimelineProtocolCoverageTest, RankOffsetRequestDefaultsInvalidOrMissingStringTimestampToZero) {
    for (const auto *params : {"{}", R"({"startTime":"not-a-timestamp"})", R"({"startTime":123})",
             R"({"startTime":"999999999999999999999999999999"})"}) {
        SCOPED_TRACE(params);
        auto request = Parse(REQ_RES_RANK_OFFSET, params);
        const auto *typed = dynamic_cast<RankOffsetRequest *>(request.get());
        ASSERT_NE(typed, nullptr);
        EXPECT_EQ(typed->params.startTime, 0);
    }
}

TEST_F(TimelineProtocolCoverageTest, E2EResponseSerializesNestedRecordsHighlightsAndAllDurations) {
    KernelE2ETimeResponse response;
    response.result = true;
    response.requestId = 42;
    response.moduleName = "timeline";
    response.body.totalCount = 3;
    response.body.normalCount = 1;
    response.body.fallbackCount = 1;
    response.body.incompleteCount = 1;
    response.body.launchMatchRate = 0.75;
    KernelE2ETimeRecordDto parent;
    parent.id = "parent";
    parent.opName = "MatMul";
    parent.pathType = "normal";
    parent.isParent = true;
    parent.prepareTime = 10;
    parent.pythonApiTime = 20;
    parent.enqueueTime = 30;
    parent.queueTime = 40;
    parent.pipeline2Time = 50;
    parent.launchTime = 60;
    parent.endToEndTime = 210;
    parent.status = "normal";
    parent.highlightSlices.push_back({"launch", "LaunchKernel", 100, 60, "pid-1", "tid-2", "slice-3", ""});
    KernelE2ETimeRecordDto child;
    child.id = "child";
    child.opName = "MatMul-child";
    child.pathType = "fallback";
    child.status = "incomplete";
    child.diagnostic = "launch slice missing";
    child.highlightSlices.push_back({"launch", "", 0, 0, "", "", "", "not found"});
    parent.children.push_back(child);
    response.body.records.push_back(parent);

    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    EXPECT_EQ((*json)["requestId"].GetUint(), 42);
    EXPECT_TRUE((*json)["result"].GetBool());
    EXPECT_STREQ((*json)["command"].GetString(), REQ_RES_KERNEL_E2E_TIME.c_str());
    ExpectJson((*json)["body"], R"({"records":[{"id":"parent","opName":"MatMul","pathType":"normal",
        "isParent":true,"prepareTime":10,"pythonApiTime":20,"enqueueTime":30,"queueTime":40,
        "pipeline2Time":50,"launchTime":60,"endToEndTime":210,"status":"normal","diagnostic":"",
        "highlightSlices":[{"role":"launch","name":"LaunchKernel","startTime":100,"duration":60,
            "pid":"pid-1","tid":"tid-2","id":"slice-3"}],
        "children":[{"id":"child","opName":"MatMul-child","pathType":"fallback","isParent":false,
            "status":"incomplete","diagnostic":"launch slice missing",
            "highlightSlices":[{"role":"launch","name":"","startTime":0,"duration":0,
                "missingReason":"not found"}]}]}],
        "totalCount":3,"normalCount":1,"fallbackCount":1,"incompleteCount":1,"launchMatchRate":0.75})");
}

TEST_F(TimelineProtocolCoverageTest, E2EResponseRetainsZeroDurationsAndOmitsUnavailableDurations) {
    KernelE2ETimeResponse response;
    KernelE2ETimeRecordDto record;
    record.prepareTime = 0;
    record.launchTime = 0;
    response.body.records.push_back(record);
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ASSERT_EQ((*json)["body"]["records"].Size(), 1);
    ExpectJson((*json)["body"]["records"][0], R"({"id":"","opName":"","pathType":"","isParent":false,
        "prepareTime":0,"launchTime":0,"status":"","diagnostic":"","highlightSlices":[]})");
}

TEST_F(TimelineProtocolCoverageTest, E2EResponseRepresentsEmptyResultsWithZeroSummary) {
    KernelE2ETimeResponse response;
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"records":[],"totalCount":0,"normalCount":0,"fallbackCount":0,
        "incompleteCount":0,"launchMatchRate":0})");
}

TEST_F(TimelineProtocolCoverageTest, FtraceResponsePreservesDynamicColumnValuesAndPagination) {
    SystemViewFtraceStatResponse response;
    response.data = {
        {{"name", "worker"}, {"duration", "12.5"}, {"pid", "41"}}, {{"name", "quoted \"worker\""}, {"duration", "0"}}};
    response.pageParam = {2, 15, 31};
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"data":[{"name":"worker","duration":"12.5","pid":"41"},
        {"name":"quoted \"worker\"","duration":"0"}],"current":2,"pageSize":15,"count":31})");
}

TEST_F(TimelineProtocolCoverageTest, FtraceResponsePreservesEmptyRowsAndEmptyResultSet) {
    SystemViewFtraceStatResponse response;
    auto empty = Serialize(response);
    ASSERT_TRUE(empty.has_value());
    ExpectJson((*empty)["body"], R"({"data":[],"current":0,"pageSize":0,"count":0})");
    response.data.emplace_back();
    auto row = Serialize(response);
    ASSERT_TRUE(row.has_value());
    ExpectJson((*row)["body"]["data"], "[{}]");
}

TEST_F(TimelineProtocolCoverageTest, MemcpyOverallResponseSerializesThreadAndTypeStatistics) {
    MemcpyOverallResponse response;
    response.command = REQ_RES_MEMCPY_OVERALL;
    response.pageParam = {2, 10, 12};
    response.isLoading = true;
    MemcpyOverallRes thread{"thread-1", "stream-1", 1024, 12.5, 2, 512, 256, 768, 6.25, 2.5, 10, 1, {}};
    thread.children.push_back({"h2d", "Host to Device", 1024, 12.5, 2, 512, 256, 768, 6.25, 2.5, 10, 2, {}});
    response.details.push_back(thread);
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"data":[{"key":"thread-1","name":"stream-1","totalSize":1024,
        "totalTime":12.5,"number":2,"avgSize":512,"minSize":256,"maxSize":768,
        "avgTime":6.25,"minTime":2.5,"maxTime":10,"level":1,
        "children":[{"key":"h2d","name":"Host to Device","totalSize":1024,"totalTime":12.5,"number":2,
            "avgSize":512,"minSize":256,"maxSize":768,"avgTime":6.25,"minTime":2.5,"maxTime":10,"level":2}]}],
        "count":12,"pageSize":10,"current":2,"isLoading":true})");
}

TEST_F(TimelineProtocolCoverageTest, MemcpyOverallResponseLimitsNestedStatisticsToFiveLevels) {
    MemcpyOverallResponse response;
    response.command = REQ_RES_MEMCPY_OVERALL;
    response.details.emplace_back();
    auto *node = &response.details.front();
    for (int level = 1; level <= 6; ++level) {
        node->level = level;
        node->name = "level-" + std::to_string(level);
        if (level < 6) {
            node->children.emplace_back();
            node = &node->children.front();
        }
    }
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    const auto *serialized = &(*json)["body"]["data"][0];
    for (int level = 1; level <= 5; ++level) {
        EXPECT_EQ((*serialized)["level"].GetInt(), level);
        EXPECT_EQ((*serialized)["name"].GetString(), "level-" + std::to_string(level));
        if (level < 5) {
            ASSERT_TRUE(serialized->HasMember("children"));
            ASSERT_EQ((*serialized)["children"].Size(), 1);
            serialized = &(*serialized)["children"][0];
        }
    }
    EXPECT_FALSE(serialized->HasMember("children"));
}

TEST_F(TimelineProtocolCoverageTest, MemcpyOverallResponseSerializesEmptyPage) {
    MemcpyOverallResponse response;
    response.command = REQ_RES_MEMCPY_OVERALL;
    response.pageParam = {1, 20, 0};
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"data":[],"count":0,"pageSize":20,"current":1,"isLoading":false})");
}

TEST_F(TimelineProtocolCoverageTest, KernelOverallResponseSerializesAllStatisticsAndLoadingState) {
    KernelOverallResponse response;
    response.details.push_back({"kernel-1", "MatMul", "AI_CORE", 15.5, 0.25, 2, 7.75, 5.5, 10});
    response.pageParam = {3, 10, 24};
    response.isLoading = true;
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"data":[{"key":"kernel-1","type":"MatMul","acceleratorCore":"AI_CORE",
        "totalTime":15.5,"ratio":0.25,"number":2,"avgTime":7.75,"minTime":5.5,"maxTime":10}],
        "count":24,"pageSize":10,"current":3,"isLoading":true})");
}

TEST_F(TimelineProtocolCoverageTest, KernelOverallResponseSerializesEmptyPage) {
    KernelOverallResponse response;
    response.pageParam = {1, 20, 0};
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"data":[],"count":0,"pageSize":20,"current":1,"isLoading":false})");
}

TEST_F(TimelineProtocolCoverageTest, MemcpyDetailResponsePreservesExplicitIdsAndAssignsMissingIds) {
    MemcpyDetailResponse response;
    response.body.rankId = "rank-2";
    response.body.count = 30;
    response.body.pageSize = 3;
    response.body.currentPage = 2;
    response.body.sameOperatorsDetails = {
        {100, 10, 128, "slice-1", "H2D"}, {200, 20, 256, "", "D2H"}, {300, 30, 512, "", "D2D"}};
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"sameOperatorsDetails":[
        {"timestamp":100,"duration":10,"size":128,"id":"slice-1","name":"H2D"},
        {"timestamp":200,"duration":20,"size":256,"id":0,"name":"D2H"},
        {"timestamp":300,"duration":30,"size":512,"id":1,"name":"D2D"}],
        "rankId":"rank-2","count":30,"pageSize":3,"currentPage":2})");
}

TEST_F(TimelineProtocolCoverageTest, MemcpyDetailResponseOmitsUnavailableRankIdForEmptyResults) {
    MemcpyDetailResponse response;
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"sameOperatorsDetails":[],"count":0,"pageSize":0,"currentPage":0})");
}

TEST_F(TimelineProtocolCoverageTest, RankOffsetResponsePreservesNegativeAndPositiveOffsets) {
    RankOffsetResponse response;
    response.body.baseOffset = 1000;
    response.body.successList = {{"rank-0", -500}, {"rank-1", 0}, {"rank-2", 600}};
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"result":[{"rankId":"rank-0","offset":-500},
        {"rankId":"rank-1","offset":0},{"rankId":"rank-2","offset":600}],"baseOffset":1000})");
    ExpectJson((*json)["error"], R"({"code":0,"message":""})");
}

TEST_F(TimelineProtocolCoverageTest, RankOffsetResponseSerializesEmptyResultsAndDomainError) {
    RankOffsetResponse response;
    response.body.error = {7, "no matching slices"};
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"result":[],"baseOffset":0})");
    ExpectJson((*json)["error"], R"({"code":7,"message":"no matching slices"})");
}

TEST_F(TimelineProtocolCoverageTest, OverallResponseSerializesStatisticsAndUnmatchedKeywords) {
    SystemViewOverallResponse response;
    SystemViewOverallRes category;
    category.totalTime = 50.5;
    category.ratio = 0.5;
    category.nums = 2;
    category.avg = 25.25;
    category.max = 30;
    category.min = 20.5;
    category.name = "Computing Time";
    category.id = "computing";
    SystemViewOverallRes child;
    child.name = "No samples";
    child.id = "empty";
    child.level = 2;
    child.max = 0;
    category.children.push_back(child);
    response.details.push_back(category);
    response.unmatchedCustomClassificationKeywords = {"missing_op", "another_op"};
    response.pageParam = {1, 10, 1};
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    ExpectJson((*json)["body"], R"({"data":[{"totalTime":50.5,"ratio":0.5,"nums":2,"avg":25.25,
        "max":30,"min":20.5,"name":"Computing Time","id":"computing","level":1,
        "children":[{"totalTime":0,"ratio":0,"nums":"-","avg":"-","max":"-","min":"-",
            "name":"No samples","id":"empty","level":2}]}],"count":1,"pageSize":10,"current":1,
        "isLoading":false,"unmatchedCustomClassificationKeywords":["missing_op","another_op"]})");
}

TEST_F(TimelineProtocolCoverageTest, OverallResponseLimitsNestedCategoriesToFiveLevels) {
    SystemViewOverallResponse response;
    response.details.emplace_back();
    auto *node = &response.details.front();
    for (uint32_t level = 1; level <= 6; ++level) {
        node->level = level;
        node->max = 0;
        if (level < 6) {
            node->children.emplace_back();
            node = &node->children.front();
        }
    }
    auto json = Serialize(response);
    ASSERT_TRUE(json.has_value());
    const auto *serialized = &(*json)["body"]["data"][0];
    for (uint32_t level = 1; level <= 5; ++level) {
        EXPECT_EQ((*serialized)["level"].GetUint(), level);
        if (level < 5) {
            ASSERT_TRUE(serialized->HasMember("children"));
            ASSERT_EQ((*serialized)["children"].Size(), 1);
            serialized = &(*serialized)["children"][0];
        }
    }
    EXPECT_FALSE(serialized->HasMember("children"));
}

struct InvalidE2EParams {
    const char *name;
    const char *rankId;
    uint64_t startTime;
    uint64_t endTime;
    uint64_t minTime;
    uint64_t current;
    uint64_t pageSize;
};

class TimelineE2EParamsCoverageTest : public ::testing::TestWithParam<InvalidE2EParams> {};

TEST_P(TimelineE2EParamsCoverageTest, RejectsInvalidRangeRankAndPagination) {
    const auto &input = GetParam();
    KernelE2ETimeParams params;
    params.rankId = input.rankId;
    params.startTime = input.startTime;
    params.endTime = input.endTime;
    params.current = input.current;
    params.pageSize = input.pageSize;
    std::string error;
    EXPECT_FALSE(params.CheckParams(input.minTime, error));
    EXPECT_FALSE(error.empty());
}

INSTANTIATE_TEST_SUITE_P(InvalidInput, TimelineE2EParamsCoverageTest,
    ::testing::Values(InvalidE2EParams{"ReversedRange", "rank-0", 20, 10, 0, 1, 10},
        InvalidE2EParams{"AbsoluteTimeOverflow", "rank-0", 0, UINT64_MAX, 1, 1, 10},
        InvalidE2EParams{"MissingRank", "", 0, 10, 0, 1, 10},
        InvalidE2EParams{"ZeroCurrentPage", "rank-0", 0, 10, 0, 0, 10},
        InvalidE2EParams{"ZeroPageSize", "rank-0", 0, 10, 0, 1, 0}),
    [](const ::testing::TestParamInfo<InvalidE2EParams> &info) { return info.param.name; });

TEST_F(TimelineProtocolCoverageTest, E2EParamsAcceptBoundaryTimestampWithoutOverflow) {
    KernelE2ETimeParams params;
    params.rankId = "rank-0";
    params.startTime = UINT64_MAX - 10;
    params.endTime = UINT64_MAX - 10;
    std::string error;
    EXPECT_TRUE(params.CheckParams(10, error));
    EXPECT_TRUE(error.empty());
}
} // namespace
