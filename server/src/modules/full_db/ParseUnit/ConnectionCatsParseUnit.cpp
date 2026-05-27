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

#include "ConnectionCatsParseUnit.h"
#include "ConstantDefs.h"
#include "DbTraceDataBase.h"
#include "ParseUnitManager.h"
#include "TableDefs.h"

namespace Dic::Module::FullDb {
std::string ConnectionCatsParseUnit::GetUnitName() const { return CONNECTION_UNIT; }

bool ConnectionCatsParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<DbTraceDataBase> &database, std::string &error) {
    return true;
}

bool ConnectionCatsParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<DbTraceDataBase> &database, std::string &error) {
    bool res = database->InitConnectionCats();
    if (!res) {
        error = "Fail to init connection category.";
    }
    return res;
}

ParseUnitRegistrar<ConnectionCatsParseUnit> unitRegConnection(CONNECTION_UNIT);
}
