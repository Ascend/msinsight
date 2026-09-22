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

#ifndef PROFILER_SERVER_TIMERANGEUTILS_H
#define PROFILER_SERVER_TIMERANGEUTILS_H

#include <cstdint>
#include <limits>

namespace Dic::Module::Timeline {
inline uint64_t AddTimestampOffset(uint64_t timestamp, uint64_t offset) {
    const uint64_t maxTimestamp = std::numeric_limits<uint64_t>::max();
    return timestamp > maxTimestamp - offset ? maxTimestamp : timestamp + offset;
}
} // namespace Dic::Module::Timeline

#endif // PROFILER_SERVER_TIMERANGEUTILS_H
