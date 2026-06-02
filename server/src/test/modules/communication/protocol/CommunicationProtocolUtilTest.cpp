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
#include "CommunicationProtocol.h"
#include "CommunicationProtocolResponse.h"

using namespace Dic::Protocol;
const uint64_t NUMBER_HUNDRED = 100;
const uint64_t NUMBER_THOUSAND = 100;
class CommunicationProtocolUtilTest : public ::testing::Test {
  protected:
    void SetUp() override { protocol.Register(); }

    void TearDown() override { protocol.UnRegister(); }

    void CheckResponseBaseStruct(const std::optional<Dic::document_t> &jsonOptional) {
        ASSERT_TRUE(jsonOptional.has_value());
        ASSERT_TRUE(jsonOptional.value().HasMember("body"));
    }

    Dic::Protocol::CommunicationProtocol protocol;
};

TEST_F(CommunicationProtocolUtilTest, ToCommunicationAdvisorRequestNormalTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "communication", "type": "request", "resultCallbackId": 0,
        "command": "communication/advisor", "params": {}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result->id, 1);
    EXPECT_EQ(result->command, "communication/advisor");
    EXPECT_EQ(result->type, ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(result->moduleName, MODULE_COMMUNICATION);
}

TEST_F(CommunicationProtocolUtilTest, ToAdvisorRequestLackIdTestReturnNull) {
    std::string reqJson = R"({"moduleName": "communication", "type": "request", "resultCallbackId": 0,
        "command": "communication/advisor", "params": {}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = protocol.FromJson(json, err);
    EXPECT_EQ(result, nullptr);
    EXPECT_EQ(err, "Failed to set request base info of communication advisor request.");
}

TEST_F(CommunicationProtocolUtilTest, ToOperatorListResponseTest) {
    const std::string KEY_MIN_TIME = "minTime";
    const std::string KEY_MAX_TIME = "maxTime";
    Dic::Protocol::OperatorListsResponse response;
    std::string err;
    OperatorListsResponseBody body;
    response.body.rankLists.emplace_back("1");
    response.body.dbPathList.emplace_back("test");
    response.body.minTime = NUMBER_HUNDRED;
    response.body.maxTime = NUMBER_THOUSAND;
    OperatorTimeItem op = {"op1", 100, 900};
    CompareData<std::vector<OperatorTimeItem>> compareData;
    compareData.compare.push_back(op);
    response.body.opLists.push_back(compareData);
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.value()["body"][KEY_MIN_TIME.c_str()], response.body.minTime);
    EXPECT_EQ(jsonOptional.value()["body"][KEY_MAX_TIME.c_str()], response.body.maxTime);
}

TEST_F(CommunicationProtocolUtilTest, ToCommunicationAdvisorResponseEmptyDataTest) {
    CommunicationAdvisorResponse response;
    std::string err;
    response.body.items.clear();
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
}

