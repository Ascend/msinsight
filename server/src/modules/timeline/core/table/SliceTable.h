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

#ifndef PROFILER_SERVER_SLICETABLE_H
#define PROFILER_SERVER_SLICETABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct SlicePO {
    uint64_t id = 0;
    uint64_t timestamp = 0;
    uint64_t duration = 0;
    uint64_t endTime = 0;
    uint64_t trackId = 0;
    std::string name;
    std::string args;
    std::string cat;
    std::string flagId;
    std::string groupId;
    std::string cname;
};
class SliceTable : public Table<SlicePO> {
public:
    SliceTable() = default;
    ~SliceTable() = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {{SliceColumn::ID, IdHandle},
            {SliceColumn::TIMESTAMP, TimeStampHandle}, {SliceColumn::DURATION, DurationHandle},
            {SliceColumn::NAME, NameHandle}, {SliceColumn::TRACKID, TrackIdHandle}, {SliceColumn::CAT, CatHandle},
            {SliceColumn::ARGS, ArgsHandle}, {SliceColumn::CNAME, CnameHandle}, {SliceColumn::ENDTIME, EndTimeHandle},
            {SliceColumn::FLAGID, FlagIdHandle}, {SliceColumn::GROUPID, GroupIdHandle}};

        return assignMap;
    }
    const std::string &GetTableName() override
    {
        static std::string tableName = "slice";
        return tableName;
    }
    static void IdHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void TimeStampHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void DurationHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void NameHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void TrackIdHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void CatHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void ArgsHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void CnameHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void EndTimeHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void FlagIdHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void GroupIdHandle(SlicePO &slicePo, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}


#endif // PROFILER_SERVER_SLICETABLE_H
