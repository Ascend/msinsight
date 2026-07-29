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
 * -------------------------------------------------------------------------
 */

#ifndef PROFILER_SERVER_MEMSNAPSHOTALLOCATIONDATAPROCESSOR_H
#define PROFILER_SERVER_MEMSNAPSHOTALLOCATIONDATAPROCESSOR_H

#include <vector>
#include "MemSnapshotResponseDTO.h"

namespace Dic::Module::MemSnapshot {
class MemSnapshotAllocationDataProcessor {
  public:
    static std::vector<Protocol::AllocationRecordDTO> ExtractAllocationTurningPoints(
        const std::vector<Protocol::AllocationRecord> &records);
    static std::vector<Protocol::ReservedRecordDTO> CompressReservedLine(
        const std::vector<Protocol::AllocationRecord> &records);

  private:
    static constexpr int64_t MAX_TIMESTAMP_INTERVAL_COUNT = 1000;
};
}

#endif // PROFILER_SERVER_MEMSNAPSHOTALLOCATIONDATAPROCESSOR_H
