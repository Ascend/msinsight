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
#include <gtest/gtest.h>
#include "TimelineProtocolUtil.h"
#include "TimelineProtocolResponse.h"
#include "JsonUtil.h"
class TimelineProtocolUtilTest : public ::testing::Test {};

TEST_F(TimelineProtocolUtilTest, ImportActionResponseSerializesIsFtrace) {
    Dic::Protocol::ImportActionResponse response;
    response.body.isFtrace = true;

    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    ASSERT_TRUE(jsonOp.has_value());
    const auto &json = jsonOp.value();
    ASSERT_TRUE(json.HasMember("body"));
    ASSERT_TRUE(json["body"].HasMember("isFtrace"));
    EXPECT_TRUE(json["body"]["isFtrace"].GetBool());
}

/**
 * 测试ThreadTracesResponseToJson正常情况
 */
TEST_F(TimelineProtocolUtilTest, TestThreadTracesResponseToJsonNormal) {
    Dic::Protocol::UnitThreadTracesResponse response;
    const uint64_t curDep = 1;
    response.result = true;
    response.body.currentMaxDepth = curDep;
    const uint64_t maxDep = 2;
    response.body.maxDepth = maxDep;
    Dic::Protocol::ThreadTraces trace;
    trace.name = "kkk";
    const uint64_t end = 4444;
    trace.endTime = end;
    trace.id = "hhh";
    trace.threadId = "lll";
    const uint64_t dur = 777;
    trace.duration = dur;
    const uint64_t str = 444;
    trace.startTime = str;
    trace.pid = "ggg";
    trace.cname = "qqq";
    const uint64_t dep = 1;
    trace.depth = dep;
    std::vector<Dic::Protocol::ThreadTraces> traces;
    traces.emplace_back(trace);
    response.body.data.emplace_back(traces);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":true,\"command\":\"unit/"
        "threadTraces\",\"moduleName\":\"unknown\",\"body\":{\"maxDepth\":2,\"currentMaxDepth\":1,"
        "\"havePythonFunction\":false,\"isLoading\":false,\"data\":[[{\"name\":\"kkk\",\"duration\":777,"
        "\"startTime\":444,\"endTime\":4444,"
        "\"depth\":1,\"threadId\":\"lll\",\"cname\":\"qqq\",\"id\":\"hhh\"}]]}}";
    EXPECT_EQ(json, jsonStr);
}

/**
 * 测试ThreadTracesResponseToJson的error情况
 */
