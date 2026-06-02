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
#include "GlobalProtocol.h"
#include "ProtocolDefs.h"
#include "GlobalProtocolRequest.h"
#include "GlobalProtocolResponse.h"
#include "GlobalProtocolEvent.h"

using namespace Dic::Protocol;

class GlobalProtocolUtilTest : public ::testing::Test {
  protected:
    void SetUp() override { protocol.Register(); }

    void TearDown() override { protocol.UnRegister(); }

    Dic::Protocol::GlobalProtocol protocol;
};

TEST_F(GlobalProtocolUtilTest, ToProjectExplorerInfoClearRequest) {
    std::string reqJson = R"({"id": 2, "moduleName": "global", "type": "request",
        "command": "files/clearProjectExplorer", "resultCallbackId": 0, "params": {}})";
    Dic::document_t json;
    json.Parse(reqJson.c_str());
    std::string err;
    auto result = dynamic_cast<ProjectExplorerInfoClearRequest &>(*(protocol.FromJson(json, err)));
    EXPECT_EQ(result.command, REQ_RES_PROJECT_EXPLORER_CLEAR);
}

TEST_F(GlobalProtocolUtilTest, ToSetParallelStrategyResponseTest) {
    Dic::Protocol::ProjectExplorerInfoClearResponse response;
    std::string err;
    response.result = false;
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(response, err);
    EXPECT_EQ(jsonOptional.value()["result"], response.result);
}

TEST_F(GlobalProtocolUtilTest, TransformReadFileFailEventToJsonTest) {
    Dic::Protocol::ReadFileFailEvent event;
    std::string err;
    event.body.filePath = "aaa";
    event.body.error = "bbb";
    std::optional<Dic::document_t> jsonOptional = protocol.ToJson(event, err);
    EXPECT_TRUE(jsonOptional.has_value());
    auto &value = jsonOptional.value();

    EXPECT_TRUE(value.HasMember("body"));
    auto &body = value["body"];

    EXPECT_TRUE(body.HasMember("filePath"));
    EXPECT_EQ(body["filePath"].GetString(), event.body.filePath);

    EXPECT_TRUE(body.HasMember("error"));
    EXPECT_EQ(body["error"].GetString(), event.body.error);
}
