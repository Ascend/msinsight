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
#include "StringIdsTable.h"
namespace Dic::Module::Timeline {
void StringIdsTable::SetId(StringIdsPO &stringIdsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    stringIdsPO.id = resultSet->GetUint64(StringIdsColumn::ID);
}

void StringIdsTable::SetValue(StringIdsPO &stringIdsPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    stringIdsPO.value = resultSet->GetString(StringIdsColumn::VALUE);
}

std::unordered_map<uint64_t, std::string> StringIdsTable::QueryStrMap(
    const std::vector<uint64_t> &ids, const std::string &fileId) {
    std::unordered_map<uint64_t, std::string> res;
    std::vector<StringIdsPO> stringIdsPOs;
    Select(StringIdsColumn::ID, StringIdsColumn::VALUE).In(StringIdsColumn::ID, ids).ExcuteQuery(fileId, stringIdsPOs);
    for (const auto &item : stringIdsPOs) {
        res[item.id] = item.value;
    }
    return res;
}

std::unordered_map<uint64_t, std::string> StringIdsTable::QueryStrMapByValues(
    const std::vector<std::string> &values, const std::string &fileId) {
    std::unordered_map<uint64_t, std::string> res;
    std::vector<StringIdsPO> stringIdsPOs;
    Select(StringIdsColumn::ID, StringIdsColumn::VALUE)
        .In(StringIdsColumn::VALUE, values)
        .ExcuteQuery(fileId, stringIdsPOs);
    for (const auto &item : stringIdsPOs) {
        res[item.id] = item.value;
    }
    return res;
}
}
