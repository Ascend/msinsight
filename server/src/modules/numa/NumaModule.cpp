/*
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * MindStudio is licensed under Mulan PSL v2.
 */
#include "NumaModule.h"

#include "QueryNumaOverviewHandler.h"

namespace Dic::Module::Numa {
void NumaModule::RegisterRequestHandlers() {
    requestHandlerMap.clear();
    requestHandlerMap.emplace(Protocol::REQ_RES_NUMA_OVERVIEW, std::make_unique<QueryNumaOverviewHandler>());
}

void NumaModule::OnRequest(std::unique_ptr<Protocol::Request> request) { BaseModule::OnRequest(std::move(request)); }
} // namespace Dic::Module::Numa
