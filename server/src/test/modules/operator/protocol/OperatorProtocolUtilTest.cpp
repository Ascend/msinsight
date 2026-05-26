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
#include <OperatorProtocol.h>
#include <GlobalDefs.h>
#include <OperatorProtocolRequest.h>
#include "ProtocolDefs.h"
#include "OperatorProtocolUtil.h"
#include "OperatorProtocolResponse.h"

using namespace Dic::Protocol;
const std::vector<std::string> DETAIL_KEY = {"name", "type", "accCore", "startTime", "duration", "waitTime", "blockNum",
    "inputShape", "inputType", "inputFormat", "outputShape", "outputType", "outputFormat"};

class OperatorProtocolUtilTest : public ::testing::Test {
  protected:
    void SetUp() override { operatorProtocol.Register(); }

    void TearDown() override { operatorProtocol.UnRegister(); }

    void CheckResponseBaseStruct(const std::optional<Dic::document_t> &jsonOptional) {
        EXPECT_EQ(jsonOptional.has_value(), true);
        EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
        EXPECT_EQ(jsonOptional.value()["body"].HasMember("data"), true);
        EXPECT_EQ(jsonOptional.value()["body"]["data"].IsArray(), true);
    }

    void CheckOperatorDetailInfoResStruct(const Dic::json_t &item, const OperatorDetailInfoRes &res) {
        CheckOperatorDetailInfoPmuResStruct(item, res);
        EXPECT_EQ(item.MemberCount(), DETAIL_KEY.size());
    }

    void CheckOperatorDetailInfoPmuResStruct(const Dic::json_t &item, const OperatorDetailInfoRes &res) {
        for (const auto &it : DETAIL_KEY) {
            EXPECT_TRUE(item.HasMember(it.c_str()));
        }
        EXPECT_EQ(item["name"].GetString(), res.name);
        EXPECT_EQ(item["type"].GetString(), res.type);
        EXPECT_EQ(item["accCore"].GetString(), res.accCore);
        EXPECT_EQ(item["startTime"].GetString(), res.startTime);
        EXPECT_EQ(item["duration"].GetString(), res.duration);
        EXPECT_EQ(item["waitTime"].GetString(), res.waitTime);
        EXPECT_EQ(item["blockNum"].GetString(), res.blockNum);
        EXPECT_EQ(item["inputShape"].GetString(), res.inputShape);
        EXPECT_EQ(item["inputType"].GetString(), res.inputType);
        EXPECT_EQ(item["inputFormat"].GetString(), res.inputFormat);
        EXPECT_EQ(item["outputShape"].GetString(), res.outputShape);
        EXPECT_EQ(item["outputType"].GetString(), res.outputType);
        EXPECT_EQ(item["outputFormat"].GetString(), res.outputFormat);
    }
    Dic::Protocol::OperatorProtocol operatorProtocol;
};

TEST_F(OperatorProtocolUtilTest, ToOperatorCategoryInfoRequestTest) {
    std::string reqJson = R"({"id": 2, "moduleName": "operator", "type": "request", "command": "operator/category",
        "resultCallbackId": 0, "params": {"rankId": "1", "group": "Operator", "topK": 15}})";
    OperatorDurationReqParams expect = {"1", "1", "Operator", 15};
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = dynamic_cast<OperatorCategoryInfoRequest &>(*(operatorProtocol.FromJson(json, err))).params;
    EXPECT_EQ(result.rankId, expect.rankId);
    EXPECT_EQ(result.group, expect.group);
    EXPECT_EQ(result.topK, expect.topK);
}

TEST_F(OperatorProtocolUtilTest, ToOperatorComputeUnitInfoRequestTest) {
    std::string reqJson = R"({"id": 4, "moduleName": "operator", "type": "request", "command": "operator/compute_unit",
        "resultCallbackId": 0, "params": {"rankId": "2", "group": "Operator Type", "topK": -1}})";
    OperatorDurationReqParams expect = {"2", "2", "Operator Type", -1};
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = dynamic_cast<OperatorComputeUnitInfoRequest &>(*(operatorProtocol.FromJson(json, err))).params;
    EXPECT_EQ(result.rankId, expect.rankId);
    EXPECT_EQ(result.group, expect.group);
    EXPECT_EQ(result.topK, expect.topK);
}

