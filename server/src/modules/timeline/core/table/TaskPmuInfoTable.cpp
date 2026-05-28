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
#include "TaskPmuInfoTable.h"
namespace Dic::Module::Timeline {
void TaskPmuInfoTable::GlobalTaskIdHandle(
    TaskPmuInfoPO &taskPmuInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPmuInfoPO.globalTaskId = resultSet->GetUint64(TaskPmuInfoColumn::GLOBAL_TASK_ID);
}

void TaskPmuInfoTable::NameHandle(TaskPmuInfoPO &taskPmuInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPmuInfoPO.name = resultSet->GetUint64(TaskPmuInfoColumn::NAME_ID);
}

void TaskPmuInfoTable::ValueHandle(TaskPmuInfoPO &taskPmuInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPmuInfoPO.value = resultSet->GetDouble(TaskPmuInfoColumn::VALUE_ID);
}
}
