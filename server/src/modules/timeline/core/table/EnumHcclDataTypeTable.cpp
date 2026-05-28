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
#include "EnumHcclDataTypeTable.h"
namespace Dic::Module::Timeline {
void EnumHcclDataTypeTable::SetId(
    EnumHcclDataTypePO &enumHcclDataTypePO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    enumHcclDataTypePO.id = resultSet->GetUint64(EnumHcclDataTypeClumn::ID);
}

void EnumHcclDataTypeTable::SetName(
    EnumHcclDataTypePO &enumHcclDataTypePO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    enumHcclDataTypePO.name = resultSet->GetString(EnumHcclDataTypeClumn::NAME);
}

std::unordered_map<uint64_t, std::string> EnumHcclDataTypeTable::QueryStrMap(
    const std::vector<uint64_t> &ids, const std::string &fileId) {
    std::unordered_map<uint64_t, std::string> res;
    std::vector<EnumHcclDataTypePO> enumHcclDataTypePO;
    Select(EnumHcclDataTypeClumn::ID, EnumHcclDataTypeClumn::NAME)
        .In(EnumHcclDataTypeClumn::ID, ids)
        .ExcuteQuery(fileId, enumHcclDataTypePO);
    for (const auto &item : enumHcclDataTypePO) {
        res[item.id] = item.name;
    }
    return res;
}
}
