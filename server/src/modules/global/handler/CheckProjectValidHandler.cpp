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
constexpr uint64_t FILE_COUNT_LIMIT = 100000; // 最大遍历文件数量
namespace Module = Dic::Module;
std::unordered_map<std::string, long long> FILE_MAX_SIZE = {
    {".csv", CSV_SIZE}, {".json", JSON_AND_BIN_SIZE}, {".bin", JSON_AND_BIN_SIZE}, {".db", JSON_AND_BIN_SIZE}};
}

bool Dic::Module::CheckProjectValidHandler::HandleRequest(std::unique_ptr<Request> requestPtr) {
    auto &request = dynamic_cast<ProjectCheckValidRequest &>(*requestPtr.get());
    std::unique_ptr<ProjectCheckValidResponse> responsePtr = std::make_unique<ProjectCheckValidResponse>();
    ProjectCheckValidResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    ProjectErrorType error = ProjectErrorType::NO_ERRORS;
    if (!CheckRequestParamsValid(request.params, error)) {
        response.body.result = static_cast<int>(error);
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool Dic::Module::CheckProjectValidHandler::CheckRequestParamsValid(
    ProjectCheckParams &params, ProjectErrorType &error) {
    std::string errorMsg;
    bool safe = std::all_of(params.dataPath.begin(), params.dataPath.end(), [&error](const std::string &p) {
        if (!CheckPathSafety(p, error)) {
            return false;
        }
        return true;
    });
    if (!safe) {
        return false;
    }
    if (!params.ConvertToRealPath(errorMsg)) {
        error = ProjectErrorType::FILE_NOT_EXISTS;
        return false;
    }
    uint64_t fileCount = 0;
    for (const auto &path : params.dataPath) {
        if (++fileCount > FILE_COUNT_LIMIT) {
            break;
        }
        std::string tempPath = StringUtil::ToUtf8Str(path);
        auto filePath = fs::u8path(tempPath);
        if (!fs::exists(filePath)) {
            continue;
        }
        if (!CheckProjectFile(params, filePath, error)) {
            return false;
        }
    }
    error = ProjectExplorerManager::Instance().CheckProjectConflict(params.projectName, params.dataPath[0]);
    if (error != ProjectErrorType::NO_ERRORS) {
        return false;
    }
    return true;
}

bool Dic::Module::CheckProjectValidHandler::CheckProjectFile(
    ProjectCheckParams &params, const fs::path &filePath, ProjectErrorType &error) {
    if (FILE_MAX_SIZE.count(filePath.extension().string()) == 0) {
        return true;
    }
    if (!CheckFileSize(filePath)) {
        error = ProjectErrorType::EXISTING_LARGE_FILES;
        return false;
    }
    if (!FileUtil::CheckFilePathLength(filePath.string())) {
        error = ProjectErrorType::EXCEEDS_MXIMUN_LENGTH;
        return false;
    }
    return true;
}

bool Dic::Module::CheckProjectValidHandler::CheckFileSize(const fs::path &filePath) {
    if (FileUtil::GetFileSize(filePath.u8string().c_str()) > FILE_MAX_SIZE[filePath.extension().string()]) {
        return false;
    }
    return true;
}
bool CheckProjectValidHandler::CheckPathSafety(const std::string &path, ProjectErrorType &error) {
    if (!FileUtil::CheckPathSecurity(path)) {
        error = ProjectErrorType::IS_UNSAFE_PATH;
        return false;
    }
    if (FileUtil::IsFolder(path)) {
        return true;
    }
    if (!FileUtil::IsRegularFile(path)) {
        error = ProjectErrorType::IS_NOT_REGULAR_FILE;
        return false;
    }
    if (!FileUtil::CheckFileSize(path)) {
        error = ProjectErrorType::EXISTING_LARGE_FILES;
        return false;
    }
    return true;
}
}
}