TEST_F(OperatorProtocolUtilTest, ToOperatorStatisticInfoRequestTest) {
    std::string reqJson = R"(
        {"id": 4, "moduleName": "operator", "type": "request", "command": "operator/statistic", "resultCallbackId": 0,
        "params": {"rankId": "2", "group": "Input Shape", "topK": -1, "current": 2, "pageSize": 20,
        "orderBy": "avg_time", "order": "Asc", "filters": [{"columnName": "name", "value": "MatMul"}]}})";
    OperatorStatisticReqParams expect = {
        false, "2", "2", "Input Shape", -1, 2, 20, "avg_time", "Asc", {{"name", "MatMul"}}};
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = dynamic_cast<OperatorStatisticInfoRequest &>(*(operatorProtocol.FromJson(json, err))).params;
    EXPECT_EQ(result.rankId, expect.rankId);
    EXPECT_EQ(result.group, expect.group);
    EXPECT_EQ(result.topK, expect.topK);
    EXPECT_EQ(result.current, expect.current);
    EXPECT_EQ(result.pageSize, expect.pageSize);
    EXPECT_EQ(result.orderBy, expect.orderBy);
    EXPECT_EQ(result.order, expect.order);
}

TEST_F(OperatorProtocolUtilTest, ToOperatorDetailInfoRequestTest) {
    std::string reqJson = R"({"id": 4, "moduleName": "operator", "type": "request", "command": "operator/details",
        "resultCallbackId": 0, "params": {"rankId": "2", "group": "Operator Type", "topK": 100, "current": 3,
        "pageSize": 50, "orderBy": "cnt", "order": "Desc", "filters": [{"columnName": "opType", "value": "MatMul"}]}})";
    OperatorStatisticReqParams expect = {
        false, "2", "2", "Operator Type", 100, 3, 50, "cnt", "Desc", {{"op_type", "MatMul"}}};
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = dynamic_cast<OperatorDetailInfoRequest &>(*(operatorProtocol.FromJson(json, err))).params;
    EXPECT_EQ(result.rankId, expect.rankId);
    EXPECT_EQ(result.group, expect.group);
    EXPECT_EQ(result.topK, expect.topK);
    EXPECT_EQ(result.current, expect.current);
    EXPECT_EQ(result.pageSize, expect.pageSize);
    EXPECT_EQ(result.orderBy, expect.orderBy);
    EXPECT_EQ(result.order, expect.order);
}

TEST_F(OperatorProtocolUtilTest, ToOperatorMoreInfoRequestTest) {
    std::string reqJson = R"({"id": 4, "moduleName": "operator", "type": "request", "command": "operator/more_info",
        "resultCallbackId": 0, "params": {"rankId": "2", "group": "Operator Type", "topK": 100,
        "opType": "MatMul", "opName": "MatMul", "shape": "", "accCore": "AI_CORE",
        "current": 3, "pageSize": 50, "orderBy": "cnt", "order": "Desc", "filters": []}})";
    OperatorMoreInfoReqParams expect = {
        "2", "2", "Operator Type", 100, "MatMul", "MatMul", "", "AI_CORE", 3, 50, "cnt", "Desc", {}};
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = dynamic_cast<OperatorMoreInfoRequest &>(*(operatorProtocol.FromJson(json, err))).params;
    EXPECT_EQ(result.rankId, expect.rankId);
    EXPECT_EQ(result.group, expect.group);
    EXPECT_EQ(result.topK, expect.topK);
    EXPECT_EQ(result.opType, expect.opType);
    EXPECT_EQ(result.opName, expect.opName);
    EXPECT_EQ(result.shape, expect.shape);
    EXPECT_EQ(result.accCore, expect.accCore);
    EXPECT_EQ(result.current, expect.current);
    EXPECT_EQ(result.pageSize, expect.pageSize);
    EXPECT_EQ(result.orderBy, expect.orderBy);
    EXPECT_EQ(result.order, expect.order);
}

