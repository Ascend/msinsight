/* Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2: http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_QUERY_MEM_SNAPSHOT_FRAME_TREE_HANDLER_H
#define PROFILER_SERVER_QUERY_MEM_SNAPSHOT_FRAME_TREE_HANDLER_H
#include "MemSnapshotRequestHandler.h"
namespace Dic::Module::MemSnapshot {
class QueryMemSnapshotFrameTreeHandler : public MemSnapshotRequestHandler {
  public:
    QueryMemSnapshotFrameTreeHandler() {
        command = Protocol::REQ_RES_MEM_SNAPSHOT_FRAME_TREE;
        async = true;
    }
    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;
};
}
#endif
