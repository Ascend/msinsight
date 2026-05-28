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
#include "FlowTable.h"
namespace Dic {
namespace Module {
namespace Timeline {
void FlowTable::IdHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    flowPO.id = resultSet->GetUint64(FlowColumn::ID);
}
void FlowTable::FlowIdHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    flowPO.flowId = resultSet->GetString(FlowColumn::FLOW_ID);
}
void FlowTable::NameHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    flowPO.name = resultSet->GetString(FlowColumn::NAME);
}
void FlowTable::CatHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    flowPO.cat = resultSet->GetString(FlowColumn::CAT);
}
void FlowTable::TrackIdHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    flowPO.trackId = resultSet->GetUint64(FlowColumn::TRACK_ID);
}
void FlowTable::TimeStampHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    flowPO.timestamp = resultSet->GetUint64(FlowColumn::TIMESTAMP);
}
void FlowTable::TypeHandle(FlowPO &flowPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    flowPO.type = resultSet->GetString(FlowColumn::TYPE);
}
}
}
}