// OperatorExportDetailsRequest请求反序列化校验
TEST_F(OperatorProtocolUtilTest, ToOperatorExportDetailsRequestTest) {
    std::string reqJson = R"({"id": 1, "moduleName": "operator", "type": "request", "resultCallbackId": 0,
        "command": "operator/exportDetails", "params": {"isCompare": true, "rankId": "1", "deviceId": "1",
        "group": "group", "topK": 5}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = dynamic_cast<OperatorExportDetailsRequest &>(*(operatorProtocol.FromJson(json, err))).params;
    const int64_t exceptTopK = 5;
    EXPECT_EQ(result.isCompare, true);
    EXPECT_EQ(result.rankId, "1");
    EXPECT_EQ(result.topK, exceptTopK);
    EXPECT_EQ(result.group, "group");
}

TEST_F(OperatorProtocolUtilTest, ToOperatorCategoryInfoResponseTest) {
    Dic::Protocol::OperatorCategoryInfoResponse response;
    std::string err;
    // empty data
    response.data = {};
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    // valid data
    response.data = {{"FlashAttentionScore", 28179.42}, {"DSAStatelessGenBitMask", 21827.46}};
    jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        EXPECT_EQ(item.HasMember("name"), true);
        EXPECT_EQ(item["name"].GetString(), response.data[i].name);
        EXPECT_EQ(item.HasMember("duration"), true);
        EXPECT_EQ(item["duration"].GetDouble(), response.data[i++].duration);
        EXPECT_EQ(item.MemberCount(), 2); // 2, name and duration
    }
}

TEST_F(OperatorProtocolUtilTest, ToOperatorComputeUnitInfoResponseTest) {
    Dic::Protocol::OperatorComputeUnitInfoResponse response;
    std::string err;
    // empty data
    response.data = {};
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    // valid data
    response.data = {{"AI_CORE", 28179.42}, {"AI_CPU", 21827.46}, {"VECTOR_CORE", 2000.46}};
    jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        EXPECT_EQ(item.HasMember("name"), true);
        EXPECT_EQ(item["name"].GetString(), response.data[i].name);
        EXPECT_EQ(item.HasMember("duration"), true);
        EXPECT_EQ(item["duration"].GetDouble(), response.data[i++].duration);
        EXPECT_EQ(item.MemberCount(), 2); // 2, name and duration
    }
}

// OperatorExportDetails响应序列化
TEST_F(OperatorProtocolUtilTest, ToOperatorExportDetailsResponseTest) {
    Dic::Protocol::OperatorExportDetailsResponse response;
    response.exceedingFileLimit = true;
    response.filePath = "filePath";
    std::string err;
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    auto body = jsonOptional.value()["body"].GetObj();
    EXPECT_EQ(body.HasMember("exceedingFileLimit"), true);
    EXPECT_EQ(body.HasMember("filePath"), true);
    EXPECT_EQ(body["exceedingFileLimit"].GetBool(), true);
    EXPECT_EQ(body["filePath"].GetString(), response.filePath);
}

TEST_F(OperatorProtocolUtilTest, ToOperatorStatisticInfoResponseTest) {
    Dic::Protocol::OperatorStatisticInfoResponse response;
    std::string err;
    // empty data
    response.total = 0;
    response.data = {};
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("total"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["total"], response.total);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
    // valid data
    response.data = {{{"FlashAttentionScore", "FlashAttentionScore", "1;;1;2", "MIX_AIC", "28179.4", "4", "7044.8",
                          "7111.4", "6982.4"},
        {"FlashAttentionScore", "FlashAttentionScore", "1;;1;2", "MIX_AIC", "28179.4", "4", "7044.8", "7111.4",
            "6982.4"},
        {"FlashAttentionScore", "FlashAttentionScore", "1;;1;2", "MIX_AIC", "28179.4", "4", "7044.8", "7111.4",
            "6982.4"}}};
    response.total = response.data.size();
    jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"]["total"], response.total);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        EXPECT_EQ(item.HasMember("diff"), true);
        EXPECT_EQ(item["diff"].HasMember("opType"), true);
        EXPECT_EQ(item["diff"]["opType"].GetString(), response.data[i].compare.opType);
        EXPECT_EQ(item["diff"].HasMember("accCore"), true);
        EXPECT_EQ(item["diff"]["accCore"].GetString(), response.data[i].compare.accCore);
        EXPECT_EQ(item["diff"].HasMember("totalTime"), true);
        EXPECT_EQ(item["diff"]["totalTime"].GetString(), response.data[i].compare.totalTime);
        EXPECT_EQ(item["diff"].HasMember("maxTime"), true);
        EXPECT_EQ(item["diff"]["maxTime"].GetString(), response.data[i++].compare.maxTime);
        EXPECT_EQ(item.MemberCount(), 3); // 3, diff compare and baseline
        EXPECT_EQ(item["diff"].MemberCount(), 9); // 9, name and duration
    }
}

