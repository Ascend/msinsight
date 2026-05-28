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
#include "WsSessionManager.h"
#include "ProjectExplorerManager.h"
#include "UpdateProjectExplorerInfoHandler.h"

namespace Dic {
namespace Module {
using namespace Dic::Server;
using namespace Global;

bool UpdateProjectExplorerInfoHandler::HandleRequest(std::unique_ptr<Request> requestPtr) {
    auto &request = dynamic_cast<ProjectExplorerInfoUpdateRequest &>(*requestPtr.get());
    std::unique_ptr<ProjectExplorerInfoUpdateResponse> responsePtr =
        std::make_unique<ProjectExplorerInfoUpdateResponse>();
    ProjectExplorerInfoUpdateResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    bool res = ProjectExplorerManager::Instance().UpdateProjectName(
        request.params.oldProjectName, request.params.newProjectName);
    SendResponse(std::move(responsePtr), res);
    return res;
}
} // end of namespace Module
} // Dic
