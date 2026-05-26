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
#include "MemScopeDefs.h"
#include "MemScopeEventTree.h"

namespace Dic::Module::MemScope {
MemScopeMemoryDetailTreeNode::MemScopeMemoryDetailTreeNode(std::string nodeTag) {
    tag = std::move(nodeTag);
    name = GetNodeNameByTag(tag);
}

std::vector<std::unique_ptr<MemScopeMemoryDetailTreeNode>> MemScopeMemoryDetailTreeNode::BuildForestByOrderedTags(
    const std::set<std::string> &sortedTags) {
    std::vector<std::unique_ptr<MemScopeMemoryDetailTreeNode>> roots;
    std::unordered_map<std::string, MemScopeMemoryDetailTreeNode *> nodeMap;

    for (const std::string &fullTag : sortedTags) {
        if (fullTag.empty()) {
            continue;
        }
        const size_t lastAt = fullTag.find_last_of(OWNER_STRING_DELIMITER);
        if (lastAt == std::string::npos) {
            // 顶层节点 如（PTA、MINDSPORE、ATB等）
            auto node = std::make_unique<MemScopeMemoryDetailTreeNode>(fullTag);
            nodeMap[fullTag] = node.get();
            roots.push_back(std::move(node));
            continue;
        }
        // 非顶层节点
        std::string parentPath = fullTag.substr(0, lastAt);
        auto it = nodeMap.find(parentPath);
        if (it == nodeMap.end()) {
            // 理论上不应该发生。前序应该处理好所有标签的拆解并排序
            continue;
        }
        MemScopeMemoryDetailTreeNode *parent = it->second;
        auto child = std::make_unique<MemScopeMemoryDetailTreeNode>(fullTag);
        MemScopeMemoryDetailTreeNode *childPtr = child.get();
        parent->children.push_back(std::move(child));
        nodeMap[fullTag] = childPtr;
    }
    return roots;
}

std::string MemScopeMemoryDetailTreeNode::GetNodeNameByTag(const std::string &tag) {
    if (MEM_SCOPE_ALLOC_OWNER_NAME_MAP.find(tag) != MEM_SCOPE_ALLOC_OWNER_NAME_MAP.end()) {
        return MEM_SCOPE_ALLOC_OWNER_NAME_MAP.at(tag);
    }
    std::string lcpOwnerTag;
    std::string resultName;
    for (auto &baseTag : MEM_SCOPE_ALLOC_OWNER_FIXED_TAGS) {
        std::string tempLCP = StringUtil::FindLCP(tag, baseTag);
        if (tempLCP.size() > lcpOwnerTag.size()) {
            lcpOwnerTag = tempLCP;
            resultName = tag.substr(tempLCP.size());
        }
    }
    if (resultName.empty()) {
        resultName = MEM_SCOPE_ALLOC_OWNER_DEFAULT_NAME;
    }
    return resultName;
}
} // Dic::Module::MemScope
