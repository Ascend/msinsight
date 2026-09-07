/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_NUMA_DATA_SOURCE_RESOLVER_H
#define PROFILER_SERVER_NUMA_DATA_SOURCE_RESOLVER_H

#include <memory>
#include <string>

#include "Database.h"

namespace Dic::Module::Numa {

struct NumaDataSourceContext {
    std::string rankId;
    std::string fileId;
};

/**
 * @brief 为一次 NUMA 请求选择可用数据库。
 *
 * 仅解析 DataBaseManager 中已注册且包含完整 NUMA 表的 Insight 主库。
 */
class NumaDataSourceResolver {
  public:
    [[nodiscard]] std::shared_ptr<Database> Resolve(const NumaDataSourceContext &context) const;
};

} // namespace Dic::Module::Numa

#endif // PROFILER_SERVER_NUMA_DATA_SOURCE_RESOLVER_H
