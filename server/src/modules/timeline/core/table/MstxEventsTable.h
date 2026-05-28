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

#ifndef PROFILER_SERVER_MSTXEVENTSTABLE_H
#define PROFILER_SERVER_MSTXEVENTSTABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct MstxEventsPO {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    uint64_t endTime = 0;
    uint64_t eventType = 0;
    uint64_t rangeId = 0;
    uint64_t category = 0;
    uint64_t message = 0;
    uint64_t globalTid = 0;
    uint64_t endGlobalTid = 0;
    uint64_t domainId = 0;
    uint64_t connectionId = 0;
    uint64_t depth = 0;
};
class MstxEventsTable : public Table<MstxEventsPO> {
  public:
    MstxEventsTable() = default;
    ~MstxEventsTable() override = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {MstxEventsColumn::ID, SetId},
            {MstxEventsColumn::TIMESTAMP, SetTimestamp},
            {MstxEventsColumn::ENDTIME, SetEndTime},
            {MstxEventsColumn::EVENT_TYPE, SetEventType},
            {MstxEventsColumn::RANG_ID, SetRangeId},
            {MstxEventsColumn::CATEGORY, SetCategory},
            {MstxEventsColumn::MESSAGE, SetMessage},
            {MstxEventsColumn::GLOBAL_TID, SetGlobalTid},
            {MstxEventsColumn::END_GLOBAL_TID, SetEndGlobalTid},
            {MstxEventsColumn::DOMAIN_ID, SetDomainId},
            {MstxEventsColumn::CONNECTION_ID, SetConnectionId},
            {MstxEventsColumn::DEPTH, SetDepth},
        };
        return assignMap;
    }

    const std::string &GetTableName() override {
        static std::string tableName = "MSTX_EVENTS";
        return tableName;
    }
    static void SetId(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetTimestamp(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetEndTime(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetEventType(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetRangeId(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetCategory(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetMessage(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetGlobalTid(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetEndGlobalTid(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetDomainId(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetConnectionId(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetDepth(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_MSTXEVENTSTABLE_H
