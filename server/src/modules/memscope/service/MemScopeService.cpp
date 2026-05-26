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
#include <algorithm>
#include <stack>
#include "DataBaseManager.h"
#include "MemScopeService.h"

namespace Dic {
namespace Module {
namespace MemScope {
/**
 * 该方法用于将现有的owner再做拆分, 并**原地修改**
 * @param owners 原数据库中查询出的owners，如PTA@ops@aten
 */
static void SplitOwnerInPlace(std::set<std::string> &owners) {
    std::set<std::string> subOwners;
    for (auto &owner : owners) {
        std::istringstream ss(owner);
        std::string part, current;
        while (std::getline(ss, part, OWNER_STRING_DELIMITER)) {
            current = current.empty() ? part : current + OWNER_STRING_DELIMITER + part;
            subOwners.insert(current);
        }
    }
    owners.insert(subOwners.begin(), subOwners.end());
}

void SetMemoryDetailTreeNodesSizeByQueryDatabase(const std::shared_ptr<FullDb::MemScopeDatabase> database,
    std::vector<std::unique_ptr<MemScopeMemoryDetailTreeNode>> &nodes, const BuildDetailTreeContext &buildCtx,
    const int currentDepth) {
    if (currentDepth > MAX_TREE_DEPTH) {
        Server::ServerLog::Warn("[MemScope]The depth of current tree has exceeded %.", MAX_TREE_DEPTH);
        return;
    }
    if (database == nullptr) {
        Server::ServerLog::Error("[MemScope]Some exception occurred: db connections error");
        return;
    }
    for (auto &node : nodes) {
        if (node == nullptr) {
            // 理论上不应该出现，此处做一个防护
            Server::ServerLog::Warn("[MemScope]The node is nullptr.");
            return;
        }
        node->size = database->QueryTotalSizeUntilTimestampUsingOwner(buildCtx.deviceId, buildCtx.timestamp, node->tag);
        if (!node->children.empty()) {
            // 递归构造子节点的大小
            SetMemoryDetailTreeNodesSizeByQueryDatabase(database, node->children, buildCtx, currentDepth + 1);
        }
    }
}

bool MemScopeService::BuildMemoryAllocDetailTreeByContext(
    std::unique_ptr<MemScopeMemoryDetailTreeNode> &detailTree, BuildDetailTreeContext &buildCtx) {
    // detailTree的根节点一定为HAL，但通过标签树无法识别出这样的结构，因此固定构造
    if (detailTree == nullptr) {
        detailTree = std::make_unique<MemScopeMemoryDetailTreeNode>(MEM_SCOPE_ALLOC_OWNER_HAL);
    }
    auto database = Timeline::DataBaseManager::Instance().GetMemScopeDatabase("");
    if (database == nullptr) {
        Server::ServerLog::Error("Cannot get memscope db connections from database manager");
        return false;
    }
    const auto lastHalAllocation = database->QueryLatestAllocationWithinTimestamp(
        buildCtx.deviceId, MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_HAL, buildCtx.timestamp);
    detailTree->size = lastHalAllocation.has_value() ? lastHalAllocation->totalSize : 0;
    const uint64_t minTimestamp = database->GetGlobalMinTimestamp();
    const uint64_t maxTimestamp = database->GetGlobalMaxTimestamp();
    if (buildCtx.relativeTime) {
        buildCtx.timestamp += minTimestamp;
    }
    if (buildCtx.timestamp > maxTimestamp || buildCtx.timestamp < minTimestamp) {
        Server::ServerLog::Warn("Parse memory alloc details failed: invalid timestamp");
        return true;
    }
    // 使用set进行存储（set默认进行字典升序排序）
    std::set<std::string> owners{};
    std::vector<std::string> decomposeEventTypes{buildCtx.eventType};
    // 当查看PTA或MINDSPORE内存池时，拆解也带上ATB、PTA_WORKSPACE
    if (buildCtx.eventType == MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_PTA ||
        buildCtx.eventType == MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_MINDSPORE) {
        decomposeEventTypes.push_back(MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_ATB);
        decomposeEventTypes.push_back(MEM_SCOPE_DUMP_EVENT_TYPE::MALLOC_FREE_PTA_WORKSPACE);
    }
    for (auto &decomposeEventType : decomposeEventTypes) {
        database->QueryMemoryBlocksOwnersReleasedAfterTimestamp(
            buildCtx.deviceId, decomposeEventType, buildCtx.timestamp, owners);
    }
    if (owners.empty()) {
        Server::ServerLog::Warn("Parse memory alloc details: empty data.");
        return true;
    }
    SplitOwnerInPlace(owners);
    auto roots = MemScopeMemoryDetailTreeNode::BuildForestByOrderedTags(owners);
    SetMemoryDetailTreeNodesSizeByQueryDatabase(database, roots, buildCtx, 0);
    if (detailTree->size <= 0) {
        detailTree->size = 0;
        for (const auto &node : roots) {
            detailTree->size += node->size;
        }
    }
    detailTree->children.insert(
        detailTree->children.end(), std::move_iterator(roots.begin()), std::move_iterator(roots.end()));
    return true;
}

bool MemScopeService::IsValidMemoryEventType(const std::string &event, const std::string &eventType) {
    if (EVENT_TYPE_MAP.find(event) == EVENT_TYPE_MAP.end()) {
        return false;
    }
    const auto &eventTypes = EVENT_TYPE_MAP.at(event);
    return eventTypes.find(eventType) != eventTypes.end();
}
} // MemScope
} // Module
} // Dic
