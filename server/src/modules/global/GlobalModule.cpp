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
#include "GlobalModule.h"
#include "pch.h"
#include "HeartCheckHandler.h"
#include "FilesGetHandler.h"
#include "UpdateProjectExplorerInfoHandler.h"
#include "GetProjectExplorerInfoHandler.h"
#include "DeleteProjectExplorerInfoHandler.h"
#include "CheckProjectValidHandler.h"
#include "SetBaselineHandler.h"
#include "CancelBaselineHandler.h"
#include "ProtocolDefs.h"
#include "GetModuleConfigHandler.h"
#include "ClearProjectExplorerHandler.h"

namespace Dic::Module {
using namespace Dic::Module;
using namespace Dic::Server;
using namespace Dic::Module::Global;

GlobalModule::GlobalModule() : BaseModule() { moduleName = MODULE_GLOBAL; }

GlobalModule::~GlobalModule() { requestHandlerMap.clear(); }

void GlobalModule::RegisterRequestHandlers() {
    requestHandlerMap.clear();
    requestHandlerMap.emplace(REQ_RES_HEART_CHECK, std::make_unique<HeartCheckHandler>());
    requestHandlerMap.emplace(REQ_RES_FILES_GET, std::make_unique<FilesGetHandler>());
    requestHandlerMap.emplace(REQ_RES_PROJECT_EXPLORER_UPDATE, std::make_unique<UpdateProjectExplorerInfoHandler>());
    requestHandlerMap.emplace(REQ_RES_PROJECT_EXPLORER_INFO_GET, std::make_unique<GetProjectExplorerInfoHandler>());
    requestHandlerMap.emplace(
        REQ_RES_PROJECT_EXPLORER_INFO_DELETE, std::make_unique<DeleteProjectExplorerInfoHandler>());
    requestHandlerMap.emplace(REQ_RES_PROJECT_EXPLORER_CLEAR, std::make_unique<ClearProjectExplorerHandler>());
    requestHandlerMap.emplace(REQ_RES_PROJECT_VALID_CHECK, std::make_unique<CheckProjectValidHandler>());
    requestHandlerMap.emplace(REQ_RES_PROJECT_SET_BASELINE, std::make_unique<SetBaselineHandler>());
    requestHandlerMap.emplace(REQ_RES_PROJECT_CANCEL_BASELINE, std::make_unique<CancelBaselineHandler>());
    requestHandlerMap.emplace(REQ_RES_GET_MODULE_CONFIG, std::make_unique<GetModuleConfigHandler>());
}

void GlobalModule::OnRequest(std::unique_ptr<Protocol::Request> request) { BaseModule::OnRequest(std::move(request)); }
}
