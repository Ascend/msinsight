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

#include "KernelDetailTable.h"
namespace Dic::Module::Timeline {
void KernelDetailTable::SetId(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.id = resultSet->GetUint64(KernelDetailColumn::ID);
}

void KernelDetailTable::SetRankId(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.rankId = resultSet->GetString(KernelDetailColumn::RANK_ID);
}

void KernelDetailTable::SetStepId(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.stepId = resultSet->GetString(KernelDetailColumn::STEP_ID);
}

void KernelDetailTable::SetName(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.name = resultSet->GetString(KernelDetailColumn::NAME);
}

void KernelDetailTable::SetOpType(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.opType = resultSet->GetString(KernelDetailColumn::OP_TYPE);
}

void KernelDetailTable::SetAcceleratorCore(
    KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.acceleratorCore = resultSet->GetString(KernelDetailColumn::ACCELERATOR_CORE);
}

void KernelDetailTable::SetStartTime(
    KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.startTime = resultSet->GetUint64(KernelDetailColumn::START_TIME);
}

void KernelDetailTable::SetDuration(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.duration = resultSet->GetUint64(KernelDetailColumn::DURATION);
}

void KernelDetailTable::SetWaitTime(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.waitTime = resultSet->GetUint64(KernelDetailColumn::WAIT_TIME);
}

void KernelDetailTable::SetBlockDim(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.blockDim = resultSet->GetUint64(KernelDetailColumn::BLOCK_DIM);
}

void KernelDetailTable::SetInputShapes(
    KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.inputShapes = resultSet->GetString(KernelDetailColumn::INPUT_SHAPES);
}

void KernelDetailTable::SetInputDataTypes(
    KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.inputDataTypes = resultSet->GetString(KernelDetailColumn::INPUT_DATA_TYPES);
}

void KernelDetailTable::SetInputFormats(
    KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.inputFormats = resultSet->GetString(KernelDetailColumn::INPUT_FORMATS);
}

void KernelDetailTable::SetOutPutShapes(
    KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.outputShapes = resultSet->GetString(KernelDetailColumn::OUTPUT_SHAPES);
}

void KernelDetailTable::SetOutPutDataTypes(
    KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.outputDataTypes = resultSet->GetString(KernelDetailColumn::OUTPUT_DATA_TYPES);
}

void KernelDetailTable::SetOutPutFormats(
    KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    kernelDetailPO.outputFormats = resultSet->GetString(KernelDetailColumn::OUTPUT_FORMATS);
}
}