TEST_F(CommunicationProtocolUtilTest, ToCommunicationAdvisorResponseNormalDataTest) {
    CommunicationAdvisorResponse response;
    CommunicationAdvisorInfo info{"Packet Analysis",
        {{"Category", {"SDMA", "RDMA"}}, {"Min Size", {"16MB", "1MB"}},
            {"Packet Size <= Min Size Percentage", {"2.1", "0.5"}}}};
    response.body.items.push_back(info);
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    const int two = 2;
    CheckResponseBaseStruct(jsonOptional);
    ASSERT_TRUE(jsonOptional.value()["body"].HasMember("items"));
    ASSERT_TRUE(jsonOptional.value()["body"]["items"].IsArray());
    ASSERT_EQ(jsonOptional.value()["body"]["items"].Size(), 1);
    ASSERT_TRUE(jsonOptional.value()["body"]["items"][0].HasMember("name"));
    EXPECT_EQ(jsonOptional.value()["body"]["items"][0]["name"], "Packet Analysis");
    ASSERT_TRUE(jsonOptional.value()["body"]["items"][0].HasMember("statistics"));
    ASSERT_TRUE(jsonOptional.value()["body"]["items"][0]["statistics"].IsObject());
    ASSERT_TRUE(jsonOptional.value()["body"]["items"][0]["statistics"].HasMember("Category"));
    ASSERT_TRUE(jsonOptional.value()["body"]["items"][0]["statistics"]["Category"].IsArray());
    ASSERT_EQ(jsonOptional.value()["body"]["items"][0]["statistics"]["Category"].Size(), two);
    EXPECT_EQ(jsonOptional.value()["body"]["items"][0]["statistics"]["Category"][0], "SDMA");
    EXPECT_EQ(jsonOptional.value()["body"]["items"][0]["statistics"]["Category"][1], "RDMA");
    ASSERT_TRUE(jsonOptional.value()["body"]["items"][0]["statistics"].HasMember("Min Size"));
    ASSERT_TRUE(jsonOptional.value()["body"]["items"][0]["statistics"]["Min Size"].IsArray());
    ASSERT_EQ(jsonOptional.value()["body"]["items"][0]["statistics"]["Min Size"].Size(), two);
    EXPECT_EQ(jsonOptional.value()["body"]["items"][0]["statistics"]["Min Size"][0], "16MB");
    EXPECT_EQ(jsonOptional.value()["body"]["items"][0]["statistics"]["Min Size"][1], "1MB");
    ASSERT_TRUE(jsonOptional.value()["body"]["items"][0]["statistics"].HasMember("Packet Size <= Min Size Percentage"));
    ASSERT_TRUE(jsonOptional.value()["body"]["items"][0]["statistics"]["Packet Size <= Min Size Percentage"].IsArray());
    ASSERT_EQ(jsonOptional.value()["body"]["items"][0]["statistics"]["Packet Size <= Min Size Percentage"].Size(), two);
    EXPECT_EQ(jsonOptional.value()["body"]["items"][0]["statistics"]["Packet Size <= Min Size Percentage"][0], "2.1");
    EXPECT_EQ(jsonOptional.value()["body"]["items"][0]["statistics"]["Packet Size <= Min Size Percentage"][1], "0.5");
}

TEST_F(CommunicationProtocolUtilTest, ToOperatorDetailsResponseReturnFalseWithEmptyData) {
    OperatorDetailsResponse response;
    response.body.count = 0;
    response.body.pageSize = 0;
    response.body.currentPage = 0;
    response.body.allOperators = {};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body["count"].GetInt(), response.body.count);
    EXPECT_EQ(body["pageSize"].GetInt(), response.body.pageSize);
    EXPECT_EQ(body["currentPage"].GetInt(), response.body.currentPage);
    EXPECT_EQ(body["allOperators"].GetArray().Size(), 0);
}

TEST_F(CommunicationProtocolUtilTest, ToOperatorDetailsResponseReturnTrueWithNormalData) {
    OperatorDetailsResponse response;
    response.body.count = 122;
    response.body.pageSize = 50;
    response.body.currentPage = 12;
    response.body.allOperators = {
        {"AAA", 1, 2, 3, 4, 5, 6, 7, 8, 9, 10},
        {"BBB", 11, 12, 13, 14, 15, 16, 17, 18, 19, 20},
    };
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body["count"].GetInt(), response.body.count);
    EXPECT_EQ(body["pageSize"].GetInt(), response.body.pageSize);
    EXPECT_EQ(body["currentPage"].GetInt(), response.body.currentPage);
    EXPECT_EQ(body["allOperators"].GetArray().Size(), response.body.allOperators.size());
    size_t index = 0;
    for (const auto &item : body["allOperators"].GetArray()) {
        EXPECT_EQ(item["operatorName"].GetString(), response.body.allOperators[index].operatorName);
        EXPECT_EQ(item["startTime"].GetDouble(), response.body.allOperators[index].startTime);
        EXPECT_EQ(item["sdmaBw"].GetDouble(), response.body.allOperators[index++].sdmaBw);
    }
}

TEST_F(CommunicationProtocolUtilTest, ToBandwidthDataResponseReturnFalseWithEmptyData) {
    BandwidthDataResponse response = {};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body.HasMember("items"), true);
    EXPECT_EQ(body["items"].GetArray().Size(), 0);
}

