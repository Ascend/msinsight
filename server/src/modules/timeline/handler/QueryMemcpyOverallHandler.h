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

#ifndef PROFILER_SERVER_QUERYMEMCPYOVERALLHANDLER_H
#define PROFILER_SERVER_QUERYMEMCPYOVERALLHANDLER_H

#include <memory>
#include <string>

#include "MemcpyOverallDatabaseAccesser.h"
#include "TimelineRequestHandler.h"
#include "VirtualTraceDatabase.h"

namespace Dic::Module::Timeline {
// 内部累加统计结构（带安全初始化）
struct StatsAccumulator {
    uint64_t totalSize = 0;
    double totalTime = 0.0;
    uint64_t count = 0;
    uint64_t minSize = std::numeric_limits<uint64_t>::max();
    uint64_t maxSize = 0;
    double minTime = std::numeric_limits<double>::max();
    double maxTime = 0.0;

    void Update(uint64_t size, double duration) {
        totalSize += size;
        totalTime += duration;
        ++count;
        minSize = std::min(minSize, size);
        maxSize = std::max(maxSize, size);
        minTime = std::min(minTime, duration);
        maxTime = std::max(maxTime, duration);
    }

    // 安全获取极值（count=0时返回0，避免前端处理NaN/异常值）
    [[nodiscard]] uint64_t GetMinSize() const { return count != 0 ? minSize : 0; }
    [[nodiscard]] uint64_t GetMaxSize() const { return count != 0 ? maxSize : 0; }
    [[nodiscard]] double GetMinTime() const { return count != 0 ? minTime : 0.0; }
    [[nodiscard]] double GetMaxTime() const { return count != 0 ? maxTime : 0.0; }
    [[nodiscard]] double GetAvgSize() const {
        // 保留两位小数
        constexpr int decimalPlaces = 2;
        return NumberUtil::DoubleReservedNDigits(
            (count ? static_cast<double>(totalSize) / static_cast<double>(count) : 0.0), decimalPlaces);
    }
    [[nodiscard]] double GetAvgTime() const {
        // 保留两位小数
        constexpr int decimalPlaces = 2;
        return NumberUtil::DoubleReservedNDigits((count ? totalTime / static_cast<double>(count) : 0.0), decimalPlaces);
    }
};

/**
 * 从Memcpy记录构建统计结果（无副作用、无依赖）
 * @note 内部实现细节，供QueryMemcpyOverallHandler使用，测试专用
 * @warning 不要直接在业务代码中调用（未来可能调整签名）
 */
void BuildMemcpyOverallResult(
    const std::vector<MemcpyRecord> &records, MemcpyOverallResponse &response, uint32_t current, uint32_t pageSize);

class QueryMemcpyOverallHandler : public TimelineRequestHandler {
  public:
    QueryMemcpyOverallHandler() { command = Protocol::REQ_RES_MEMCPY_OVERALL; };

    ~QueryMemcpyOverallHandler() override = default;

    bool HandleRequest(std::unique_ptr<Protocol::Request> requestPtr) override;

  private:
    static bool CalMemcpyData(MemcpyOverallRequest &request, MemcpyOverallResponse &response, std::string &error,
        const std::shared_ptr<VirtualTraceDatabase> &database);
};
} // end of namespace Dic::Module::Timeline

#endif //PROFILER_SERVER_QUERYMEMCPYOVERALLHANDLER_H
