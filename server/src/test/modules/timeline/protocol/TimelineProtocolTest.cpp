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
#include "ProtocolTest.cpp"
#include "TimelineProtocol.h"
#include "JsonUtil.h"
#include "ProtocolDefs.h"
#include "TimelineProtocolResponse.h"
#include "TimelineProtocolRequest.h"
#include "TimelineProtocolEvent.h"

class TimelineProtocolTest : ProtocolTest {};

TEST_F(ProtocolTest, ToImportActionRequestTest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "import/action", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::json_t path(Dic::kArrayType);
    path.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "path", path, allocator);
    Dic::JsonUtil::AddMember(params, "projectAction", 0, allocator);
    Dic::JsonUtil::AddMember(params, "isConflict", false, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToUnitThreadTracesRequestTest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "unit/threadTraces", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToUnitThreadTracesSummaryRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "unit/threadTracesSummary", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToUnitThreadsRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "unit/threads", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToThreadDetailRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "unit/threadDetail", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(ProtocolTest, ToResetWindowRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "remote/reset", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToSearchCountRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "search/count", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToSearchSliceRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "search/slice", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToRemoteDeleteRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "remote/delete", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::json_t rankId(Dic::kArrayType);
    rankId.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "777", rankId, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToFlowCategoryListRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "flow/categoryList", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToFlowCategoryEventsRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "flow/categoryEvents", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToUnitCounterRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "unit/counter", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToSystemViewRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "unit/systemView", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToKernelDetailRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "unit/kernelDetails", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToOneKernelRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "unit/one/kernelDetail", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}
TEST_F(ProtocolTest, ToUnitThreadsOperatorsRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "query/all/same/operators/duration", allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(ProtocolTest, ToTableDataNameListRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", Dic::Protocol::REQ_RES_TABLE_DATA_NAME_LIST, allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = timelineProtocol.FromJson(json, error).get()->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(ProtocolTest, ToTableDataDetailRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", Dic::Protocol::REQ_RES_TABLE_DATA_DETAIL, allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    auto requestPtr = timelineProtocol.FromJson(json, error);
    auto &request = dynamic_cast<Dic::Protocol::TableDataDetailRequest &>(*requestPtr);
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

TEST_F(ProtocolTest, ToCreateCurveRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", Dic::Protocol::REQ_RES_CREATE_CURVE, allocator);
    timelineProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    auto requestPtr = timelineProtocol.FromJson(json, error);
    auto &request = dynamic_cast<Dic::Protocol::CreateCurveRequest &>(*requestPtr);
    EXPECT_EQ(request.id, tempId);
}

TEST_F(ProtocolTest, ResponseToJson) {
    EXPECT_NO_THROW({
        Dic::Protocol::TimelineProtocol timelineProtocol;
        timelineProtocol.Register();
        std::string error;
        Dic::Protocol::ImportActionResponse response1;
        timelineProtocol.ToJson(response1, error);
        Dic::Protocol::UnitThreadTracesResponse response2;
        timelineProtocol.ToJson(response2, error);
        Dic::Protocol::UnitThreadTracesSummaryResponse response3;
        timelineProtocol.ToJson(response3, error);
        Dic::Protocol::UnitThreadsResponse response4;
        timelineProtocol.ToJson(response4, error);
        Dic::Protocol::UnitThreadDetailResponse response5;
        timelineProtocol.ToJson(response5, error);
        Dic::Protocol::ResetWindowResponse response8;
        timelineProtocol.ToJson(response8, error);
        Dic::Protocol::SearchCountResponse response9;
        timelineProtocol.ToJson(response9, error);
        Dic::Protocol::SearchSliceResponse response10;
        timelineProtocol.ToJson(response10, error);
        Dic::Protocol::RemoteDeleteResponse response11;
        timelineProtocol.ToJson(response11, error);
        Dic::Protocol::FlowCategoryListResponse response12;
        timelineProtocol.ToJson(response12, error);
        Dic::Protocol::FlowCategoryEventsResponse response13;
        timelineProtocol.ToJson(response13, error);
        Dic::Protocol::UnitCounterResponse response14;
        timelineProtocol.ToJson(response14, error);
        Dic::Protocol::SystemViewResponse response15;
        timelineProtocol.ToJson(response15, error);
        Dic::Protocol::KernelDetailsResponse response16;
        timelineProtocol.ToJson(response16, error);
        Dic::Protocol::OneKernelResponse response17;
        timelineProtocol.ToJson(response17, error);
        Dic::Protocol::UnitThreadsOperatorsResponse response18;
        timelineProtocol.ToJson(response18, error);
    });
}

TEST_F(ProtocolTest, ToSystemViewOverallResponseTest) {
    Dic::Protocol::TimelineProtocol timelineProtocol;
    timelineProtocol.Register();
    std::string error;
    Dic::Protocol::SystemViewOverallResponse response;
    response.details = {
        {1.0, 30, 3, 4.0, 5.0, 3.0, "computing",
            {{1.0, 30, 3, 4.0, 5.0, 3.0, "fa",
                 {{1.0, 30, 3, 4.0, 5.0, 3.0, "fa-fwb"}, {1.0, 30, 3, 4.0, 5.0, 3.0, "fa-bwb"}}},
                {1.0, 30, 3, 4.0, 5.0, 3.0, "matmal", {}}}},
        {2.0, 40, 5, 4.0, 5.0, 3.0, "communication", {}},
    };
    response.pageParam.total = response.details.size();

    std::optional<Dic::document_t> jsonOptional = timelineProtocol.ToJson(response, error);
    EXPECT_EQ(jsonOptional.has_value(), true);
    EXPECT_EQ(jsonOptional.value().HasMember("body"), true);
    EXPECT_EQ(jsonOptional.value()["body"].HasMember("data"), true);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].IsArray(), true);
    EXPECT_EQ(jsonOptional.value()["body"]["data"].Size(), response.details.size());
    size_t i = 0;
    for (auto &item : jsonOptional.value()["body"]["data"].GetArray()) {
        EXPECT_EQ(item["name"].GetString(), response.details[i].name);
        EXPECT_EQ(item["totalTime"].GetDouble(), response.details[i].totalTime);
        EXPECT_EQ(item["ratio"].GetDouble(), response.details[i].ratio);
        EXPECT_EQ(item["nums"].GetUint(), response.details[i].nums);
        EXPECT_EQ(item["avg"].GetDouble(), response.details[i].avg);
        EXPECT_EQ(item["max"].GetDouble(), response.details[i].max);
        EXPECT_EQ(item["min"].GetDouble(), response.details[i].min);
        i++;
    }
}

TEST_F(ProtocolTest, EventToJson) {
    EXPECT_NO_THROW({
        Dic::Protocol::TimelineProtocol timelineProtocol;
        timelineProtocol.Register();
        std::string error;
        Dic::Protocol::ParseSuccessEvent event;
        timelineProtocol.ToJson(event, error);
        Dic::Protocol::ParseFailEvent event2;
        timelineProtocol.ToJson(event2, error);
        Dic::Protocol::ParseClusterCompletedEvent event3;
        timelineProtocol.ToJson(event3, error);
        Dic::Protocol::ParseClusterStep2CompletedEvent event4;
        timelineProtocol.ToJson(event4, error);
        Dic::Protocol::ParseMemoryCompletedEvent event5;
        timelineProtocol.ToJson(event5, error);
        Dic::Protocol::ModuleResetEvent event6;
        timelineProtocol.ToJson(event6, error);
    });
}

TEST_F(ProtocolTest, TestSetRequestBaseInfoNormal) {
    std::string command = "lll";
    Dic::Protocol::Request request(command);
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    const uint32_t expectId = 100;
    const uint32_t expectResultCallbackId = 55;
    Dic::JsonUtil::AddMember(json, "id", expectId, allocator);
    Dic::JsonUtil::AddMember(json, "command", command, allocator);
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "params", "{}", allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "projectName", "mmmmmmmmm", allocator);
    Dic::JsonUtil::AddMember(json, "resultCallbackId", expectResultCallbackId, allocator);
    Dic::Protocol::ProtocolUtil::SetRequestBaseInfo(request, json);
    EXPECT_EQ(request.id, expectId);
    EXPECT_EQ(request.command, command);
    EXPECT_TRUE(request.type == Dic::Protocol::ProtocolMessage::Type::REQUEST);
    EXPECT_EQ(request.moduleName, "hhh");
    EXPECT_EQ(request.projectName, "mmmmmmmmm");
}

TEST_F(ProtocolTest, TestSetRequestBaseInfoWhenNotNormal) {
    std::string command = "lll";
    Dic::Protocol::Request request(command);
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    const uint32_t expectId = 100;
    const uint32_t expectResultCallbackId = 55;
    Dic::JsonUtil::AddMember(json, "id", "100", allocator);
    Dic::JsonUtil::AddMember(json, "command", expectId, allocator);
    Dic::JsonUtil::AddMember(json, "type", expectId, allocator);
    Dic::JsonUtil::AddMember(json, "params", expectId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", expectId, allocator);
    Dic::JsonUtil::AddMember(json, "projectName", expectId, allocator);
    Dic::JsonUtil::AddMember(json, "resultCallbackId", expectResultCallbackId, allocator);
    Dic::Protocol::ProtocolUtil::SetRequestBaseInfo(request, json);
    EXPECT_EQ(request.id, 0);
    EXPECT_EQ(request.command, command);
    EXPECT_TRUE(request.type == Dic::Protocol::ProtocolMessage::Type::NONE);
    EXPECT_EQ(request.moduleName, "unknown");
    EXPECT_EQ(request.projectName, "");
}
