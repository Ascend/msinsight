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
#include "BaselineManagerService.h"
#include "ProjectParserFactory.h"
#include "ProjectExplorerManager.h"
#include "GlobalDefs.h"
#include "BaselineManager.h"
#include "SetBaselineHandler.h"
namespace Dic {
namespace Module {
using namespace Dic::Server;
using namespace Global;

bool SetBaselineHandler::HandleRequest(std::unique_ptr<Request> requestPtr) {
    auto &request = dynamic_cast<BaselineSettingRequest &>(*requestPtr);
    std::unique_ptr<BaselineSettingResponse> responsePtr = std::make_unique<BaselineSettingResponse>();
    BaselineSettingResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    if (request.params.projectName.empty()) {
        response.body.errorMessage = "Set baseline failed, project name or filepath can't be empty!";
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    BaselineInfo baselineInfo;
    baselineInfo.clusterBaseLine = request.params.baselineClusterPath;
    baselineInfo.parsedFilePath = request.params.filePath;
    bool res = BaselineManagerService::InitBaselineData(request, baselineInfo);
    response.body.rankId = baselineInfo.rankId;
    response.body.host = baselineInfo.host;
    response.body.errorMessage = baselineInfo.errorMessage;
    response.body.cardName = baselineInfo.cardName;
    response.body.isCluster = baselineInfo.isCluster;
    response.body.isFtrace = baselineInfo.isFtrace;
    response.body.cluster = baselineInfo.clusterBaseLine;
    response.body.fileId = baselineInfo.fileId;
    SendResponse(std::move(responsePtr), res);
    // 发送解析成功消息（注意需在返回set/Baseline响应体之后）
    ProjectParserBase::SendAllParseSuccess();
    return res;
}
} // end of namespace Module
} // Dic
