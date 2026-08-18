/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
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

#include "QueryKernelMfuListHandler.h"

#include "DataBaseManager.h"
#include "DbClusterDataBase.h"
#include "KernelMfuDatabaseAccesser.h"

namespace Dic::Module::Timeline {
using namespace Dic::Server;

bool QueryKernelMfuListHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<Protocol::KernelMfuListRequest &>(*requestPtr);
    auto responsePtr = std::make_unique<Protocol::KernelMfuListResponse>();
    auto &response = *responsePtr;
    SetBaseResponse(request, response);
    response.current = request.params.current;
    response.pageSize = request.params.pageSize;

    std::string error;
    if (!request.params.CheckParams(error)) {
        ServerLog::Warn(error);
        SetTimelineError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false, error);
        return false;
    }

    const auto database = DataBaseManager::Instance().GetClusterDatabase(request.params.clusterPath);
    if (database == nullptr) {
        SetTimelineError(ErrorCode::QUERY_KERNEL_MFU_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    if (std::dynamic_pointer_cast<FullDb::DbClusterDataBase>(database) == nullptr) {
        SendResponse(std::move(responsePtr), true);
        return true;
    }

    const auto status = KernelMfuDatabaseAccesser::QueryList(
        database, request.params, response.data, response.rankOptions, response.count);
    if (status == KernelMfuQueryStatus::FAILED) {
        SetTimelineError(ErrorCode::QUERY_KERNEL_MFU_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    response.available = status == KernelMfuQueryStatus::SUCCESS;
    SendResponse(std::move(responsePtr), true);
    return true;
}
} // namespace Dic::Module::Timeline
