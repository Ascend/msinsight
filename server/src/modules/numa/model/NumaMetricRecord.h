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

#ifndef PROFILER_SERVER_NUMA_METRIC_RECORD_H
#define PROFILER_SERVER_NUMA_METRIC_RECORD_H

#include <cstdint>
#include <string>

namespace Dic::Module::Numa {

/**
 * @brief 数据库聚合后的一条 NUMA 指标记录。
 *
 * Repository 将数据库中的层级定义和采样点聚合为该结构，Assembler 再根据
 * hierarchyDepth、socketName 和 numaName 将扁平记录组装成前端拓扑。
 *
 * 例如一条 NUMA 内存读取记录可能为：
 * socketName = "Socket 0"，numaName = "NUMA Node 0"，
 * metricName = "DRAM Read Bandwidth"，value = 12.5。
 */
struct NumaMetricRecord {
    /** 对应 NUMA_LEVELS_HIERARCHY_NAMES.rowid，用于标识数据库中的层级记录。 */
    int64_t levelId = 0;

    /** 层级深度：0 表示系统级，1 表示 Socket/分组级，2 表示 NUMA 指标级。 */
    int hierarchyDepth = 0;

    /** 最外层节点名称，通常为 "Socket 0"。 */
    std::string socketName;

    /** 第二层节点名称，通常为 "NUMA Node 0" 或 "Cross Socket Bandwidth"。 */
    std::string numaName;

    /** 数据库中的指标原始名称，例如 "DRAM Read Bandwidth"。 */
    std::string metricName;

    /** 指标说明；优先取最深层级的非空 description。 */
    std::string description;

    /** 数据库原始单位，例如 "GB/s"、"GOps/s" 或 "%"。 */
    std::string measurementUnit;

    /** 当前时间范围内是否存在采样值；层级存在但没有采样时为 false。 */
    bool hasValue = false;

    /** 当前时间范围内参与聚合的有效采样点数量。 */
    uint64_t sampleCount = 0;

    /** Repository 返回的原始聚合值；速率指标的时间积分由 Converter 完成。 */
    double value = 0;

    /** 参与聚合的最早绝对时间戳。 */
    uint64_t minTimestamp = 0;

    /** 参与聚合的最晚绝对时间戳。 */
    uint64_t maxTimestamp = 0;
};

} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_NUMA_METRIC_RECORD_H