TEST_F(TimelineProtocolUtilTest, TestThreadTracesResponseToJsonError) {
    Dic::Protocol::UnitThreadTracesResponse response;
    response.result = false;
    const uint64_t errorCode = 3;
    Dic::Protocol::ErrorMessage error = {errorCode, "ll"};
    response.error = error;
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"unit/"
        "threadTraces\",\"moduleName\":\"unknown\",\"error\":{\"code\":3,\"message\":\"ll\"},\"body\":{\"maxDepth\":0,"
        "\"currentMaxDepth\":0,\"havePythonFunction\":false,\"isLoading\":false,\"data\":[]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, TestUnitThreadsResponseToJsonError) {
    Dic::Protocol::UnitThreadsResponse response;
    Dic::Protocol::SliceGroupItem sliceGroupItem;
    response.body.data.emplace_back(sliceGroupItem);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"unit/"
        "threads\",\"moduleName\":\"unknown\",\"body\":{\"emptyFlag\":false,\"data\":[{\"title\":\"\",\"wallDuration\":"
        "0,\"occurrences\":0,\"avgWallDuration\":0,\"maxWallDuration\":0,\"minWallDuration\":0,"
        "\"selfTime\":0,\"processes\":[],\"metaTypeList\":[]}]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, TestUnitFlowsResponseToJson) {
    Dic::Protocol::UnitFlowsResponse response;
    Dic::Protocol::UnitSingleFlow unitSingleFlow;
    Dic::Protocol::UnitCatFlows unitCatFlows;
    unitCatFlows.flows.emplace_back(unitSingleFlow);
    response.body.unitAllFlows.emplace_back(unitCatFlows);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"unit/"
        "flows\",\"moduleName\":\"unknown\",\"body\":{\"unitAllFlows\":[{\"cat\":\"\",\"flows\":[{\"title\":\"\","
        "\"cat\":\"\",\"id\":\"\",\"from\":{\"pid\":\"\",\"tid\":\"\",\"timestamp\":0,\"duration\":0,\"depth\":0,"
        "\"name\":\"\",\"id\":\"\",\"metaType\":\"\",\"rankId\":\"\"},\"to\":{\"pid\":\"\",\"tid\":\"\",\"timestamp\":"
        "0,\"duration\":0,\"depth\":0,\"name\":\"\",\"id\":\"\",\"metaType\":\"\",\"rankId\":\"\"}}]}]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, TestFlowCategoryEventsResponseToJson) {
    Dic::Protocol::FlowCategoryEventsResponse response;
    std::unique_ptr<Dic::Protocol::UnitSingleFlow> unitSingleFlow = std::make_unique<Dic::Protocol::UnitSingleFlow>();
    response.body.flowDetailList.emplace_back(std::move(unitSingleFlow));
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"flow/"
        "categoryEvents\",\"moduleName\":\"unknown\",\"body\":{\"flowDetailList\":[{\"category\":\"\",\"from\":{"
        "\"pid\":\"\",\"tid\":\"\",\"timestamp\":0,\"depth\":0,\"rankId\":\"\"},\"to\":{\"pid\":\"\",\"tid\":\"\","
        "\"timestamp\":0,\"depth\":0,\"rankId\":\"\"}}]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, TestEventsViewResponseToJson) {
    Dic::Protocol::EventsViewResponse response;
    Dic::Protocol::EventsViewColumnAttr eventsViewColumnAttr;
    std::unique_ptr<Dic::Protocol::EventDetail> host = std::make_unique<Dic::Protocol::HostEventDetail>();
    std::unique_ptr<Dic::Protocol::EventDetail> device = std::make_unique<Dic::Protocol::DeviceEventDetail>();
    class Other : public Dic::Protocol::EventDetail {};
    std::unique_ptr<Dic::Protocol::EventDetail> other = std::make_unique<Other>();
    response.body.eventDetailList.emplace_back(std::move(host));
    response.body.eventDetailList.emplace_back(std::move(device));
    response.body.eventDetailList.emplace_back(std::move(other));
    response.body.columnList.emplace_back(eventsViewColumnAttr);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"unit/"
        "eventView\",\"moduleName\":\"unknown\",\"body\":{\"eventDetails\":[{\"id\":\"\",\"name\":\"\",\"start\":0,"
        "\"duration\":0,\"depth\":0,\"threadId\":\"\",\"processId\":\"\",\"tid\":\"\",\"pid\":\"\"},{\"id\":\"\","
        "\"name\":\"\",\"start\":0,\"duration\":0,\"depth\":0,\"threadId\":\"\",\"processId\":\"\",\"threadName\":\"\","
        "\"rankId\":\"\"}],\"columnList\":[{\"name\":\"\",\"type\":\"\",\"key\":\"\"}],\"count\":0,\"pageSize\":0,"
        "\"currentPage\":0}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, TestCommunicationKernelResponseToJson) {
    Dic::Protocol::CommunicationKernelResponse response;
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
}

TEST_F(TimelineProtocolUtilTest, TestUnitCounterResponseToJson) {
    Dic::Protocol::UnitCounterResponse response;
    Dic::Protocol::UnitCounterData unitCounterData;
    unitCounterData.valueJsonStr = "{\"name\":\"\",\"type\":\"\",\"key\":\"\"}";
    Dic::Protocol::UnitCounterData unitCounterData2;
    unitCounterData2.valueJsonStr = "lll";
    response.body.data.emplace_back(unitCounterData);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr = "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"unit/"
                                "counter\",\"moduleName\":\"unknown\",\"body\":{\"data\":[{\"timestamp\":0,\"value\":{"
                                "\"name\":\"\",\"type\":\"\",\"key\":\"\"}}]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, RankOffsetResponseSerializesWithNestedResultAndErrorObject) {
    Dic::Protocol::RankOffsetResponse response;
    response.result = true;
    response.body.baseOffset = 42;
    Dic::Protocol::RankOffsetItem item;
    item.rankId = "rank1";
    item.offset = 100;
    item.processId = {"pid1", "pid2"};
    response.body.successList.emplace_back(item);

    auto jsonOp = Dic::Protocol::ToResponseJson(response);

    ASSERT_TRUE(jsonOp.has_value());
    const auto &json = jsonOp.value();
    ASSERT_TRUE(json.HasMember("body"));
    ASSERT_TRUE(json["body"].IsObject());
    ASSERT_TRUE(json["body"].HasMember("result"));
    ASSERT_TRUE(json["body"]["result"].IsArray());
    ASSERT_EQ(json["body"]["result"].Size(), 1);
    EXPECT_TRUE(json["body"]["result"][0].HasMember("rankId"));
    EXPECT_STREQ(json["body"]["result"][0]["rankId"].GetString(), "rank1");
    EXPECT_FALSE(json["body"]["result"][0].HasMember("cardId"));
    EXPECT_FALSE(json["body"]["result"][0].HasMember("body"));
    ASSERT_TRUE(json["body"].HasMember("baseOffset"));
    ASSERT_TRUE(json["body"]["baseOffset"].IsUint64());
    EXPECT_EQ(json["body"]["baseOffset"].GetUint64(), 42);
    EXPECT_FALSE(json.HasMember("errors"));
    ASSERT_TRUE(json.HasMember("error"));
    ASSERT_TRUE(json["error"].IsObject());
    ASSERT_TRUE(json["error"].HasMember("code"));
    ASSERT_TRUE(json["error"]["code"].IsInt());
    EXPECT_EQ(json["error"]["code"].GetInt(), 0);
    ASSERT_TRUE(json["error"].HasMember("message"));
    ASSERT_TRUE(json["error"]["message"].IsString());
    EXPECT_STREQ(json["error"]["message"].GetString(), "");
}

