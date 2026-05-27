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
#include "SummaryProtocolRequest.h"
#include "ParallelStrategyAlgorithmManager.h"
#include "SummarySlowRankAdvisorHandler.h"

namespace Dic::Module::Summary {
bool SummarySlowRankAdvisorHandler::HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) {
    auto &request = dynamic_cast<QueryParallelismArrangementRequest &>(*requestPtr);
    std::unique_ptr<SummarySlowRankAdvisorResponse> responsePtr = std::make_unique<SummarySlowRankAdvisorResponse>();
    SummarySlowRankAdvisorResponse &response = *responsePtr;
    SetBaseResponse(request, response);

    // check request parameter
    std::string err;
    if (!request.params.CheckParams(err)) {
        SetSummaryError(ErrorCode::PARAMS_ERROR);
        SendResponse(std::move(responsePtr), false, err);
        return false;
    }
    auto database = Timeline::DataBaseManager::Instance().GetClusterDatabase(request.params.clusterPath);
    if (database == nullptr) {
        SetSummaryError(ErrorCode::CONNECT_DATABASE_FAILED);
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    auto algPtr = ParallelStrategyAlgorithmManager::Instance().GetAlgorithmByProjectName(database->GetDbPath());
    if (algPtr == nullptr) {
        SendResponse(std::move(responsePtr), false);
        return false;
    }
    BaseParallelStrategyAlgorithm &algorithm = *algPtr;
    // Summary慢卡建议依赖于排布数据查询parallelism/arrangement/all和性能指标查询parallelism/performance/data的计算结果
    response.body.topNElements = algorithm.GetTopNAdviceInfo(response.body.matchSuccess);
    if (response.body.matchSuccess && response.body.topNElements.empty()) {
        response.body.hasSlowRank = false; // 当前维度无明显快慢卡问题
    }
    SendResponse(std::move(responsePtr), true);
    return true;
}
}
