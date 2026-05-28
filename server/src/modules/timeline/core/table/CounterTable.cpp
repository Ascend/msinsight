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
#include "CounterTable.h"
namespace Dic::Module::Timeline {
void CounterTable::SetId(CounterPO &counterPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    counterPO.id = resultSet->GetUint64(CounterColumn::ID);
}

void CounterTable::SetName(CounterPO &counterPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    counterPO.name = resultSet->GetString(CounterColumn::NAME);
}

void CounterTable::SetPid(CounterPO &counterPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    counterPO.pid = resultSet->GetString(CounterColumn::PID);
}

void CounterTable::SetTimestamp(CounterPO &counterPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    counterPO.timestamp = resultSet->GetUint64(CounterColumn::TIMESTAMP);
}

void CounterTable::SetCat(CounterPO &counterPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    counterPO.cat = resultSet->GetString(CounterColumn::CAT);
}

void CounterTable::SetArgs(CounterPO &counterPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    counterPO.args = resultSet->GetString(CounterColumn::ARGS);
}
}
