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
#include "IEProtocol.h"
#include "JsonUtil.h"
#include "ProtocolDefs.h"
#include "IEProtocolResquest.h"
#include "IEProtocolResponse.h"
#include "IEProtocolEvent.h"
#include "IEProtocolUtil.h"

class IEProtocolTest : public ::testing::Test {};

TEST_F(IEProtocolTest, ToIEUsageViewParamsRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::IEProtocol ieProtocol;
    ieProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", Dic::Protocol::REQ_RES_IE_VIEW, allocator);
    ieProtocol.FromJson(json, error);
    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(params, "rankId", "kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "type", "nnnnnnnnnn", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "IE", allocator);
    auto requestPtr = ieProtocol.FromJson(json, error);
    auto &request = dynamic_cast<Dic::Protocol::IEUsageViewParamsRequest &>(*requestPtr);
    std::string rankId = request.params.rankId;
    std::string type = request.params.type;
    auto id = request.id;
    EXPECT_EQ(id, tempId);
    EXPECT_EQ(rankId, "kkkkkk");
    EXPECT_EQ(type, "nnnnnnnnnn");
}

TEST_F(IEProtocolTest, ToIETableRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::IEProtocol ieProtocol;
    ieProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", Dic::Protocol::REQ_RES_IE_TABLE_VIEW, allocator);
    ieProtocol.FromJson(json, error);
    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "IE", allocator);
    auto requestPtr = ieProtocol.FromJson(json, error);
    auto &request = dynamic_cast<Dic::Protocol::IETableRequest &>(*requestPtr);
    std::string rankId = request.params.rankId;
    std::string type = request.params.type;
    auto id = request.id;
    std::string errMsg;
    auto res = request.params.CommonCheck(errMsg);
    EXPECT_EQ(res, false);
    EXPECT_EQ(errMsg, "Page size invalid!");
    request.params.pageSize = 50; // 50
    request.params.CommonCheck(errMsg);
    EXPECT_EQ(errMsg, "Current page invalid!");
    request.params.currentPage = 3; // 3
    auto res2 = request.params.CommonCheck(errMsg);
    EXPECT_EQ(res2, true);
    EXPECT_EQ(id, tempId);
}

TEST_F(IEProtocolTest, ToIEGroupRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::IEProtocol ieProtocol;
    ieProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", Dic::Protocol::REQ_RES_IE_DATA_GROUP, allocator);
    ieProtocol.FromJson(json, error);
    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(params, "rankId", "lllllllllll", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "IE", allocator);
    auto requestPtr = ieProtocol.FromJson(json, error);
    auto &request = dynamic_cast<Dic::Protocol::IEGroupRequest &>(*requestPtr);
    std::string rankId = request.params.rankId;
    auto id = request.id;
    EXPECT_EQ(id, tempId);
    EXPECT_EQ(rankId, "lllllllllll");
}

TEST_F(IEProtocolTest, TestIEUsageViewResponseToJsonNormal) {
    Dic::Protocol::IEUsageViewResponse response;
    response.data.legends.emplace_back("ll");
    response.data.title = "mmm";
    response.data.desc = "mmmmmmmmm";
    std::vector<std::string> line;
    line.emplace_back("bbbbbb");
    response.data.lines.emplace_back(line);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"IE/usage/"
        "view\",\"moduleName\":\"unknown\",\"body\":{\"legends\":[\"ll\"],\"lines\":[[\"bbbbbb\"]],\"title\":\"mmm\","
        "\"description\":\"mmmmmmmmm\"}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(IEProtocolTest, TestIETableViewResponseToJsonNormal) {
    Dic::Protocol::IETableViewResponse response;
    std::map<std::string, std::string> data;
    data["jjj"] = "kkkkk";
    response.data.columnData.emplace_back(data);
    response.data.totalNum = 50; // 50
    Dic::Protocol::Column col;
    col.type = "text";
    col.name = "jjj";
    col.key = "jjj";
    response.data.columnAttr.emplace_back(col);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"IE/table/"
        "view\",\"moduleName\":\"unknown\",\"body\":{\"totalNum\":50,\"operatorDetail\":[{\"jjj\":\"kkkkk\"}],"
        "\"columnAttr\":[{\"name\":\"jjj\",\"type\":\"text\",\"key\":\"jjj\"}]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(IEProtocolTest, TestIEGroupResponseToJsonNormal) {
    Dic::Protocol::IEGroupResponse response;
    Dic::Protocol::IEGroupData group;
    group.value = "jjjjjjjjj";
    group.label = "hhhhhhhhhhhhhhhhh";
    response.data.emplace_back(group);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr = "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"IE/"
                                "group\",\"moduleName\":\"unknown\",\"body\":{\"groups\":[{\"label\":"
                                "\"hhhhhhhhhhhhhhhhh\",\"value\":\"jjjjjjjjj\"}]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(IEProtocolTest, TestParseStatisticCompletedEventToJsonNormal) {
    Dic::Protocol::ParseStatisticCompletedEvent event;
    Dic::Protocol::IEGroupData group;
    event.rankIds.emplace_back("mmmmmmmmmm");
    auto jsonOp = Dic::Protocol::ToEventJson(event);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"event\",\"id\":0,\"event\":\"parse/statisticCompleted\",\"moduleName\":\"unknown\","
        "\"body\":{\"rankIds\":[\"mmmmmmmmmm\"],\"dbPath\":\"\"}}";
    EXPECT_EQ(json, jsonStr);
}
