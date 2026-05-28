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

#include "DataBaseManager.h"
#include "ProjectExplorerManager.h"
#include "QueryCommunicationKernelHandler.h"

namespace Dic {
namespace Module {
namespace Timeline {
using namespace Dic::Server;

bool QueryCommunicationKernelHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    CommunicationKernelRequest &request = dynamic_cast<CommunicationKernelRequest &>(*requestPtr.get());
    std::unique_ptr<CommunicationKernelResponse> responsePtr = std::make_unique<CommunicationKernelResponse>();
    CommunicationKernelResponse &response = *responsePtr.get();
    SetBaseResponse(request, response);
    std::shared_ptr<VirtualTraceDatabase> database = nullptr;
    if (!request.fileId.empty()) {
        database = DataBaseManager::Instance().GetTraceDatabaseByFileId(request.fileId);
    } else {
        if (ProjectExplorerManager::Instance().IsTextMultiCluster(request.projectName)) {
            request.params.rankId =
                StringUtil::StrJoin(FileUtil::GetFileName(request.params.clusterPath), "_", request.params.rankId);
        }
        database = GetTraceDatabaseByRankId(request);
        if (database == nullptr) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
    }
    if (!database->QueryCommunicationKernelInfo(request.params.name, request.params.rankId, response.body)) {
        SetTimelineError(ErrorCode::QUERY_COMMUNICATION_KERNEL_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    // 判断是否具备集群数据并且包含集群文件
    if (!Global::ProjectExplorerManager::Instance().IsClusterData(request.projectName)) {
        SetTimelineError(ErrorCode::PROJECT_IS_NOT_CLUSTER);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    // 根据通信算子name,startTime查询step、group
    request.params.rankId = GetRealRankId(request.params.rankId);
    auto clusterDbList = Timeline::DataBaseManager::Instance().GetAllClusterDatabase();
    bool flag = false;
    for (auto &clusterDb : clusterDbList) {
        if (!clusterDb || !clusterDb->QueryIterationAndCommunicationGroup(request.params, response.body)) {
            continue;
        }
        flag = true;
        break;
    }
    if (!flag) {
        SetTimelineError(ErrorCode::QUERY_COMMUNICATION_KERNEL_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}

std::string QueryCommunicationKernelHandler::GetRealRankId(const std::string &rankId) {
    if (rankId.empty()) {
        return rankId;
    }
    std::vector<std::string> rankAfterSplit = StringUtil::Split(rankId, " ");
    // 存在 Host + 空格 + rankId的情況，这种情况下需要去掉Host信息
    if (rankAfterSplit.size() > 1) {
        return rankAfterSplit[1];
    }
    return rankId;
}

std::shared_ptr<VirtualTraceDatabase> QueryCommunicationKernelHandler::GetTraceDatabaseByRankId(
    CommunicationKernelRequest &request) {
    std::shared_ptr<VirtualTraceDatabase> database =
        DataBaseManager::Instance().GetTraceDatabaseByRankId(request.params.rankId);
    if (database == nullptr) {
        database = Timeline::DataBaseManager::Instance().GetTraceDatabaseInCluster(
            request.params.clusterPath, request.params.rankId);
        if (database == nullptr) {
            SetTimelineError(ErrorCode::CONNECT_DATABASE_FAILED);
        }
    }
    return database;
}
}
}
}
