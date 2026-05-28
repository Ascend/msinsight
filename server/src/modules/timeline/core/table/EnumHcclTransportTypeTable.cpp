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
#include "EnumHcclTransportTypeTable.h"
namespace Dic::Module::Timeline {
void EnumHcclTransportTypeTable::SetId(
    EnumHcclTransportTypePO &enumHcclTransportTypePO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    enumHcclTransportTypePO.id = resultSet->GetUint64(EnumHcclTransportTypeClumn::ID);
}

void EnumHcclTransportTypeTable::SetName(
    EnumHcclTransportTypePO &enumHcclTransportTypePO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    enumHcclTransportTypePO.name = resultSet->GetString(EnumHcclTransportTypeClumn::NAME);
}

std::unordered_map<uint64_t, std::string> EnumHcclTransportTypeTable::QueryStrMap(
    const std::vector<uint64_t> &ids, const std::string &fileId) {
    std::unordered_map<uint64_t, std::string> res;
    std::vector<EnumHcclTransportTypePO> enumHcclTransportTypePO;
    Select(EnumHcclTransportTypeClumn::ID, EnumHcclTransportTypeClumn::NAME)
        .In(EnumHcclTransportTypeClumn::ID, ids)
        .ExcuteQuery(fileId, enumHcclTransportTypePO);
    for (const auto &item : enumHcclTransportTypePO) {
        res[item.id] = item.name;
    }
    return res;
}
}
