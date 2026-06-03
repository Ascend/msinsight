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
#include <GlobalDefs.h>
#include "ProtocolDefs.h"
#include "SummaryProtocol.h"
#include "SummaryProtocolUtil.h"
#include "SummaryProtocolRequest.h"
#include "SummaryProtocolResponse.h"

#ifdef GetObject
// a former included windows.h might have defined a macro called GetObject, which affects
// GetObject defined here. This ensures the macro does not get applied
#pragma push_macro("GetObject")
#define RAPIDJSON_WINDOWS_GETOBJECT_WORKAROUND_APPLIED
#undef GetObject
#endif

using namespace Dic::Protocol;
using namespace Dic::Module;
class SummaryProtocolUtilTest : public ::testing::Test {
  protected:
    void SetUp() override { protocol.Register(); }

    void TearDown() override { protocol.UnRegister(); }

    Dic::Protocol::SummaryProtocol protocol;
};

TEST_F(SummaryProtocolUtilTest, ToTopNRequestNormalTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/queryTopData", "resultCallbackId": 0,
        "params": {"isCompare": true, "clusterPath": "/data"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->command, "summary/queryTopData");
    EXPECT_EQ(result->type, ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(result->moduleName, MODULE_SUMMARY);
}

TEST_F(SummaryProtocolUtilTest, ToTopNRequestLackIdTestReturnNull) {
    std::string reqJson = R"({"moduleName": "summary", "type": "request",
        "command": "summary/queryTopData", "resultCallbackId": 0, "params": {"isCompare": true}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(err, "Failed to set request base info of topN request.");
}

TEST_F(SummaryProtocolUtilTest, ToStatisticsRequestNormalTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/statistic", "resultCallbackId": 0,
        "params": {"rankId": "0", "stepId": "1", "timeFlag": "all", "clusterPath": "/data"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->command, "summary/statistic");
    EXPECT_EQ(result->type, ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(result->moduleName, MODULE_SUMMARY);
}

TEST_F(SummaryProtocolUtilTest, ToStatisticsRequestLackParamsTestReturnNull) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/statistic", "resultCallbackId": 0, "paramx": {"rankId": "0"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(err, "Failed to set request base info of statistics request.");
}

TEST_F(SummaryProtocolUtilTest, ToComputeDetailRequestNormalTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/queryComputeDetail", "resultCallbackId": 0,
        "params": {"rankId": "0", "dbPath": "/data/db", "currentPage": 1, "timeFlag": "all", "pageSize": 10,
        "orderBy": "time", "order": "desc", "clusterPath": "/data"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->command, "summary/queryComputeDetail");
    EXPECT_EQ(result->type, ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(result->moduleName, MODULE_SUMMARY);
}

TEST_F(SummaryProtocolUtilTest, ToComputeDetailRequestLackModuleNameTestReturnNull) {
    std::string reqJson = R"({"id": 1, "type": "request",
        "command": "summary/queryComputeDetail", "resultCallbackId": 0, "params": {"rankId": "0"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(err, "Failed to set request base info of compute detail request.");
}

TEST_F(SummaryProtocolUtilTest, ToCommunicationRequestNormalTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/queryCommunicationDetail", "resultCallbackId": 0,
        "params": {"rankId": "0", "currentPage": 1, "timeFlag": "all", "pageSize": 10, "orderBy": "time",
        "order": "desc", "clusterPath": "/data"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->command, "summary/queryCommunicationDetail");
    EXPECT_EQ(result->type, ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(result->moduleName, MODULE_SUMMARY);
}

TEST_F(SummaryProtocolUtilTest, ToCommunicationRequestLackIdTestReturnNull) {
    std::string reqJson = R"({"moduleName": "summary", "type": "request",
        "command": "summary/queryCommunicationDetail", "resultCallbackId": 0, "params": {"rankId": "0"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(err, "Failed to set request base info of communication request.");
}

TEST_F(SummaryProtocolUtilTest, ToImportExpertDataRequestNormalTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/importExpertData", "resultCallbackId": 0,
        "params": {"filePath": "/data/expert.json", "version": "1.0", "clusterPath": "/data"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->command, "summary/importExpertData");
    EXPECT_EQ(result->type, ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(result->moduleName, MODULE_SUMMARY);
}

TEST_F(SummaryProtocolUtilTest, ToImportExpertDataRequestLackParamsTestReturnNull) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/importExpertData", "resultCallbackId": 0})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SummaryProtocolUtilTest, ToQueryExpertHotspotRequestNormalTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/queryExpertHotspot", "resultCallbackId": 0,
        "params": {"modelStage": "prefill", "version": "1.0", "layerNum": 60, "expertNum": 8,
        "denseLayerList": [0, 1], "clusterPath": "/data"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->command, "summary/queryExpertHotspot");
    EXPECT_EQ(result->type, ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(result->moduleName, MODULE_SUMMARY);
}

TEST_F(SummaryProtocolUtilTest, ToQueryExpertHotspotRequestLackParamsTestReturnNull) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/queryExpertHotspot", "resultCallbackId": 0})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result, nullptr);
}

TEST_F(SummaryProtocolUtilTest, ToQueryModelInfoRequestNormalTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/queryModelInfo", "resultCallbackId": 0, "params": {"clusterPath": "/data"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->command, "summary/queryModelInfo");
    EXPECT_EQ(result->type, ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(result->moduleName, MODULE_SUMMARY);
}

TEST_F(SummaryProtocolUtilTest, ToQueryModelInfoRequestLackIdTestReturnNull) {
    std::string reqJson = R"({"moduleName": "summary", "type": "request",
        "command": "summary/queryModelInfo", "resultCallbackId": 0, "params": {"clusterPath": "/data"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(err, "Failed to set request base info of query model info request.");
}

TEST_F(SummaryProtocolUtilTest, ToSummarySlowRankAdvisorRequestNormalTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/slowRank/advisor", "resultCallbackId": 0,
        "params": {"algorithm": "test", "tpSize": 2, "ppSize": 3, "dpSize": 4, "epSize": 1,
        "dimension": "ep-dp-pp-cp-tp", "clusterPath": "/data"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->type, ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(result->moduleName, MODULE_SUMMARY);
}

TEST_F(SummaryProtocolUtilTest, ToSummarySlowRankAdvisorRequestLackKeyAlgorithmTestReturnNull) {
    std::string reqJson = R"({"id": 1, "moduleName": "summary", "type": "request",
        "command": "summary/slowRank/advisor", "resultCallbackId": 0, "params": {"tpSize": 2}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(err, "Query parallelism arrangement request didn't have key: algorithm");
}

TEST_F(SummaryProtocolUtilTest, ToQueryParallelStrategyRequestTest) {
    std::string reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "summary/query/parallelStrategy", "resultCallbackId": 0, "params": {}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = dynamic_cast<QueryParallelStrategyRequest &>(*(protocol.FromJson(json, err)));
    EXPECT_EQ(result.command, REQ_RES_SUMMARY_QUERY_PARALLEL_STRATEGY);
}

TEST_F(SummaryProtocolUtilTest, ToSetParallelStrategyRequestTest) {
    std::string reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "summary/set/parallelStrategy", "resultCallbackId": 0, "params": {}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto request = protocol.FromJson(json, err);
    EXPECT_TRUE(request == nullptr);
    reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "summary/set/parallelStrategy", "resultCallbackId": 0, "params":
        {"algorithm": "test", "tpSize": 2, "ppSize": 3, "dpSize": 4}})";
    json.Parse(reqJson.c_str());
    auto result = dynamic_cast<SetParallelStrategyRequest &>(*(protocol.FromJson(json, err)));
    EXPECT_EQ(result.command, REQ_RES_SUMMARY_SET_PARALLEL_STRATEGY);
    EXPECT_EQ(result.params.config.algorithm, "test");
    EXPECT_EQ(result.params.config.tpSize, 2); // tp = 2
    EXPECT_EQ(result.params.config.ppSize, 3); // pp = 3
    EXPECT_EQ(result.params.config.dpSize, 4); // dp = 4
    EXPECT_EQ(result.params.config.cpSize, 1);
    EXPECT_EQ(result.params.config.epSize, 1);
}

TEST_F(SummaryProtocolUtilTest, ToSetParallelStrategyRequestWithCpAndTpTest) {
    Dic::document_t json;
    std::string err;
    std::string reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "summary/set/parallelStrategy", "resultCallbackId": 0, "params":
        {"algorithm": "test", "tpSize": 2, "ppSize": 3, "dpSize": 4, "cpSize": 5, "epSize": 6}})";
    json.Parse(reqJson.c_str());
    auto result = dynamic_cast<SetParallelStrategyRequest &>(*(protocol.FromJson(json, err)));
    const int64_t expectCp = 5;
    const int64_t expectEp = 6;
    EXPECT_EQ(result.command, REQ_RES_SUMMARY_SET_PARALLEL_STRATEGY);
    EXPECT_EQ(result.params.config.algorithm, "test");
    EXPECT_EQ(result.params.config.tpSize, 2); // tp = 2
    EXPECT_EQ(result.params.config.ppSize, 3); // pp = 3
    EXPECT_EQ(result.params.config.dpSize, 4); // dp = 4
    EXPECT_EQ(result.params.config.cpSize, expectCp);
    EXPECT_EQ(result.params.config.epSize, expectEp);
}

TEST_F(SummaryProtocolUtilTest, ToQueryParallelismArrangementRequestWillReturnTrueWhenInputCorrect) {
    Dic::document_t json;
    std::string err;
    std::string reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "parallelism/arrangement/all", "resultCallbackId": 0, "params": {"algorithm": "test",
        "tpSize": 2, "ppSize": 3, "dpSize": 4, "cpSize": 5, "epSize": 6,
        "moeTpSize": 7, "dimension": "ep-dp-pp-cp-tp"}})";
    json.Parse(reqJson.c_str());
    auto result = dynamic_cast<QueryParallelismArrangementRequest &>(*(protocol.FromJson(json, err)));
    EXPECT_EQ(result.command, REQ_RES_PARALLELISM_ARRANGEMENT_ALL);
    EXPECT_EQ(result.params.config.algorithm, "test");
    EXPECT_EQ(result.params.config.tpSize, 2); // tp = 2
    EXPECT_EQ(result.params.config.ppSize, 3); // pp = 3
    EXPECT_EQ(result.params.config.dpSize, 4); // dp = 4
    EXPECT_EQ(result.params.config.cpSize, 5); // cp = 5
    EXPECT_EQ(result.params.config.epSize, 6); // ep = 6
    EXPECT_EQ(result.params.config.moeTpSize, 7); // moeTp = 7
    EXPECT_EQ(result.params.dimension, "ep-dp-pp-cp-tp");
}

TEST_F(SummaryProtocolUtilTest, ToQueryParallelismArrangementRequestWillReturnNullWhenInputWrong) {
    Dic::document_t json;
    std::string err;
    std::string reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "parallelism/arrangement/all", "resultCallbackId": 0, "params": {
        "tpSize": 2, "ppSize": 3, "dpSize": 4, "cpSize": 5, "epSize": 6, "dimension": "ep-dp-pp-cp-tp"}})";
    json.Parse(reqJson.c_str());
    auto result = protocol.FromJson(json, err);
    EXPECT_TRUE(result == nullptr);
    reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "parallelism/arrangement/all", "resultCallbackId": 0, "params": {"algorithm": "test",
        "ppSize": 3, "dpSize": 4, "cpSize": 5, "epSize": 6, "dimension": "ep-dp-pp-cp-tp"}})";
    json.Parse(reqJson.c_str());
    result = protocol.FromJson(json, err);
    EXPECT_TRUE(result == nullptr);
    reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "parallelism/arrangement/all", "resultCallbackId": 0, "params": {"algorithm": "test",
        "tpSize": 2, "dpSize": 4, "cpSize": 5, "epSize": 6, "dimension": "ep-dp-pp-cp-tp"}})";
    json.Parse(reqJson.c_str());
    result = protocol.FromJson(json, err);
    EXPECT_TRUE(result == nullptr);
}

