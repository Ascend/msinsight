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
#include "EnumHcclLinkTypeTable.h"
namespace Dic::Module::Timeline {
void EnumHcclLinkTypeTable::SetId(
    EnumHcclLinkTypePO &enumHcclLinkTypePO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    enumHcclLinkTypePO.id = resultSet->GetUint64(EnumHcclLinkTypeClumn::ID);
}

void EnumHcclLinkTypeTable::SetName(
    EnumHcclLinkTypePO &enumHcclLinkTypePO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    enumHcclLinkTypePO.name = resultSet->GetString(EnumHcclLinkTypeClumn::NAME);
}

std::unordered_map<uint64_t, std::string> EnumHcclLinkTypeTable::QueryStrMap(
    const std::vector<uint64_t> &ids, const std::string &fileId) {
    std::unordered_map<uint64_t, std::string> res;
    std::vector<EnumHcclLinkTypePO> enumHcclLinkTypePO;
    Select(EnumHcclLinkTypeClumn::ID, EnumHcclLinkTypeClumn::NAME)
        .In(EnumHcclLinkTypeClumn::ID, ids)
        .ExcuteQuery(fileId, enumHcclLinkTypePO);
    for (const auto &item : enumHcclLinkTypePO) {
        res[item.id] = item.name;
    }
    return res;
}
}
