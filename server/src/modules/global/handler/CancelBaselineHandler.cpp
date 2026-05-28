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
#include "ProjectExplorerManager.h"
#include "GlobalDefs.h"
#include "BaselineManagerService.h"
#include "CancelBaselineHandler.h"
namespace Dic {
namespace Module {
using namespace Dic::Server;
using namespace Global;

bool Dic::Module::CancelBaselineHandler::HandleRequest(std::unique_ptr<Request> requestPtr) {
    auto &request = dynamic_cast<BaselineCancelRequest &>(*requestPtr.get());
    std::unique_ptr<BaselineCancelResponse> responsePtr = std::make_unique<BaselineCancelResponse>();
    BaselineCancelResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    // 基线为当前工程卡时不需要清理缓存
    bool isCurrentProject = request.projectName == request.params.projectName;
    BaselineManagerService::ResetBaseline(!isCurrentProject);
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
}
