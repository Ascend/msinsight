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
#include "GlobalProtocol.h"
#include "GlobalProtocolUtil.h"

namespace Dic {
namespace Protocol {
using namespace Dic::Server;
using namespace rapidjson;
#pragma region <<Response to json>>
template <typename RESPONSE> std::optional<document_t> ToResponseJson(const RESPONSE &response) {
    ServerLog::Warn("The func that transfomer response to json is not implemented. command:", response.command);
    return std::nullopt;
}

template <> std::optional<document_t> ToResponseJson<TokenHeartCheckResponse>(const TokenHeartCheckResponse &response) {
    document_t json(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    return std::optional<document_t>{std::move(json)};
}

json_t FolderToJson(const std::unique_ptr<Folder> &folder, RAPIDJSON_DEFAULT_ALLOCATOR &allocator) {
    json_t json(kObjectType);
    JsonUtil::AddMember(json, "name", folder->name, allocator);
    JsonUtil::AddMember(json, "path", folder->path, allocator);
    json_t childrenFiles(kArrayType);
    for (const auto &file : folder->childrenFiles) {
        json_t jFile(kObjectType);
        JsonUtil::AddMember(jFile, "path", file->path, allocator);
        JsonUtil::AddMember(jFile, "name", file->name, allocator);
        childrenFiles.PushBack(jFile, allocator);
    }
    JsonUtil::AddMember(json, "childrenFiles", childrenFiles, allocator);
    json_t childrenFolders(kArrayType);
    for (const auto &childrenFolder : folder->childrenFolders) {
        childrenFolders.PushBack(FolderToJson(childrenFolder, allocator), allocator);
    }
    JsonUtil::AddMember(json, "childrenFolders", childrenFolders, allocator);
    return json;
}

template <> std::optional<document_t> ToResponseJson<FilesGetResponse>(const FilesGetResponse &response) {
    document_t json(kObjectType);
    json_t body(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(body, "path", response.body.path, allocator);
    json_t childrenFiles(kArrayType);
    for (const auto &file : response.body.childrenFiles) {
        json_t jFile(kObjectType);
        JsonUtil::AddMember(jFile, "path", file->path, allocator);
        JsonUtil::AddMember(jFile, "name", file->name, allocator);
        childrenFiles.PushBack(jFile, allocator);
    }
    JsonUtil::AddMember(body, "childrenFiles", childrenFiles, allocator);
    JsonUtil::AddMember(body, "exist", response.body.exist, allocator);
    json_t childrenFolders(kArrayType);
    for (const auto &folder : response.body.childrenFolders) {
        childrenFolders.PushBack(FolderToJson(folder, allocator), allocator);
    }
    JsonUtil::AddMember(body, "childrenFolders", childrenFolders, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<ProjectExplorerInfoUpdateResponse>(
    const ProjectExplorerInfoUpdateResponse &response) {
    document_t json(kObjectType);
    json_t body(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<ProjectExplorerInfoGetResponse>(
    const ProjectExplorerInfoGetResponse &response) {
    document_t json(kObjectType);
    json_t body(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    json_t projectDirectoryJson(kArrayType);
    for (const auto &item : response.body.projectDirectoryList) {
        json_t temp(kObjectType);
        JsonUtil::AddMember(temp, "projectName", item.projectName, allocator);
        json_t fileNameListJson(kArrayType);
        for (const auto &fileNameItem : item.fileName) {
            fileNameListJson.PushBack(json_t().SetString(fileNameItem->parseFilePath.c_str(), allocator), allocator);
        }
        JsonUtil::AddMember(temp, "fileName", fileNameListJson, allocator);
        json_t children(kArrayType);
        for (const auto &fileItem : item.projectTree) {
            children.PushBack(fileItem->SerializeToJson(allocator), allocator);
        }
        JsonUtil::AddMember(temp, "children", children, allocator);
        projectDirectoryJson.PushBack(temp, allocator);
    }
    JsonUtil::AddMember(body, "projectDirectoryList", projectDirectoryJson, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<ProjectExplorerInfoDeleteResponse>(
    const ProjectExplorerInfoDeleteResponse &response) {
    document_t json(kObjectType);
    json_t body(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<ProjectExplorerInfoClearResponse>(
    const ProjectExplorerInfoClearResponse &response) {
    document_t json(kObjectType);
    json_t body(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <>
std::optional<document_t> ToResponseJson<ProjectCheckValidResponse>(const ProjectCheckValidResponse &response) {
    document_t json(kObjectType);
    json_t body(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(body, "result", response.body.result, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<BaselineSettingResponse>(const BaselineSettingResponse &response) {
    document_t json(kObjectType);
    json_t body(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(body, "rankId", response.body.rankId, allocator);
    JsonUtil::AddMember(body, "host", response.body.host, allocator);
    JsonUtil::AddMember(body, "cardName", response.body.cardName, allocator);
    JsonUtil::AddMember(body, "isCluster", response.body.isCluster, allocator);
    JsonUtil::AddMember(body, "errorMessage", response.body.errorMessage, allocator);
    JsonUtil::AddMember(body, "dbPath", response.body.fileId, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

template <> std::optional<document_t> ToResponseJson<BaselineCancelResponse>(const BaselineCancelResponse &response) {
    document_t json(kObjectType);
    json_t body(kObjectType);
    ProtocolUtil::SetResponseJsonBaseInfo(response, json);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}

#pragma endregion

#pragma region <<Event to json>>
template <> std::optional<document_t> ToEventJson<ReadFileFailEvent>(const ReadFileFailEvent &event) {
    document_t json(kObjectType);
    json_t body(kObjectType);
    ProtocolUtil::SetEventJsonBaseInfo(event, json);
    auto &allocator = json.GetAllocator();
    JsonUtil::AddMember(body, "filePath", event.body.filePath, allocator);
    JsonUtil::AddMember(body, "error", event.body.error, allocator);
    JsonUtil::AddMember(json, "body", body, allocator);
    return std::optional<document_t>{std::move(json)};
}
#pragma endregion
} // end of namespace Protocol
} // end of namespace Dic
