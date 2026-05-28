// /*
//  * -------------------------------------------------------------------------
//  * This file is part of the MindStudio project.
//  * Copyright (c)  2026 Huawei Technologies Co.,Ltd.
//  *
//  * MindStudio is licensed under Mulan PSL v2.
//  * You can use this software according to the terms and conditions of the Mulan PSL v2.
//  * You may obtain a copy of Mulan PSL v2 at:
//  *
//  *          http://license.coscl.org.cn/MulanPSL2
//  *
//  * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
//  * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
//  * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
//  * See the Mulan PSL v2 for more details.
//  * -------------------------------------------------------------------------
//  *

#include "TritonService.h"
#include <algorithm>
#include <limits>

namespace Dic::Module::Triton {
TritonService::TritonService() {}

TritonService::~TritonService() = default;

uint64_t TritonService::GetMemorySize(std::string_view kernelName, std::string_view memoryType) {
    // current given 192kB
    return 192 * 1024 * 8;
}

TritonService &TritonService::Instance() {
    static TritonService instance;
    return instance;
}

void TritonService::Reset() {
    header_ = {};
    records_.clear();
    addressManager_.clear();
    compileInfo_.clear();
}

void TritonService::UpdateRecord(std::map<std::string, TritonRecord> &&records) {
    records_ = std::move(records);
    for (auto &[type, record] : records_) {
        if (addressManager_.find(type) == addressManager_.end()) {
            addressManager_[type] = std::make_unique<VirtualAddressManager>();
        }
        addressManager_[type]->ManageRecord(record);
    }
}

void TritonService::SetHeader(TritonMemeHeader &&header) { header_ = std::move(header); }

bool TritonService::ContainsMemDataOf(const std::string &memType) const {
    if (records_.find(memType) != records_.end()) {
        return true;
    }
    Server::ServerLog::Warn("Not found current memory type data");
    return false;
}

std::vector<TritonTensorSegment> TritonService::QuerySegmentsContainRange(
    const std::string &memoryType, uint64_t timestamp) const {
    std::vector<TritonTensorSegment> result;
    if (!ContainsMemDataOf(memoryType)) {
        return result;
    }
    for (const auto &seg : records_.at(memoryType).segments) {
        if (seg.start <= timestamp && timestamp <= seg.end) {
            result.push_back(seg);
        }
    }
    return result;
}

std::vector<TritonTensorBlock> TritonService::QueryBlocksContainRange(
    const std::string &memoryType, uint64_t start, uint64_t end) const {
    std::vector<TritonTensorBlock> result;
    auto tranSegmentToBlock = [](const TritonTensorSegment &item, std::vector<TritonTensorBlock> &blocks) {
        for (const auto &block : item.blocks) {
            TritonTensorBlock newBlock = block;
            newBlock.buffer = item.buffer;
            newBlock.sourceLocation = item.sourceLocation;
            blocks.push_back(std::move(newBlock));
        }
    };
    if (!ContainsMemDataOf(memoryType)) {
        return result;
    }

    bool isFullRange = (start == 0 && end == std::numeric_limits<uint64_t>::max());
    const TritonRecord &record = records_.at(memoryType);
    for (const auto &segment : record.segments) {
        if (isFullRange) {
            tranSegmentToBlock(segment, result);
        } else if (start <= end) {
            if (segment.start <= start && segment.end >= end) {
                tranSegmentToBlock(segment, result);
            }
        }
    }
    return result;
}

void TritonService::UpdateCompileInfo(const std::string &scopeType, std::pair<std::string, std::string> &&compileInfo) {
    compileInfo_[scopeType] = std::move(compileInfo);
}

std::string TritonService::GetCompileStatus(const std::string &scopeType) const {
    if (compileInfo_.find(scopeType) == compileInfo_.end()) {
        return "Unknown";
    }
    return compileInfo_.at(scopeType).first;
}

std::string TritonService::GetCompileErrMsg(const std::string &scopeType) const {
    if (compileInfo_.find(scopeType) == compileInfo_.end()) {
        return "Unknown";
    }
    return compileInfo_.at(scopeType).second;
}
} // Triton
// Module
// Dic
