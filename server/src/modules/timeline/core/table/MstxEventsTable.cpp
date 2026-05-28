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

#include "MstxEventsTable.h"
namespace Dic::Module::Timeline {
void MstxEventsTable::SetId(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.id = resultSet->GetUint64(MstxEventsColumn::ID);
}

void MstxEventsTable::SetTimestamp(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.timestamp = resultSet->GetUint64(MstxEventsColumn::TIMESTAMP);
}

void MstxEventsTable::SetEndTime(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.endTime = resultSet->GetUint64(MstxEventsColumn::ENDTIME);
}

void MstxEventsTable::SetEventType(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.eventType = resultSet->GetUint64(MstxEventsColumn::EVENT_TYPE);
}

void MstxEventsTable::SetRangeId(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.rangeId = resultSet->GetUint64(MstxEventsColumn::RANG_ID);
}

void MstxEventsTable::SetCategory(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.category = resultSet->GetUint64(MstxEventsColumn::CATEGORY);
}

void MstxEventsTable::SetMessage(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.message = resultSet->GetUint64(MstxEventsColumn::MESSAGE);
}

void MstxEventsTable::SetGlobalTid(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.globalTid = resultSet->GetUint64(MstxEventsColumn::GLOBAL_TID);
}

void MstxEventsTable::SetEndGlobalTid(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.endGlobalTid = resultSet->GetUint64(MstxEventsColumn::END_GLOBAL_TID);
}

void MstxEventsTable::SetConnectionId(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.connectionId = resultSet->GetUint64(MstxEventsColumn::CONNECTION_ID);
}

void MstxEventsTable::SetDomainId(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.domainId = resultSet->GetUint64(MstxEventsColumn::DOMAIN_ID);
}

void MstxEventsTable::SetDepth(MstxEventsPO &mstxEventsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    mstxEventsPO.depth = resultSet->GetUint64(MstxEventsColumn::DEPTH);
}
}
