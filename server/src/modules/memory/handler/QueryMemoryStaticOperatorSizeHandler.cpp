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
#include <cmath>
#include "DataBaseManager.h"
#include "BaselineManager.h"
#include "QueryMemoryStaticOperatorSizeHandler.h"

namespace Dic {
namespace Module {
namespace Memory {
using namespace Dic::Server;
bool QueryMemoryStaticOperatorSizeHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    MemoryStaticOperatorSizeRequest &request = dynamic_cast<MemoryStaticOperatorSizeRequest &>(*requestPtr);
    std::unique_ptr<MemoryStaticOperatorSizeResponse> responsePtr =
        std::make_unique<MemoryStaticOperatorSizeResponse>();
    MemoryStaticOperatorSizeResponse &response = *responsePtr;
    SetBaseResponse(request, response);
    std::string errorMsg;
    if (!request.params.CommonCheck(errorMsg)) {
        SetMemoryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId(request.params.rankId);
    if (!database) {
        SetMemoryError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }

    if (!request.params.isCompare) {
        if (!database->QueryStaticOperatorSize(request.params, response.size.minSize, response.size.maxSize)) {
            SetMemoryError(ErrorCode::QUERY_MEMORY_STATIC_OPERATOR_SIZE_FAILED);
            SendResponse(std::move(responsePtr), false);
            return false;
        }
    } else {
        StaticOperatorSize compareData;
        StaticOperatorSize baselineData;
        if (!GetRespectiveData(database, compareData, baselineData, request, errorMsg)) {
            SendResponse(std::move(responsePtr), false);
            return false;
        }
        ExecuteComparisonAlgorithm(compareData, baselineData, response);
    }
    // 接口查询成功时，为了避免浮点数舍入误差导致的问题，对minSize下取整，maxSize上取整，保证表格中算子size在minSize到maxSize之内
    response.size.minSize = std::floor(response.size.minSize);
    response.size.maxSize = std::ceil(response.size.maxSize);
    SendResponse(std::move(responsePtr), true);
    return true;
}

bool QueryMemoryStaticOperatorSizeHandler::GetRespectiveData(std::shared_ptr<VirtualMemoryDataBase> database,
    Dic::Protocol::StaticOperatorSize &compareData, Dic::Protocol::StaticOperatorSize &baselineData,
    Dic::Protocol::MemoryStaticOperatorSizeRequest &request, std::string &errorMsg) {
    std::string baselineId = Global::BaselineManager::Instance().GetBaselineId();
    if (baselineId.empty()) {
        errorMsg = "Failed to get baseline id.";
        SetMemoryError(ErrorCode::GET_BASELINE_ID_FAILED);
        return false;
    }
    auto databaseBaseline = Timeline::DataBaseManager::Instance().GetMemoryDatabaseByRankId(baselineId);
    if (!databaseBaseline) {
        errorMsg = "Failed to connect to database of baseline.";
        SetMemoryError(ErrorCode::CONNECT_DATABASE_FAILED);
        return false;
    }
    if (!database->QueryStaticOperatorSize(request.params, compareData.minSize, compareData.maxSize)) {
        errorMsg = "Failed to query memory static operator size compare data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_STATIC_OPERATOR_SIZE_COMPARE_FAILED);
        return false;
    }
    if (!databaseBaseline->QueryStaticOperatorSize(request.params, baselineData.minSize, baselineData.maxSize)) {
        errorMsg = "Failed to query memory static operator size baseline data.";
        SetMemoryError(ErrorCode::QUERY_MEMORY_STATIC_OPERATOR_SIZE_BASELINE_FAILED);
        return false;
    }
    return true;
}

void QueryMemoryStaticOperatorSizeHandler::ExecuteComparisonAlgorithm(
    const Dic::Protocol::StaticOperatorSize &compareData, const Dic::Protocol::StaticOperatorSize &baselineData,
    Dic::Protocol::MemoryStaticOperatorSizeResponse &response) {
    response.size.minSize = compareData.minSize - baselineData.maxSize;
    response.size.maxSize = compareData.maxSize - baselineData.minSize;
}

} // end of namespace Memory
} // end of namespace Module
} // end of namespace Dic