TEST_F(TimelineProtocolUtilTest, TestTableDataNameListResponseToJson) {
    Dic::Protocol::TableDataNameListResponse response;
    response.body.layers.emplace_back("llllllllllll", "kkkkkkkkkk");
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"tableData/"
        "nameList\",\"moduleName\":\"unknown\",\"body\":{\"layers\":[{\"name\":\"llllllllllll\",\"description\":"
        "\"kkkkkkkkkk\"}]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, TestTableDataDatailResponseToJson) {
    Dic::Protocol::TableDataDetailResponse response;
    response.body.totalNum = 50; // 50
    Dic::Protocol::TableColumn col;
    col.key = "ggg";
    col.name = "ggg";
    col.type = "ggg";
    response.body.columnAttr.emplace_back(col);
    std::map<std::string, std::string> data;
    data["ggg"] = "ggggggggggggggggggggggggg";
    response.body.columnData.emplace_back(data);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"tableData/"
        "detail\",\"moduleName\":\"unknown\",\"body\":{\"totalNum\":50,\"tableData\":[{\"ggg\":"
        "\"ggggggggggggggggggggggggg\"}],\"columnAttr\":[{\"name\":\"ggg\",\"type\":\"ggg\",\"key\":\"ggg\"}]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, TestCreateCurveResponseToJson) {
    Dic::Protocol::CreateCurveResponse response;
    response.body.curveName = "lllllllll";
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr = "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"create/"
                                "curve\",\"moduleName\":\"unknown\",\"body\":{\"curveName\":\"lllllllll\"}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, TestUnitThreadsOperatorsResponseToJson) {
    Dic::Protocol::UnitThreadsOperatorsResponse response;
    Dic::Protocol::SameOperatorsDetails sameOperatorsDetails;
    response.body.sameOperatorsDetails.emplace_back(sameOperatorsDetails);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
}

TEST_F(TimelineProtocolUtilTest, TestSearchAllSlicesResponseToJson) {
    Dic::Protocol::SearchAllSlicesResponse response;
    Dic::Protocol::SearchAllSlices searchAllSlices;
    response.body.searchAllSlices.emplace_back(searchAllSlices);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
}

TEST_F(TimelineProtocolUtilTest, TestAllSuccessEventToJson) {
    Dic::Protocol::AllSuccessEvent response;
    Dic::Protocol::CardOffset cardOffset;
    response.body.cardOffsets.emplace_back(cardOffset);
    auto jsonOp = Dic::Protocol::ToEventJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"event\",\"id\":0,\"event\":\"allPagesSuccess\",\"moduleName\":\"unknown\",\"body\":{"
        "\"isAllPageParsed\":false,\"cardOffsets\":[{\"cardId\":\"\",\"offset\":0}],\"minTime\":\"0\"}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, TestParseMemoryCompletedEventToJson) {
    Dic::Protocol::ParseMemoryCompletedEvent response;
    Dic::Protocol::MemorySuccess memorySuccess;
    response.memoryResult.emplace_back(memorySuccess);
    auto jsonOp = Dic::Protocol::ToEventJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr = "{\"type\":\"event\",\"id\":0,\"event\":\"parse/memoryCompleted\","
                                "\"moduleName\":\"unknown\",\"body\":{\"isCluster\":false,"
                                "\"memoryResult\":[{\"rankId\":\"\",\"hasMemory\":false,\"dbPath\":\"\","
                                "\"rankInfo\":{\"clusterId\":\"\",\"host\":\"\",\"rankId\":\"\","
                                "\"rankName\":\"\",\"deviceId\":\"\"}}],\"dbPath\":\"\"}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(TimelineProtocolUtilTest, TestParseProgressEventToJson) {
    Dic::Protocol::ParseProgressEvent response;
    auto jsonOp = Dic::Protocol::ToEventJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
}
