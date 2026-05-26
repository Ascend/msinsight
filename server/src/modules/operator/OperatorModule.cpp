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
#include "OperatorRequestHandler.h"
#include "QueryOpCategoryInfoHandler.h"
#include "QueryOpComputeUnitHandler.h"
#include "QueryOpStatisticInfoHandler.h"
#include "QueryOpDetailInfoHandler.h"
#include "QueryOpMoreInfoHandler.h"
#include "ExportOpDetailsHandler.h"
#include "ProtocolDefs.h"
#include "OperatorModule.h"

namespace Dic::Module {
using namespace Operator;

OperatorModule::OperatorModule() { moduleName = MODULE_OPERATOR; }

OperatorModule::~OperatorModule() { requestHandlerMap.clear(); }

void OperatorModule::RegisterRequestHandlers() {
    requestHandlerMap.clear();
    requestHandlerMap.emplace(REQ_RES_OPERATOR_CATEGORY_INFO, std::make_unique<QueryOpCategoryInfoHandler>());
    requestHandlerMap.emplace(REQ_RES_OPERATOR_COMPUTE_UNIT_INFO, std::make_unique<QueryOpComputeUnitHandler>());
    requestHandlerMap.emplace(REQ_RES_OPERATOR_STATISTIC_INFO, std::make_unique<QueryOpStatisticInfoHandler>());
    requestHandlerMap.emplace(REQ_RES_OPERATOR_DETAIL_INFO, std::make_unique<QueryOpDetailInfoHandler>());
    requestHandlerMap.emplace(REQ_RES_OPERATOR_MORE_INFO, std::make_unique<QueryOpMoreInfoHandler>());
    requestHandlerMap.emplace(REQ_RES_OPERATOR_EXPORT_DETAILS, std::make_unique<ExportOpDetailsHandler>());
}

void OperatorModule::OnRequest(std::unique_ptr<Protocol::Request> request) {
    BaseModule::OnRequest(std::move(request));
}

}
