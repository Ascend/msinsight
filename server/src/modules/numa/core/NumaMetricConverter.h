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

#ifndef PROFILER_SERVER_NUMA_METRIC_CONVERTER_H
#define PROFILER_SERVER_NUMA_METRIC_CONVERTER_H

#include "NumaMetricRecord.h"
#include "NumaProtocol.h"

namespace Dic::Module::Numa {

/**
 * @brief 将数据库指标记录转换为前端协议指标。
 *
 * 转换时保留数据库中的名称、说明和值，并把已知指标名称映射为稳定的前端 key。
 * 对于数据库新增的未知指标，会根据原始名称生成规范化 key，而不会丢弃该指标。
 * `GB/s` 和 `GOps/s` 已由 Repository 在时间范围内求和，因此返回单位分别改为
 * `GB` 和 `GOps`，表示所选时间范围内的累计量。
 *
 * @param record Repository 查询得到的指标记录。
 * @return 可直接放入 NUMA 响应的协议指标。
 *
 * @code
 * NumaMetricRecord record;
 * record.metricName = "DRAM Read Bandwidth";
 * record.measurementUnit = "GB/s";
 * Protocol::NumaMetricData metric = ConvertNumaMetric(record);
 * // metric.key == "dramRead"，metric.unit == "GB"
 * @endcode
 */
Protocol::NumaMetricData ConvertNumaMetric(const NumaMetricRecord &record);

} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_NUMA_METRIC_CONVERTER_H
