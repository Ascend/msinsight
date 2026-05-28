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

#include "pch.h"
#include "GlobalProtocolRequest.h"
#include "GlobalProtocolResponse.h"
#include "GlobalProtocolUtil.h"
#include "GlobalProtocol.h"

namespace Dic {
namespace Protocol {
void GlobalProtocol::RegisterJsonToRequestFuncs() {
    jsonToReqFactory.emplace(REQ_RES_HEART_CHECK, ToHeartCheckRequest);
    jsonToReqFactory.emplace(REQ_RES_FILES_GET, ToFilesGetRequest);
    jsonToReqFactory.emplace(REQ_RES_PROJECT_EXPLORER_UPDATE, ToProjectExplorerUpdateRequest);
    jsonToReqFactory.emplace(REQ_RES_PROJECT_EXPLORER_INFO_GET, ToProjectExplorerInfoGetRequest);
    jsonToReqFactory.emplace(REQ_RES_PROJECT_EXPLORER_INFO_DELETE, ToProjectExplorerInfoDeleteRequest);
    jsonToReqFactory.emplace(REQ_RES_PROJECT_EXPLORER_CLEAR, ToProjectExplorerInfoClearRequest);
    jsonToReqFactory.emplace(REQ_RES_PROJECT_VALID_CHECK, ToProjectValidCheckRequest);
    jsonToReqFactory.emplace(REQ_RES_PROJECT_SET_BASELINE, ToSetBaselineRequest);
    jsonToReqFactory.emplace(REQ_RES_PROJECT_CANCEL_BASELINE, ToCancelBaselineRequest);
    jsonToReqFactory.emplace(REQ_RES_GET_MODULE_CONFIG, ToHeartCheckRequest);
}

void GlobalProtocol::RegisterResponseToJsonFuncs() {
    resToJsonFactory.emplace(REQ_RES_HEART_CHECK, ToTokenHeartCheckResponseJson);
    resToJsonFactory.emplace(REQ_RES_FILES_GET, ToFilesGetResponseJson);
    resToJsonFactory.emplace(REQ_RES_PROJECT_EXPLORER_UPDATE, ToProjectExplorerInfoUpdateResponseJson);
    resToJsonFactory.emplace(REQ_RES_PROJECT_EXPLORER_INFO_GET, ToProjectExplorerInfoGetResponseJson);
    resToJsonFactory.emplace(REQ_RES_PROJECT_EXPLORER_INFO_DELETE, ToProjectExplorerInfoDeleteResponseJson);
    resToJsonFactory.emplace(REQ_RES_PROJECT_EXPLORER_CLEAR, ToProjectExplorerInfoClearResponseJson);
    resToJsonFactory.emplace(REQ_RES_PROJECT_VALID_CHECK, ToProjectValidCheckResponseJson);
    resToJsonFactory.emplace(REQ_RES_PROJECT_SET_BASELINE, ToSetBaselineResponseJson);
    resToJsonFactory.emplace(REQ_RES_PROJECT_CANCEL_BASELINE, ToCancelBaselineResponseJson);
    resToJsonFactory.emplace(REQ_RES_GET_MODULE_CONFIG, ToGetModuleConfigResponseJson);
}

void GlobalProtocol::RegisterEventToJsonFuncs() {
    eventToJsonFactory.emplace(EVENT_FILES_READ_FAIL, ToReadFileFailEventJson);
}

#pragma region <<Json To Request>>
std::unique_ptr<Request> GlobalProtocol::ToHeartCheckRequest(const json_t &json, std::string &error) {
    std::unique_ptr<HeartCheckRequest> reqPtr = std::make_unique<HeartCheckRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    return reqPtr;
}

std::unique_ptr<Request> GlobalProtocol::ToFilesGetRequest(const json_t &json, std::string &error) {
    std::unique_ptr<FilesGetRequest> reqPtr = std::make_unique<FilesGetRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request base info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.path, json["params"], "path");
    return reqPtr;
}

std::unique_ptr<Request> GlobalProtocol::ToProjectExplorerUpdateRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ProjectExplorerInfoUpdateRequest> reqPtr = std::make_unique<ProjectExplorerInfoUpdateRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request project explorer update info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.newProjectName, json["params"], "newProjectName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.oldProjectName, json["params"], "oldProjectName");
    return reqPtr;
}

std::unique_ptr<Request> GlobalProtocol::ToProjectExplorerInfoGetRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ProjectExplorerInfoGetRequest> reqPtr = std::make_unique<ProjectExplorerInfoGetRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request project explorer get info, command is: " + reqPtr->command;
        return nullptr;
    }
    return reqPtr;
}

