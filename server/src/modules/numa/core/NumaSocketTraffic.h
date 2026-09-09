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

#ifndef PROFILER_SERVER_NUMA_SOCKET_TRAFFIC_H
#define PROFILER_SERVER_NUMA_SOCKET_TRAFFIC_H

#include <map>
#include <string>
#include <vector>

#include "NumaMetricRecord.h"
#include "NumaProtocol.h"

namespace Dic::Module::Numa {

struct SocketTrafficMetric {
    Protocol::NumaMetricData metric;
    bool outgoing = false;
};

using SocketTrafficKey = std::pair<std::string, std::string>;
using SocketTrafficMetricMap = std::map<SocketTrafficKey, SocketTrafficMetric>;

bool IsSocketTrafficHierarchy(const NumaMetricRecord &record);

void RecordSocketTrafficMetric(
    const NumaMetricRecord &record, Protocol::NumaMetricData metric, SocketTrafficMetricMap &metrics);

void AddSocketConnections(const std::vector<Protocol::NumaSocketData> &sockets,
    const SocketTrafficMetricMap &trafficMetrics, std::vector<Protocol::NumaConnectionData> &connections);

} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_NUMA_SOCKET_TRAFFIC_H
