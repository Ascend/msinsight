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

#ifndef PROFILER_SERVER_VIRTUALADDRESSMANAGER_H
#define PROFILER_SERVER_VIRTUALADDRESSMANAGER_H

#include <list>
#include <map>
#include "TritonMemoryDefs.h"

namespace Dic::Module::Triton {

class VirtualAddressManager {
  public:
    VirtualAddressManager();

    // Process a triton record and assign virtual addresses to all its blocks
    void ManageRecord(TritonRecord &tritonRecord);

    // Allocate contiguous space, returns the virtual address
    uint64_t Allocate(uint64_t size);

    // Free the previously allocated space
    void Free(uint64_t address, uint64_t size);

    void Reset();

  private:
    struct FreeBlock {
        uint64_t address;
        uint64_t size;
        FreeBlock(uint64_t addr, uint64_t sz) : address(addr), size(sz) {}
        bool operator<(const FreeBlock &other) const { return address < other.address; }
    };
    uint64_t current_top_address_;
    std::list<FreeBlock> free_blocks_;

    enum class MemoryEventType { FREE, ALLOCATE };

    struct MemoryEvent {
        uint64_t timestamp;
        MemoryEventType type;
        TritonTensorBlock *block;

        MemoryEvent(uint64_t ts, MemoryEventType t, TritonTensorBlock *b) : timestamp(ts), type(t), block(b) {}

        bool operator<(const MemoryEvent &other) const {
            if (timestamp != other.timestamp) {
                return timestamp < other.timestamp;
            }
            return type < other.type; // FREE before ALLOCATE
        }
    };
};

} // namespace Dic::Module::Triton

#endif // PROFILER_SERVER_VIRTUALADDRESSMANAGER_H
