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

#ifndef PROFILER_SERVER_ENUMHCCLRDMATYPETABLE_H
#define PROFILER_SERVER_ENUMHCCLRDMATYPETABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct EnumHcclRdmaTypePO {
    uint64_t id = 0;
    std::string name;
};
class EnumHcclRdmaTypeTable : public Table<EnumHcclRdmaTypePO> {
  public:
    EnumHcclRdmaTypeTable() = default;
    ~EnumHcclRdmaTypeTable() override = default;
    std::unordered_map<uint64_t, std::string> QueryStrMap(const std::vector<uint64_t> &ids, const std::string &fileId);

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {EnumHcclRdmaTypeClumn::ID, SetId},
            {EnumHcclRdmaTypeClumn::NAME, SetName},
        };
        return assignMap;
    }

    const std::string &GetTableName() override {
        static std::string tableName = "ENUM_HCCL_RDMA_TYPE";
        return tableName;
    }
    static void SetId(EnumHcclRdmaTypePO &enumHcclRdmaTypePO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void SetName(EnumHcclRdmaTypePO &enumHcclRdmaTypePO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_ENUMHCCLRDMATYPETABLE_H
