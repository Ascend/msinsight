/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_NUMA_OVERVIEW_ASSEMBLER_H
#define PROFILER_SERVER_NUMA_OVERVIEW_ASSEMBLER_H

#include <vector>

#include "NumaMetricRecord.h"
#include "NumaProtocol.h"

namespace Dic::Module::Numa {

/**
 * @brief NUMA 概览页面所需的完整业务数据。
 *
 * Repository 返回的是数据库中的扁平指标记录；本结构体则对应前端真正需要的
 * 时间范围、系统指标、Socket/NUMA 拓扑和可点击通信链路。
 */
struct NumaOverviewData {
    uint64_t rangeStart = 0;
    uint64_t rangeEnd = 0;
    std::vector<Protocol::NumaMetricData> totalMetrics;
    std::vector<Protocol::NumaSocketData> sockets;
    std::vector<Protocol::NumaConnectionData> connections;
};

/**
 * @brief 将数据库指标记录组装为 NUMA 高层架构图数据。
 *
 * 该类不访问数据库，也不发送响应，只负责业务结构转换。因此它可以脱离 Handler
 * 单独测试，后续扩展 Socket 数量或新增链路类型时也不会影响请求处理流程。
 */
class NumaOverviewAssembler {
  public:
    /**
     * @brief 根据数据库中实际存在的层级和指标构建 NUMA 概览。
     * @param records Repository 查询得到的扁平指标记录；调用方保留其所有权。
     * @return 可直接写入 `NumaOverviewResponse` 的完整数据。
     *
     * @code
     * NumaOverviewAssembler assembler;
     * NumaOverviewData overview = assembler.Build(records);
     * @endcode
     */
    NumaOverviewData Build(const std::vector<NumaMetricRecord> &records) const;
};

} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_NUMA_OVERVIEW_ASSEMBLER_H