TEST_F(SummaryProtocolUtilTest, ToQueryParallelismPerformanceRequestWillReturnTrueWhenInputCorrect) {
    Dic::document_t json;
    std::string err;
    std::string reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "parallelism/performance/data", "params": {"algorithm": "test", "tpSize": 2, "ppSize": 3,
        "dpSize": 4, "cpSize": 5, "epSize": 6, "moeTpSize": 7,
        "dimension": "ep-dp-pp-cp-tp", "step": "all"}})";
    json.Parse(reqJson.c_str());
    auto result = dynamic_cast<QueryParallelismPerformanceRequest &>(*(protocol.FromJson(json, err)));
    EXPECT_EQ(result.command, REQ_RES_PARALLELISM_PERFORMANCE_DATA);
    EXPECT_EQ(result.params.config.algorithm, "test");
    EXPECT_EQ(result.params.config.tpSize, 2); // tp = 2
    EXPECT_EQ(result.params.config.ppSize, 3); // pp = 3
    EXPECT_EQ(result.params.config.dpSize, 4); // dp = 4
    EXPECT_EQ(result.params.config.cpSize, 5); // cp = 5
    EXPECT_EQ(result.params.config.epSize, 6); // ep = 6
    EXPECT_EQ(result.params.config.moeTpSize, 7); // moeTp = 7
    EXPECT_EQ(result.params.dimension, "ep-dp-pp-cp-tp");
    EXPECT_EQ(result.params.step, "all");
}

