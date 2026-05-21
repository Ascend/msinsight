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
#include "QueryMemScopePythonTraceHandler.h"
#include "QueryMemScopeMemoryDetailHandler.h"
#include "QueryMemScopeBlockHandler.h"
#include "QueryMemScopeAllocationHandler.h"
#include "QueryMemScopeEventHandler.h"
#include "QueryMemSnapshotBlockHandler.h"
#include "QueryMemSnapshotAllocationHandler.h"
#include "QueryMemSnapshotEventHandler.h"
#include "QueryMemSnapshotDetailHandler.h"
#include "QueryMemSnapshotStateHandler.h"
#include "QueryMemSnapshotLeakStatsHandler.h"
#include "MemScopeModule.h"

using namespace Dic::Module::MemScope;

namespace Dic::Module {
MemScopeModule::MemScopeModule() { moduleName = MODULE_MEM_SCOPE; }

MemScopeModule::~MemScopeModule() { requestHandlerMap.clear(); }

void MemScopeModule::RegisterRequestHandlers() {
    requestHandlerMap.clear();
    requestHandlerMap.emplace(REQ_RES_MEM_SCOPE_MEMORY_ALLOCATIONS, std::make_unique<QueryMemScopeAllocationHandler>());
    requestHandlerMap.emplace(REQ_RES_MEM_SCOPE_MEMORY_BLOCKS, std::make_unique<QueryMemScopeBlockHandler>());
    requestHandlerMap.emplace(REQ_RES_MEM_SCOPE_MEMORY_DETAILS, std::make_unique<QueryMemScopeMemoryDetailHandler>());
    requestHandlerMap.emplace(REQ_RES_MEM_SCOPE_PYTHON_TRACES, std::make_unique<QueryMemScopePythonTraceHandler>());
    requestHandlerMap.emplace(REQ_RES_MEM_SCOPE_EVENTS, std::make_unique<QueryMemScopeEventHandler>());
    requestHandlerMap.emplace(REQ_RES_MEM_SNAPSHOT_BLOCKS, std::make_unique<QueryMemSnapshotBlockHandler>());
    requestHandlerMap.emplace(REQ_RES_MEM_SNAPSHOT_ALLOCATIONS, std::make_unique<QueryMemSnapshotAllocationHandler>());
    requestHandlerMap.emplace(REQ_RES_MEM_SNAPSHOT_EVENTS, std::make_unique<QueryMemSnapshotEventHandler>());
    requestHandlerMap.emplace(REQ_RES_MEM_SNAPSHOT_DETAIL, std::make_unique<QueryMemSnapshotDetailHandler>());
    requestHandlerMap.emplace(REQ_RES_MEM_SNAPSHOT_STATE, std::make_unique<QueryMemSnapshotStateHandler>());
    requestHandlerMap.emplace(REQ_RES_MEM_SNAPSHOT_LEAK_STATS, std::make_unique<QueryMemSnapshotLeakStatsHandler>());
}

void MemScopeModule::OnRequest(std::unique_ptr<Protocol::Request> request) {
    BaseModule::OnRequest(std::move(request));
}
} // end of namespace Dic::Module
