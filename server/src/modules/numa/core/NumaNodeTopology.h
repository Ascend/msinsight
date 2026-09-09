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

#ifndef PROFILER_SERVER_NUMA_NODE_TOPOLOGY_H
#define PROFILER_SERVER_NUMA_NODE_TOPOLOGY_H

#include <vector>

#include "NumaProtocol.h"

namespace Dic::Module::Numa {

/**
 * @brief 根据一个 Socket 中真实存在的 NUMA 指标生成节点通信链路。
 *
 * 生成规则：
 * - 每个 NUMA 节点都生成与 DRAM 的双向链路；缺少采样时指标标记为不可用；
 * - 一个 Socket 恰好包含两个 NUMA 时，生成这两个 NUMA 之间的通信链路；
 *   缺少 Cross SCCL 采样时仍保留链路和不可用指标；
 * - NUMA 数量大于两个时，不猜测 SCCL 配对关系，也不会按数组顺序两两配对。
 *
 * @param socket 已完成指标挂载的 Socket 数据。
 * @param connections 输出链路集合；新链路追加到现有元素末尾。
 */
void AddNumaNodeConnections(
    const Protocol::NumaSocketData &socket, std::vector<Protocol::NumaConnectionData> &connections);

} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_NUMA_NODE_TOPOLOGY_H