TEST_F(SummaryProtocolUtilTest, ToQueryParallelismPerformanceRequestTest) {
    Dic::document_t json;
    std::string err;
    std::string reqJson = "{\"id\":46,\"moduleName\":\"summary\",\"type\":\"request\",\"command\":"
                          "\"parallelism/performance/data\",\"projectName\":\"test\""
                          ",\"params\":{\"step\":\"All\",\"baselineStep\":null,"
                          "\"algorithm\":\"megatron-lm(tp-cp-ep-dp-pp)\","
                          "\"dimension\":\"ep-dp\",\"ppSize\":2,\"tpSize\":2,"
                          "\"cpSize\":2,\"dpSize\":2,\"epSize\":2,\"moeTpSize\":1,"
                          "\"clusterPath\":\"test\",\"isCompare\":false}}";
    json.Parse(reqJson.c_str());
    auto result = dynamic_cast<QueryParallelismPerformanceRequest &>(*(protocol.FromJson(json, err)));
    EXPECT_EQ(result.command, REQ_RES_PARALLELISM_PERFORMANCE_DATA);
    EXPECT_EQ(result.params.config.algorithm, "megatron-lm(tp-cp-ep-dp-pp)");
    EXPECT_EQ(result.params.config.tpSize, 2); // tp = 2
    EXPECT_EQ(result.params.config.ppSize, 2); // pp = 2
    EXPECT_EQ(result.params.config.dpSize, 2); // dp = 2
    EXPECT_EQ(result.params.config.cpSize, 2); // cp = 2
    EXPECT_EQ(result.params.config.epSize, 2); // ep = 2
    EXPECT_EQ(result.params.config.moeTpSize, 1); // moeTp = 1
    EXPECT_EQ(result.params.dimension, "ep-dp");
    EXPECT_EQ(result.params.step, "All");
    EXPECT_EQ(result.params.clusterPath, "test");
}

