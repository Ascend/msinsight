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

#ifndef PROFILER_SERVER_COMPUTETASKINFOTABLE_H
#define PROFILER_SERVER_COMPUTETASKINFOTABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct ComputeTaskInfoPO {
    uint64_t name = 0;
    uint64_t globalTaskId = 0;
    uint64_t blockDim = 0;
    uint64_t mixBlockDim = 0;
    uint64_t taskType = 0;
    uint64_t opType = 0;
    uint64_t inputFormats = 0;
    uint64_t inputDataTypes = 0;
    uint64_t inputShapes = 0;
    uint64_t outputFormats = 0;
    uint64_t outputDataTypes = 0;
    uint64_t outputShapes = 0;
    uint64_t attrInfo = 0;
    uint64_t waitNs = 0;
};
class ComputeTaskInfoTable : public Table<ComputeTaskInfoPO> {
  public:
    ComputeTaskInfoTable() = default;
    ~ComputeTaskInfoTable() override = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {ComputeTaskInfoColumn::NAME, SetName},
            {ComputeTaskInfoColumn::GLOBAL_TASK_ID, SetGlobalTaskId},
            {ComputeTaskInfoColumn::BLOCK_DIM, SetBlockDim},
            {ComputeTaskInfoColumn::MIX_BLOCK_DIM, SetMixBlockDim},
            {ComputeTaskInfoColumn::TASK_TYPE, SetTaskType},
            {ComputeTaskInfoColumn::OP_TYPE, SetOpType},
            {ComputeTaskInfoColumn::INPUT_FORMATS, SetInputFormats},
            {ComputeTaskInfoColumn::INPUT_DATA_TYPES, SetInputDataTypes},
            {ComputeTaskInfoColumn::INPUT_SHAPES, SetInputShapes},
            {ComputeTaskInfoColumn::OUTOUT_FORMATS, SetOutputFormats},
            {ComputeTaskInfoColumn::OUTPUT_DATA_TYPES, SetOutputDataTypes},
            {ComputeTaskInfoColumn::OUTPUT_SHAPES, SetOutputShapes},
            {ComputeTaskInfoColumn::ATTRINFO, SetAttrInfo},
            {ComputeTaskInfoColumn::WAIT_NS, SetWaitNs},
        };

        return assignMap;
    }
    const std::string &GetTableName() override {
        static std::string tableName = "COMPUTE_TASK_INFO";
        return tableName;
    }
    static void SetName(ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetGlobalTaskId(
        ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetBlockDim(ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetMixBlockDim(ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetTaskType(ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetOpType(ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetInputFormats(
        ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetInputDataTypes(
        ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetInputShapes(ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetOutputFormats(
        ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetOutputDataTypes(
        ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetOutputShapes(
        ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetAttrInfo(ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetWaitNs(ComputeTaskInfoPO &computeTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_COMPUTETASKINFOTABLE_H
