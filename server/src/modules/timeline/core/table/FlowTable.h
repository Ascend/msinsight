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

#ifndef PROFILER_SERVER_FLOWTABLE_H
#define PROFILER_SERVER_FLOWTABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct FlowPO {
    uint64_t id = 0;
    std::string flowId;
    std::string name;
    std::string cat;
    uint64_t trackId = 0;
    uint64_t timestamp = 0;
    std::string type;
};

class FlowTable : public Table<FlowPO> {
  public:
    FlowTable() = default;
    ~FlowTable() = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {{FlowColumn::ID, IdHandle},
            {FlowColumn::FLOW_ID, FlowIdHandle}, {FlowColumn::NAME, NameHandle}, {FlowColumn::CAT, CatHandle},
            {FlowColumn::TRACK_ID, TrackIdHandle}, {FlowColumn::TIMESTAMP, TimeStampHandle},
            {FlowColumn::TYPE, TypeHandle}};

        return assignMap;
    }
    const std::string &GetTableName() override {
        static std::string tableName = "flow";
        return tableName;
    }
    static void IdHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void FlowIdHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void NameHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void CatHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void TrackIdHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void TimeStampHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void TypeHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_FLOWTABLE_H
