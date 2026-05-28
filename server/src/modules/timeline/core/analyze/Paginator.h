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

#ifndef PROFILER_SERVER_PAGINATOR_H
#define PROFILER_SERVER_PAGINATOR_H

#include <cstdint>
#include <vector>
#include <utility>
#include <algorithm>

#include "NumberSafeUtil.h"

template <typename T> class Paginator {
  public:
    using RecordList = std::vector<T>;

    // 分页构造函数
    explicit Paginator(const RecordList &records, uint32_t pageSize = 10) : records_(records), pageSize_(pageSize) {}

    // 获取指定页码的记录
    RecordList GetPage(uint32_t pageNumber) const {
        if (pageNumber == 0 || records_.empty()) {
            return {}; // 页码从1开始
        }

        uint64_t startIdx = NumberSafe::Muls(pageNumber - 1, pageSize_);
        if (startIdx >= records_.size()) {
            return {}; // 超出范围
        }

        uint32_t endIdx = std::min(startIdx + static_cast<uint64_t>(pageSize_), static_cast<uint64_t>(records_.size()));
        return RecordList(records_.begin() + startIdx, records_.begin() + endIdx);
    }

    // 记录总数
    [[nodiscard]] uint32_t GetTotal() const { return records_.size(); }

  private:
    RecordList records_; // 原始记录列表
    uint32_t pageSize_; // 每页大小
};

#endif //PROFILER_SERVER_PAGINATOR_H
