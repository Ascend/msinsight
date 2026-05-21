/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_QUERY_MEM_SNAPSHOT_LEAK_STATS_HANDLER_H
#define PROFILER_SERVER_QUERY_MEM_SNAPSHOT_LEAK_STATS_HANDLER_H

#include "MemSnapshotRequestHandler.h"

namespace Dic::Module::MemSnapshot {
class QueryMemSnapshotLeakStatsHandler : public MemSnapshotRequestHandler {
  public:
    QueryMemSnapshotLeakStatsHandler() { command = Protocol::REQ_RES_MEM_SNAPSHOT_LEAK_STATS; }
    ~QueryMemSnapshotLeakStatsHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    const std::string LOG_TAG = "[MemSnapshotLeakStatsHandler] ";
};
} // namespace Dic::Module::MemSnapshot

#endif // PROFILER_SERVER_QUERY_MEM_SNAPSHOT_LEAK_STATS_HANDLER_H
