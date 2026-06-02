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
#include "ProtocolDefs.h"
#include "IECurveHandler.h"
#include "IECurveTableDatailHandler.h"
#include "CurveGroupHandler.h"
#include "IEModule.h"
namespace Dic::Module {
using namespace Dic::Module::IE;
IEModule::IEModule() : BaseModule() { moduleName = MODULE_IE; }

IEModule::~IEModule() { requestHandlerMap.clear(); }

void IEModule::RegisterRequestHandlers() {
    requestHandlerMap.clear();
    requestHandlerMap.emplace(REQ_RES_IE_VIEW, std::make_unique<IECurveHandler>());
    requestHandlerMap.emplace(REQ_RES_IE_TABLE_VIEW, std::make_unique<IECurveTableDatailHandler>());
    requestHandlerMap.emplace(REQ_RES_IE_DATA_GROUP, std::make_unique<CurveGroupHandler>());
}

void IEModule::OnRequest(std::unique_ptr<Protocol::Request> request) { BaseModule::OnRequest(std::move(request)); }
} // namespace Dic::Module
