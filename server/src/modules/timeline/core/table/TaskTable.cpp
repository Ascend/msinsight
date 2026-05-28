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
#include "TaskTable.h"
namespace Dic::Module::Timeline {
void TaskTable::IdHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.id = resultSet->GetUint64(TaskColumn::ROW_ID);
}
void TaskTable::TimestampHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.timestamp = resultSet->GetUint64(TaskColumn::TIMESTAMP);
}
void TaskTable::EndTimeHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.endTime = resultSet->GetUint64(TaskColumn::ENDTIME);
}
void TaskTable::DeviceIdHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.deviceId = resultSet->GetUint64(TaskColumn::DECICED_ID);
}
void TaskTable::ConnectionIdHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.connectionId = resultSet->GetInt64(TaskColumn::CONNECTION_ID);
}
void TaskTable::GlobalTaskIdHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.globalTaskId = resultSet->GetUint64(TaskColumn::GLOBAL_TASK_ID);
}
void TaskTable::GlobalPidHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.globalPid = resultSet->GetUint64(TaskColumn::GLOBAL_PID);
}
void TaskTable::TaskTypeHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.taskType = resultSet->GetUint64(TaskColumn::TASK_TYPE);
}
void TaskTable::ContextIdHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.contextId = resultSet->GetUint64(TaskColumn::CONTEXT_ID);
}
void TaskTable::StreamIdHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.streamId = resultSet->GetUint64(TaskColumn::STREAM_ID);
}
void TaskTable::TaskIdHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.taskId = resultSet->GetUint64(TaskColumn::TASK_ID);
}
void TaskTable::ModelIdHandle(TaskPO &taskPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    taskPO.modelId = resultSet->GetUint64(TaskColumn::MODEL_ID);
}
}
