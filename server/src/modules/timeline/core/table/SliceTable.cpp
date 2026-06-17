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
#include "SliceTable.h"
namespace Dic {
namespace Module {
namespace Timeline {
void SliceTable::IdHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet)
{
    slicePo.id = resultSet->GetUint64(SliceColumn::ID);
}
void SliceTable::TimeStampHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet)
{
    slicePo.timestamp = resultSet->GetUint64(SliceColumn::TIMESTAMP);
}
void SliceTable::EndTimeHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet)
{
    slicePo.endTime = resultSet->GetUint64(SliceColumn::ENDTIME);
}
void SliceTable::DurationHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet)
{
    slicePo.duration = resultSet->GetUint64(SliceColumn::DURATION);
}
void SliceTable::NameHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet)
{
    slicePo.name = resultSet->GetString(SliceColumn::NAME);
}
void SliceTable::CatHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet)
{
    slicePo.cat = resultSet->GetString(SliceColumn::CAT);
}
void SliceTable::ArgsHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet)
{
    slicePo.args = resultSet->GetString(SliceColumn::ARGS);
}
void SliceTable::CnameHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet)
{
    slicePo.cname = resultSet->GetString(SliceColumn::CNAME);
}
void SliceTable::TrackIdHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet)
{
    slicePo.trackId = resultSet->GetUint64(SliceColumn::TRACKID);
}
void SliceTable::FlagIdHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet)
{
    slicePo.flagId = resultSet->GetString(SliceColumn::FLAGID);
}
void SliceTable::GroupIdHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet) {
    slicePo.groupId = resultSet->GetString(SliceColumn::GROUPID);
}
}
}
}
