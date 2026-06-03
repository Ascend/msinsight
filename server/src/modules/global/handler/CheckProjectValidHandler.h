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

#ifndef PROFILER_SERVER_CHECKPROJECTVALIDHANDLER_H
#define PROFILER_SERVER_CHECKPROJECTVALIDHANDLER_H

#include <vector>
#include <string>
#include <dirent.h>
#include <sys/stat.h> // 包含 struct stat

#include "GlobalHandler.h"
#include "GlobalProtocolRequest.h"

namespace Dic {
namespace Module {

class CheckProjectValidHandler : public GlobalHandler {
  public:
    CheckProjectValidHandler() { command = REQ_RES_PROJECT_VALID_CHECK; }
    ~CheckProjectValidHandler() override = default;

    bool HandleRequest(std::unique_ptr<Request> requestPtr) override;

  private:
    static bool CheckRequestParamsValid(
        ProjectCheckParams &params, ProjectErrorType &error, std::vector<ProjectCheckBody::ErrorDetail> &errors);
    static bool CheckProjectFile(const fs::path &filePath, ProjectErrorType &error,
        std::vector<ProjectCheckBody::ErrorDetail> &errors, int layer);
    static bool CheckFileSize(const fs::path &filePath);
    static bool CheckPathSafety(const std::string &path, ProjectErrorType &error,
        std::vector<ProjectCheckBody::ErrorDetail> &errors, int layer);
    static bool CheckPathByBfs(const std::vector<std::string> &paths, ProjectErrorType &error,
        std::vector<ProjectCheckBody::ErrorDetail> &errors);
};
} // end of namespace Module
} // Dic

#endif // PROFILER_SERVER_CHECKPROJECTVALIDHANDLER_H
