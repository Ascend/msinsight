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
#include "ComputeTaskInfoTable.h"
namespace Dic::Module::Timeline {
void ComputeTaskInfoTable::SetName(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.name = resultSet->GetUint64(ComputeTaskInfoColumn::NAME);
}

void ComputeTaskInfoTable::SetGlobalTaskId(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.globalTaskId = resultSet->GetUint64(ComputeTaskInfoColumn::GLOBAL_TASK_ID);
}

void ComputeTaskInfoTable::SetBlockNum(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.blockNum = resultSet->GetUint64(ComputeTaskInfoColumn::BLOCK_NUM);
}

void ComputeTaskInfoTable::SetMixBlockDim(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.mixBlockDim = resultSet->GetUint64(ComputeTaskInfoColumn::MIX_BLOCK_DIM);
}

void ComputeTaskInfoTable::SetGridDim(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.gridDim = resultSet->GetUint64(ComputeTaskInfoColumn::GRID_DIM);
}

void ComputeTaskInfoTable::SetBlockDim(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.blockDim = resultSet->GetUint64(ComputeTaskInfoColumn::BLOCK_DIM);
}

void ComputeTaskInfoTable::SetTaskType(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.taskType = resultSet->GetUint64(ComputeTaskInfoColumn::TASK_TYPE);
}

void ComputeTaskInfoTable::SetOpType(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.opType = resultSet->GetUint64(ComputeTaskInfoColumn::OP_TYPE);
}

void ComputeTaskInfoTable::SetInputFormats(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.inputFormats = resultSet->GetUint64(ComputeTaskInfoColumn::INPUT_FORMATS);
}

void ComputeTaskInfoTable::SetInputDataTypes(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.inputDataTypes = resultSet->GetUint64(ComputeTaskInfoColumn::INPUT_DATA_TYPES);
}

void ComputeTaskInfoTable::SetInputShapes(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.inputShapes = resultSet->GetUint64(ComputeTaskInfoColumn::INPUT_SHAPES);
}

void ComputeTaskInfoTable::SetOutputFormats(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.outputFormats = resultSet->GetUint64(ComputeTaskInfoColumn::OUTOUT_FORMATS);
}

void ComputeTaskInfoTable::SetOutputDataTypes(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.outputDataTypes = resultSet->GetUint64(ComputeTaskInfoColumn::OUTPUT_DATA_TYPES);
}

void ComputeTaskInfoTable::SetOutputShapes(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.outputShapes = resultSet->GetUint64(ComputeTaskInfoColumn::OUTPUT_SHAPES);
}

void ComputeTaskInfoTable::SetAttrInfo(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.attrInfo = resultSet->GetUint64(ComputeTaskInfoColumn::ATTRINFO);
}

void ComputeTaskInfoTable::SetWaitNs(
    ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    computeTaskInfoPO.waitNs = resultSet->GetUint64(ComputeTaskInfoColumn::WAIT_NS);
}
}
