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
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_QUERY_MEM_SNAPSHOT_ALLOCATION_HANDLER_H
#define PROFILER_SERVER_QUERY_MEM_SNAPSHOT_ALLOCATION_HANDLER_H

#include "MemSnapshotRequestHandler.h"

namespace Dic {
namespace Module {
namespace MemSnapshot {
class QueryMemSnapshotAllocationHandler : public MemSnapshotRequestHandler {
  public:
    QueryMemSnapshotAllocationHandler() { command = Protocol::REQ_RES_MEM_SNAPSHOT_ALLOCATIONS; }
    ~QueryMemSnapshotAllocationHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
};

} // namespace MemSnapshot
} // namespace Module
} // namespace Dic

#endif // PROFILER_SERVER_QUERY_MEM_SNAPSHOT_ALLOCATION_HANDLER_H
