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
#include "OverlapAnalysisParseUnit.h"
#include "ParseUnitManager.h"
#include "ConstantDefs.h"
#include "DbTraceDataBase.h"
#include "TableDefs.h"

namespace Dic::Module::FullDb {
std::string OverlapAnalysisParseUnit::GetUnitName() const { return OVERLAP_ANALYSIS_UNIT; }

bool OverlapAnalysisParseUnit::PreCheck(
    const ParseUnitParams &params, const std::shared_ptr<DbTraceDataBase> &database, std::string &error) {
    if (!database->CheckTableExist(TABLE_OVERLAP_ANALYSIS)) {
        error = "Generate overlap analysis:Table OVERLAP_ANALYSIS is not exist.";
        return false;
    }
    return true;
}

bool OverlapAnalysisParseUnit::HandleParseProcess(
    const ParseUnitParams &params, const std::shared_ptr<DbTraceDataBase> &database, std::string &error) {
    if (!database->InitStmt()) {
        error = "Fail to init stmt when generate overlap analysis.";
        return false;
    }
    return database->GenerateOverlapAnalysis();
}

ParseUnitRegistrar<OverlapAnalysisParseUnit> unitRegOverlap(OVERLAP_ANALYSIS_UNIT);
}
