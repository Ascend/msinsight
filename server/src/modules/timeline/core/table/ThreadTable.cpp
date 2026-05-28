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
#include "ThreadTable.h"

namespace Dic {
namespace Module {
namespace Timeline {
void ThreadTable::TrackIdHandle(ThreadPO &threadPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    threadPO.trackId = resultSet->GetUint64(ThreadColumn::TRACK_ID);
}

void ThreadTable::TidHandle(ThreadPO &threadPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    threadPO.tid = resultSet->GetString(ThreadColumn::TID);
}

void ThreadTable::PidHandle(ThreadPO &threadPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    threadPO.pid = resultSet->GetString(ThreadColumn::PID);
}

void ThreadTable::ThreadNameHandle(ThreadPO &threadPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    threadPO.threadName = resultSet->GetString(ThreadColumn::THREAD_NAME);
}

void ThreadTable::ThreadSortIndexHandle(ThreadPO &threadPO, const std::unique_ptr<SqliteResultSet> &resultSet) {
    threadPO.threadSortIndex = resultSet->GetUint64(ThreadColumn::THREAD_SORT_INDEX);
}
}
}
}
