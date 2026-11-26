/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
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

#ifndef PROFILER_SERVER_SLICE_READ_TABLE_H
#define PROFILER_SERVER_SLICE_READ_TABLE_H

#include "DomainObject.h"
#include "Table.h"
#include "TextTableColum.h"

namespace Dic::Module::Timeline {
// Cache reads use the same SQL/conversions as SliceTable without materializing full SlicePO rows.
class SliceDomainTable : public Table<SliceDomain> {
  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static const std::unordered_map<std::string_view, assign> assignMap = {
            {SliceColumn::ID,
                [](SliceDomain &row, const auto &result) { row.id = result->GetUint64(SliceColumn::ID); }},
            {SliceColumn::TIMESTAMP,
                [](SliceDomain &row, const auto &result) {
                    row.timestamp = result->GetUint64(SliceColumn::TIMESTAMP);
                }},
            {SliceColumn::ENDTIME,
                [](SliceDomain &row, const auto &result) { row.endTime = result->GetUint64(SliceColumn::ENDTIME); }},
            {SliceColumn::GROUPID,
                [](SliceDomain &row, const auto &result) { row.groupId = result->GetString(SliceColumn::GROUPID); }},
            {SliceColumn::DEPTH,
                [](SliceDomain &row, const auto &result) { row.depth = result->GetUint32(SliceColumn::DEPTH); }}};
        return assignMap;
    }

    const std::string &GetTableName() override {
        static const std::string tableName = "slice";
        return tableName;
    }
};

class SliceIdTable : public Table<uint64_t> {
  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static const std::unordered_map<std::string_view, assign> assignMap = {
            {SliceColumn::ID, [](uint64_t &id, const auto &result) { id = result->GetUint64(SliceColumn::ID); }}};
        return assignMap;
    }

    const std::string &GetTableName() override {
        static const std::string tableName = "slice";
        return tableName;
    }
};
}

#endif // PROFILER_SERVER_SLICE_READ_TABLE_H