TEST_F(OperatorProtocolUtilTest, ToOperatorMoreInfoResponseTest) {
    Dic::Protocol::OperatorMoreInfoResponse response;
    std::string err;
    // empty data
    response.total = 0;
    response.level = "";
    response.data = {};
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("total"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["total"], response.total);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("level"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["level"].GetString(), response.level);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
    // valid data
    response.data = {{"0", "1", "FlashAttentionScore", "FlashAttentionScore", "MIX_AIC", "131.2", "7111.4", "1.8", "24",
                         "8192,2,640;8192,2,640", "DT_BF16;DT_BF16", "FORMAT_ND;FORMAT_ND", "655360;655360",
                         "FLOAT;FLOAT", "FORMAT_ND;FORMAT_ND"},
        {"1", "2", "MatMul", "MatMul", "AI_CORE", "108.09", "1505.92", "0.05", "24", "16384,2560;5120,2560",
            "DT_BF16;DT_BF16", "FORMAT_ND;FORMAT_ND", "16384,5120", "DT_BF16", "FORMAT_ND"}};
    response.total = response.data.size();
    response.level = "l2";
    jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"]["total"], response.total);
    EXPECT_EQ(jsonOptional.value()["body"]["level"].GetString(), response.level);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        CheckOperatorDetailInfoResStruct(item, response.data[i++]);
    }
}

TEST_F(OperatorProtocolUtilTest, ToOperatorDetailInfoResponseTest) {
    Dic::Protocol::OperatorDetailInfoResponse response;
    std::string err;
    // empty data
    response.total = 0;
    response.level = "";
    response.data = {};
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("total"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["total"], response.total);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("level"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["level"].GetString(), response.level);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
    // valid data
    response.data = {{{"0", "1", "ApplyAdamW", "NA", "NA", "131.2", "7111.4", "1.8", "24", "", "", "", "", "", ""},
                         {"0", "1", "ApplyAdamW", "NA", "NA", "131.2", "7111.4", "1.8", "24", "", "", "", "", "", ""},
                         {"0", "1", "ApplyAdamW", "NA", "NA", "131.2", "7111.4", "1.8", "24", "", "", "", "", "", ""}},
        {{"1", "2", "MatMul", "NA", "NA", "108.09", "1505.92", "0.05", "24", "", "", "", "", "", ""},
            {"1", "2", "MatMul", "NA", "NA", "108.09", "1505.92", "0.05", "24", "", "", "", "", "", ""},
            {"1", "2", "MatMul", "NA", "NA", "108.09", "1505.92", "0.05", "24", "", "", "", "", "", ""}}};
    response.total = response.data.size();
    response.level = "l0";
    jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"]["total"], response.total);
    EXPECT_EQ(jsonOptional.value()["body"]["level"].GetString(), response.level);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        CheckOperatorDetailInfoResStruct(item["diff"], response.data[i++].diff);
    }
}

