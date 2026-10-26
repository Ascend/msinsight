/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2. See the License file in the project root for details.
 */
#include <gtest/gtest.h>
#include "TimelineProtocol.h"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"

namespace Dic::Protocol::Test {
class FreeAttributionProtocolTest : public testing::Test {
  protected:
    void SetUp() override { protocol.Register(); }

    std::unique_ptr<Request> Parse(const std::string &command, const std::string &params) {
        document_t json;
        const auto source = std::string(R"({"id":7,"type":"request","moduleName":"timeline",)") +
            R"("fileId":"original.db","command":")" + command + R"(","params":)" + params + "}";
        json.Parse(source.c_str());
        EXPECT_FALSE(json.HasParseError());
        return protocol.FromJson(json, error);
    }

    TimelineProtocol protocol;
    std::string error;
};

TEST_F(FreeAttributionProtocolTest, SummaryPreservesRangeSortPagingAndExplicitDatabase) {
    auto request = Parse(REQ_RES_FREE_ATTRIBUTION_SUMMARY,
        R"({"rankId":"rank-1","dbPath":"selected.db","startTime":150,"endTime":200,
        "current":2,"pageSize":20,"orderBy":"freeCount","order":"asc"})");
    const auto *typed = dynamic_cast<FreeAttributionSummaryRequest *>(request.get());
    ASSERT_NE(typed, nullptr) << error;
    EXPECT_EQ(typed->params.rankId, "rank-1");
    EXPECT_EQ(typed->params.dbPath, "selected.db");
    EXPECT_EQ(typed->params.startTime, 150U);
    EXPECT_EQ(typed->params.endTime, 200U);
    EXPECT_EQ(typed->params.current, 2U);
    EXPECT_EQ(typed->params.pageSize, 20U);
    EXPECT_EQ(typed->params.orderBy, "freeCount");
    EXPECT_EQ(typed->params.order, "asc");
    EXPECT_TRUE(typed->params.rangeMode.empty());
    EXPECT_TRUE(typed->params.searchName.empty());
}

TEST_F(FreeAttributionProtocolTest, DetailsUsesIndependentDefaultsAndPreservesOpaqueGroup) {
    auto request = Parse(REQ_RES_FREE_ATTRIBUTION_DETAILS, R"({"rankId":"rank-1","groupId":"api:aten::add"})");
    const auto *typed = dynamic_cast<FreeAttributionDetailsRequest *>(request.get());
    ASSERT_NE(typed, nullptr) << error;
    EXPECT_EQ(typed->params.groupId, "api:aten::add");
    EXPECT_EQ(typed->params.orderBy, "freeStartTime");
    EXPECT_EQ(typed->params.order, "asc");
    EXPECT_EQ(typed->params.dbPath, "original.db");
    EXPECT_EQ(typed->params.startTime, typed->params.endTime);
    EXPECT_TRUE(typed->params.rangeMode.empty());
    EXPECT_TRUE(typed->params.searchName.empty());
}