TEST_F(SummaryProtocolUtilTest, ToQueryParallelismPerformanceRequestWillReturnNullWhenInputWithWrong) {
    Dic::document_t json;
    std::string err;
    std::string reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "parallelism/performance/data", "params": {"tpSize": 2, "ppSize": 3,
        "dpSize": 4, "cpSize": 5, "epSize": 6, "dimension": "ep-dp-pp-cp-tp", "step": "all"}})";
    json.Parse(reqJson.c_str());
    auto result = protocol.FromJson(json, err);
    EXPECT_TRUE(result == nullptr);
    reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "parallelism/performance/data", "params": {"algorithm": "test", "ppSize": 3, "cpSize": 4,
        "dpSize": 5, "epSize": 6, "dimension": "ep-dp-pp-cp-tp",
        "step": "all"}})";
    json.Parse(reqJson.c_str());
    result = protocol.FromJson(json, err);
    EXPECT_TRUE(result == nullptr);
    reqJson = R"({"id": 2, "moduleName": "summary", "type": "request", "command": "parallelism/performance/data",
        "params": {"algorithm": "test", "tpSize": 3, "cpSize": 4, "dpSize": 5, "epSize": 6,
        "dimension": "ep-dp-pp-cp-tp", "step": "all"}})";
    json.Parse(reqJson.c_str());
    result = protocol.FromJson(json, err);
    EXPECT_TRUE(result == nullptr);
}

TEST_F(SummaryProtocolUtilTest, ToQueryParallelStrategyResponseTest) {
    Dic::Protocol::QueryParallelStrategyResponse response;
    std::string err;
    response.config.algorithm = "megatron-lm";
    const int64_t expectCp = 7;
    const int64_t expectEp = 9;
    const int64_t expectMoeTp = 9;
    response.config.tpSize = 8; // tp = 8
    response.config.ppSize = 4; // pp = 4
    response.config.dpSize = 2; // dp = 2
    response.config.cpSize = expectCp;
    response.config.epSize = expectEp;
    response.config.moeTpSize = expectMoeTp;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.value()["body"][KEY_ALGORITHM.c_str()], response.config.algorithm.c_str());
    EXPECT_EQ(jsonOptional.value()["body"][KEY_TP_SIZE.c_str()], response.config.tpSize);
    EXPECT_EQ(jsonOptional.value()["body"][KEY_PP_SIZE.c_str()], response.config.ppSize);
    EXPECT_EQ(jsonOptional.value()["body"][KEY_DP_SIZE.c_str()], response.config.dpSize);
    EXPECT_EQ(jsonOptional.value()["body"][KEY_CP_SIZE.c_str()], response.config.cpSize);
    EXPECT_EQ(jsonOptional.value()["body"][KEY_EP_SIZE.c_str()], response.config.epSize);
    EXPECT_EQ(jsonOptional.value()["body"][KEY_MOE_TP_SIZE.c_str()], response.config.moeTpSize);
}