TEST_F(OperatorProtocolUtilTest, ToOperatorDetailInfoRequestWhenPmuDataExist) {
    Dic::Protocol::OperatorDetailInfoResponse response;
    std::string err;
    // pmu数据新增
    std::map<std::string, std::string> data = {
        {"aic_total_time", "1"}, {"aic_totle_cycle", "2"}, {"aiv_bw", "3"}, {"aic_bw", "4"}, {"aic_rate", "5"}};
    response.pmuHeaders = {"aic_total_time", "aic_totle_cycle", "aiv_bw", "aic_bw", "aic_rate"};
    response.data = {
        {{"0", "1", "ApplyAdamW", "NA", "NA", "131.2", "7111.4", "1.8", "24", "", "", "", "", "", "", data},
            {"0", "1", "ApplyAdamW", "NA", "NA", "131.2", "7111.4", "1.8", "24", "", "", "", "", "", "", data},
            {"0", "1", "ApplyAdamW", "NA", "NA", "131.2", "7111.4", "1.8", "24", "", "", "", "", "", "", data}},
        {{"1", "2", "MatMul", "NA", "NA", "108.09", "1505.92", "0.05", "24", "", "", "", "", "", "", data},
            {"1", "2", "MatMul", "NA", "NA", "108.09", "1505.92", "0.05", "24", "", "", "", "", "", "", data},
            {"1", "2", "MatMul", "NA", "NA", "108.09", "1505.92", "0.05", "24", "", "", "", "", "", "", data}}};
    response.total = response.data.size();
    response.level = "l0";
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(response, err);
    jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"]["total"], response.total);
    EXPECT_EQ(jsonOptional.value()["body"]["level"].GetString(), response.level);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
    EXPECT_EQ(jsonOptional.value()["body"]["pmuHeaders"].Size(), response.pmuHeaders.size());
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        CheckOperatorDetailInfoPmuResStruct(item["compare"], response.data[i++].compare);
    }
}

TEST_F(OperatorProtocolUtilTest, ToOperatorDetailInfoRequestWhenPmuDataNotExist) {
    Dic::Protocol::OperatorDetailInfoResponse response;
    std::string err;
    response.pmuHeaders = {};
    response.data = {
        {{"0", "1", "ApplyAdamW", "NA", "NA", "131.2", "7111.4", "1.8", "24", "", "", "", "", "", "", {}},
            {"0", "1", "ApplyAdamW", "NA", "NA", "131.2", "7111.4", "1.8", "24", "", "", "", "", "", "", {}},
            {"0", "1", "ApplyAdamW", "NA", "NA", "131.2", "7111.4", "1.8", "24", "", "", "", "", "", "", {}}},
        {{"1", "2", "MatMul", "NA", "NA", "108.09", "1505.92", "0.05", "24", "", "", "", "", "", "", {}},
            {"1", "2", "MatMul", "NA", "NA", "108.09", "1505.92", "0.05", "24", "", "", "", "", "", "", {}},
            {"1", "2", "MatMul", "NA", "NA", "108.09", "1505.92", "0.05", "24", "", "", "", "", "", "", {}}}};
    response.total = response.data.size();
    response.level = "l0";
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(response, err);
    jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"]["total"], response.total);
    EXPECT_EQ(jsonOptional.value()["body"]["level"].GetString(), response.level);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
    EXPECT_EQ(jsonOptional.value()["body"]["pmuHeaders"].Size(), response.pmuHeaders.size());
    int i = 0;
    for (const auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        CheckOperatorDetailInfoPmuResStruct(item["compare"], response.data[i++].compare);
    }
}

TEST_F(OperatorProtocolUtilTest, ToOperatorDetailInfoRequestWhenPmuDataNotExistAndDataIsEmpty) {
    Dic::Protocol::OperatorDetailInfoResponse response;
    std::string err;
    // empty data
    response.total = 0;
    response.level = "";
    response.data = {};
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(response, err);
    CheckResponseBaseStruct(jsonOptional);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("total"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["total"], response.total);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("level"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["level"].GetString(), response.level);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("pmuHeaders"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["pmuHeaders"].Size(), response.pmuHeaders.size());
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.data.size());
}

TEST_F(OperatorProtocolUtilTest, ToOperatorParseStatusEventTest) {
    Dic::Protocol::OperatorParseStatusEvent event;
    std::string err;
    event.data = {.rankId = "1", .status = true, .error = "error"};
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(event, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("rankId"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["rankId"].GetString(), event.data.rankId);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("status"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["status"].GetBool(), event.data.status);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("error"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["error"].GetString(), event.data.error);
}

TEST_F(OperatorProtocolUtilTest, ToOperatorParseClearEventTest) {
    Dic::Protocol::OperatorParseClearEvent event;
    std::string err;
    std::optional<Dic::document_t> jsonOptional = operatorProtocol.ToJson(event, err);
    EXPECT_EQ(jsonOptional.has_value(), true);
}
