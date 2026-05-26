/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 * PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_QUERY_MEM_SNAPSHOT_EVENT_HANDLER_H
#define PROFILER_SERVER_QUERY_MEM_SNAPSHOT_EVENT_HANDLER_H

#include "MemSnapshotRequestHandler.h"

namespace Dic::Module::MemSnapshot {
class QueryMemSnapshotEventHandler : public MemSnapshotRequestHandler {
  public:
    QueryMemSnapshotEventHandler() { command = Protocol::REQ_RES_MEM_SNAPSHOT_EVENTS; }
    ~QueryMemSnapshotEventHandler() override = default;
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    // 分片请求全量事件列表时，校验请求参数是否存在溢出风险
    bool CheckEventsPaginationParamsOnListRequest(const PaginationParam &params, std::string &errorMsg) const;
    const std::string LOG_TAG = "[MemSnapshotHandler] ";
};
} // namespace Dic::Module::MemSnapshot

#endif // PROFILER_SERVER_QUERY_MEM_SNAPSHOT_EVENT_HANDLER_H
