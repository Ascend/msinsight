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

#include "SourceModule.h"
#include "ProtocolDefs.h"
#include "QueryCodeFileHandler.h"
#include "QueryApiLineHandler.h"
#include "QueryApiLineDynamicHandler.h"
#include "QueryApiInstructionsHandler.h"
#include "QueryApiInstructionsDynamicHandler.h"
#include "QueryDetailsBaseInfoHandler.h"
#include "QueryDetailsLoadInfoHandler.h"
#include "QueryDetailsMemoryGraphHandler.h"
#include "QueryDetailsMemoryTableHandler.h"
#include "QueryInterCoreLoadAnalysisGraphHandler.h"
#include "QueryDetailsRooflineHandler.h"
#include "QueryCachelineRecordHandler.h"

namespace Dic::Module {
using namespace Dic::Module::Source;
SourceModule::SourceModule() : BaseModule() { moduleName = MODULE_SOURCE; }

SourceModule::~SourceModule() { requestHandlerMap.clear(); }

void SourceModule::RegisterRequestHandlers() {
    requestHandlerMap.clear();
    requestHandlerMap.emplace(REQ_RES_SOURCE_CODE_FILE, std::make_unique<QueryCodeFileHandler>());
    requestHandlerMap.emplace(REQ_RES_SOURCE_API_LINE, std::make_unique<QueryApiLineHandler>());
    requestHandlerMap.emplace(REQ_RES_SOURCE_API_LINE_DYNAMIC, std::make_unique<QueryApiLineDynamicHandler>());
    requestHandlerMap.emplace(REQ_RES_SOURCE_API_INSTRUCTIONS, std::make_unique<QueryApiInstructionsHandler>());
    requestHandlerMap.emplace(
        REQ_RES_SOURCE_API_INSTRUCTIONS_DYNAMIC, std::make_unique<QueryApiInstructionsDynamicHandler>());
    requestHandlerMap.emplace(REQ_RES_DETAILS_BASE_INFO, std::make_unique<QueryDetailsBaseInfoHandler>());
    requestHandlerMap.emplace(REQ_RES_DETAILS_COMPUTE_LOAD_INFO, std::make_unique<QueryDetailsLoadInfoHandler>());
    requestHandlerMap.emplace(REQ_RES_DETAILS_COMPUTE_MEMORY_GRAPH, std::make_unique<QueryDetailsMemoryGraphHandler>());
    requestHandlerMap.emplace(REQ_RES_DETAILS_COMPUTE_MEMORY_TABLE, std::make_unique<QueryDetailsMemoryTableHandler>());
    requestHandlerMap.emplace(
        REQ_RES_DETAILS_INTER_CORE_LOAD_GRAPH, std::make_unique<QueryInterCoreLoadAnalysisGraphHandler>());
    requestHandlerMap.emplace(std::string(REQ_RES_DETAILS_ROOFLINE), std::make_unique<QueryDetailsRooflineHandler>());
    requestHandlerMap.emplace(REQ_RES_CACHELINE_RECORD, std::make_unique<QueryCachelineRecordHandler>());
}

void SourceModule::OnRequest(std::unique_ptr<Protocol::Request> request) { BaseModule::OnRequest(std::move(request)); }
} // end of namespace Module
