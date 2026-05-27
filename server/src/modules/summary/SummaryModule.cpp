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

#include "SummaryTopRankHandler.h"
#include "SummaryStatisticsHandler.h"
#include "QueryComputeDetailInfoHandler.h"
#include "QueryCommunicationDetailHandler.h"
#include "QueryParallelStrategyConfigHandler.h"
#include "SetParallelStrategyConfigHandler.h"
#include "QueryFwdBwdTimelineHandler.h"
#include "QueryParallelismArrangementHandler.h"
#include "QueryParallelismPerformanceHandler.h"
#include "ImportExpertDataHandler.h"
#include "QueryExpertHotspotHandler.h"
#include "QueryModelInfoHandler.h"
#include "SummarySlowRankAdvisorHandler.h"
#include "ProtocolDefs.h"
#include "SummaryModule.h"

namespace Dic {
namespace Module {
using namespace Dic::Module::Summary;
SummaryModule::SummaryModule() : BaseModule() { moduleName = MODULE_SUMMARY; }

SummaryModule::~SummaryModule() { requestHandlerMap.clear(); }

void SummaryModule::RegisterRequestHandlers() {
    requestHandlerMap.clear();
    requestHandlerMap.emplace(REQ_RES_SUMMARY_QUERY_TOP_DATA, std::make_unique<SummaryTopRankHandler>());
    requestHandlerMap.emplace(REQ_RES_SUMMARY_STATISTIC, std::make_unique<SummaryStatisticsHandler>());
    requestHandlerMap.emplace(REQ_RES_COMPUTE_DETAIL, std::make_unique<QueryComputeDetailInfoHandler>());
    requestHandlerMap.emplace(REQ_RES_COMMUNICATION_DETAIL, std::make_unique<QueryCommunicationDetailHandler>());
    requestHandlerMap.emplace(
        REQ_RES_SUMMARY_QUERY_PARALLEL_STRATEGY, std::make_unique<QueryParallelStrategyConfigHandler>());
    requestHandlerMap.emplace(
        REQ_RES_SUMMARY_SET_PARALLEL_STRATEGY, std::make_unique<SetParallelStrategyConfigHandler>());
    requestHandlerMap.emplace(REQ_RES_PIPELINE_FWD_BWD_TIMELINE, std::make_unique<QueryFwdBwdTimelineHandler>());
    requestHandlerMap.emplace(
        REQ_RES_PARALLELISM_ARRANGEMENT_ALL, std::make_unique<QueryParallelismArrangementHandler>());
    requestHandlerMap.emplace(
        REQ_RES_PARALLELISM_PERFORMANCE_DATA, std::make_unique<QueryParallelismPerformanceHandler>());
    requestHandlerMap.emplace(REQ_RES_QUERY_EXPERT_HOTSPOT, std::make_unique<QueryExpertHotspotHandler>());
    requestHandlerMap.emplace(REQ_RES_IMPORT_EXPERT_DATA, std::make_unique<ImportExpertDataHandler>());
    requestHandlerMap.emplace(REQ_RES_QUERY_MODEL_INFO, std::make_unique<QueryModelInfoHandler>());
    requestHandlerMap.emplace(REQ_RES_SUMMARY_SLOW_RANK_ADVISOR, std::make_unique<SummarySlowRankAdvisorHandler>());
}

void SummaryModule::OnRequest(std::unique_ptr<Protocol::Request> request) {
    BaseModule::OnRequest(std::move(request));
};
} // end of namespace Module
} // end of namespace Dic