TEST_F(SummaryProtocolUtilTest, ToSetParallelStrategyResponseTest) {
    Dic::Protocol::SetParallelStrategyResponse response;
    std::string err;
    response.result = false;
    response.msg = "test";
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.value()["body"][KEY_RESULT.c_str()], response.result);
    EXPECT_EQ(jsonOptional.value()["body"][KEY_MSG.c_str()], response.msg.c_str());
}

TEST_F(SummaryProtocolUtilTest, ToQueryFwdBwdTimelineRequestWillReturnNullWhenInputWrong) {
    std::string reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "parallelism/pipeline/fwdBwdTimeline", "resultCallbackId": 0, "params": {}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto request = protocol.FromJson(json, err);
    EXPECT_TRUE(request == nullptr);
    reqJson = R"({"id": 2, "moduleName": "summary", "type": "request",
        "command": "parallelism/pipeline/fwdBwdTimeline", "resultCallbackId": 0, "params": {"step": "2"}})";
    json.Parse(reqJson.c_str());
    request = protocol.FromJson(json, err);
    EXPECT_TRUE(request == nullptr);
}

TEST_F(SummaryProtocolUtilTest, ToQueryFwdBwdTimelineRequestWillReturnTrueWhenInputCorrect) {
    std::string reqJson = R"({"id": 2, "moduleName": "summary", "type": "request", "resultCallbackId": 0,
        "command": "parallelism/pipeline/fwdBwdTimeline", "params": {"stepId": "2", "stageId": "3"}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = dynamic_cast<PipelineFwdBwdTimelineRequest &>(*(protocol.FromJson(json, err)));
    EXPECT_EQ(result.command, REQ_RES_PIPELINE_FWD_BWD_TIMELINE);
    EXPECT_EQ(result.params.stepId, "2");
    EXPECT_EQ(result.params.stageId, "3");
}

