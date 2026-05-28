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
#include "CommucationTaskInfoTable.h"
namespace Dic::Module::Timeline {
void CommucationTaskInfoTable::IdHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.id = resultSet->GetUint64(CommucationTaskInfoColumn::ROW_ID);
}
void CommucationTaskInfoTable::NameHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.name = resultSet->GetUint64(CommucationTaskInfoColumn::NAME);
}
void CommucationTaskInfoTable::GlobalTaskIdHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.globalTaskId = resultSet->GetUint64(CommucationTaskInfoColumn::GLOBAL_TASK_ID);
}
void CommucationTaskInfoTable::TaskTypeHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.taskType = resultSet->GetUint64(CommucationTaskInfoColumn::TASK_TYPE);
}
void CommucationTaskInfoTable::PlaneIdHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.planeId = resultSet->GetUint64(CommucationTaskInfoColumn::PLANE_ID);
}
void CommucationTaskInfoTable::GroupNameHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.groupName = resultSet->GetUint64(CommucationTaskInfoColumn::GROUPNAME);
}
void CommucationTaskInfoTable::NotifyIdHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.notifyId = resultSet->GetUint64(CommucationTaskInfoColumn::NOTIFY_ID);
}
void CommucationTaskInfoTable::RdmaTypeHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.rdmaType = resultSet->GetUint64(CommucationTaskInfoColumn::RDMA_TYPE);
}
void CommucationTaskInfoTable::SrcRankHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.srcRank = resultSet->GetUint64(CommucationTaskInfoColumn::SRC_RANK);
}
void CommucationTaskInfoTable::DstRankHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.dstRank = resultSet->GetUint64(CommucationTaskInfoColumn::DST_RANK);
}
void CommucationTaskInfoTable::TransportTypeHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.transportType = resultSet->GetUint64(CommucationTaskInfoColumn::TRANSPORT_TYPE);
}
void CommucationTaskInfoTable::SizeHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.size = resultSet->GetUint64(CommucationTaskInfoColumn::SIZE);
}
void CommucationTaskInfoTable::DataTypeHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.dataType = resultSet->GetUint64(CommucationTaskInfoColumn::DATA_TYPE);
}
void CommucationTaskInfoTable::LinkTypeHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.linkType = resultSet->GetUint64(CommucationTaskInfoColumn::LINK_TYPE);
}
void CommucationTaskInfoTable::OpIdHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.opId = resultSet->GetUint64(CommucationTaskInfoColumn::OP_ID);
}

void CommucationTaskInfoTable::BandwidthHandle(
    CommucationTaskInfoPO &commucationTaskInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    commucationTaskInfoPO.bandwidth = resultSet->GetDouble(CommucationTaskInfoColumn::BANDWIDTH);
}
}
