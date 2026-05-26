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

#ifndef PROFILER_SERVER_MEM_SCOPE_EVENT_TREE_H
#define PROFILER_SERVER_MEM_SCOPE_EVENT_TREE_H

#include "pch.h"

namespace Dic {
namespace Module {
namespace MemScope {
// memscope owner可能的表达前缀
// 为便于构造树形结构逻辑Owner, 实际不存在
const std::string MEM_SCOPE_ALLOC_OWNER_HAL = "HAL";
const std::string MEM_SCOPE_ALLOC_OWNER_HAL_NAME = "Process_Total_Memory_Allocation";
const std::string MEM_SCOPE_ALLOC_OWNER_CANN = "CANN";
const std::string MEM_SCOPE_ALLOC_OWNER_CANN_NAME = "CANN_Total_Memory_Allocation";
const std::string MEM_SCOPE_ALLOC_OWNER_ATB = "ATB";
const std::string MEM_SCOPE_ALLOC_OWNER_ATB_NAME = "ATB_Reserved";
const std::string MEM_SCOPE_ALLOC_OWNER_MINDSPORE = "MINDSPORE";
const std::string MEM_SCOPE_ALLOC_OWNER_MINDSPORE_NAME = "MindSpore_Reserved";
// 只有PTA支持细分
const std::string MEM_SCOPE_ALLOC_OWNER_PTA = "PTA";
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_NAME = "PTA_Reserved";
// 代表PTA-算子申请
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_OPS = "PTA@ops";
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_OPS_ATEN = "PTA@ops@aten";
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_OPS_NAME = "PTA_Operator";
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_OPS_ATEN_NAME = "ATEN";
// 代表PTA-模型  还可细分
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_MODEL = "PTA@model";
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_NAME = "PTA_Model";
// 代表PTA-模型-优化器
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_OPTIMIZER = "PTA@model@optimizer_state";
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_OPTIMIZER_NAME = "Optimizer";
// 代表PTA-模型-权重
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_WEIGHT = "PTA@model@weight";
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_WEIGHT_NAME = "Weight";
// 代表PTA-模型-梯度
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_GRADIENT = "PTA@model@gradient";
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_GRADIENT_NAME = "Gradient";
// 代表PTA-WORKSPACE
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE = "PTA_WORKSPACE";
const std::string MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE_NAME = "PTA_Workspace";
// 缺省名
const std::string MEM_SCOPE_ALLOC_OWNER_DEFAULT_NAME = "Other";
const std::set<std::string> MEM_SCOPE_ALLOC_OWNER_FIXED_TAGS = {MEM_SCOPE_ALLOC_OWNER_HAL, MEM_SCOPE_ALLOC_OWNER_CANN,
    MEM_SCOPE_ALLOC_OWNER_ATB, MEM_SCOPE_ALLOC_OWNER_MINDSPORE, MEM_SCOPE_ALLOC_OWNER_PTA,
    MEM_SCOPE_ALLOC_OWNER_PTA_OPS, MEM_SCOPE_ALLOC_OWNER_PTA_OPS_ATEN, MEM_SCOPE_ALLOC_OWNER_PTA_MODEL,
    MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_OPTIMIZER, MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_WEIGHT,
    MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_GRADIENT, MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE};
const std::set<std::string> MEM_SCOPE_ALLOC_OWNER_FRAMEWORK_BASE_TAGS = {MEM_SCOPE_ALLOC_OWNER_ATB,
    MEM_SCOPE_ALLOC_OWNER_MINDSPORE, MEM_SCOPE_ALLOC_OWNER_PTA, MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE};
const std::set<std::string> MEM_SCOPE_ALLOC_OWNER_CANN_BASE_TAGS = {MEM_SCOPE_ALLOC_OWNER_CANN};
const std::unordered_map<std::string, std::string> MEM_SCOPE_ALLOC_OWNER_NAME_MAP = {
    {MEM_SCOPE_ALLOC_OWNER_HAL, MEM_SCOPE_ALLOC_OWNER_HAL_NAME},
    {MEM_SCOPE_ALLOC_OWNER_ATB, MEM_SCOPE_ALLOC_OWNER_ATB_NAME},
    {MEM_SCOPE_ALLOC_OWNER_PTA, MEM_SCOPE_ALLOC_OWNER_PTA_NAME},
    {MEM_SCOPE_ALLOC_OWNER_MINDSPORE, MEM_SCOPE_ALLOC_OWNER_MINDSPORE_NAME},
    {MEM_SCOPE_ALLOC_OWNER_CANN, MEM_SCOPE_ALLOC_OWNER_CANN_NAME},
    {MEM_SCOPE_ALLOC_OWNER_PTA_OPS, MEM_SCOPE_ALLOC_OWNER_PTA_OPS_NAME},
    {MEM_SCOPE_ALLOC_OWNER_PTA_OPS_ATEN, MEM_SCOPE_ALLOC_OWNER_PTA_OPS_ATEN_NAME},
    {MEM_SCOPE_ALLOC_OWNER_PTA_MODEL, MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_NAME},
    {MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_OPTIMIZER, MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_OPTIMIZER_NAME},
    {MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_WEIGHT, MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_WEIGHT_NAME},
    {MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_GRADIENT, MEM_SCOPE_ALLOC_OWNER_PTA_MODEL_GRADIENT_NAME},
    {MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE, MEM_SCOPE_ALLOC_OWNER_PTA_WORKSPACE_NAME}};
// 树最大深度
const int MAX_TREE_DEPTH = 20;

// 基于标签生成的内存分类分级树
class MemScopeMemoryDetailTreeNode {
  public:
    explicit MemScopeMemoryDetailTreeNode(std::string nodeTag);
    std::string name;
    uint64_t size{0};
    std::string tag;
    std::vector<std::unique_ptr<MemScopeMemoryDetailTreeNode>> children;

    static std::vector<std::unique_ptr<MemScopeMemoryDetailTreeNode>> BuildForestByOrderedTags(
        const std::set<std::string> &sortedTags);
    static std::string GetNodeNameByTag(const std::string &tag);
};
} // Memory
} // Module
} // Dic
#endif // PROFILER_SERVER_MEM_SCOPE_EVENT_TREE_H
