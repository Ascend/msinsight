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

#ifndef PROFILER_SERVER_THREADTABLE_H
#define PROFILER_SERVER_THREADTABLE_H
#include "TextTableColum.h"
#include "Table.h"
namespace Dic::Module::Timeline {
struct ThreadPO {
    uint64_t trackId = 0;
    std::string tid;
    std::string pid;
    std::string threadName;
    uint64_t threadSortIndex = 0;
};
class ThreadTable : public Table<ThreadPO> {
  public:
    ThreadTable() = default;
    ~ThreadTable() = default;

  protected:
    const std::unordered_map<std::string_view, assign> &GetAssignMap() override {
        static std::unordered_map<std::string_view, assign> assignMap = {{ThreadColumn::TRACK_ID, TrackIdHandle},
            {ThreadColumn::TID, TidHandle}, {ThreadColumn::PID, PidHandle},
            {ThreadColumn::THREAD_NAME, ThreadNameHandle}, {ThreadColumn::THREAD_SORT_INDEX, ThreadSortIndexHandle}};

        return assignMap;
    }
    const std::string &GetTableName() override {
        static std::string tableName = "thread";
        return tableName;
    }
    static void TrackIdHandle(ThreadPO &threadPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void TidHandle(ThreadPO &threadPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void PidHandle(ThreadPO &threadPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void ThreadNameHandle(ThreadPO &threadPO, const std::unique_ptr<SqliteResultSet> &resultSet);
    static void ThreadSortIndexHandle(ThreadPO &threadPO, const std::unique_ptr<SqliteResultSet> &resultSet);
};
}
#endif // PROFILER_SERVER_THREADTABLE_H
