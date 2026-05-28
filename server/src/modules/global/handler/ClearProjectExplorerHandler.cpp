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
#include "BaselineManager.h"
#include "ProjectParserFactory.h"
#include "ClearProjectExplorerHandler.h"

namespace Dic {
namespace Module {
using namespace Dic::Server;
using namespace Global;

bool ClearProjectExplorerHandler::HandleRequest(std::unique_ptr<Request> requestPtr) {
    auto &request = dynamic_cast<ProjectExplorerInfoClearRequest &>(*requestPtr.get());
    std::unique_ptr<ProjectExplorerInfoClearResponse> responsePtr =
        std::make_unique<ProjectExplorerInfoClearResponse>();
    ProjectExplorerInfoClearResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    // 清空当前已加载数据
    std::vector<std::string> projectNameList = request.params.projectNameList;
    bool isNeedReset = projectNameList.empty() ||
        std::find(projectNameList.begin(), projectNameList.end(), request.projectName) != projectNameList.end();
    if (isNeedReset) {
        ParserFactory::Reset();
        BaselineManager::Instance().Reset();
    }
    // 清空db
    bool res = ProjectExplorerManager::Instance().ClearProjectExplorer(projectNameList);
    SendResponse(std::move(responsePtr), res);
    return res;
}
} // end of namespace Module
} // Dic