TEST_F(CommunicationProtocolUtilTest, ToBandwidthDataResponseReturnTrueWithNormalData) {
    BandwidthDataResponse response;
    response.body.items = {
        {"AAA", 1, 2, 3, 4},
        {"BBB", 5, 6, 7, 8},
    };
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body.HasMember("items"), true);
    EXPECT_EQ(body["items"].GetArray().Size(), response.body.items.size());
    size_t index = 0;
    for (const auto &item : body["items"].GetArray()) {
        EXPECT_EQ(item["transportType"].GetString(), response.body.items[index].transportType);
        EXPECT_EQ(item["transitTime"].GetDouble(), response.body.items[index].transitTime);
        EXPECT_EQ(item["largePacketRatio"].GetDouble(), response.body.items[index++].largePacketRatio);
    }
}

TEST_F(CommunicationProtocolUtilTest, ToDurationResponseReturnFalseWithEmptyData) {
    DurationResponse response = {};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body.HasMember("items"), true);
    EXPECT_EQ(body["items"].GetArray().Size(), 0);
    EXPECT_EQ(body.HasMember("advice"), true);
    EXPECT_EQ(body["advice"].GetArray().Size(), 0);
}

TEST_F(CommunicationProtocolUtilTest, ToDurationResponseReturnTrueWithNormalData) {
    DurationResponse response = {};
    response.body.durationList = {{"AAA", "/root/data",
        {{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}, {11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22}}}};
    response.body.bwStatistics = {{"AAA", 1, 2, 3, 4, 5}, {"BBB", 31, 32, 33, 34, 35}};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body.HasMember("items"), true);
    EXPECT_EQ(body["items"].GetArray().Size(), response.body.durationList.size());
    EXPECT_EQ(body.HasMember("advice"), true);
    EXPECT_EQ(body["advice"].GetArray().Size(), response.body.bwStatistics.size());
    size_t index = 0;
    for (const auto &item : body["items"].GetArray()) {
        EXPECT_EQ(item["rankId"].GetString(), response.body.durationList[index].rankId);
        EXPECT_EQ(item["dbPath"].GetString(), response.body.durationList[index].dbPath);
        EXPECT_EQ(item["compareData"]["baseline"]["sdmaBw"],
            response.body.durationList[index++].durationData.baseline.sdmaBw);
    }
    index = 0;
    for (const auto &item : body["advice"].GetArray()) {
        EXPECT_EQ(item["type"].GetString(), response.body.bwStatistics[index].type);
        EXPECT_EQ(item["avg"].GetDouble(), response.body.bwStatistics[index].avgBw);
        EXPECT_EQ(item["time"].GetDouble(), response.body.bwStatistics[index++].allTime);
    }
}

TEST_F(CommunicationProtocolUtilTest, ToMatrixGroupResponseReturnFalseWithEmptyData) {
    MatrixGroupResponse response = {};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body.HasMember("data"), true);
    EXPECT_EQ(body["data"].GetArray().Size(), 0);
}

TEST_F(CommunicationProtocolUtilTest, ToMatrixGroupResponseReturnTrueWithNormalData) {
    MatrixGroupResponse response = {};
    response.body.groupList = {{"A1", "A2", "A3"}, {"B1", "B2", "B3"}};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body.HasMember("data"), true);
    EXPECT_EQ(body["data"].GetArray().Size(), response.body.groupList.size());
    size_t index = 0;
    for (const auto &item : body["data"].GetArray()) {
        EXPECT_EQ(item["group"].GetString(), response.body.groupList[index].group);
        EXPECT_EQ(item["parallelStrategy"].GetString(), response.body.groupList[index].parallelStrategy);
        EXPECT_EQ(item["type"].GetString(), response.body.groupList[index++].type);
    }
}

TEST_F(CommunicationProtocolUtilTest, ToMatrixListResponseWithEmptyData) {
    MatrixListResponse response = {};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body.HasMember("matrixList"), true);
    EXPECT_EQ(body["matrixList"].GetArray().Size(), 0);
}

