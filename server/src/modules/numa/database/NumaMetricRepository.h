/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_NUMA_METRIC_REPOSITORY_H
#define PROFILER_SERVER_NUMA_METRIC_REPOSITORY_H

#include <memory>
#include <vector>

#include "Database.h"
#include "NumaMetricRecord.h"

namespace Dic::Module::Numa {

/**
 * @brief NUMA 指标数据访问对象。
 *
 * 该类只负责 SQL、参数绑定和结果集解析，不负责生成 Socket、NUMA 或通信链路。
 * Repository 不拥有数据库的唯一所有权，而是与 DataBaseManager 共享数据库对象。
 *
 * 使用示例：
 * @code
 * NumaMetricRepository repository(database);
 * std::vector<NumaMetricRecord> records;
 * const bool success = repository.QueryMetricRecords(startTime, endTime, records);
 * @endcode
 */
class NumaMetricRepository {
  public:
    /**
     * @brief 创建 Repository。
     * @param database 已打开且包含 NUMA 查询表的数据源共享指针；传入空指针时查询返回 false。
     */
    explicit NumaMetricRepository(std::shared_ptr<Database> database);

    /**
     * @brief 查询并聚合 NUMA 指标。
     *
     * 当 startTime < endTime 时，查询闭区间 [startTime, endTime]；否则查询全部采样点。
     * `GB/s` 和 `GOps/s` 指标采用 SUM，其余指标采用 AVG。时间参数通过预编译语句绑定，
     * 不会直接拼接到 SQL 中。
     *
     * @param startTime 相对 NUMA 首个采样点的开始时间；0 与 0 表示不按时间过滤。
     * @param endTime 相对 NUMA 首个采样点的结束时间。
     * @param records 输出记录集合；函数保留调用方已有元素，并在末尾追加查询结果。
     * @return SQL 创建和执行均成功时返回 true，否则返回 false。
     */
    bool QueryMetricRecords(uint64_t startTime, uint64_t endTime, std::vector<NumaMetricRecord> &records) const;

  private:
    std::shared_ptr<Database> database_;
};

} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_NUMA_METRIC_REPOSITORY_H
