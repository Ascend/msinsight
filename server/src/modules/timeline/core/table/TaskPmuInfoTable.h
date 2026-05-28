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

#ifndef PROFILER_SERVER_TASKPMUINFOTABLE_H
#define PROFILER_SERVER_TASKPMUINFOTABLE_H
#include "Table.h"
#include "TextTableColum.h"
namespace Dic::Module::Timeline {
struct TaskPmuInfoPO {
    uint64_t globalTaskId;
    uint64_t name;
    double value;
};

class TaskPmuInfoTable : public Table<TaskPmuInfoPO> {
  public:
    TaskPmuInfoTable() = default;
    virtual ~TaskPmuInfoTable() = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {
            {TaskPmuInfoColumn::GLOBAL_TASK_ID, GlobalTaskIdHandle}, {TaskPmuInfoColumn::NAME_ID, NameHandle},
            {TaskPmuInfoColumn::VALUE_ID, ValueHandle}};
        return assignMap;
    }

    const std::string &GetTableName() override {
        static std::string tableName = "TASK_PMU_INFO";
        return tableName;
    }

    static void GlobalTaskIdHandle(TaskPmuInfoPO &taskPmuInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void NameHandle(TaskPmuInfoPO &taskPmuInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void ValueHandle(TaskPmuInfoPO &taskPmuInfoPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_TASKPMUINFOTABLE_H