TEST_F(FreeAttributionProtocolTest, PreservesOptionalSearchNameInBothRequestTypes) {
    for (const auto *name : {"", "aten::MatMul"}) {
        const auto params =
            std::string(R"({"rankId":"rank-1","groupId":"status:ambiguous","searchName":")") + name + R"("})";
        auto summary = Parse(REQ_RES_FREE_ATTRIBUTION_SUMMARY, params);
        const auto *summaryRequest = dynamic_cast<FreeAttributionSummaryRequest *>(summary.get());
        ASSERT_NE(summaryRequest, nullptr) << error;
        EXPECT_EQ(summaryRequest->params.searchName, name);
        auto details = Parse(REQ_RES_FREE_ATTRIBUTION_DETAILS, params);
        const auto *detailsRequest = dynamic_cast<FreeAttributionDetailsRequest *>(details.get());
        ASSERT_NE(detailsRequest, nullptr) << error;
        EXPECT_EQ(detailsRequest->params.searchName, name);
    }
}

TEST_F(FreeAttributionProtocolTest, RejectsNonStringSearchNamesInBothRequestTypes) {
    for (const auto *name : {"null", "false", "0", "[]", "{}"}) {
        SCOPED_TRACE(name);
        const auto params = std::string(R"({"rankId":"rank-1","searchName":)") + name + "}";
        EXPECT_EQ(Parse(REQ_RES_FREE_ATTRIBUTION_SUMMARY, params), nullptr);
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(Parse(REQ_RES_FREE_ATTRIBUTION_DETAILS, params), nullptr);
        EXPECT_FALSE(error.empty());
    }
}

TEST_F(FreeAttributionProtocolTest, PreservesExplicitRangeModesInBothRequestTypes) {
    for (const auto &mode : {std::string("all"), std::string("range"), std::string("empty")}) {
        SCOPED_TRACE(mode);
        const auto params = std::string(R"({"rankId":"rank-1","groupId":"matched:aten::add",)") +
            R"("startTime":0,"endTime":0,"rangeMode":")" + mode + R"("})";
        auto summary = Parse(REQ_RES_FREE_ATTRIBUTION_SUMMARY, params);
        const auto *summaryRequest = dynamic_cast<FreeAttributionSummaryRequest *>(summary.get());
        ASSERT_NE(summaryRequest, nullptr) << error;
        EXPECT_EQ(summaryRequest->params.rangeMode, mode);
        EXPECT_EQ(summaryRequest->params.startTime, 0U);
        EXPECT_EQ(summaryRequest->params.endTime, 0U);
        auto details = Parse(REQ_RES_FREE_ATTRIBUTION_DETAILS, params);
        const auto *detailsRequest = dynamic_cast<FreeAttributionDetailsRequest *>(details.get());
        ASSERT_NE(detailsRequest, nullptr) << error;
        EXPECT_EQ(detailsRequest->params.rangeMode, mode);
    }
}

TEST_F(FreeAttributionProtocolTest, RejectsNonStringOrEmptyRangeModesInBothRequestTypes) {
    for (const auto *mode : {"null", "false", "0", "[]", "{}", R"("")"}) {
        SCOPED_TRACE(mode);
        const auto params = std::string(R"({"rankId":"rank-1","rangeMode":)") + mode + "}";
        EXPECT_EQ(Parse(REQ_RES_FREE_ATTRIBUTION_SUMMARY, params), nullptr);
        EXPECT_FALSE(error.empty());
        EXPECT_EQ(Parse(REQ_RES_FREE_ATTRIBUTION_DETAILS, params), nullptr);
        EXPECT_FALSE(error.empty());
    }
}

TEST_F(FreeAttributionProtocolTest, RejectsMalformedParametersInsteadOfSilentlyUsingDefaults) {
    for (const auto &params : {"null", "[]", R"({"rankId":1})", R"({"current":-1})", R"({"startTime":1.5})",
             R"({"pageSize":"100"})", R"({"orderBy":false})"}) {
        SCOPED_TRACE(params);
        EXPECT_EQ(Parse(REQ_RES_FREE_ATTRIBUTION_SUMMARY, params), nullptr);
        EXPECT_FALSE(error.empty());
    }
}

TEST_F(FreeAttributionProtocolTest, SerializesSummaryTotalsWithoutConvertingNanoseconds) {
    FreeAttributionSummaryResponse response;
    response.body.groups.push_back({"api:add", "matched", "aten::add", 80001, 2});
    response.body.totalGroups = 1;
    response.body.totalFreeDuration = 80001;
    response.body.totalFreeCount = 2;
    const auto json = protocol.ToJson(response, error);
    ASSERT_TRUE(json.has_value()) << error;
    const auto &body = (*json)["body"];
    EXPECT_EQ(body["totalFreeDuration"].GetUint64(), 80001U);
    ASSERT_EQ(body["groups"].Size(), 1U);
    EXPECT_STREQ(body["groups"][0]["apiName"].GetString(), "aten::add");
    EXPECT_EQ(body["groups"][0]["freeCount"].GetUint64(), 2U);
}

TEST_F(FreeAttributionProtocolTest, SerializesSeparateTargetsAndAllAmbiguousCandidates) {
    FreeAttributionDetailsResponse response;
    Module::Timeline::FreeAttributionRecord record;
    record.detailId = "free:1";
    record.freeDuration = 80;
    record.attributionStatus = "ambiguous";
    record.freeTarget = {"1", "Free", "rank-1", "derived.db", "device-pid", "free-tid", 0, "OVERLAP_ANALYSIS", 100, 80};
    Module::Timeline::FreeAttributionTarget api = {
        "2", "aten::add", "Host", "source.db", "host-pid", "host-tid", 2, "Python", 90, 10};
    record.candidateApiTargets.push_back(api);
    api.id = "3";
    record.candidateApiTargets.push_back(api);
    response.body.records.push_back(record);
    response.body.totalRecords = 1;
    auto json = protocol.ToJson(response, error);
    ASSERT_TRUE(json.has_value()) << error;
    const auto &item = (*json)["body"]["records"][0];
    EXPECT_FALSE(item.HasMember("hostApiTarget"));
    EXPECT_STREQ(item["freeTarget"]["dbPath"].GetString(), "derived.db");
    ASSERT_EQ(item["candidateApiTargets"].Size(), 2U);
    EXPECT_STREQ(item["candidateApiTargets"][0]["cardId"].GetString(), "Host");
    EXPECT_STREQ(item["candidateApiTargets"][0]["dbPath"].GetString(), "source.db");
    EXPECT_STREQ(item["candidateApiTargets"][1]["id"].GetString(), "3");
    response.body.records[0].hostApiTarget = api;
    json = protocol.ToJson(response, error);
    ASSERT_TRUE(json.has_value()) << error;
    EXPECT_STREQ((*json)["body"]["records"][0]["hostApiTarget"]["pid"].GetString(), "host-pid");
}
}