TEST_F(SummaryProtocolUtilTest, ToQueryFwdBwdTimelineResponseTestWillReturnWhenEmptyInput) {
    Dic::Protocol::PipelineFwdBwdTimelineResponse response{};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("minTime"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("minTime"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("rankList"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["rankList"].GetArray().Size(), 0);
}

TEST_F(SummaryProtocolUtilTest, ToQueryFwdBwdTimelineResponseTestWillReturnWhenNormalInput) {
    Dic::Protocol::PipelineFwdBwdTimelineResponse response{};
    response.body.maxTime = 10086; // 10086
    response.body.minTime = 10010; // 10010
    response.body.rankLists = {"0", "1"};
    PipelineFwdBwdTimelineByComponent rank0FwdBwd = {"FWD/BWD",
        {{"FP", 3000, 123456, 135678, 0, "FWD/BWD", "0", "1", "FWD"},
            {"BP", 5000, 147890, 234567, 0, "FWD/BWD", "0", "1", "BWD"}}};
    PipelineFwdBwdTimelineByComponent rank0P2P = {
        "P2P", {{"hcom_send", 3000, 136789, 137890, 0, "P2P", "0", "1", "SEND"}}};
    PipelineFwdBwdTimelineByRank rank0 = {"0", {"FWD/BWD", "P2P"}, {rank0FwdBwd, rank0P2P}};
    PipelineFwdBwdTimelineByComponent rank1FwdBwd = {
        "FWD/BWD", {{"FP", 3000, 123456, 135678, 0, "FWD/BWD", "0", "1", "FWD"}}};
    PipelineFwdBwdTimelineByRank rank1 = {"1", {"FWD/BWD"}, {rank1FwdBwd}};

    response.body.rankDataList = {rank0, rank1};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("minTime"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["minTime"], response.body.minTime);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("rankList"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["rankList"].GetArray().Size(), response.body.rankDataList.size());
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["rankList"].GetArray()) {
        auto tmp = response.body.rankDataList.at(i);
        EXPECT_EQ(item.HasMember("rank"), true);
        EXPECT_EQ(item["rank"].GetString(), tmp.rankId);
        EXPECT_EQ(item.HasMember("componentList"), true);
        EXPECT_EQ(item["componentList"].GetArray().Size(), tmp.componentDataList.size());
        int j = 0;
        for (const auto &componentItem : item["componentList"].GetArray()) {
            auto componentTmp = tmp.componentDataList.at(j);
            EXPECT_EQ(componentItem.HasMember("component"), true);
            EXPECT_EQ(componentItem["component"].GetString(), componentTmp.component);
            EXPECT_EQ(componentItem.HasMember("traceList"), true);
            EXPECT_EQ(componentItem["traceList"].GetArray().Size(), componentTmp.traceList.size());
            j++;
        }
        i++;
    }
}

TEST_F(SummaryProtocolUtilTest, ToQueryParallelismArrangementResponseTestWillReturnWhenNormalInput) {
    Dic::Protocol::ParallelismArrangementResponse response{};
    IndicatorAttr attr = {.number = 0,
        .key = "computingTime",
        .name = "computing time",
        .renderHeatMap = true,
        .renderChart = false,
        .visible = true,
        .chart = "bar",
        .stack = "time",
        .yAxisType = "time"};
    response.arrangeData.indicators.push_back(attr);
    Position pos = {0, 0};
    Element ele;
    ele.index = 0;
    ele.name = "ep0-dp0-cp0-pp0-tp0";
    ele.position = pos;
    ele.indexAttributes["tpIndex"] = 0;
    response.arrangeData.arrangements.push_back(ele);
    response.arrangeData.size = response.arrangeData.arrangements.size();
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("arrangements"), true);
    ASSERT_EQ(jsonOptional.value()["body"]["arrangements"].GetArray().Size(), response.arrangeData.size);
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["arrangements"].GetArray()) {
        auto tmp = response.arrangeData.arrangements.at(i);
        EXPECT_EQ(item["index"].GetUint(), tmp.index);
        EXPECT_EQ(item["name"].GetString(), tmp.name);
        EXPECT_EQ(item.HasMember("position"), true);
        EXPECT_EQ(item.HasMember("attribute"), true);
        i++;
    }
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("indicators"), true);
    ASSERT_EQ(jsonOptional.value()["body"]["indicators"].GetArray().Size(), response.arrangeData.indicators.size());
    i = 0;
    for (const auto &item : jsonOptional.value()["body"]["indicators"].GetArray()) {
        auto tmp = response.arrangeData.indicators.at(i);
        EXPECT_EQ(item["key"].GetString(), tmp.key);
        EXPECT_EQ(item["name"].GetString(), tmp.name);
        i++;
    }
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("connections"), true);
}

TEST_F(SummaryProtocolUtilTest, ToQueryParallelismPerformanceResponseTestWillReturnWhenNormalInput) {
    Dic::Protocol::ParallelismPerformanceResponse response{};
    IndicatorDataStructVo indicator;
    indicator.index = 0;
    indicator.indicators.compare["computingTime"] = 100; // 100
    response.indicatorData.performanceData.push_back(indicator);
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("performance"), true);
    ASSERT_EQ(
        jsonOptional.value()["body"]["performance"].GetArray().Size(), response.indicatorData.performanceData.size());
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["performance"].GetArray()) {
        auto tmp = response.indicatorData.performanceData.at(i);
        EXPECT_EQ(item["index"].GetUint(), tmp.index);
        EXPECT_EQ(item["indicators"]["compare"]["computingTime"].GetDouble(), tmp.indicators.compare["computingTime"]);
        i++;
    }
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("advice"), true);
}

