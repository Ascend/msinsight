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

#ifndef PROFILER_SERVER_TRITONSERVICE_H
#define PROFILER_SERVER_TRITONSERVICE_H
#include <mutex>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <string_view>
#include "TritonMemoryDefs.h"
#include "VirtualAddressManager.h"
#include "GlobalDefs.h"

namespace Dic::Module::Triton {

class TritonService {
  public:
    static TritonService &Instance();
    TritonService(const TritonService &) = delete;
    TritonService(TritonService &&) = delete;
    TritonService &operator=(const TritonService &) = delete;
    void Reset();
    void UpdateRecord(std::map<std::string, TritonRecord> &&records);
    void SetHeader(TritonMemeHeader &&header);
    bool ContainsMemDataOf(const std::string &memType) const;
    [[nodiscard]] TritonMemeHeader GetHeader() const { return header_; }
    [[nodiscard]] std::vector<TritonTensorSegment> QuerySegmentsContainRange(
        const std::string &scopeType, uint64_t timestamp) const;
    [[nodiscard]] std::vector<TritonTensorBlock> QueryBlocksContainRange(
        const std::string &scopeType, uint64_t start, uint64_t end) const;
    void UpdateCompileInfo(const std::string &scopeType, std::pair<std::string, std::string> &&compileInfo);
    [[nodiscard]] std::string GetCompileStatus(const std::string &scopeType) const;
    [[nodiscard]] std::string GetCompileErrMsg(const std::string &scopeType) const;

  private:
    TritonService();
    ~TritonService();
    uint64_t GetMemorySize(std::string_view kernelName, std::string_view scopeType);
    TritonMemeHeader header_;
    std::map<std::string, TritonRecord> records_;
    std::map<std::string, std::pair<std::string, std::string>>
        compileInfo_; // scopeType -- compileStatus, compileErrMsg
    std::map<std::string, std::unique_ptr<VirtualAddressManager>> addressManager_; // memoryTpe -- manager
};
}

#endif //PROFILER_SERVER_TRITONSERVICE_H
