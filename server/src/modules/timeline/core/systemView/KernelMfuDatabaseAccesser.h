/*
 * -------------------------------------------------------------------------
 * This file is part of the MindStudio project.
 * Copyright (c) 2026 Huawei Technologies Co.,Ltd.
 *
 * MindStudio is licensed under Mulan PSL v2.
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

#ifndef PROFILER_SERVER_KERNEL_MFU_DATABASE_ACCESSER_H
#define PROFILER_SERVER_KERNEL_MFU_DATABASE_ACCESSER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "TimelineProtocolRequest.h"
#include "TimelineProtocolResponse.h"
#include "VirtualClusterDatabase.h"

namespace Dic::Module::Timeline {
enum class KernelMfuQueryStatus { SUCCESS, UNAVAILABLE, FAILED };

class KernelMfuDatabaseAccesser {
  public:
    static KernelMfuQueryStatus CheckAvailability(const std::shared_ptr<VirtualClusterDatabase> &database);

    static KernelMfuQueryStatus QueryList(const std::shared_ptr<VirtualClusterDatabase> &database,
        const Protocol::KernelMfuListParams &params, std::vector<Protocol::KernelMfuRow> &rows,
        std::vector<std::string> &rankOptions, uint64_t &count);
};
} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_KERNEL_MFU_DATABASE_ACCESSER_H