TEST_F(SummaryProtocolUtilTest, ToSummarySlowRankAdvisorResponseTestWillReturnWhenNormalInput) {
    Dic::Protocol::SummarySlowRankAdvisorResponse response{};
    Module::AdviceInfoForSlowRank adviceInfo;
    adviceInfo.name = "dp4-pp0-cp1-tp2";
    adviceInfo.index = 9; // 9
    adviceInfo.synchronizeTime["tp"] = 1.23; // 1.23
    adviceInfo.synchronizeTime["cp"] = 4.56; // 4.56
    adviceInfo.synchronizeTime["dp"] = 7.89; // 7.89
    response.body.topNElements.push_back(adviceInfo);
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("topNElements"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["topNElements"].GetArray().Size(), response.body.topNElements.size());
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["topNElements"].GetArray()) {
        auto tmp = response.body.topNElements.at(i);
        EXPECT_EQ(item["index"].GetUint(), tmp.index);
        EXPECT_EQ(item["name"].GetString(), tmp.name);
        EXPECT_EQ(item.HasMember("tpSynchronizeTime"), true);
        EXPECT_EQ(item["tpSynchronizeTime"], tmp.synchronizeTime["tp"]);
        EXPECT_EQ(item.HasMember("cpSynchronizeTime"), true);
        EXPECT_EQ(item["cpSynchronizeTime"], tmp.synchronizeTime["cp"]);
        EXPECT_EQ(item.HasMember("dpSynchronizeTime"), true);
        EXPECT_EQ(item["dpSynchronizeTime"], tmp.synchronizeTime["dp"]);
    }
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("hasSlowRank"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("matchSuccess"), true);
}

// 基础信息数据序列化校验
TEST_F(SummaryProtocolUtilTest, SummaryTopRankResponseSuccess) {
    SummaryTopRankResponse response;
    SummaryBaseInfo compare{5, {"1", "2"}, 100, 100, "filePath", 10, 5, {"1"}};
    SummaryBaseInfo baseline{5, {"1", "2"}, 100, 100, "filePath", 10, 5, {"1"}};
    response.body.baseInfo.compare = compare;
    response.body.baseInfo.baseline = baseline;
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("baseInfo"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["baseInfo"].HasMember("baseline"), true);
    auto baselineJson = jsonOptional.value()["body"]["baseInfo"]["baseline"].GetObject();
    EXPECT_EQ(baselineJson["rankCount"].GetUint(), baseline.rankCount);
    EXPECT_EQ(baselineJson["rankList"].GetArray().Size(), baseline.rankList.size());
    EXPECT_EQ(baselineJson["dataSize"].GetDouble(), baseline.dataSize);
    EXPECT_EQ(baselineJson["collectStartTime"].GetInt(), baseline.collectStartTime);
    EXPECT_EQ(baselineJson["filePath"].GetString(), baseline.filePath);
    EXPECT_EQ(baselineJson["collectDuration"].GetDouble(), baseline.collectDuration);
    EXPECT_EQ(baselineJson["stepNum"].GetUint(), baseline.stepNum);
    EXPECT_EQ(baselineJson["stepList"].GetArray().Size(), baseline.stepList.size());
    EXPECT_EQ(jsonOptional.value()["body"]["baseInfo"].HasMember("compare"), true);
    auto compareJson = jsonOptional.value()["body"]["baseInfo"]["compare"].GetObject();
    EXPECT_EQ(compareJson["rankCount"].GetUint(), compare.rankCount);
    EXPECT_EQ(compareJson["rankList"].GetArray().Size(), compare.rankList.size());
    EXPECT_EQ(compareJson["dataSize"].GetDouble(), compare.dataSize);
    EXPECT_EQ(compareJson["collectStartTime"].GetInt(), compare.collectStartTime);
    EXPECT_EQ(compareJson["filePath"].GetString(), compare.filePath);
    EXPECT_EQ(compareJson["collectDuration"].GetDouble(), compare.collectDuration);
    EXPECT_EQ(compareJson["stepNum"].GetUint(), compare.stepNum);
    EXPECT_EQ(compareJson["stepList"].GetArray().Size(), compare.stepList.size());
}

// 查询模型信息数据序列化校验
TEST_F(SummaryProtocolUtilTest, QueryModelInfoResponseSuccess) {
    QueryModelInfoResponse response;
    response.body = {60, {0, 1}, 200};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObject();
    EXPECT_EQ(body["layerNum"].GetInt(), response.body.layerNum);
    EXPECT_EQ(body["expertNum"].GetInt(), response.body.expertNum);
    EXPECT_EQ(body["denseLayerList"].GetArray().Size(), response.body.denseLayerList.size());
}

TEST_F(SummaryProtocolUtilTest, ToQueryExpertHotspotResponseJsonWhenNormalInput) {
    QueryExpertHotspotResponse response{};
    ExpertHotspotStruct info{"prefill", 0, 100, 0, 0, "1", 0};
    response.body.hotspotInfos.push_back(info);
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("hotspotInfos"), true);
    ASSERT_EQ(jsonOptional.value()["body"]["hotspotInfos"].GetArray().Size(), response.body.hotspotInfos.size());
}

