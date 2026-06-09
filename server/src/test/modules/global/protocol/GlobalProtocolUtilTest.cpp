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
#include "GlobalProtocolResponse.h"
#include "JsonUtil.h"
#include "GlobalProtocolUtil.h"
#include "SystemMemoryDatabaseDef.h"
class GlobalProtocolUtilTest : public ::testing::Test {};

TEST_F(GlobalProtocolUtilTest, TestTokenHeartCheckResponse) {
    Dic::Protocol::TokenHeartCheckResponse response;
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
}

TEST_F(GlobalProtocolUtilTest, TestFilesGetResponseToJson) {
    Dic::Protocol::FilesGetResponse response;
    std::unique_ptr<Dic::Protocol::File> file1 = std::make_unique<Dic::Protocol::File>();
    std::unique_ptr<Dic::Protocol::File> file2 = std::make_unique<Dic::Protocol::File>();
    std::unique_ptr<Dic::Protocol::Folder> folder = std::make_unique<Dic::Protocol::Folder>();
    std::unique_ptr<Dic::Protocol::Folder> folder2 = std::make_unique<Dic::Protocol::Folder>();
    folder2->childrenFiles.emplace_back(std::move(file2));
    folder->childrenFolders.emplace_back(std::move(folder2));
    response.body.childrenFiles.emplace_back(std::move(file1));
    response.body.childrenFolders.emplace_back(std::move(folder));
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
}

TEST_F(GlobalProtocolUtilTest, TestProjectExplorerInfoUpdateResponseToJson) {
    Dic::Protocol::ProjectExplorerInfoUpdateResponse response;
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
}

TEST_F(GlobalProtocolUtilTest, TestProjectExplorerInfoGetResponseToJson) {
    Dic::Protocol::ProjectExplorerInfoGetResponse response;
    Dic::Protocol::ProjectDirectoryInfo projectDirectoryInfo;
    auto fileInfo = std::make_shared<Dic::Module::Global::ParseFileInfo>();
    fileInfo->parseFilePath = "lllll";
    projectDirectoryInfo.fileName.emplace_back(fileInfo);
    response.body.projectDirectoryList.emplace_back(projectDirectoryInfo);
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr = "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,"
                                "\"command\":\"files/getProjectExplorer\",\"moduleName\":\"unknown\","
                                "\"body\":{\"projectDirectoryList\":"
                                "[{\"projectName\":\"\",\"fileName\":[\"lllll\"],\"children\":[]}]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(GlobalProtocolUtilTest, TestProjectExplorerInfoDeleteResponseToJson) {
    Dic::Protocol::ProjectExplorerInfoDeleteResponse response;
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr = "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"files/"
                                "deleteProjectExplorer\",\"moduleName\":\"unknown\",\"body\":{}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(GlobalProtocolUtilTest, TestProjectCheckValidResponseToJson) {
    Dic::Protocol::ProjectCheckValidResponse response;
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr = "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"files/"
                                "checkProjectValid\",\"moduleName\":\"unknown\",\"body\":{\"result\":0}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(GlobalProtocolUtilTest, TestProjectCheckValidResponseToJsonWithErrors) {
    Dic::Protocol::ProjectCheckValidResponse response;
    response.body.result = static_cast<int>(Dic::Protocol::ProjectErrorType::IS_UNSAFE_PATH);
    response.body.errorDetail.emplace_back(Dic::Protocol::ProjectCheckBody::ErrorDetail{
        .layer = 1,
        .error = static_cast<int>(Dic::Protocol::ProjectErrorType::IS_UNSAFE_PATH),
        .path = "/tmp/bad_file.json",
        .message = "The path contains invalid characters.",
    });
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr =
        "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"files/"
        "checkProjectValid\",\"moduleName\":\"unknown\",\"body\":{\"result\":2,\"errorDetail\":[{\"layer\":1,\"error\":"
        "2,"
        "\"path\":\"/tmp/bad_file.json\",\"message\":\"The path contains invalid characters.\"}]}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(GlobalProtocolUtilTest, TestBaselineSettingResponseToJson) {
    Dic::Protocol::BaselineSettingResponse response;
    response.body.isFtrace = true;
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr = "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":"
                                "\"global/setBaseline\",\"moduleName\":\"unknown\",\"body\":{\"rankId\":\"\","
                                "\"host\":\"\",\"cardName\":\"\",\"isCluster\":false,\"errorMessage\":\"\",\""
                                "dbPath\":\"\",\"isFtrace\":true}}";
    EXPECT_EQ(json, jsonStr);
}

TEST_F(GlobalProtocolUtilTest, TestBaselineCancelResponseToJson) {
    Dic::Protocol::BaselineCancelResponse response;
    auto jsonOp = Dic::Protocol::ToResponseJson(response);
    EXPECT_EQ(jsonOp.has_value(), true);
    const std::string json = Dic::JsonUtil::JsonDump(jsonOp.value());
    const std::string jsonStr = "{\"type\":\"response\",\"id\":0,\"requestId\":0,\"result\":false,\"command\":\"global/"
                                "cancelBaseline\",\"moduleName\":\"unknown\",\"body\":{}}";
    EXPECT_EQ(json, jsonStr);
}
