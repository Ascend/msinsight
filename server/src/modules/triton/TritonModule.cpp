// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026 Huawei Technologies Co.,Ltd.
//  *
//  * MindStudio is licensed under Mulan PSL v2.
//  * You can use this software according to the terms and conditions of the Mulan PSL v2.
//  * You may obtain a copy of Mulan PSL v2 at:
//  *
//  *          http://license.coscl.org.cn/MulanPSL2
//  *
//  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//  * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//  * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//  * See the Mulan PSL v2 for more details.
//  * -------------------------------------------------------------------------
//  *

#include "TritonModule.h"
#include "ProtocolDefs.h"
#include "BaseModule.h"
#include "handler/QueryTritonMemoryBlocksHandler.h"
#include "handler/QueryTritonBasicInfoHandler.h"
#include "handler/QueryTritonMemoryUsageHandler.h"

namespace Dic::Module::Triton {
void TritonModule::RegisterRequestHandlers() {
    requestHandlerMap.clear();
    requestHandlerMap.emplace(
        Protocol::REQ_RES_TRITON_MEMORY_BLOCKS, std::make_unique<QueryTritonMemoryBlocksHandler>());
    requestHandlerMap.emplace(
        Protocol::REQ_RES_TRITON_MEMORY_BASIC_INFO, std::make_unique<QueryTritonBasicInfoHandler>());
    requestHandlerMap.emplace(Protocol::REQ_RES_TRITON_MEMORY_USAGE, std::make_unique<QueryTritonMemoryUsageHandler>());
}

void TritonModule::OnRequest(std::unique_ptr<Protocol::Request> request) { BaseModule::OnRequest(std::move(request)); }
}