std::unique_ptr<Request> GlobalProtocol::ToProjectExplorerInfoDeleteRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ProjectExplorerInfoDeleteRequest> reqPtr = std::make_unique<ProjectExplorerInfoDeleteRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request project explorer delete info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.projectName, json["params"], "projectName");
    if (json["params"].HasMember("dataPath") && json["params"]["dataPath"].IsArray()) {
        for (const auto &item : json["params"]["dataPath"].GetArray()) {
            if (item.IsString()) {
                reqPtr->params.dataPath.emplace_back(item.GetString());
            }
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> GlobalProtocol::ToProjectExplorerInfoClearRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ProjectExplorerInfoClearRequest> reqPtr = std::make_unique<ProjectExplorerInfoClearRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request project explorer get info, command is: " + reqPtr->command;
        return nullptr;
    }
    if (json["params"].HasMember("projectNameList") && json["params"]["projectNameList"].IsArray()) {
        for (const auto &item : json["params"]["projectNameList"].GetArray()) {
            if (item.IsString()) {
                reqPtr->params.projectNameList.emplace_back(item.GetString());
            }
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> GlobalProtocol::ToProjectValidCheckRequest(const json_t &json, std::string &error) {
    std::unique_ptr<ProjectCheckValidRequest> reqPtr = std::make_unique<ProjectCheckValidRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request project explorer delete info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.projectName, json["params"], "projectName");
    if (json["params"].HasMember("dataPath") && json["params"]["dataPath"].IsArray()) {
        for (const auto &item : json["params"]["dataPath"].GetArray()) {
            if (item.IsString()) {
                reqPtr->params.dataPath.emplace_back(item.GetString());
            }
        }
    }
    return reqPtr;
}

std::unique_ptr<Request> GlobalProtocol::ToSetBaselineRequest(const json_t &json, std::string &error) {
    std::unique_ptr<BaselineSettingRequest> reqPtr = std::make_unique<BaselineSettingRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request project explorer delete info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.projectName, json["params"], "projectName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.filePath, json["params"], "filePath");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.baselineClusterPath, json["params"], "baselineClusterPath");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.currentClusterPath, json["params"], "currentClusterPath");
    return reqPtr;
}

std::unique_ptr<Request> GlobalProtocol::ToCancelBaselineRequest(const json_t &json, std::string &error) {
    std::unique_ptr<BaselineCancelRequest> reqPtr = std::make_unique<BaselineCancelRequest>();
    if (!ProtocolUtil::SetRequestBaseInfo(*reqPtr, json)) {
        error = "Failed to set request project explorer delete info, command is: " + reqPtr->command;
        return nullptr;
    }
    JsonUtil::SetByJsonKeyValue(reqPtr->params.projectName, json["params"], "projectName");
    JsonUtil::SetByJsonKeyValue(reqPtr->params.filePath, json["params"], "filePath");
    return reqPtr;
}

#pragma endregion

#pragma region <<Response To Json>>

std::optional<document_t> GlobalProtocol::ToTokenHeartCheckResponseJson(const Response &response) {
    return ToResponseJson<TokenHeartCheckResponse>(dynamic_cast<const TokenHeartCheckResponse &>(response));
}

std::optional<document_t> GlobalProtocol::ToFilesGetResponseJson(const Response &response) {
    return ToResponseJson<FilesGetResponse>(dynamic_cast<const FilesGetResponse &>(response));
}

std::optional<document_t> GlobalProtocol::ToProjectExplorerInfoUpdateResponseJson(const Response &response) {
    return ToResponseJson<ProjectExplorerInfoUpdateResponse>(
        dynamic_cast<const ProjectExplorerInfoUpdateResponse &>(response));
}

std::optional<document_t> GlobalProtocol::ToProjectExplorerInfoGetResponseJson(const Response &response) {
    return ToResponseJson<ProjectExplorerInfoGetResponse>(
        dynamic_cast<const ProjectExplorerInfoGetResponse &>(response));
}
std::optional<document_t> GlobalProtocol::ToProjectExplorerInfoDeleteResponseJson(const Response &response) {
    return ToResponseJson<ProjectExplorerInfoDeleteResponse>(
        dynamic_cast<const ProjectExplorerInfoDeleteResponse &>(response));
}

std::optional<document_t> GlobalProtocol::ToProjectExplorerInfoClearResponseJson(const Response &response) {
    return ToResponseJson<ProjectExplorerInfoClearResponse>(
        dynamic_cast<const ProjectExplorerInfoClearResponse &>(response));
}

std::optional<document_t> GlobalProtocol::ToProjectValidCheckResponseJson(const Response &response) {
    return ToResponseJson<ProjectCheckValidResponse>(dynamic_cast<const ProjectCheckValidResponse &>(response));
}

std::optional<document_t> GlobalProtocol::ToSetBaselineResponseJson(const Response &response) {
    return ToResponseJson<BaselineSettingResponse>(dynamic_cast<const BaselineSettingResponse &>(response));
}

std::optional<document_t> GlobalProtocol::ToCancelBaselineResponseJson(const Response &response) {
    return ToResponseJson<BaselineCancelResponse>(dynamic_cast<const BaselineCancelResponse &>(response));
}

std::optional<document_t> GlobalProtocol::ToGetModuleConfigResponseJson(const Response &response) {
    auto resPtr = dynamic_cast<const ModuleConfigGetResponse &>(response);
    document_t json(kObjectType);
    json_t body(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    json_t configs(kArrayType);
    for (const auto &item : resPtr.configs) {
        configs.PushBack(json_t().SetString(item.c_str(), allocator), allocator);
    }
    JsonUtil::AddMember(body, "configs", configs, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}
#pragma endregion

#pragma region <<Event To Json>>
std::optional<document_t> GlobalProtocol::ToReadFileFailEventJson(const Event &event) {
    return ToEventJson<ReadFileFailEvent>(dynamic_cast<const ReadFileFailEvent &>(event));
}
#pragma endregion
} // namespace Protocol
} // namespace Dic
