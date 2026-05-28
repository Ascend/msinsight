#include "VirtualAddressManager.h"
#include <algorithm>
#include "TritonMemoryDefs.h"
#include "TritonService.h"
#include "ServerLog.h"

namespace Dic::Module::Triton {
using namespace Dic::Server;
VirtualAddressManager::VirtualAddressManager() : current_top_address_(0) { Reset(); }

void VirtualAddressManager::ManageRecord(TritonRecord &tritonRecord) {
    std::vector<MemoryEvent> events;
    // Disassemble blocks into allocation and free events
    for (auto &segment : tritonRecord.segments) {
        for (auto &block : segment.blocks) {
            events.push_back(MemoryEvent(block.start, MemoryEventType::ALLOCATE, &block));
            events.push_back(MemoryEvent(block.end, MemoryEventType::FREE, &block));
        }
    }

    // Sort events
    std::sort(events.begin(), events.end());

    // Manage available memory and assign calculated addresses
    Reset();
    for (const auto &event : events) {
        if (event.type == MemoryEventType::ALLOCATE) {
            event.block->virtualAddress = Allocate(event.block->size);
            event.block->id = std::to_string(event.block->virtualAddress) + std::to_string(event.block->start);
        } else if (event.type == MemoryEventType::FREE) {
            Free(event.block->virtualAddress, event.block->size);
        }
    }
}

void VirtualAddressManager::Reset() {
    free_blocks_.clear();
    current_top_address_ = 0;
}

uint64_t VirtualAddressManager::Allocate(uint64_t size) {
    if (size == 0) {
        return 0;
    }

    for (auto it = free_blocks_.begin(); it != free_blocks_.end(); ++it) {
        if (it->size >= size) {
            uint64_t allocated_addr = it->address;
            if (it->size == size) {
                free_blocks_.erase(it);
            } else {
                it->address += size;
                it->size -= size;
            }
            return allocated_addr;
        }
    }

    // Allocate from top unallocated space
    uint64_t allocated_addr = current_top_address_;
    current_top_address_ += size;
    return allocated_addr;
}

void VirtualAddressManager::Free(uint64_t address, uint64_t size) {
    if (address == UINT64_MAX) {
        return;
    }

    auto it = free_blocks_.begin();
    while (it != free_blocks_.end() && it->address < address) {
        ++it;
    }
    it = free_blocks_.insert(it, VirtualAddressManager::FreeBlock(address, size));

    // Merge with previous
    if (it != free_blocks_.begin()) {
        auto prev = std::prev(it);
        if (prev->address + prev->size == it->address) {
            prev->size += it->size;
            free_blocks_.erase(it);
            it = prev;
        }
    }

    // Merge with next
    auto next = std::next(it);
    if (next != free_blocks_.end()) {
        if (it->address + it->size == next->address) {
            it->size += next->size;
            free_blocks_.erase(next);
        }
    }

    // Merge with top unallocated space
    if (it->address + it->size == current_top_address_) {
        current_top_address_ = it->address;
        free_blocks_.erase(it);
    }
}

} // namespace Dic::Module::Triton
