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
#include "EnumHcclRdmaTypeTable.h"
namespace Dic::Module::Timeline {
void EnumHcclRdmaTypeTable::SetId(
    EnumHcclRdmaTypePO &enumHcclRdmaTypePO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    enumHcclRdmaTypePO.id = resultSet->GetUint64(EnumHcclRdmaTypeClumn::ID);
}

void EnumHcclRdmaTypeTable::SetName(
    EnumHcclRdmaTypePO &enumHcclRdmaTypePO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    enumHcclRdmaTypePO.name = resultSet->GetString(EnumHcclRdmaTypeClumn::NAME);
}

std::unordered_map<uint64_t, std::string> EnumHcclRdmaTypeTable::QueryStrMap(
    const std::vector<uint64_t> &ids, const std::string &fileId) {
    std::unordered_map<uint64_t, std::string> res;
    std::vector<EnumHcclRdmaTypePO> enumHcclRdmaTypePO;
    Select(EnumHcclRdmaTypeClumn::ID, EnumHcclRdmaTypeClumn::NAME)
        .In(EnumHcclRdmaTypeClumn::ID, ids)
        .ExcuteQuery(fileId, enumHcclRdmaTypePO);
    for (const auto &item : enumHcclRdmaTypePO) {
        res[item.id] = item.name;
    }
    return res;
}
}
