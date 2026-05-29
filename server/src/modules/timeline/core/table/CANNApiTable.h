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

#ifndef PROFILER_SERVER_CANNAPITABLE_H
#define PROFILER_SERVER_CANNAPITABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct CANNApiPO {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    uint64_t endTime = 0;
    uint64_t type = 0;
    uint64_t globalTid = 0;
    uint64_t name = 0;
    uint64_t depth = 0;
};
class CANNApiTable : public Table<CANNApiPO> {
  public:
    CANNApiTable() = default;
    virtual ~CANNApiTable() = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {{CANNApiColumn::ID, SetId},
            {CANNApiColumn::TIMESTAMP, SetTimestamp}, {CANNApiColumn::ENDTIME, SetEndTime},
            {CANNApiColumn::TYPE, SetType}, {CANNApiColumn::GLOBAL_TID, SetGlobalTid}, {CANNApiColumn::NAME, SetName},
            {CANNApiColumn::DEPTH, SetDepth}};

        return assignMap;
    }
    const std::string &GetTableName() override {
        static std::string tableName = "CANN_API";
        return tableName;
    }
    static void SetId(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetTimestamp(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetEndTime(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetType(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetGlobalTid(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetName(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetDepth(CANNApiPO &cannApiPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_CANNAPITABLE_H
