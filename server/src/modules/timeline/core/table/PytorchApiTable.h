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

#ifndef PROFILER_SERVER_PYROECHAPITABLE_H
#define PROFILER_SERVER_PYROECHAPITABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct PytorchApiPO {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    uint64_t endTime = 0;
    uint64_t globalTid = 0;
    uint64_t connectionId = 0;
    uint64_t name = 0;
    std::string sequenceNumber;
    std::string fwdThreadId;
    std::string inputDtypes;
    std::string inputShapes;
    std::string callchainId;
    uint64_t type = 0;
};
class PytorchApiTable : public Table<PytorchApiPO> {
  public:
    PytorchApiTable() = default;
    ~PytorchApiTable() override = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {PytorchApiColumn::ID, SetId},
            {PytorchApiColumn::TIMESTAMP, SetTimestamp},
            {PytorchApiColumn::ENDTIME, SetEndTime},
            {PytorchApiColumn::GLOBAL_TID, SetGlobalTid},
            {PytorchApiColumn::CONNECTIONID, SetConnectionId},
            {PytorchApiColumn::NAME, SetName},
            {PytorchApiColumn::SEQUENCE_NUMBER, SetSequenceNumber},
            {PytorchApiColumn::FWD_THREAD_ID, SetFwdThreadId},
            {PytorchApiColumn::INPUT_DTYPES, SetInputDtypes},
            {PytorchApiColumn::INPUT_SHAPES, SetInputShapes},
            {PytorchApiColumn::CALL_CHAIN_ID, SetCallchainId},
            {PytorchApiColumn::TYPE, SetType},
        };
        return assignMap;
    }

    const std::string &GetTableName() override {
        static std::string tableName = "PYTORCH_API";
        return tableName;
    }
    static void SetId(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetTimestamp(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetEndTime(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetGlobalTid(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetConnectionId(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetName(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetSequenceNumber(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetFwdThreadId(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetInputDtypes(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetInputShapes(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetCallchainId(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetType(PytorchApiPO &pytorchApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_PYROECHAPITABLE_H