TEST_F(CommunicationProtocolUtilTest, ToMatrixListResponseWithNormalData) {
    MatrixListResponse response;
    response.body.matrixList = {{0, 1,
                                    {
                                        {"A1", "A2", 1, 2, 3},
                                        {"B1", "B2", 4, 5, 6},
                                    }},
        {1, 0,
            {
                {"C1", "C2", 7, 8, 9},
                {"D1", "D2", 10, 11, 12},
            }}};
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body.HasMember("matrixList"), true);
    EXPECT_EQ(body["matrixList"].GetArray().Size(), response.body.matrixList.size());
    size_t index = 0;
    for (const auto &item : body["matrixList"].GetArray()) {
        EXPECT_EQ(item["srcRank"].GetInt(), response.body.matrixList[index].srcRank);
        EXPECT_EQ(item["dstRank"].GetInt(), response.body.matrixList[index].dstRank);
        EXPECT_EQ(item["matrixData"]["baseline"]["transportType"].GetString(),
            response.body.matrixList[index++].matrixData.baseline.transportType);
    }
}

TEST_F(CommunicationProtocolUtilTest, TestCommunicationSlowRankAnalysisResponse) {
    CommunicationSlowRankAnalysisResponse response;
    response.body.hasAdvice = true;
    response.body.fastRankId = "7";
    response.body.fastTotalElapseTime = 300; // 300
    RankDetailsForSlowRank slowRank = {"1", 100, 200, {}}; // 100 200
    OpDetailsForSlowRank op1 = {"op1", 10, 10, 10, 10, 10}; // 10
    OpDetailsForSlowRank op2 = {"op2", 20, 20, 20, 20, 20}; // 20
    slowRank.opDetails.push_back(op1);
    slowRank.opDetails.push_back(op2);
    response.body.slowRankList.push_back(slowRank);
    std::string err;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);

    ASSERT_TRUE(jsonOptional.value().HasMember("body"));
    auto body = jsonOptional.value()["body"].GetObj();
    ASSERT_TRUE(body.HasMember("hasAdvice"));
    EXPECT_TRUE(body["hasAdvice"].GetBool());
    ASSERT_TRUE(body.HasMember("fastRankId"));
    EXPECT_EQ(body["fastRankId"].GetString(), response.body.fastRankId);
    ASSERT_TRUE(body.HasMember("fastTotalElapseTime"));
    EXPECT_EQ(body["fastTotalElapseTime"].GetDouble(), response.body.fastTotalElapseTime);

    ASSERT_TRUE(body.HasMember("data"));
    auto data = body["data"].GetArray();
    ASSERT_EQ(data.Size(), 1);

    auto slowRankDetail = data[0].GetObj();
    ASSERT_TRUE(slowRankDetail.HasMember("rankId"));
    EXPECT_EQ(slowRankDetail["rankId"].GetString(), slowRank.rankId);
    ASSERT_TRUE(slowRankDetail.HasMember("totalDiffTime"));
    EXPECT_EQ(slowRankDetail["totalDiffTime"].GetDouble(), slowRank.totalDiffTime);
    ASSERT_TRUE(slowRankDetail.HasMember("totalElapseTime"));
    EXPECT_EQ(slowRankDetail["totalElapseTime"].GetDouble(), slowRank.totalElapseTime);

    ASSERT_TRUE(slowRankDetail.HasMember("opList"));
    auto opDetails = slowRankDetail["opList"].GetArray();
    ASSERT_EQ(opDetails.Size(), 2); // 2

    auto opDetail1 = opDetails[0].GetObj();
    ASSERT_TRUE(opDetail1.HasMember("name"));
    EXPECT_STREQ(opDetail1["name"].GetString(), "op1");
    ASSERT_TRUE(opDetail1.HasMember("startTime"));
    EXPECT_EQ(opDetail1["startTime"].GetDouble(), op1.startTime);
    ASSERT_TRUE(opDetail1.HasMember("diffTime"));
    EXPECT_EQ(opDetail1["diffTime"].GetDouble(), op1.diffTime);
    ASSERT_TRUE(opDetail1.HasMember("elapseTime"));
    EXPECT_EQ(opDetail1["elapseTime"].GetDouble(), op1.elapseTime);
    ASSERT_TRUE(opDetail1.HasMember("maxTime"));
    EXPECT_EQ(opDetail1["maxTime"].GetDouble(), op1.maxElapseTime);
    ASSERT_TRUE(opDetail1.HasMember("maxStartTime"));
    EXPECT_EQ(opDetail1["maxStartTime"].GetDouble(), op1.maxStartTime);
}
