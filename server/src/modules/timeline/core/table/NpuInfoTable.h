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

#ifndef PROFILER_SERVER_NPUINFOTABLE_H
#define PROFILER_SERVER_NPUINFOTABLE_H
#include "string"
#include "Table.h"
#include "TextTableColum.h"

namespace Dic::Module::Timeline {
struct NpuInfoPo {
    std::string name;
    uint64_t id = 0;
};

class NpuInfoTable : public Table<NpuInfoPo> {
  public:
    NpuInfoTable() = default;
    ~NpuInfoTable() override = default;

  private:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {NpuInfoColumn::ID, SetId},
            {NpuInfoColumn::NAME, SetName},
        };
        return assignMap;
    }

    const std::string &GetTableName() override {
        static std::string tableName = "NPU_INFO";
        return tableName;
    }
    static void SetId(NpuInfoPo &npuInfoPo, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetName(NpuInfoPo &npuInfoPo, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_NPUINFOTABLE_H
