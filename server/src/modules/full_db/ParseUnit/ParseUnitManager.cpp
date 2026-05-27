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
#include "ParseUnitManager.h"
#include "ConstantDefs.h"
#include "ServerLog.h"
namespace Dic::Module::FullDb {
ParseUnitManager &ParseUnitManager::Instance() {
    static ParseUnitManager instance;
    return instance;
}

void ParseUnitManager::RegisterUnit(const std::string &name, Creator unit) { unitMap[name] = unit; }

void ParseUnitManager::ExecuteUnitList(const ParseUnitParams &params, const std::vector<std::string> &unitNameList) {
    for (const auto &unit_name : unitNameList) {
        auto it = unitMap.find(unit_name);
        if (it != unitMap.end()) {
            Server::ServerLog::Info("Start execute parse unit, unit name:", it->first);
            bool res = it->second()->Handle(params);
            Server::ServerLog::Info("End execute parse unit, unit name:", it->first, ", result:", res);
        }
    }
}
}
