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

#ifndef PROFILER_SERVER_KERNELDETAILTABLE_H
#define PROFILER_SERVER_KERNELDETAILTABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct KernelDetailPO {
    uint64_t id = 0;
    std::string rankId;
    std::string stepId;
    std::string name;
    std::string opType;
    std::string acceleratorCore;
    uint64_t startTime = 0;
    uint64_t duration = 0;
    uint64_t waitTime = 0;
    uint64_t blockDim = 0;
    std::string inputShapes;
    std::string inputDataTypes;
    std::string inputFormats;
    std::string outputShapes;
    std::string outputDataTypes;
    std::string outputFormats;
};

class KernelDetailTable : public Table<KernelDetailPO> {
  public:
    KernelDetailTable() = default;
    ~KernelDetailTable() override = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {KernelDetailColumn::ID, SetId},
            {KernelDetailColumn::RANK_ID, SetRankId},
            {KernelDetailColumn::STEP_ID, SetStepId},
            {KernelDetailColumn::NAME, SetName},
            {KernelDetailColumn::OP_TYPE, SetOpType},
            {KernelDetailColumn::ACCELERATOR_CORE, SetAcceleratorCore},
            {KernelDetailColumn::START_TIME, SetStartTime},
            {KernelDetailColumn::DURATION, SetDuration},
            {KernelDetailColumn::WAIT_TIME, SetWaitTime},
            {KernelDetailColumn::BLOCK_DIM, SetBlockDim},
            {KernelDetailColumn::INPUT_SHAPES, SetInputShapes},
            {KernelDetailColumn::INPUT_DATA_TYPES, SetInputDataTypes},
            {KernelDetailColumn::INPUT_FORMATS, SetInputFormats},
            {KernelDetailColumn::OUTPUT_SHAPES, SetOutPutShapes},
            {KernelDetailColumn::OUTPUT_DATA_TYPES, SetOutPutDataTypes},
            {KernelDetailColumn::OUTPUT_FORMATS, SetOutPutFormats},
        };
        return assignMap;
    }

    const std::string &GetTableName() override {
        static std::string tableName = "kernel_detail";
        return tableName;
    }
    static void SetId(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetRankId(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetStepId(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetName(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetOpType(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetAcceleratorCore(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetStartTime(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetDuration(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetWaitTime(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetBlockDim(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetInputShapes(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetInputDataTypes(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetInputFormats(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetOutPutShapes(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetOutPutDataTypes(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetOutPutFormats(KernelDetailPO &kernelDetailPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_KERNELDETAILTABLE_H
