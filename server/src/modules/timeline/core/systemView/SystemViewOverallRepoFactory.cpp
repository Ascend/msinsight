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
#include "SystemViewOverallDbRepo.h"
#include "SystemViewOverallTextRepo.h"
#include "SystemViewOverallRepoFactory.h"
using namespace Dic::Server;
namespace Dic::Module::Timeline {
SystemViewOverallRepoFactory::SystemViewOverallRepoFactory() {
    systemViewOverallRepoMap.emplace(DataType::TEXT, std::make_shared<SystemViewOverallTextRepo>());
    systemViewOverallRepoMap.emplace(DataType::DB, std::make_shared<SystemViewOverallDbRepo>());
}

SystemViewOverallRepoFactory::~SystemViewOverallRepoFactory() { systemViewOverallRepoMap.clear(); }

void SystemViewOverallRepoFactory::Reset() { systemViewOverallRepoMap.clear(); }

std::shared_ptr<SystemViewOverallRepoInterface> SystemViewOverallRepoFactory::GetSystemViewOverallRepo(DataType type) {
    if (systemViewOverallRepoMap.find(type) == systemViewOverallRepoMap.end()) {
        ServerLog::Error("Can't find repository for system view overall.");
        return nullptr;
    }
    return systemViewOverallRepoMap.at(type);
}
}
