/* Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 * Licensed under Mulan PSL v2: http://license.coscl.org.cn/MulanPSL2
 */
#ifndef PROFILER_SERVER_MEM_SNAPSHOT_FRAME_TREE_SERVICE_H
#define PROFILER_SERVER_MEM_SNAPSHOT_FRAME_TREE_SERVICE_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Dic::Module::MemSnapshot {
struct SnapshotFrame {
    std::string filename;
    int64_t line{0};
    std::string name;
};

struct SnapshotFrameMetrics {
    uint64_t sizeBytes{0};
    uint64_t requestedBytes{0};
    uint64_t blockCount{0};
};

struct SnapshotFrameNode {
    std::string nodeId;
    std::optional<std::string> parentId;
    uint64_t depth{0};
    std::string kind;
    std::optional<SnapshotFrame> frame;
    SnapshotFrameMetrics total;
    SnapshotFrameMetrics self;
    uint64_t childCount{0};
};

struct SnapshotFrameTree {
    std::vector<SnapshotFrameNode> nodes;
    uint64_t dynamicBlocks{0};
    uint64_t resolvedBlocks{0};
    uint64_t orderedFrameBlocks{0};
};

class MemSnapshotFrameTreeService {
  public:
    // Python 统一查询和聚合；C++ 仅调用、校验结果并缓存最近一棵完整树供分页，不持有数据库连接。
    static std::shared_ptr<const SnapshotFrameTree> Query(const std::string &dataKey, const std::string &deviceId,
        int64_t eventId, bool includeStatic, std::optional<uint32_t> requestId = std::nullopt);
    // 接收线程先登记，再异步执行，避免线程池调度逆序使旧点击覆盖新点击。
    static void NotifyRequest(uint32_t requestId);
    static void ClearCache();
};
}
#endif
