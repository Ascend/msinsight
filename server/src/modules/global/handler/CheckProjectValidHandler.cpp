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
#include "FileUtil.h"
#include "pch.h"
#include <algorithm>
#include <queue>
#include <system_error>
#include "WsSessionManager.h"
#include "ProjectExplorerManager.h"
#include "CheckProjectValidHandler.h"

namespace Dic {
namespace Module {
using namespace Dic::Server;
using namespace Global;

namespace {
constexpr long long CSV_SIZE = 2ULL * 1024 * 1024 * 1024;
constexpr long long JSON_AND_BIN_SIZE = 10ULL * 1024 * 1024 * 1024;
constexpr uint64_t FILE_COUNT_LIMIT = 1000000; // 最大遍历文件数量
constexpr size_t PATH_SECURITY_QUEUE_LIMIT = 1000000; // 最大待校验队列长度
std::unordered_map<std::string, long long> FILE_MAX_SIZE = {
    {".csv", CSV_SIZE}, {".json", JSON_AND_BIN_SIZE}, {".bin", JSON_AND_BIN_SIZE}, {".db", JSON_AND_BIN_SIZE}};

struct PathCheckQueueItem {
    std::string path;
    int layer = 0;
};

void AddCheckError(std::vector<ProjectCheckBody::ErrorDetail> &errors, ProjectErrorType error, const std::string &path,
    const std::string &message, int layer) {
    errors.emplace_back(ProjectCheckBody::ErrorDetail{
        .layer = layer,
        .error = static_cast<int>(error),
        .path = path,
        .message = message.empty() ? "Project path check failed." : message,
    });
}

void UpdateFirstError(ProjectErrorType &error, const std::vector<ProjectCheckBody::ErrorDetail> &errors) {
    if (error == ProjectErrorType::NO_ERRORS && !errors.empty()) {
        error = static_cast<ProjectErrorType>(errors.front().error);
    }
}
}

bool Dic::Module::CheckProjectValidHandler::HandleRequest(std::unique_ptr<Request> requestPtr) {
    auto &request = dynamic_cast<ProjectCheckValidRequest &>(*requestPtr.get());
    std::unique_ptr<ProjectCheckValidResponse> responsePtr = std::make_unique<ProjectCheckValidResponse>();
    ProjectCheckValidResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    ProjectErrorType error = ProjectErrorType::NO_ERRORS;
    std::vector<ProjectCheckBody::ErrorDetail> errors;
    CheckRequestParamsValid(request.params, error, errors);
    response.body.error = error;
    response.body.result = static_cast<int>(error);
    response.body.errorDetail = std::move(errors);
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool Dic::Module::CheckProjectValidHandler::CheckRequestParamsValid(
    ProjectCheckParams &params, ProjectErrorType &error, std::vector<ProjectCheckBody::ErrorDetail> &errors) {
    std::string errorMsg;
    if (params.dataPath.empty()) {
        error = ProjectErrorType::FILE_NOT_EXISTS;
        AddCheckError(errors, error, "", "File not exist.", 0);
        return false;
    }
    if (!CheckPathByBfs(params.dataPath, error, errors)) {
        return false;
    }
    if (!params.ConvertToRealPath(errorMsg)) {
        error = ProjectErrorType::FILE_NOT_EXISTS;
        AddCheckError(errors, error, params.dataPath.empty() ? "" : params.dataPath.front(), errorMsg, 0);
        return false;
    }
    error = ProjectExplorerManager::Instance().CheckProjectConflict(params.projectName, params.dataPath[0]);
    if (error != ProjectErrorType::NO_ERRORS) {
        return false;
    }
    return true;
}

bool Dic::Module::CheckProjectValidHandler::CheckProjectFile(
    const fs::path &filePath, ProjectErrorType &error, std::vector<ProjectCheckBody::ErrorDetail> &errors, int layer) {
    if (FILE_MAX_SIZE.count(filePath.extension().string()) == 0) {
        return true;
    }
    std::string localFilePath = StringUtil::ToLocalStr(filePath.u8string());
    if (!CheckFileSize(filePath)) {
        error = ProjectErrorType::EXISTING_LARGE_FILES;
        AddCheckError(errors, error, localFilePath, "The file size exceeds the limit.", layer);
        return false;
    }
    if (!FileUtil::CheckFilePathLength(localFilePath)) {
        error = ProjectErrorType::EXCEEDS_MXIMUN_LENGTH;
        AddCheckError(errors, error, localFilePath, "The file path length exceeds the limit.", layer);
        return false;
    }
    return true;
}

bool Dic::Module::CheckProjectValidHandler::CheckFileSize(const fs::path &filePath) {
    std::string localFilePath = StringUtil::ToLocalStr(filePath.u8string());
    if (FileUtil::GetFileSize(localFilePath.c_str()) > FILE_MAX_SIZE[filePath.extension().string()]) {
        return false;
    }
    return true;
}
bool CheckProjectValidHandler::CheckPathSafety(
    const std::string &path, ProjectErrorType &error, std::vector<ProjectCheckBody::ErrorDetail> &errors, int layer) {
    auto checkResult = FileUtil::IsFolder(path) ? FileUtil::CheckPathSecurity(path)
                                                : FileUtil::CheckPathSecurity(path, CHECK_FILE_READ);
    if (!checkResult) {
        error = ProjectErrorType::IS_UNSAFE_PATH;
        AddCheckError(errors, error, path, checkResult.errMsg, layer);
        return false;
    }
    if (FileUtil::IsFolder(path)) {
        return true;
    }
    if (!FileUtil::IsRegularFile(path)) {
        error = ProjectErrorType::IS_NOT_REGULAR_FILE;
        AddCheckError(errors, error, path, "The path is not a regular file.", layer);
        return false;
    }
    return true;
}

bool EnqueuePath(std::queue<PathCheckQueueItem> &pending, std::vector<ProjectCheckBody::ErrorDetail> &errors,
    const std::string &path, int layer) {
    if (pending.size() >= PATH_SECURITY_QUEUE_LIMIT) {
        AddCheckError(errors, ProjectErrorType::IS_UNSAFE_PATH, path, "Too many paths to check.", layer);
        return false;
    }
    pending.push({path, layer});
    return true;
}

bool EnqueueSubPaths(const PathCheckQueueItem &item, std::queue<PathCheckQueueItem> &pending,
    std::vector<ProjectCheckBody::ErrorDetail> &errors) {
    std::error_code errorCode;
    std::string tempPath = StringUtil::ToUtf8Str(item.path);
    fs::directory_iterator iter(fs::u8path(tempPath), errorCode);
    if (errorCode) {
        AddCheckError(errors, ProjectErrorType::IS_UNSAFE_PATH, item.path, errorCode.message(), item.layer);
        return true;
    }
    fs::directory_iterator end;
    std::vector<std::string> subPaths;
    while (iter != end) {
        subPaths.emplace_back(StringUtil::ToLocalStr(iter->path().u8string()));
        iter.increment(errorCode);
        if (errorCode) {
            AddCheckError(errors, ProjectErrorType::IS_UNSAFE_PATH, item.path, errorCode.message(), item.layer);
            return true;
        }
    }
    std::sort(subPaths.begin(), subPaths.end());
    for (const auto &subPath : subPaths) {
        if (!EnqueuePath(pending, errors, subPath, item.layer + 1)) {
            return false;
        }
    }
    return true;
}

bool CheckProjectValidHandler::CheckPathByBfs(const std::vector<std::string> &paths, ProjectErrorType &error,
    std::vector<ProjectCheckBody::ErrorDetail> &errors) {
    std::queue<PathCheckQueueItem> pending;
    bool canContinue = true;
    for (const auto &path : paths) {
        canContinue = EnqueuePath(pending, errors, path, 0);
        if (!canContinue) {
            break;
        }
    }
    uint64_t fileCount = 0;
    while (!pending.empty() && canContinue) {
        auto item = pending.front();
        pending.pop();
        if (++fileCount > FILE_COUNT_LIMIT) {
            AddCheckError(errors, ProjectErrorType::IS_UNSAFE_PATH, item.path, "Too many paths to check.", item.layer);
            break;
        }
        ProjectErrorType itemError = ProjectErrorType::NO_ERRORS;
        if (!CheckPathSafety(item.path, itemError, errors, item.layer)) {
            continue;
        }
        std::string tempPath = StringUtil::ToUtf8Str(item.path);
        auto filePath = fs::u8path(tempPath);
        if (fs::exists(filePath) && !CheckProjectFile(filePath, itemError, errors, item.layer)) {
            continue;
        }
        if (FileUtil::IsFolder(item.path)) {
            canContinue = EnqueueSubPaths(item, pending, errors);
        }
    }
    UpdateFirstError(error, errors);
    return errors.empty();
}
}
}
