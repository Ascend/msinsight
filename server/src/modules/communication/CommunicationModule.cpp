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

#include "CommunicationModule.h"
#include "CommunicationOperatorDetailsHandler.h"
#include "CommunicationOperatorListsHandler.h"
#include "BandwidthHandler.h"
#include "DistributionHandler.h"
#include "MatrixSortOpNamesHandler.h"
#include "IterationsHandler.h"
#include "DurationListHandler.h"
#include "OperatorNamesHandler.h"
#include "GroupHandler.h"
#include "MatrixListHandler.h"
#include "CommunicationAdvisorHandler.h"
#include "CommunicationSlowRankAnalysisHandler.h"
#include "ProtocolDefs.h"

namespace Dic {
namespace Module {
using namespace Communication;
CommunicationModule::CommunicationModule() : BaseModule() { moduleName = MODULE_COMMUNICATION; }

CommunicationModule::~CommunicationModule() { requestHandlerMap.clear(); }

void CommunicationModule::RegisterRequestHandlers() {
    requestHandlerMap.clear();
    requestHandlerMap.emplace(
        REQ_RES_COMMUNICATION_OPERATOR_DETAILS, std::make_unique<CommunicationOperatorDetailsHandler>());
    requestHandlerMap.emplace(REQ_RES_COMMUNICATION_BANDWIDTH, std::make_unique<BandwidthHandler>());
    requestHandlerMap.emplace(REQ_RES_COMMUNICATION_DISTRIBUTION, std::make_unique<DistributionHandler>());
    requestHandlerMap.emplace(REQ_RES_COMMUNICATION_ITERATIONS, std::make_unique<IterationsHandler>());
    requestHandlerMap.emplace(REQ_RES_COMMUNICATION_OPERATORNAMES, std::make_unique<OperatorNamesHandler>());
    requestHandlerMap.emplace(REQ_RES_COMMUNICATION_SORT_OP, std::make_unique<MatrixSortOpNamesHandler>());
    requestHandlerMap.emplace(REQ_RES_COMMUNICATION_LIST, std::make_unique<DurationListHandler>());
    requestHandlerMap.emplace(REQ_RES_COMMUNICATION_MATRIX_GROUP, std::make_unique<GroupHandler>());
    requestHandlerMap.emplace(REQ_RES_COMMUNICATION_MATRIX_BANDWIDTH, std::make_unique<MatrixListHandler>());
    requestHandlerMap.emplace(
        REQ_RES_COMMUNICATION_OPERATOR_LISTS, std::make_unique<CommunicationOperatorListsHandler>());
    requestHandlerMap.emplace(REQ_RES_COMMUNICATION_ADVISOR, std::make_unique<CommunicationAdvisorHandler>());
    requestHandlerMap.emplace(
        REQ_RES_COMMUNICATION_DURATION_SLOW_RANK_LIST, std::make_unique<CommunicationSlowRankAnalysisHandler>());
}

void CommunicationModule::OnRequest(std::unique_ptr<Protocol::Request> request) {
    BaseModule::OnRequest(std::move(request));
};
} // end of namespace Module
} // end of namespace Dic