TEST_F(SummaryProtocolUtilTest, ToImportExpertDataResponseJsonWhenNormalInput) {
    ImportExpertDataResponse response{};
    response.result = true;
    response.msg = "";
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("result"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["result"].GetBool(), true);
}

TEST_F(SummaryProtocolUtilTest, ToTestQueryParallelStrategyResponse) {
    using namespace Dic::Module;
    Dic::Protocol::QueryParallelStrategyResponse response{};
    const int expectId = 1;
    response.config = {
        .algorithm = MEGATRON_LM_TP_CP_EP_DP_PP_ALG, .ppSize = 0, .tpSize = 2, .dpSize = 1, .cpSize = 1, .epSize = 1};
    EXPECT_EQ(response.IsValid(), false);
    response.SetDefault();
    EXPECT_EQ(response.config.algorithm, MEGATRON_LM_TP_CP_EP_DP_PP_ALG);
    EXPECT_EQ(response.config.ppSize, expectId);
    const int expectTp = 2;
    EXPECT_EQ(response.config.tpSize, expectTp);
}

TEST_F(SummaryProtocolUtilTest, ToTestQueryParallelStrategyResponse2) {
    using namespace Dic::Module;
    Dic::Protocol::QueryParallelStrategyResponse response{};
    const int expectId = 1;
    response.config = {
        .algorithm = MEGATRON_LM_TP_CP_PP_EP_DP_ALG, .ppSize = 0, .tpSize = 2, .dpSize = 1, .cpSize = 1, .epSize = 1};
    EXPECT_EQ(response.IsValid(), false);
    response.SetDefault();
    EXPECT_EQ(response.config.algorithm, MEGATRON_LM_TP_CP_PP_EP_DP_ALG);
    EXPECT_EQ(response.config.ppSize, expectId);
    const int expectTp = 2;
    EXPECT_EQ(response.config.tpSize, expectTp);
}

TEST_F(SummaryProtocolUtilTest, ToTestQueryParallelStrategyResponseWhenTpPpDp) {
    using namespace Dic::Module;
    Dic::Protocol::QueryParallelStrategyResponse response{};
    const int expectId = 1;
    response.config = {
        .algorithm = MEGATRON_LM_TP_CP_PP_EP_DP_ALG, .ppSize = 0, .tpSize = 2, .dpSize = 1, .cpSize = 1, .epSize = 1};
    EXPECT_EQ(response.IsValid(), false);
    response.SetDefault();
    EXPECT_EQ(response.config.algorithm, MEGATRON_LM_TP_CP_PP_EP_DP_ALG);
    EXPECT_EQ(response.config.ppSize, expectId);
    const int expectTp = 2;
    EXPECT_EQ(response.config.tpSize, expectTp);
}

TEST_F(SummaryProtocolUtilTest, ToTestQueryParallelStrategyResponseWhenTpDpPp) {
    using namespace Dic::Module;
    Dic::Protocol::QueryParallelStrategyResponse response{};
    const int expectId = 1;
    response.config = {
        .algorithm = MEGATRON_LM_TP_CP_EP_DP_PP_ALG, .ppSize = 0, .tpSize = 2, .dpSize = 1, .cpSize = 1, .epSize = 1};
    EXPECT_EQ(response.IsValid(), false);
    response.SetDefault();
    EXPECT_EQ(response.config.algorithm, MEGATRON_LM_TP_CP_EP_DP_PP_ALG);
    EXPECT_EQ(response.config.ppSize, expectId);
    const int expectTp = 2;
    EXPECT_EQ(response.config.tpSize, expectTp);
}

TEST_F(SummaryProtocolUtilTest, ToTestQueryParallelStrategyResponseWhenInvalid) {
    using namespace Dic::Module;
    Dic::Protocol::QueryParallelStrategyResponse response{};
    const int expectId = 1;
    response.config = {.algorithm = "LLLLLLLLLLLL", .ppSize = 0, .tpSize = 2, .dpSize = 1, .cpSize = 1, .epSize = 1};
    EXPECT_EQ(response.IsValid(), false);
    response.SetDefault();
    EXPECT_EQ(response.config.algorithm, MEGATRON_LM_TP_CP_EP_DP_PP_ALG);
    EXPECT_EQ(response.config.ppSize, expectId);
    const int expectTp = 2;
    EXPECT_EQ(response.config.tpSize, expectTp);
}
