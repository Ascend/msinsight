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
#include "JsonUtil.h"
#include "GlobalProtocolResponse.h"
#include "GlobalProtocol.h"
class GlobalProtocolTest : public ::testing::Test {};

TEST_F(GlobalProtocolTest, ToHeartCheckRequestTest) {
    const uint64_t tempId = 89;
    Dic::Protocol::GlobalProtocol globalProtocol;
    globalProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "heartCheck", allocator);
    globalProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::json_t path(Dic::kArrayType);
    path.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "path", path, allocator);
    Dic::JsonUtil::AddMember(params, "projectAction", 0, allocator);
    Dic::JsonUtil::AddMember(params, "isConflict", false, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = globalProtocol.FromJson(json, error)->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(GlobalProtocolTest, ToProjectExplorerUpdateRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::GlobalProtocol globalProtocol;
    globalProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "files/updateProjectExplorer", allocator);
    globalProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::json_t path(Dic::kArrayType);
    path.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "path", path, allocator);
    Dic::JsonUtil::AddMember(params, "projectAction", 0, allocator);
    Dic::JsonUtil::AddMember(params, "isConflict", false, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = globalProtocol.FromJson(json, error)->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(GlobalProtocolTest, ToProjectExplorerInfoGetRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::GlobalProtocol globalProtocol;
    globalProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "files/getProjectExplorer", allocator);
    globalProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::json_t path(Dic::kArrayType);
    path.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "path", path, allocator);
    Dic::JsonUtil::AddMember(params, "projectAction", 0, allocator);
    Dic::JsonUtil::AddMember(params, "isConflict", false, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = globalProtocol.FromJson(json, error)->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(GlobalProtocolTest, ToProjectExplorerInfoDeleteRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::GlobalProtocol globalProtocol;
    globalProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "files/deleteProjectExplorer", allocator);
    globalProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::json_t path(Dic::kArrayType);
    path.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "path", path, allocator);
    Dic::JsonUtil::AddMember(params, "projectAction", 0, allocator);
    Dic::JsonUtil::AddMember(params, "isConflict", false, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = globalProtocol.FromJson(json, error)->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(GlobalProtocolTest, ToProjectExplorerInfoClearRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::GlobalProtocol globalProtocol;
    globalProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "files/clearProjectExplorer", allocator);
    globalProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::json_t path(Dic::kArrayType);
    path.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "path", path, allocator);
    Dic::JsonUtil::AddMember(params, "projectAction", 0, allocator);
    Dic::JsonUtil::AddMember(params, "isConflict", false, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::json_t projectNameList(Dic::kArrayType);
    projectNameList.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "projectNameList", projectNameList, allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = globalProtocol.FromJson(json, error)->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(GlobalProtocolTest, ToProjectValidCheckRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::GlobalProtocol globalProtocol;
    globalProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "files/checkProjectValid", allocator);
    globalProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::json_t path(Dic::kArrayType);
    path.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "path", path, allocator);
    Dic::JsonUtil::AddMember(params, "projectAction", 0, allocator);
    Dic::JsonUtil::AddMember(params, "isConflict", false, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::json_t dataPath(Dic::kArrayType);
    dataPath.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "dataPath", dataPath, allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = globalProtocol.FromJson(json, error)->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(GlobalProtocolTest, ToSetBaselineRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::GlobalProtocol globalProtocol;
    globalProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "global/setBaseline", allocator);
    globalProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::json_t path(Dic::kArrayType);
    path.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "path", path, allocator);
    Dic::JsonUtil::AddMember(params, "projectAction", 0, allocator);
    Dic::JsonUtil::AddMember(params, "isConflict", false, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = globalProtocol.FromJson(json, error)->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(GlobalProtocolTest, ToCancelBaselineRequest) {
    const uint64_t tempId = 89;
    Dic::Protocol::GlobalProtocol globalProtocol;
    globalProtocol.Register();
    std::string error;
    Dic::document_t json(Dic::kObjectType);
    auto &allocator = json.GetAllocator();
    Dic::JsonUtil::AddMember(json, "type", "request", allocator);
    Dic::JsonUtil::AddMember(json, "command", "global/cancelBaseline", allocator);
    globalProtocol.FromJson(json, error);

    Dic::json_t params(Dic::kObjectType);
    Dic::json_t path(Dic::kArrayType);
    path.PushBack("kkkkkk", allocator);
    Dic::JsonUtil::AddMember(params, "path", path, allocator);
    Dic::JsonUtil::AddMember(params, "projectAction", 0, allocator);
    Dic::JsonUtil::AddMember(params, "isConflict", false, allocator);
    Dic::JsonUtil::AddMember(json, "id", tempId, allocator);
    Dic::JsonUtil::AddMember(json, "moduleName", "hhh", allocator);
    Dic::JsonUtil::AddMember(json, "params", params, allocator);
    unsigned int id = globalProtocol.FromJson(json, error)->id;
    EXPECT_EQ(id, tempId);
}

TEST_F(GlobalProtocolTest, ResponseToJson) {
    EXPECT_NO_THROW({
        Dic::Protocol::GlobalProtocol timelineProtocol;
        timelineProtocol.Register();
        std::string error;
        Dic::Protocol::TokenHeartCheckResponse response1;
        timelineProtocol.ToJson(response1, error);
        Dic::Protocol::ProjectExplorerInfoUpdateResponse response2;
        timelineProtocol.ToJson(response2, error);
        Dic::Protocol::ProjectExplorerInfoGetResponse response3;
        timelineProtocol.ToJson(response3, error);
        Dic::Protocol::ProjectExplorerInfoDeleteResponse response4;
        timelineProtocol.ToJson(response4, error);
        Dic::Protocol::ProjectCheckValidResponse response5;
        timelineProtocol.ToJson(response5, error);
        Dic::Protocol::BaselineSettingResponse response6;
        timelineProtocol.ToJson(response6, error);
        Dic::Protocol::BaselineCancelResponse response7;
        timelineProtocol.ToJson(response7, error);
        Dic::Protocol::ModuleConfigGetResponse response8;
        response8.configs.emplace_back("lll");
        timelineProtocol.ToJson(response8, error);
    });
}
